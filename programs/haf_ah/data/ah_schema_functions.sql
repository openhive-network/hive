CREATE OR REPLACE FUNCTION public.get_ops_in_block( in _BLOCK_NUM INT, in _ONLY_VIRTUAL BOOLEAN, in _INCLUDE_REVERSIBLE BOOLEAN )
RETURNS TABLE(
    _trx_id TEXT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BOOLEAN,
    _timestamp TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
BEGIN

  IF (NOT _INCLUDE_REVERSIBLE) AND _BLOCK_NUM > hive.app_get_irreversible_block( 'account_history' ) THEN
    RETURN QUERY SELECT 
      NULL::TEXT,
      NULL::BIGINT,
      NULL::BIGINT,
      NULL::BOOLEAN,
      NULL::TEXT,
      NULL::TEXT,
      NULL::INT
    LIMIT 0;
    RETURN;
  END IF;

  RETURN QUERY
    SELECT
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) _trx_id,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) _trx_in_block,
      T.op_pos _op_in_trx,
      T.is_virtual _virtual_op,
      trim(both '"' from to_json(T.timestamp)::text) _timestamp,
      T.body _value,
      T.id::INT _operation_id
    FROM
      (
        --`abs` it's temporary, until position of operation is correctly saved
        SELECT
          ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id, hot.is_virtual, ho.timestamp
        FROM hive.account_history_operations_view ho
        JOIN hive.operation_types hot ON hot.id = ho.op_type_id
        WHERE ho.block_num = _BLOCK_NUM AND ( _ONLY_VIRTUAL = FALSE OR ( _ONLY_VIRTUAL = TRUE AND hot.is_virtual = TRUE ) )
      ) T
      JOIN hive.account_history_blocks_view hb ON hb.num = T.block_num
      LEFT JOIN hive.account_history_transactions_view ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block;
END
$function$
language plpgsql STABLE;

CREATE OR REPLACE FUNCTION public.get_transaction( in _TRX_HASH BYTEA, in _INCLUDE_REVERSIBLE BOOLEAN )
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
  __result hive.account_history_transactions_view%ROWTYPE;
  __multisig_number SMALLINT;
BEGIN

  SELECT * INTO __result FROM hive.account_history_transactions_view ht WHERE ht.trx_hash = _TRX_HASH;
  IF NOT _INCLUDE_REVERSIBLE AND __result.block_num > hive.app_get_irreversible_block( 'account_history' ) THEN
    RETURN QUERY SELECT
      NULL::INT,
      NULL::BIGINT,
      NULL::TEXT,
      NULL::INT,
      NULL::SMALLINT,
      NULL::TEXT,
      NULL::SMALLINT
    LIMIT 0;
    RETURN;
  END IF;

  SELECT count(*) INTO __multisig_number FROM hive.account_history_transactions_multisig_view htm WHERE htm.trx_hash = _TRX_HASH;

  RETURN QUERY
    SELECT
      __result.ref_block_num _ref_block_num,
      __result.ref_block_prefix _ref_block_prefix,
      '2016-06-20T19:34:09' _expiration,--lack of data
      __result.block_num _block_num,
      __result.trx_in_block _trx_in_block,
      encode(__result.signature, 'escape') _signature,
      __multisig_number;
END
$function$
language plpgsql STABLE;

CREATE OR REPLACE FUNCTION public.get_multi_signatures_in_transaction( in _TRX_HASH BYTEA )
RETURNS TABLE(
    _signature TEXT
)
AS
$function$
BEGIN

  RETURN QUERY
    SELECT
      encode(htm.signature, 'escape') _signature
    FROM hive.account_history_transactions_multisig_view htm
    WHERE htm.trx_hash = _TRX_HASH;
END
$function$
language plpgsql STABLE;

CREATE OR REPLACE FUNCTION public.get_ops_in_transaction( in _BLOCK_NUM INT, in _TRX_IN_BLOCK INT )
RETURNS TABLE(
    _value TEXT
)
AS
$function$
BEGIN
  RETURN QUERY
    SELECT
      ho.body _value
    FROM hive.account_history_operations_view ho
    JOIN hive.operation_types hot ON ho.op_type_id = hot.id
    WHERE ho.block_num = _BLOCK_NUM AND ho.trx_in_block = _TRX_IN_BLOCK AND hot.is_virtual = FALSE
    ORDER BY ho.id;
END
$function$
language plpgsql STABLE;

