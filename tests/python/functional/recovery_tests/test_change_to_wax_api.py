import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_database_change(node: tt.InitNode) -> None:
    # node.api.database_api.get_dynamic_global_properties()
    print()
    response = node.api.database.get_dynamic_global_properties()
    tt.logger.info(f"GDPO: {response}")