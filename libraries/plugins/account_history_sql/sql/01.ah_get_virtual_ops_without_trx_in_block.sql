DROP FUNCTION IF EXISTS ah_get_virtual_ops_without_trx_in_block;
CREATE FUNCTION ah_get_virtual_ops_without_trx_in_block( in _BLOCK_NUM INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block BIGINT,
    _op_in_trx INT,
    _virtual_op BOOLEAN,
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
  '0000000000000000000000000000000000000000' _trx_id,
  _BLOCK_NUM _block,
  4294967295 _trx_in_block,
  0 _op_in_trx,
  TRUE _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  ho.body _value,
  0 _operation_id
 FROM hive_operations ho
 JOIN hive_blocks hb ON ho.block_num = hb.num AND ho.trx_in_block = -1
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ho.block_num = _BLOCK_NUM;
END
$function$
language plpgsql STABLE;