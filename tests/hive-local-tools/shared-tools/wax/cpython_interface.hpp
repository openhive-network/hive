#ifdef __cplusplus
extern "C" {
#endif

int cpp_validate_operation( char* content );

int cpp_calculate_digest( char* content, unsigned char* digest, const char* chain_id );

int cpp_validate_transaction( char* content );

int cpp_serialize_transaction( char* content, unsigned char* serialized_transaction, unsigned int* dest_size );

#ifdef __cplusplus
}
#endif
