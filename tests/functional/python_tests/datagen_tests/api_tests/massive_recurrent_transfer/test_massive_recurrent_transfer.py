from pathlib import Path
import test_tools as tt

def test_massive_recurrent_transfer(replayed_node):
    bank_balance_after_first_pack_of_recurrent_transfers = replayed_node.api.wallet_bridge.get_account("bank")['balance']
    assert int(bank_balance_after_first_pack_of_recurrent_transfers['amount']) == 50_000 # 0,001 Hive * 50_000 accounts

    replayed_node.close()

    block_log_directory = Path(__file__).parent / 'block_log'
    timestamp_path = block_log_directory / "timestamp"
    with open(timestamp_path, encoding='utf-8') as file:
        timestamp = tt.Time.parse(file.read())

    new_timestamp = timestamp + tt.Time.days(2)

    replayed_node.run(time_offset=tt.Time.serialize(new_timestamp, format_=tt.Time.TIME_OFFSET_FORMAT) + " x50")
    tt.logger.info(f"last_block_num before: {replayed_node.get_last_block_number()}")
    replayed_node.wait_number_of_blocks(50)
    bank_balance_after_second_pack_of_recurrent_transfers = replayed_node.api.wallet_bridge.get_account("bank")['balance']
    tt.logger.info(f"last_block_num after: {replayed_node.get_last_block_number()}")

    assert int(bank_balance_after_second_pack_of_recurrent_transfers['amount']) == 100_000
