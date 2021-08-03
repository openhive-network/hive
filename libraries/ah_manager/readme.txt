*****Installation of 'ah_manager' extension*****
1. cmake -DBUILD_SHARED_LIBS=ON
2. make ah_manager
3. cd build/libraries/ah_manager
4. sudo make install

*****Creating of extension in postgres database*****
CREATE EXTENSION libah_manager;

*****Usage*****
select * from get_impacted_accounts('{"type":"transfer_operation","value":{"from":"admin","to":"steemit","amount":{"amount":"833000","precision":3,"nai":"@@000000021"},"memo":""}}')

Result:

"admin"
"steemit"
