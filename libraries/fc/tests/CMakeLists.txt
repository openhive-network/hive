
add_executable( saturation_test saturation_test.cpp )
target_link_libraries( saturation_test fc )

add_executable( ecc_test crypto/ecc_test.cpp )
target_link_libraries( ecc_test fc )

add_executable( log_test crypto/log_test.cpp )
target_link_libraries( log_test fc )

add_executable( bip_lock_test bip_lock.cpp )
target_link_libraries( bip_lock_test fc )

add_executable( sha_test sha_test.cpp )
target_link_libraries( sha_test fc )

add_executable( all_tests all_tests.cpp
                          compress/compress.cpp
                          crypto/aes_test.cpp
                          crypto/base_n_tests.cpp
                          crypto/bigint_test.cpp
                          crypto/blind.cpp
                          crypto/blowfish_test.cpp
                          crypto/ecdsa_canon_test.cpp
                          crypto/rand_test.cpp
                          crypto/sha_tests.cpp
                          network/ntp_test.cpp
                          network/http/websocket_test.cpp
                          network/http/http_test.cpp
                          network/http/processor_test.cpp
                          network/http/processors/http_unsupported_test.cpp
                          network/http/processors/http_1_1_test.cpp
                          thread/task_cancel.cpp
                          thread/thread_tests.cpp
                          api.cpp
                          hmac_test.cpp
                          bloom_test.cpp
                          real128_test.cpp
                          utf8_test.cpp
                          )
target_link_libraries( all_tests fc )