CREATE OR REPLACE FUNCTION public.enum_virtual_ops( in _FILTER INT[], in _BLOCK_RANGE_BEGIN INT, in _BLOCK_RANGE_END INT, _OPERATION_BEGIN BIGINT, in _LIMIT INT, in _INCLUDE_REVERSIBLE BOOLEAN )
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
  __upper_block_limit INT;
  __filter_info INT;
BEGIN

   SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

  IF NOT _INCLUDE_REVERSIBLE THEN 
    SELECT hive.app_get_irreversible_block( 'account_history' ) INTO __upper_block_limit;
    IF _BLOCK_RANGE_BEGIN > __upper_block_limit THEN
      RETURN QUERY SELECT 
        NULL::TEXT,
        NULL::INT,
        NULL::BIGINT,
        NULL::BIGINT,
        NULL::BOOLEAN,
        NULL::TEXT,
        NULL::TEXT,
        NULL::INT
      LIMIT 0;
      RETURN;
    ELSIF __upper_block_limit < _BLOCK_RANGE_END  THEN
      SELECT __upper_block_limit INTO _BLOCK_RANGE_END;
    END IF;
  END IF;

  RETURN QUERY
    SELECT
      (
        CASE
        WHEN T2.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( T2.trx_hash, 'escape')
        END
      ) _trx_id,
      T.block_num _block,
      (
        CASE
        WHEN T2.trx_in_block IS NULL THEN 4294967295
        ELSE T2.trx_in_block
        END
      ) _trx_in_block,
      T.op_pos _op_in_trx,
      TRUE _virtual_op,
      trim(both '"' from to_json(T.timestamp)::text) _timestamp,
      T.body _value,
      T.id::INT _operation_id
    FROM
      (
        --`abs` it's temporary, until position of operation is correctly saved
        SELECT
          ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id, ho.timestamp
          FROM hive.account_history_operations_view ho
          JOIN hive.operation_types hot ON hot.id = ho.op_type_id
          WHERE ho.block_num >= _BLOCK_RANGE_BEGIN AND ho.block_num < _BLOCK_RANGE_END
          AND hot.is_virtual = TRUE
          AND ( ( __filter_info IS NULL ) OR ( ho.op_type_id = ANY( _FILTER ) ) )
          AND ( _OPERATION_BEGIN = -1 OR ho.id >= _OPERATION_BEGIN )
          --There is `+1` because next block/operation is needed in order to do correct paging. This won't be put into result set.
          ORDER BY ho.id
          LIMIT _LIMIT + 1
      ) T
      LEFT JOIN
      (
        SELECT block_num, trx_in_block, trx_hash
        FROM hive.account_history_transactions_view ht
        WHERE ht.block_num >= _BLOCK_RANGE_BEGIN AND ht.block_num < _BLOCK_RANGE_END
      )T2 ON T.block_num = T2.block_num AND T.trx_in_block = T2.trx_in_block
      WHERE T.block_num >= _BLOCK_RANGE_BEGIN AND T.block_num < _BLOCK_RANGE_END;
END
$function$
language plpgsql STABLE;

CREATE OR REPLACE FUNCTION public.ah_get_account_history( in _FILTER INT[], in _ACCOUNT VARCHAR, _START BIGINT, _LIMIT INT, in _INCLUDE_REVERSIBLE BOOLEAN )
RETURNS TABLE(
    _trx_id TEXT,
    _block INT,
    _trx_in_block BIGINT,
    _op_in_trx BIGINT,
    _virtual_op BIGINT,
    _timestamp TEXT,
    _value TEXT,
    _operation_id INT
)
AS
$function$
DECLARE
  __account_id INT;
  __filter_info INT;
  __upper_block_limit INT;
