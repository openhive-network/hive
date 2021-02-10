DROP FUNCTION IF EXISTS ah_get_account_history;
CREATE FUNCTION ah_get_account_history( in _FILTER INT[], in _ACCOUNT VARCHAR, _START INT, _LIMIT INT )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
    _value TEXT,
    _operation_id BIGINT
)
AS
$function$
DECLARE
  __account_id INT;
  __filter_info INT;
BEGIN

  SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

  SELECT INTO __account_id ( select id from hive_accounts where name = _ACCOUNT );

  IF __filter_info IS NULL THEN
  RETURN QUERY
    SELECT
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) _trx_id,
      ho.block_num _block,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) _trx_in_block,
      abs(ho.op_pos::BIGINT) AS _op_in_trx,
      hot.is_virtual _virtual_op,
      trim(both '"' from to_json(hb.created_at)::text) _timestamp,
      ho.body _value,
      T.operation_id AS _operation_id
      FROM
      (
        SELECT hao.operation_id as operation_id, hao.account_op_seq_no as seq_no
        FROM hive_account_operations hao 
        WHERE hao.account_id = __account_id AND hao.account_op_seq_no > _START- _LIMIT AND hao.account_op_seq_no <= _START
        LIMIT _LIMIT
      ) T
    JOIN hive_operations ho ON T.operation_id = ho.id
    JOIN hive_blocks hb ON hb.num = ho.block_num
    JOIN hive_operation_types hot ON hot.id = ho.op_type_id
    LEFT JOIN hive_transactions ht ON ho.block_num = ht.block_num AND ho.trx_in_block = ht.trx_in_block
    ORDER BY seq_no
    LIMIT _LIMIT;
  ELSE
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
        hot.is_virtual _virtual_op,
        trim(both '"' from to_json(hb.created_at)::text) _timestamp,
        T.body _value,
        T.id as _operation_id
      FROM
        (
          --`abs` it's temporary, until position of operation is correctly saved
          SELECT
            ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id, hao.account_op_seq_no as seq_no
            FROM hive_operations ho
            JOIN hive_account_operations hao ON ho.id = hao.operation_id
            WHERE hao.account_id = __account_id AND hao.account_op_seq_no > _START- _LIMIT AND hao.account_op_seq_no <= _START
            AND ( ho.op_type_id = ANY( _FILTER ) ) 
            LIMIT _LIMIT
        ) T
        JOIN hive_blocks hb ON hb.num = T.block_num
        JOIN hive_operation_types hot ON hot.id = T.op_type_id
        LEFT JOIN hive_transactions ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block
      ORDER BY seq_no
      LIMIT _LIMIT;

  END IF;

END
$function$
language plpgsql STABLE;