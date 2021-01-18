DROP FUNCTION IF EXISTS ah_get_non_virtual_ops_in_block;
CREATE FUNCTION ah_get_non_virtual_ops_in_block( in _BLOCK_NUM INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block SMALLINT,
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
  0 _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  ho.body _value,
  0 _operation_id
 FROM hive_transactions ht
 JOIN hive_operations ho ON ht.block_num = ho.block_num AND ht.trx_in_block = ho.trx_in_block
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ht.block_num = _BLOCK_NUM;
END
$function$
language plpgsql STABLE;


