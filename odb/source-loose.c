#include "git-compat-util.h"
#include "abspath.h"
#include "chdir-notify.h"
#include "gettext.h"
#include "loose.h"
#include "object-file.h"
#include "odb.h"
#include "odb/source-files.h"
#include "odb/source-loose.h"
#include "odb/streaming.h"
#include "oidtree.h"
#include "strbuf.h"

static int odb_source_loose_read_object_info(struct odb_source *source,
					     const struct object_id *oid,
					     struct object_info *oi,
					     enum object_info_flags flags)
{
	struct odb_source_loose *loose = odb_source_loose_downcast(source);
	static struct strbuf buf = STRBUF_INIT;

	/*
	 * The second read shouldn't cause new loose objects to show up, unless
	 * there was a race condition with a secondary process. We don't care
	 * about this case though, so we simply skip reading loose objects a
	 * second time.
	 */
	if (flags & OBJECT_INFO_SECOND_READ)
		return -1;

	odb_loose_path(source, &buf, oid);
	return read_object_info_from_path(loose, buf.buf, oid, oi, flags);
}

/*
 * Find "oid" as a loose object in given source, open the object and return its
 * file descriptor. Returns the file descriptor on success, negative on failure.
 *
 * The "path" out-parameter will give the path of the object we found (if any).
 * Note that it may point to static storage and is only valid until another
 * call to open_loose_object().
 */
static int open_loose_object(struct odb_source_loose *loose,
			     const struct object_id *oid, const char **path)
{
	static struct strbuf buf = STRBUF_INIT;
	int fd;

	*path = odb_loose_path(&loose->base, &buf, oid);
	fd = git_open(*path);
	if (fd >= 0)
		return fd;

	return -1;
}

static void *odb_source_loose_map_object(struct odb_source_loose *loose,
					 const struct object_id *oid,
					 unsigned long *size)
{
	const char *p;
	int fd = open_loose_object(loose, oid, &p);
	void *map = NULL;
	struct stat st;

	if (fd < 0)
		return NULL;

	if (!fstat(fd, &st)) {
		*size = xsize_t(st.st_size);
		if (!*size) {
			/* mmap() is forbidden on empty files */
			error(_("object file %s is empty"), p);
			goto out;
		}

		map = xmmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
	}

out:
	close(fd);
	return map;
}

struct odb_loose_read_stream {
	struct odb_read_stream base;
	git_zstream z;
	enum {
		ODB_LOOSE_READ_STREAM_INUSE,
		ODB_LOOSE_READ_STREAM_DONE,
		ODB_LOOSE_READ_STREAM_ERROR,
	} z_state;
	void *mapped;
	unsigned long mapsize;
	char hdr[32];
	int hdr_avail;
	int hdr_used;
};

static ssize_t read_istream_loose(struct odb_read_stream *_st, char *buf, size_t sz)
{
	struct odb_loose_read_stream *st =
		container_of(_st, struct odb_loose_read_stream, base);
	size_t total_read = 0;

	switch (st->z_state) {
	case ODB_LOOSE_READ_STREAM_DONE:
		return 0;
	case ODB_LOOSE_READ_STREAM_ERROR:
		return -1;
	default:
		break;
	}

	if (st->hdr_used < st->hdr_avail) {
		size_t to_copy = st->hdr_avail - st->hdr_used;
		if (sz < to_copy)
			to_copy = sz;
		memcpy(buf, st->hdr + st->hdr_used, to_copy);
		st->hdr_used += to_copy;
		total_read += to_copy;
	}

	while (total_read < sz) {
		int status;

		st->z.next_out = (unsigned char *)buf + total_read;
		st->z.avail_out = sz - total_read;
		status = git_inflate(&st->z, Z_FINISH);

		total_read = st->z.next_out - (unsigned char *)buf;

		if (status == Z_STREAM_END) {
			git_inflate_end(&st->z);
			st->z_state = ODB_LOOSE_READ_STREAM_DONE;
			break;
		}
		if (status != Z_OK && (status != Z_BUF_ERROR || total_read < sz)) {
			git_inflate_end(&st->z);
			st->z_state = ODB_LOOSE_READ_STREAM_ERROR;
			return -1;
		}
	}
	return total_read;
}

