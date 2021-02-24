DROP FUNCTION IF EXISTS ah_get_trx;
CREATE FUNCTION ah_get_trx( in _TRX_HASH BYTEA )
RETURNS TABLE(
    _ref_block_num INT,
    _ref_block_prefix BIGINT,
    _expiration TEXT,
    _block_num INT,
    _trx_in_block SMALLINT,
    _signature TEXT,
    _multisig_number SMALLINT
)
AS
$function$
DECLARE
  __multisig_number SMALLINT;
BEGIN

  SELECT count(*) INTO __multisig_number FROM hive_transactions_multisig htm WHERE htm.trx_hash = _TRX_HASH;

  RETURN QUERY
    SELECT
      ref_block_num _ref_block_num,
      ref_block_prefix _ref_block_prefix,
      '2016-06-20T19:34:09' _expiration,--lack of data
      ht.block_num _block_num,
      ht.trx_in_block _trx_in_block,
      encode(ht.signature, 'escape') _signature,
      __multisig_number
    FROM hive_transactions ht
    WHERE ht.trx_hash = _TRX_HASH;
END
$function$
language plpgsql STABLE;