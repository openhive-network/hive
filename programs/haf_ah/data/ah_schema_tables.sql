CREATE TABLE IF NOT EXISTS public.accounts (
  id INTEGER NOT NULL,
  name VARCHAR(16) NOT NULL
)INHERITS( hive.%s );

ALTER TABLE accounts ADD CONSTRAINT accounts_pkey PRIMARY KEY ( id );
ALTER TABLE accounts ADD CONSTRAINT accounts_uniq UNIQUE ( name );

CREATE TABLE IF NOT EXISTS public.account_operations
(
  account_id integer not null,--- Identifier of account involved in given operation.
  account_op_seq_no integer not null,--- Operation sequence number specific to given account. 
  operation_id bigint not null  --- Id of operation held in hive.opreations table.
)INHERITS( hive.%s );

CREATE INDEX IF NOT EXISTS account_operations_account_op_seq_no_id_idx ON account_operations(account_id, account_op_seq_no, operation_id);
CREATE INDEX IF NOT EXISTS account_operations_operation_id_idx ON account_operations (operation_id);
