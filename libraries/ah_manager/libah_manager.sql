CREATE FUNCTION my_test( a INTEGER )
RETURNS INTEGER AS '$libdir/libah_manager.so', 'my_test' LANGUAGE C;