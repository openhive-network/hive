DROP FUNCTION IF EXISTS ah_get_ops_in_block;
CREATE FUNCTION ah_get_ops_in_block( in _BLOCK_NUM INT )
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
 SELECT *
 FROM
  ah_get_non_virtual_ops_in_block( _BLOCK_NUM )

 UNION ALL

 SELECT *
 FROM
  ah_get_virtual_ops_in_block( _BLOCK_NUM );

END
$function$
language plpgsql STABLE;

