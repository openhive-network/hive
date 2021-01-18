DROP FUNCTION IF EXISTS ah_get_virtual_ops_in_block;
CREATE FUNCTION ah_get_virtual_ops_in_block( in _BLOCK_NUM INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block INT,
    _op_in_trx INT,
    _virtual_op INT,
    _timestamp TEXT,
    _type TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
BEGIN

 RETURN QUERY
 SELECT
  encode( ht.trx_hash, 'hex') _trx_id,
  ht.block_num _block,
  ht.trx_in_block _trx_in_block,
  0 _op_in_trx,
  1 _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  hvo.body _value,
  0 _operation_id
 FROM hive_transactions ht
 JOIN hive_virtual_operations hvo ON ( ht.block_num = hvo.block_num AND ht.trx_in_block = hvo.trx_in_block )
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON hvo.op_type_id = hot.id
 WHERE ht.block_num = _BLOCK_NUM

 UNION ALL

 SELECT
  '0000000000000000000000000000000000000000' _trx_id,
  _BLOCK_NUM _block,
  -1 _trx_in_block,
  0 _op_in_trx,
  1 _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  hvo.body _value,
  0 _operation_id
 FROM hive_virtual_operations hvo
 JOIN hive_blocks hb ON hvo.block_num = hb.num AND hvo.trx_in_block = -1
 JOIN hive_operation_types hot ON hvo.op_type_id = hot.id
 WHERE hvo.block_num = _BLOCK_NUM
 ORDER BY _trx_in_block, _virtual_op;
END
$function$
language plpgsql STABLE;

