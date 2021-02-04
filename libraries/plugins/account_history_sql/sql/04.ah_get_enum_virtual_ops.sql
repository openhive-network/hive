DROP FUNCTION IF EXISTS ah_get_enum_virtual_ops;
CREATE FUNCTION ah_get_enum_virtual_ops( in _FILTER INT[], in _BLOCK_RANGE_BEGIN INT, in _BLOCK_RANGE_END INT, _OPERATION_BEGIN INT, in _LIMIT INT )
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
DECLARE
 __filter_info INT;
BEGIN

 SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

 RETURN QUERY
  SELECT
   encode( ht.trx_hash, 'escape') _trx_id,
   ht.block_num _block,
   ht.trx_in_block _trx_in_block,
   ho.op_pos::INT _op_in_trx,
   TRUE _virtual_op,
   trim(both '"' from to_json(hb.created_at)::text) _timestamp,
   split_part( hot.name, '::', 3) _type,
   ho.body _value,
   0 _operation_id
 FROM hive_operations ho
 JOIN hive_transactions ht ON ht.block_num = ho.block_num AND ht.trx_in_block = ho.trx_in_block
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ht.block_num >= _BLOCK_RANGE_BEGIN
  AND ht.block_num < _BLOCK_RANGE_END
  AND ( ( __filter_info IS NULL ) OR ( ho.op_type_id = ANY( _FILTER ) ) )
  AND hot.is_virtual = TRUE

 UNION ALL

 SELECT
  '0000000000000000000000000000000000000000' _trx_id,
  ho.block_num _block,
  4294967295 _trx_in_block,
  ho.op_pos::INT _op_in_trx,
  TRUE _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  ho.body _value,
  0 _operation_id
 FROM hive_operations ho
 JOIN hive_blocks hb ON ho.block_num = hb.num AND ho.trx_in_block = -1
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ho.block_num >= _BLOCK_RANGE_BEGIN
  AND ho.block_num < _BLOCK_RANGE_END
  AND ( ( __filter_info IS NULL ) OR ( ho.op_type_id = ANY( _FILTER ) ) )
  AND hot.is_virtual = TRUE
 ORDER BY _block, _trx_in_block, _op_in_trx
 LIMIT _LIMIT;
END
$function$
language plpgsql STABLE;
