#ifndef ODB_TRANSACTION_H
#define ODB_TRANSACTION_H

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

	/*
	 * The ODB source specific callback invoked to commit a transaction.
	 * Returns 0 on success, a negative error code otherwise.
	 */
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
};

/*
 * Starts an ODB transaction and returns it via `out`. Subsequent objects are
 * written to the transaction and not committed until odb_transaction_commit()
 * is invoked on the transaction. Returns 0 on success and a negative value on
 * error. Note that it is considered an error to start a new transaction if the
 * ODB already has an inflight transaction pending.
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
void odb_transaction_commit(struct odb_transaction *transaction);

/*
 * Writes the object in the provided stream into the transaction. The resulting
 * object ID is written into the out pointer. Returns 0 on success, a negative
 * error code otherwise.
 */
int odb_transaction_write_object_stream(struct odb_transaction *transaction,
					struct odb_write_stream *stream,
					size_t len, struct object_id *oid);

#endif
