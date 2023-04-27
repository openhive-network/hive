import example

op_00 = b'{"type":"transfer_operation","value":{"from":"initminer","to":"alpha","amount":"123.000 HIVE","memo":"test"}}'
print( f"{op_00} = {bool(example.validate_operation(op_00))}" )
print("====================================================================")

op_01 = b'{"type":"transfer_operation","value":{"from":"initminer","to":"alpha","amount":{"amount":"100000","precision":3,"nai":"@@000000021"},"memo":"test"}}'
print( f"{op_01} = {bool(example.validate_operation(op_01))}" )
print("====================================================================")

op_02 = b'{"type":"transfer_operation","value":{"from":"initminer","to":"alpha","amount":{"amount":"100000","precision":6,"nai":"@@000000037"},"memo":"test"}}'
print( f"{op_02} = {bool(example.validate_operation(op_02))}" )
print("====================================================================")