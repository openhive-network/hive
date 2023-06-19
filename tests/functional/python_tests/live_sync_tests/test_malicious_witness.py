import test_tools as tt


def test_witness_producing_blocks_early(prepare_4_4_4_4_4_with_time_offset):
    # TRIGGER
    # We restart one of nodes.
    networks_builder = prepare_4_4_4_4_4_with_time_offset

    tt.logger.info(f"networks_builder.networks {networks_builder.networks}")
    tt.logger.info(f"networks_builder.networks[0].nodes {networks_builder.networks[0].nodes}")
    tt.logger.info(f"networks_builder.networks[1].nodes {networks_builder.networks[1].nodes}")
    tt.logger.info(f"networks_builder.networks[2].nodes {networks_builder.networks[2].nodes}")
    tt.logger.info(f"networks_builder.networks[3].nodes {networks_builder.networks[3].nodes}")
    tt.logger.info(f"networks_builder.networks[4].nodes {networks_builder.networks[4].nodes}")

    api_node = networks_builder.networks[1].node('ApiNode0')


    tt.logger.info(f"get_witness_schedule: {api_node.api.database.get_witness_schedule()}")
    tt.logger.info(f"get_dynamic_global_properties: {api_node.api.database.get_dynamic_global_properties()}")
    from time import sleep
    sleep(120)
    tt.logger.info(f"get_witness_schedule: {api_node.api.database.get_witness_schedule()}")
    tt.logger.info(f"get_dynamic_global_properties: {api_node.api.database.get_dynamic_global_properties()}")

    # VERIFY
    assert False
