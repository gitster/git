#include "git-compat-util.h"
#include "odb/source.h"
#include "odb/transaction.h"

int odb_transaction_begin(struct object_database *odb,
			  struct odb_transaction **out,
			  enum odb_transaction_flags flags)
{
	int ret;

	if (odb->transaction) {
		*out = NULL;
		return 0;
	}

	ret = odb_source_begin_transaction(odb->sources, out, flags);
	odb->transaction = *out;

	return ret;
}

int odb_transaction_commit(struct odb_transaction *transaction)
{
	int ret;

	if (!transaction)
		return 0;

	/*
	 * Ensure the transaction ending matches the pending transaction.
	 */
	ASSERT(transaction == transaction->source->odb->transaction);

	ret = transaction->commit(transaction);
	if (ret)
		return ret;

	transaction->source->odb->transaction = NULL;
	free(transaction);

	return 0;
}

int odb_transaction_write_object_stream(struct odb_transaction *transaction,
					struct odb_write_stream *stream,
					size_t len, struct object_id *oid)
{
	return transaction->write_object_stream(transaction, stream, len, oid);
}

const char **odb_transaction_env(struct odb_transaction *transaction)
{
	if (!transaction)
		return NULL;

	return transaction->env(transaction);
}
