DROP FUNCTION IF EXISTS ah_get_enum_virtual_ops;
CREATE FUNCTION ah_get_enum_virtual_ops( in _FILTER INT[], in _BLOCK_RANGE_BEGIN INT, in _BLOCK_RANGE_END INT, _OPERATION_BEGIN BIGINT, in _LIMIT INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
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
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) _trx_id,
      T.block_num _block,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) _trx_in_block,
      T.op_pos _op_in_trx,
      TRUE _virtual_op,
      trim(both '"' from to_json(hb.created_at)::text) _timestamp,
      T.body _value,
      T.id::INT _operation_id
    FROM
      (
        --`abs` it's temporary, until position of operation is correctly saved
        SELECT
          ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id
          FROM hive_operations ho
          JOIN hive_operation_types hot ON hot.id = ho.op_type_id
          WHERE ho.block_num >= _BLOCK_RANGE_BEGIN AND ho.block_num < _BLOCK_RANGE_END
          AND hot.is_virtual = TRUE
          AND ( ( __filter_info IS NULL ) OR ( ho.op_type_id = ANY( _FILTER ) ) )
          AND ( _OPERATION_BEGIN = -1 OR ho.id >= _OPERATION_BEGIN )
          --There is `+1` because next block/operation is needed in order to do correct paging. This won't be put into result set.
          LIMIT _LIMIT + 1
      ) T
      JOIN hive_blocks hb ON hb.num = T.block_num
      LEFT JOIN hive_transactions ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block;
END
$function$
language plpgsql STABLE;
