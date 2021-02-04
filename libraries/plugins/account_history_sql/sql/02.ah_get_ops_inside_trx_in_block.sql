DROP FUNCTION IF EXISTS ah_get_ops_inside_trx_in_block;
CREATE FUNCTION ah_get_ops_inside_trx_in_block( in _BLOCK_NUM INT, in _OP_TYPE SMALLINT )
--_OP_TYPE: 0(ALL) 1(ONLY NON VIRTUAL) 2(ONLY VIRTUAL)
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
  encode( ht.trx_hash, 'escape') _trx_id,
  ht.block_num _block,
  ht.trx_in_block::BIGINT _trx_in_block,
  0 _op_in_trx,
  hot.is_virtual _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  ho.body _value,
  0 _operation_id
 FROM hive_transactions ht
 JOIN hive_operations ho ON ht.block_num = ho.block_num AND ht.trx_in_block = ho.trx_in_block
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ht.block_num = _BLOCK_NUM AND ( _OP_TYPE = 0 OR ( _OP_TYPE = 1 AND hot.is_virtual = FALSE ) OR ( _OP_TYPE = 2 AND hot.is_virtual = TRUE ) );
END
$function$
language plpgsql STABLE;
