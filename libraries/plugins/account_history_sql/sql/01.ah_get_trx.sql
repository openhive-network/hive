DROP FUNCTION IF EXISTS ah_get_trx;
CREATE FUNCTION ah_get_trx( in _TRX_HASH BYTEA )
RETURNS TABLE(
    _ref_block_num INT,
    _ref_block_prefix INT,
    _expiration TEXT,
    _block_num INT,
    _trx_in_block SMALLINT
)
AS
$function$
BEGIN
  RETURN QUERY
    SELECT
      0 _ref_block_num,--lack of data
      0 _ref_block_prefix,--lack of data
      '2016-06-20T19:34:09' _expiration,--lack of data
      ht.block_num _block_num,
      ht.trx_in_block _trx_in_block
    FROM hive_transactions ht
    WHERE ht.trx_hash = _TRX_HASH;
END
$function$
language plpgsql STABLE;
