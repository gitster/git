#include "git-compat-util.h"
#include "odb/source.h"
#include "odb/transaction.h"

int odb_transaction_begin(struct object_database *odb,
			  struct odb_transaction **out)
{
	int ret;

	if (odb->transaction) {
		*out = NULL;
		return 0;
	}

	ret = odb_source_begin_transaction(odb->sources, out);
	odb->transaction = *out;

	return ret;
}

void odb_transaction_commit(struct odb_transaction *transaction)
{
	if (!transaction)
		return;

	/*
	 * Ensure the transaction ending matches the pending transaction.
	 */
	ASSERT(transaction == transaction->source->odb->transaction);

	transaction->commit(transaction);
	transaction->source->odb->transaction = NULL;
	free(transaction);
}

int odb_transaction_write_object_stream(struct odb_transaction *transaction,
					struct odb_write_stream *stream,
					size_t len, struct object_id *oid)
{
	return transaction->write_object_stream(transaction, stream, len, oid);
}
