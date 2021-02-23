DROP FUNCTION IF EXISTS ah_get_multi_sig_in_trx;
CREATE FUNCTION ah_get_multi_sig_in_trx( in _TRX_HASH BYTEA )
RETURNS TABLE(
    _signature BYTEA
)
AS
$function$
BEGIN

  RETURN QUERY
    SELECT
      htm.signature _signature
    FROM hive_transactions_multisig htm
    WHERE htm.trx_hash = _TRX_HASH;
END
$function$
language plpgsql STABLE;