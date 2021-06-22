# Fork Tests

## Scenarios
    Some scenarios require long and similar setup, therefore we use shared fixtures to perform setup only once. Required
    setup in fixture 'world_with_witnesses' is 21 active witnesses producing blocks and irreversible block number should
    be behing head block like in real network (15-20 blocks). With default config (required_participation_rate=33) we must wait
    until block 63 to schedule any witness other then initminer (before required_participation_rate is to low and blocks won't be produced).
    Then we must wait for another 21 blocks for every witness to sign at least one block (so last irreversible block proceeds).
    Every witness must sign at least one block becouse this is required by formula for last irreversible block (else last irreversible
    block will be equal to head block).

### test_no_ah_duplicates_after_fork
    PREREQUISITES
    There exists 8 witness nodes, 4 marked as alpha and 4 marked as beta. There is also initminer and one api node in each subnetwork.
    Witnesses are equally divided between those nodes (half of witnesses are in alpha part, other half in beta part). Last irreversible
    block is behind head block.

    TRIGGER
    Using newtork_node_api we block communication between alpha and beta parts.
    After some time (when irreversible block number increases in both subnetworks) we join alpha and beta part again

    VERIFY
    Expected behaviour is that nodes in one of subnetworks (random one, alpha or beta) will perform undo and synchronize with other subnetwork.
    We check there are no duplicates in account_history_api after such scenario (issue #117).
### test_no_ah_duplicates_after_restart
    PREREQUISITES
    Node has account_history_api and account_history_rocksdb plugins turned on, and last irreversible block is behind head block about 15-20 blocks
    (like in real network).

    TRIGGER
    We restart one of nodes.

    VERIFY
    Expected behavour is that after restart node will enter live sync and there are no duplicates in account_history_api (issue #117).

### test_enum_virtual_ops
    PREREQUISITES
    Node has account_history_api and account_history_rocksdb plugins turned on, and last irreversible block is behind head block about 15-20 blocks
    (like in real network).

    TRIGGER
    We broadcast transactions (with non virtual operations).

    VERIFY
    We check that query account_history_api.enum_virtual_ops returns only virtual operations (issue #139).

### test_irreversible_block_while_sync
    PREREQUISITES
    We need to be before head block number HIVE_START_MINER_VOTING_BLOCK constant (30 in testnet). Also head block number
    must be greater then HIVE_MAX_WITNESSES (21 in both testnet and mainnet).

    TRIGGER
    Node restart at block n, HIVE_MAX_WITNESSES < n < HIVE_START_MINER_VOTING_BLOCK.

    VERIFY
    No crash, node enters live sync.

### scan_node_for_duplicates
    python3 scan_node_for_duplicates.py scheme://host:port

    Utility to check if there are no duplicates in account_history_api of given as argment node, scheme://host:port should be account_history_api address,
    can be used during manual testing, not used on gitlab CI. Example usage:
    python3 scan_node_for_duplicates.py https://api.hive.blog
    python3 scan_node_for_duplicates.py http://127.0.0.1:8091
