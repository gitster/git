#include "fsmonitor.h"
#include "fsmonitor-path-utils.h"
#include "abspath.h"
#include <errno.h>
#include <mntent.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>

static int is_remote_fs(const char *path) {
	struct statfs fs;

	if (statfs(path, &fs)) {
		error_errno(_("statfs('%s') failed"), path);
		return -1;
	}

	switch (fs.f_type) {
	case 0x61636673:  /* ACFS */
	case 0x5346414F:  /* AFS */
	case 0x00C36400:  /* CEPH */
	case 0xFF534D42:  /* CIFS */
	case 0x73757245:  /* CODA */
	case 0x19830326:  /* FHGFS */
	case 0x1161970:   /* GFS */
	case 0x47504653:  /* GPFS */
	case 0x013111A8:  /* IBRIX */
	case 0x6B414653:  /* KAFS */
	case 0x0BD00BD0:  /* LUSTRE */
	case 0x564C:      /* NCP */
	case 0x6969:      /* NFS */
	case 0x6E667364:  /* NFSD */
	case 0x7461636f:  /* OCFS2 */
	case 0xAAD7AAEA:  /* PANFS */
	case 0x517B:      /* SMB */
	case 0xBEEFDEAD:  /* SNFS */
	case 0xFE534D42:  /* SMB2 */
	case 0xBACBACBC:  /* VMHGFS */
	case 0xA501FCF5:  /* VXFS */
		return 1;
	default:
		break;
	}

	return 0;
}

static int find_mount(const char *path, const struct statvfs *fs,
	struct mntent *entry)
{
	const char *const mounts = "/proc/mounts";
	char *rp = real_pathdup(path, 1);
	struct mntent *ment = NULL;
	struct statvfs mntfs;
	FILE *fp;
	int found = 0;
	int dlen, plen, flen = 0;

	entry->mnt_fsname = NULL;
	entry->mnt_dir = NULL;
	entry->mnt_type = NULL;

	fp = setmntent(mounts, "r");
	if (!fp) {
		free(rp);
		error_errno(_("setmntent('%s') failed"), mounts);
		return -1;
	}

	plen = strlen(rp);

	/* read all the mount information and compare to path */
	while ((ment = getmntent(fp)) != NULL) {
		if (statvfs(ment->mnt_dir, &mntfs)) {
			switch (errno) {
			case EPERM:
			case ESRCH:
			case EACCES:
				continue;
			default:
				error_errno(_("statvfs('%s') failed"), ment->mnt_dir);
				endmntent(fp);
				free(rp);
				return -1;
			}
		}

		/* is mount on the same filesystem and is a prefix of the path */
		if ((fs->f_fsid == mntfs.f_fsid) &&
			!strncmp(ment->mnt_dir, rp, strlen(ment->mnt_dir))) {
			dlen = strlen(ment->mnt_dir);
			if (dlen > plen)
				continue;
			/*
			 * look for the longest prefix (including root)
			 */
			if (dlen > flen &&
				((dlen == 1 && ment->mnt_dir[0] == '/') ||
				 (!rp[dlen] || rp[dlen] == '/'))) {
				flen = dlen;
				found = 1;

				/*
				 * https://man7.org/linux/man-pages/man3/getmntent.3.html
				 *
				 * The pointer points to a static area of memory which is
				 * overwritten by subsequent calls to getmntent().
				 */
				free(entry->mnt_fsname);
				free(entry->mnt_dir);
				free(entry->mnt_type);
				entry->mnt_fsname = xstrdup(ment->mnt_fsname);
				entry->mnt_dir = xstrdup(ment->mnt_dir);
				entry->mnt_type = xstrdup(ment->mnt_type);
			}
		}
	}
	endmntent(fp);
	free(rp);

	if (!found)
		return -1;

	return 0;
}

int fsmonitor__get_fs_info(const char *path, struct fs_info *fs_info)
{
	struct mntent entry;
	struct statvfs fs;

	fs_info->is_remote = -1;
	fs_info->typename = NULL;

	if (statvfs(path, &fs))
		return error_errno(_("statvfs('%s') failed"), path);

	if (find_mount(path, &fs, &entry) < 0) {
		free(entry.mnt_fsname);
		free(entry.mnt_dir);
		free(entry.mnt_type);
		return -1;
	}

	trace_printf_key(&trace_fsmonitor,
			 "statvfs('%s') [flags 0x%08lx] '%s' '%s'",
			 path, fs.f_flag, entry.mnt_type, entry.mnt_fsname);

	fs_info->is_remote = is_remote_fs(entry.mnt_dir);
	fs_info->typename = xstrdup(entry.mnt_fsname);
	free(entry.mnt_fsname);
	free(entry.mnt_dir);
	free(entry.mnt_type);

	if (fs_info->is_remote < 0)
		return -1;

	trace_printf_key(&trace_fsmonitor,
				"'%s' is_remote: %d",
				path, fs_info->is_remote);

	return 0;
}

int fsmonitor__is_fs_remote(const char *path)
{
	struct fs_info fs;

	if (fsmonitor__get_fs_info(path, &fs)) {
		free(fs.typename);
		return -1;
	}

	free(fs.typename);

	return fs.is_remote;
}

/*
 * No-op for now.
 */
int fsmonitor__get_alias(const char *path, struct alias_info *info)
{
	return 0;
}

/*
 * No-op for now.
 */
char *fsmonitor__resolve_alias(const char *path,
	const struct alias_info *info)
{
	return NULL;
}
