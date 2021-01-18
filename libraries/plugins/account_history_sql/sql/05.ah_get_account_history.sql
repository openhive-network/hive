DROP FUNCTION IF EXISTS ah_get_account_history;
CREATE FUNCTION ah_get_account_history( in _FILTER INT[], in _ACCOUNT VARCHAR, _START INT, _LIMIT INT )
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
DECLARE
 __account_id INT;
 __filter_info INT;
BEGIN

 SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

 SELECT INTO __account_id ( select id from hive_accounts where name = _ACCOUNT );

 RETURN QUERY
 SELECT
  encode( ht.trx_hash, 'hex') _trx_id,
  ht.block_num _block,
  ht.trx_in_block _trx_in_block,
  hvo.op_pos _op_in_trx,
  1 _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  hvo.body _value,
  0 _operation_id
 FROM hive_transactions ht
 JOIN hive_virtual_operations hvo ON ht.block_num = hvo.block_num AND ht.trx_in_block = hvo.trx_in_block
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON hvo.op_type_id = hot.id
 WHERE __account_id = ANY( hvo.participants ) AND hvo.order_id >= _START AND ( ( __filter_info IS NULL ) OR ( hvo.op_type_id = ANY( _FILTER ) ) )

 UNION ALL

 SELECT
  encode( ht.trx_hash, 'hex') _trx_id,
  ht.block_num _block,
  ht.trx_in_block _trx_in_block,
  ho.op_pos _op_in_trx,
  0 _virtual_op,
  trim(both '"' from to_json(hb.created_at)::text) _timestamp,
  split_part( hot.name, '::', 3) _type,
  ho.body _value,
  0 _operation_id
 FROM hive_transactions ht
 JOIN hive_operations ho ON ht.block_num = ho.block_num AND ht.trx_in_block = ho.trx_in_block
 JOIN hive_blocks hb ON ht.block_num = hb.num
 JOIN hive_operation_types hot ON ho.op_type_id = hot.id
 WHERE __account_id = ANY( ho.participants ) AND ho.order_id >= _START AND ( ( __filter_info IS NULL ) OR ( ho.op_type_id = ANY( _FILTER ) ) )
 ORDER BY _block
 LIMIT _LIMIT;
END
$function$
language plpgsql STABLE;
