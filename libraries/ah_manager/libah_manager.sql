CREATE OR REPLACE FUNCTION get_impacted_accounts(IN text)
RETURNS SETOF text AS '$libdir/libah_manager.so', 'get_impacted_accounts' LANGUAGE C;