BEGIN

  SELECT INTO __filter_info ( select array_length( _FILTER, 1 ) );

  IF NOT _INCLUDE_REVERSIBLE THEN
    SELECT hive.app_get_irreversible_block( 'account_history' ) INTO __upper_block_limit;
  END IF;

  SELECT INTO __account_id ( select id from public.accounts where name = _ACCOUNT );

  IF __filter_info IS NULL THEN
  RETURN QUERY
    SELECT
      (
        CASE
        WHEN ht.trx_hash IS NULL THEN '0000000000000000000000000000000000000000'
        ELSE encode( ht.trx_hash, 'escape')
        END
      ) AS _trx_id,
      ho.block_num AS _block,
      (
        CASE
        WHEN ht.trx_in_block IS NULL THEN 4294967295
        ELSE ht.trx_in_block
        END
      ) AS _trx_in_block,
      (
        CASE
        WHEN ho.trx_in_block <= -1 THEN 0 ::BIGINT
        ELSE abs(ho.op_pos::BIGINT)
        END
      ) AS _op_in_trx,
      (
        CASE 
        WHEN ho.trx_in_block <= -1 THEN ho.op_pos ::BIGINT
        ELSE (ho.id - (
          SELECT nahov.id
          FROM hive.operations nahov
          JOIN hive.operation_types nhot 
          ON nahov.op_type_id = nhot.id 
          WHERE nahov.block_num=ho.block_num 
            AND nahov.trx_in_block=ho.trx_in_block 
            AND nahov.op_pos=ho.op_pos
            AND nhot.is_virtual=FALSE
          LIMIT 1
        ) ) :: BIGINT
      END
      ) AS _virtual_op,
      trim(both '"' from to_json(ho.timestamp)::text) _timestamp,
      ho.body _value,
      T.seq_no AS _operation_id
      FROM
      (
        SELECT hao.operation_id as operation_id, hao.account_op_seq_no as seq_no
        FROM public.account_operations hao 
        WHERE hao.account_id = __account_id AND hao.account_op_seq_no <= _START
        ORDER BY seq_no DESC
        LIMIT _LIMIT
      ) T
    JOIN hive.operations ho ON T.operation_id = ho.id
    JOIN hive.operation_types hot ON hot.id = ho.op_type_id
    LEFT JOIN hive.transactions ht ON ho.block_num = ht.block_num AND ho.trx_in_block = ht.trx_in_block
    WHERE ( (__upper_block_limit IS NULL) OR ho.block_num <= __upper_block_limit )
    ORDER BY _operation_id ASC
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
        (
          CASE
          WHEN ho.trx_in_block <= -1 THEN 0 ::BIGINT
          ELSE abs(ho.op_pos::BIGINT)
          END
        ) AS _op_in_trx,
        (
          CASE 
          WHEN ho.trx_in_block <= -1 THEN ho.op_pos ::BIGINT
          ELSE (ho.id - (
            SELECT nahov.id
            FROM hive.operations nahov
            JOIN hive.operation_types nhot 
            ON nahov.op_type_id = nhot.id 
            WHERE nahov.block_num=ho.block_num 
              AND nahov.trx_in_block=ho.trx_in_block 
              AND nahov.op_pos=ho.op_pos
              AND nhot.is_virtual=FALSE
            LIMIT 1
          ) ) :: BIGINT
          END
        ) AS _virtual_op,
        trim(both '"' from to_json(T.timestamp)::text) _timestamp,
        T.body _value,
        T.seq_no as _operation_id
      FROM
        (
          --`abs` it's temporary, until position of operation is correctly saved
          SELECT
            ho.id, ho.block_num, ho.trx_in_block, abs(ho.op_pos::BIGINT) op_pos, ho.body, ho.op_type_id, hao.account_op_seq_no as seq_no, timestamp, virtual_pos
            FROM hive.operations ho
            JOIN public.account_operations hao ON ho.id = hao.operation_id
            WHERE ( (__upper_block_limit IS NULL) OR ho.block_num <= __upper_block_limit )
              AND hao.account_id = __account_id
              AND hao.account_op_seq_no <= _START
              AND ( ho.op_type_id = ANY( _FILTER ) )
            ORDER BY seq_no DESC
            LIMIT _LIMIT
        ) T
        JOIN hive.operation_types hot ON hot.id = T.op_type_id
        LEFT JOIN hive.transactions ht ON T.block_num = ht.block_num AND T.trx_in_block = ht.trx_in_block
        ORDER BY _operation_id ASC
        LIMIT _LIMIT;

  END IF;

END
$function$
language plpgsql STABLE;

DROP VIEW IF EXISTS public.account_operation_count_info_view CASCADE;
CREATE OR REPLACE VIEW public.account_operation_count_info_view
AS
SELECT ha.id, ha.name, COALESCE( T.operation_count, 0 ) operation_count
FROM public.accounts ha
LEFT JOIN
(
SELECT ao.account_id account_id, COUNT(ao.account_op_seq_no) operation_count
FROM public.account_operations ao
GROUP BY ao.account_id
)T ON ha.id = T.account_id
;


