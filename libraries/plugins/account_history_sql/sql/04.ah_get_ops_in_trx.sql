DROP FUNCTION IF EXISTS ah_get_ops_in_trx;
CREATE FUNCTION ah_get_ops_in_trx( in _BLOCK_NUM INT, in _TRX_IN_BLOCK INT )
RETURNS TABLE(
    _type TEXT,
    _value TEXT
)
AS
$function$
BEGIN
 RETURN QUERY
 SELECT
  split_part( hot.name, '::', 3) _type,
  ho.body _value
 FROM hive_operations ho
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE ho.block_num = _BLOCK_NUM AND ho.trx_in_block = _TRX_IN_BLOCK;
END
$function$
language plpgsql STABLE;