static int close_istream_loose(struct odb_read_stream *_st)
{
	struct odb_loose_read_stream *st =
		container_of(_st, struct odb_loose_read_stream, base);

	if (st->z_state == ODB_LOOSE_READ_STREAM_INUSE)
		git_inflate_end(&st->z);
	munmap(st->mapped, st->mapsize);
	return 0;
}

static int odb_source_loose_read_object_stream(struct odb_read_stream **out,
					       struct odb_source *source,
					       const struct object_id *oid)
{
	struct odb_source_loose *loose = odb_source_loose_downcast(source);
	struct object_info oi = OBJECT_INFO_INIT;
	struct odb_loose_read_stream *st;
	unsigned long mapsize;
	unsigned long size_ul;
	void *mapped;

	mapped = odb_source_loose_map_object(loose, oid, &mapsize);
	if (!mapped)
		return -1;

	/*
	 * Note: we must allocate this structure early even though we may still
	 * fail. This is because we need to initialize the zlib stream, and it
	 * is not possible to copy the stream around after the fact because it
	 * has self-referencing pointers.
	 */
	CALLOC_ARRAY(st, 1);

	switch (unpack_loose_header(&st->z, mapped, mapsize, st->hdr,
				    sizeof(st->hdr))) {
	case ULHR_OK:
		break;
	case ULHR_BAD:
	case ULHR_TOO_LONG:
		goto error;
	}

	/*
	 * object_info.sizep is unsigned long* (32-bit on Windows), but
	 * st->base.size is size_t (64-bit). Use temporary variable.
	 * Note: loose objects >4GB would still truncate here, but such
	 * large loose objects are uncommon (they'd normally be packed).
	 */
	oi.sizep = &size_ul;
	oi.typep = &st->base.type;

	if (parse_loose_header(st->hdr, &oi) < 0 || st->base.type < 0)
		goto error;
	st->base.size = size_ul;

	st->mapped = mapped;
	st->mapsize = mapsize;
	st->hdr_used = strlen(st->hdr) + 1;
	st->hdr_avail = st->z.total_out;
	st->z_state = ODB_LOOSE_READ_STREAM_INUSE;
	st->base.close = close_istream_loose;
	st->base.read = read_istream_loose;

	*out = &st->base;

	return 0;
error:
	git_inflate_end(&st->z);
	munmap(mapped, mapsize);
	free(st);
	return -1;
}

static void odb_source_loose_clear_cache(struct odb_source_loose *loose)
{
	oidtree_clear(loose->cache);
	FREE_AND_NULL(loose->cache);
	memset(&loose->subdir_seen, 0,
	       sizeof(loose->subdir_seen));
}

static void odb_source_loose_reprepare(struct odb_source *source)
{
	struct odb_source_loose *loose = odb_source_loose_downcast(source);
	odb_source_loose_clear_cache(loose);
}

static void odb_source_loose_close(struct odb_source *source UNUSED)
{
	/* Nothing to do. */
}

static void odb_source_loose_reparent(const char *name UNUSED,
				      const char *old_cwd,
				      const char *new_cwd,
				      void *cb_data)
{
	struct odb_source_loose *loose = cb_data;
	char *path = reparent_relative_path(old_cwd, new_cwd,
					    loose->base.path);
	free(loose->base.path);
	loose->base.path = path;
}

static void odb_source_loose_free(struct odb_source *source)
{
	struct odb_source_loose *loose = odb_source_loose_downcast(source);
	odb_source_loose_clear_cache(loose);
	loose_object_map_clear(&loose->map);
	chdir_notify_unregister(NULL, odb_source_loose_reparent, loose);
	odb_source_release(&loose->base);
	free(loose);
}

struct odb_source_loose *odb_source_loose_new(struct odb_source_files *files)
{
	struct odb_source_loose *loose;

	CALLOC_ARRAY(loose, 1);
	odb_source_init(&loose->base, files->base.odb, ODB_SOURCE_LOOSE,
			files->base.path, files->base.local);
	loose->files = files;

	loose->base.free = odb_source_loose_free;
	loose->base.close = odb_source_loose_close;
	loose->base.reprepare = odb_source_loose_reprepare;
	loose->base.read_object_info = odb_source_loose_read_object_info;
	loose->base.read_object_stream = odb_source_loose_read_object_stream;

	if (!is_absolute_path(loose->base.path))
		chdir_notify_register(NULL, odb_source_loose_reparent, loose);

	return loose;
}
