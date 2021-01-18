DROP FUNCTION IF EXISTS ah_get_ops_in_block;
CREATE FUNCTION ah_get_ops_in_block( in _BLOCK_NUM INT, in _OP_TYPE SMALLINT )
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
 SELECT *
 FROM
  ah_get_ops_inside_trx_in_block( _BLOCK_NUM, _OP_TYPE )

 UNION ALL

 SELECT *
 FROM
  ah_get_virtual_ops_without_trx_in_block( _BLOCK_NUM );

END
$function$
language plpgsql STABLE;

