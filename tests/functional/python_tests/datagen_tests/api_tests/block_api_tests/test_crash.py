from urllib import response
import test_tools as tt

from .block_log.generate_block_log import TOTAL_BLOCKS

def test_get_block_range_crash(replayed_node):
    response = replayed_node.api.block.get_block_range(starting_block_num=60, count=30)
    assert len(response['blocks']) == 30

def test_get_block_range(replayed_node):
    step = 100
    for i in range(1, TOTAL_BLOCKS - step):
        response = replayed_node.api.block.get_block_range(starting_block_num=i, count=step)
        c = len(response['blocks'])
        tt.logger.info(f"Query for range: start_block: {i}, count: {step} gave a {c} blocks in the response")
        assert c == step

def test_get_block_range_out_of_range(replayed_node):
    step = 100
    response = replayed_node.api.block.get_block_range(starting_block_num=TOTAL_BLOCKS+step, count=step)
    c = len(response['blocks'])
    tt.logger.info(f"Query for range: start_block: {TOTAL_BLOCKS+step}, count: {step} gave a {c} blocks in the response")
    assert c == 0
