DROP FUNCTION IF EXISTS ah_get_ops_in_trx;
CREATE FUNCTION ah_get_ops_in_trx( in _BLOCK_NUM INT, in _TRX_IN_BLOCK INT )
RETURNS TABLE(
    _value TEXT
)
AS
$function$
BEGIN
  RETURN QUERY
    SELECT
      ho.body _value
    FROM hive_operations ho
    JOIN hive_operation_types hot ON ho.op_type_id = hot.id
    WHERE ho.block_num = _BLOCK_NUM AND ho.trx_in_block = _TRX_IN_BLOCK AND hot.is_virtual = FALSE
    ORDER BY ho.id;
END
$function$
language plpgsql STABLE;
