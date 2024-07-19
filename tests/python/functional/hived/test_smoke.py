import test_tools as tt

def test_run_hived() -> None:
    node = tt.InitNode()
    node.run()
    node.close()
