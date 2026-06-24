#ifndef ODB_TRANSACTION_H
#define ODB_TRANSACTION_H

#include "git-compat-util.h"
#include "gettext.h"
#include "odb.h"
#include "odb/source.h"

/*
 * A transaction may be started for an object database prior to writing new
 * objects via odb_transaction_begin(). These objects are not committed until
 * odb_transaction_commit() is invoked. Only a single transaction may be pending
 * at a time.
 *
 * Each ODB source is expected to implement its own transaction handling.
 */
struct odb_transaction {
	/* The ODB source the transaction is opened against. */
	struct odb_source *source;

	/* The ODB source specific callback invoked to commit a transaction. */
	int (*commit)(struct odb_transaction *transaction);

	/*
	 * This callback is expected to write the given object stream into
	 * the ODB transaction. Note that for now, only blobs support streaming.
	 *
	 * The resulting object ID shall be written into the out pointer. The
	 * callback is expected to return 0 on success, a negative error code
	 * otherwise.
	 */
	int (*write_object_stream)(struct odb_transaction *transaction,
				   struct odb_write_stream *stream, size_t len,
				   struct object_id *oid);

	/*
	 * This callback is expected to return a NULL-terminated array of
	 * environment variables that a child process should inherit so
	 * that its object writes participate in the transaction. The
	 * returned array is owned by the backend and remains valid until
	 * the transaction ends. May return NULL when the backend does not
	 * need to expose any state to child processes.
	 */
	const char **(*env)(struct odb_transaction *transaction);
};

/*
 * Starts an ODB transaction and returns it via `out`. Subsequent objects are
 * written to the transaction and not committed until odb_transaction_commit()
 * is invoked on the transaction. Returns 0 on success and a negative value on
 * error. If the ODB already has a pending transaction, `out` is set to NULL.
 */
int odb_transaction_begin(struct object_database *odb,
			  struct odb_transaction **out);

static inline void odb_transaction_begin_or_die(struct object_database *odb,
						struct odb_transaction **out)
{
	if (odb_transaction_begin(odb, out))
		die(_("failed to start ODB transaction"));
}

/*
 * Commits an ODB transaction making the written objects visible. If the
 * specified transaction is NULL, the function is a no-op.
 */
int odb_transaction_commit(struct odb_transaction *transaction);

/*
 * Writes the object in the provided stream into the transaction. The resulting
 * object ID is written into the out pointer. Returns 0 on success, a negative
 * error code otherwise.
 */
int odb_transaction_write_object_stream(struct odb_transaction *transaction,
					struct odb_write_stream *stream,
					size_t len, struct object_id *oid);

/*
 * Returns a NULL-terminated array of environment variables that a child
 * process should inherit so that its object writes participate in the
 * transaction, suitable for passing via child_process.env. Returns NULL if
 * the transaction is NULL or the backend does not expose any state to child
 * processes.
 */
const char **odb_transaction_env(struct odb_transaction *transaction);

#endif
