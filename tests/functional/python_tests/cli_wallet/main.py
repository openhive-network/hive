import pytest

retcode = pytest.main(["-s", "-k", 'test_direct_rc_delegations', "test_virtual_operations_bug_on_threads.py"])
