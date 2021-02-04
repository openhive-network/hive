DROP FUNCTION IF EXISTS ah_get_ops_in_block;
CREATE FUNCTION ah_get_ops_in_block( in _BLOCK_NUM INT, in _OP_TYPE SMALLINT )
RETURNS TABLE(
    _trx_id TEXT,
    _trx_in_block BIGINT,
    _op_in_trx INT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
BEGIN

 RETURN QUERY
 SELECT
  encode( ht.trx_hash, 'escape') _trx_id,
  ht.trx_in_block::BIGINT _trx_in_block,
  0 _op_in_trx,
  hot.is_virtual _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  ho.body _value,
  0 _operation_id
 FROM hive_transactions ht
 JOIN hive_operations ho ON ht.block_num = ho.block_num AND ht.trx_in_block = ho.trx_in_block
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ht.block_num = _BLOCK_NUM AND ( _OP_TYPE = 0 OR ( _OP_TYPE = 1 AND hot.is_virtual = FALSE ) OR ( _OP_TYPE = 2 AND hot.is_virtual = TRUE ) )

 UNION ALL

 SELECT
  '0000000000000000000000000000000000000000' _trx_id,
  4294967295 _trx_in_block,
  0 _op_in_trx,
  TRUE _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  ho.body _value,
  0 _operation_id
 FROM hive_operations ho
 JOIN hive_blocks hb ON ho.block_num = hb.num AND ho.trx_in_block = -1
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ho.block_num = _BLOCK_NUM;

END
$function$
language plpgsql STABLE;

