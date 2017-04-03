#include "postgres.h"

#include "replication/output_plugin.h"
#include "replication/logical.h"

#include <inttypes.h>

PG_MODULE_MAGIC;

extern void _PG_init(void);
extern void _PG_output_plugin_init(OutputPluginCallbacks *);

static void startup(
  LogicalDecodingContext *ctx,
  OutputPluginOptions *opt,
  bool is_init);

static void begin(
  LogicalDecodingContext *ctx,
  ReorderBufferTXN *txn);

static void change(
  LogicalDecodingContext *ctx,
  ReorderBufferTXN *txn,
  Relation rel,
  ReorderBufferChange *change);

static void commit(
  LogicalDecodingContext *ctx,
  ReorderBufferTXN *txn,
  XLogRecPtr commit_lsn);

void _PG_init(void) {
}

static void startup(
  LogicalDecodingContext *ctx,
  OutputPluginOptions *opt,
  bool is_init);

void _PG_output_plugin_init(OutputPluginCallbacks *cb) {
  AssertVariableIsOfType(&_PG_output_plugin_init, LogicalOutputPluginInit);

  cb->startup_cb = startup;
  cb->begin_cb = begin;
  cb->change_cb = change;
  cb->commit_cb = commit;
}

static void startup(
  LogicalDecodingContext *ctx,
  OutputPluginOptions *opt,
  bool is_init) {

  opt->output_type = OUTPUT_PLUGIN_TEXTUAL_OUTPUT;
}

void begin(
  LogicalDecodingContext *ctx,
  ReorderBufferTXN *txn) {

  OutputPluginPrepareWrite(ctx, true);
  appendStringInfoString(ctx->out, "BEGIN");
  OutputPluginWrite(ctx, true);
}

static void append_bigint(StringInfo out, Datum val) {
  appendStringInfo(out, "%" PRIu64, DatumGetInt64(val));
}

static Datum get_attribute_by_name(
  HeapTuple tuple,
  TupleDesc desc,
  const char *name,
  bool *is_null) {

  int i;

  for (i = 0; i < desc->natts; i++) {
    if (strcmp(NameStr(desc->attrs[i]->attname), name) == 0) {
      break;
    }
  }

  if (i < desc->natts) {
    return heap_getattr(tuple, i + 1, desc, is_null);

  } else {
    *is_null = true;
    return (Datum) NULL;
  }
}

void change(
  LogicalDecodingContext *ctx,
  ReorderBufferTXN *txn,
  Relation rel,
  ReorderBufferChange *change) {

  Form_pg_class form;
  TupleDesc desc;
  bool is_null_id, is_null_version, is_null_redaction;
  bool is_node, is_way, is_relation;
  Datum id, version, redaction;
  const char *table_name;
  HeapTuple tuple;

  form = RelationGetForm(rel);
  desc = RelationGetDescr(rel);
  table_name = NameStr(form->relname);

  is_node = strncmp(table_name, "nodes", 6) == 0;
  is_way = strncmp(table_name, "ways", 5) == 0;
  is_relation = strncmp(table_name, "relations", 10) == 0;

  if (is_node || is_way || is_relation) {
    const char *id_name =
      is_node ? "node_id" :
      is_way ? "way_id" :
      "relation_id";

    tuple = &change->data.tp.newtuple->tuple;
    id = get_attribute_by_name(tuple, desc, id_name, &is_null_id);
    version = get_attribute_by_name(tuple, desc, "version", &is_null_version);
    redaction = get_attribute_by_name(tuple, desc, "redaction_id", &is_null_redaction);

    if (is_null_id || is_null_version) {
      // uh, what? this shouldn't happen.
      // TODO: put some logging in here.

    } else {
      // new element
      if (change->action == REORDER_BUFFER_CHANGE_INSERT) {
        OutputPluginPrepareWrite(ctx, true);
        appendStringInfoString(ctx->out, "NEW ");
        appendStringInfoString(ctx->out, table_name);
        appendStringInfoString(ctx->out, " ");
        append_bigint(ctx->out, id);
        appendStringInfoString(ctx->out, " ");
        append_bigint(ctx->out, version);
        OutputPluginWrite(ctx, true);

      }
      // updated element with redaction
      // TODO: do we ever _unredact_ objects?
      else if ((change->action == REORDER_BUFFER_CHANGE_UPDATE) &&
               (!is_null_redaction)) {
        OutputPluginPrepareWrite(ctx, true);
        appendStringInfoString(ctx->out, "REDACT ");
        appendStringInfoString(ctx->out, table_name);
        appendStringInfoString(ctx->out, " ");
        append_bigint(ctx->out, id);
        appendStringInfoString(ctx->out, " ");
        append_bigint(ctx->out, version);
        appendStringInfoString(ctx->out, " ");
        append_bigint(ctx->out, redaction);
        OutputPluginWrite(ctx, true);
      }
    }
  }
}

void commit(
  LogicalDecodingContext *ctx,
  ReorderBufferTXN *txn,
  XLogRecPtr commit_lsn) {

  OutputPluginPrepareWrite(ctx, true);
  appendStringInfoString(ctx->out, "COMMIT");
  OutputPluginWrite(ctx, true);
}
