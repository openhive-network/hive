#ifdef __cplusplus
extern "C" {
#endif

int add_three_assets( int param_1, int param_2, int param_3 );

int add_two_numbers( int param_1, int param_2 );

int cpp_validate_operation( char* content );

int cpp_calculate_digest( char* content, unsigned char* digest );

int cpp_validate_transaction( char* content );

int cpp_serialize_transaction( char* content, unsigned char* serialized_transaction );

#ifdef __cplusplus
}
#endif
