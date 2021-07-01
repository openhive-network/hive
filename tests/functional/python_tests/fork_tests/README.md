# Fork Test

    By fork we mean here disconnection between parts of network. Most interesting is disconnection between two similarly large
    parts of network, lets say `alpha` and `beta`. By `large network` we mean that there are multiple active witnesses
    in this part of network (lets say 11 in `alpha` and 10 in `beta` part). To perform fork test we need to prepare testnet
    with 21 active witnesses and last_irreversible_block value calculated based on HIVE_IRREVERSIBLE_THRESHOLD constant.

    We must also provide required-participation rate in network, which has default walue of 33%. This means that out of last 128 block
    slots - at least 33% blocks were produced. Witnesses are scheduled after blocks 21, 42, 63 and so on. To fulfull required-participation
    we must wait until block 63 (42/128 < 33% and is not enough, 63/128 > 33%).

    In mainnet network usually last irreversible block is behind head block by 15-20 blocks (if all witnesses are participating).
    In testnet we must wait for that to happen until all witnesses are active and sign at least one block. For this reason we must
    wait another 21 blocks to allow witnesses to perform at least one sign of block.

    Then we disconnect networks. Each subnetwork will produce new blocks (because there are 11 witnesses in `alpha` and 10 in `beta`)
    therefore in each subnetwork head block will increase. Also in each subnetwork last_irreversible_block value will increase slightly,
    but should no get larger then head block number at the moment of disconnection (because of formula based on HIVE_IRREVERSIBLE_THRESHOLD).

    Then we allow subnetworks to connect again. Nodes in one of subnetwork (random one, based on witness schedule) will perform undo on blocks
    after disconnection point. This should be possible because last_irreversible_block will increase slightly, but not beyond the
    point of disconnection. After undo operation nodes should enter live sync. We check that account_history_plugin behaves correctly
    in such scenario, i.e. there are no duplicate operations around point of disconnection.
