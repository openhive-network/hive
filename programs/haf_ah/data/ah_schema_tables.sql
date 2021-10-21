CREATE SCHEMA IF NOT EXISTS hafah;

CREATE TABLE IF NOT EXISTS hafah.accounts (
  id INTEGER NOT NULL,
  name VARCHAR(16) NOT NULL
)INHERITS( hive.account_history );

ALTER TABLE hafah.accounts ADD CONSTRAINT accounts_pkey PRIMARY KEY ( id );
ALTER TABLE hafah.accounts ADD CONSTRAINT accounts_uniq UNIQUE ( name );

CREATE TABLE IF NOT EXISTS hafah.account_operations
(
  account_id INTEGER NOT NULL,--- Identifier of account involved in given operation.
  account_op_seq_no INTEGER NOT NULL,--- Operation sequence number specific to given account.
  operation_id BIGINT NOT NULL  --- Id of operation held in hive.opreations table.
)INHERITS( hive.account_history );

ALTER TABLE hafah.account_operations ADD CONSTRAINT account_operations_uniq UNIQUE ( account_id, account_op_seq_no );
CREATE INDEX IF NOT EXISTS account_operations_operation_id_idx ON hafah.account_operations (operation_id);
CREATE INDEX IF NOT EXISTS hive_operations_block_num_id_idx ON hive.operations USING btree(block_num, id); -- required by public.enum_virtual_ops_pagination
