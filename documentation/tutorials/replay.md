## Replay

In following tutorial you will learn what replay is and how to do it.

### What is replay?

Block log is a list of operations. Alice received 8 coins reward for creating block, Alice transfered 5 coins to Bob, Bob transfered 2 coins to Carol. That's all. If now Alice wants to transfer 4 coins to Dave, does she has enough coins? To answer this question we need to know actual **state** of blockchain. I mean: how much coins each user has. We can find out by repeating all operations.

| Operation                      | Alice | Bob | Carol | Dave |
| :----------------------------- | :---: | :-: | :---: | :--: |
| Initial state                  | 0     | 0   | 0     | 0    |
| Alice gets 8 coins reward      | 8     | 0   | 0     | 0    |
| Alice transfers 5 coins to Bob | 3     | 5   | 0     | 0    |
| Bob transfers 2 coins to Carol | 3     | 3   | 2     | 0    |

Now we know, that Alice doesn't have enough coins to perform a transfer and we will not accept such operation. Last row of a above table is blockchain **state**, that we got to know it by performing block log **replay**.

### How to perform replay?

To perform a replay you need to specify which block log should be used to do it. If you have block log generated in same test case in which you want to use it, you can replay node with following syntax:
```python
block_log = first_node.get_block_log()
second_node.run(replay_from=block_log)
```

You can also perform replay from block log from outside of test case. Then you should pass path to `block_log` file as `replay_from` parameter:
```python
node.run(replay_from='~/blockchain/block_log')
```

Replay can be stopped at specified block number with `stop_at_block` parameter:
```python
node.run(replay_from='~/blockchain/block_log', stop_at_block=1_000_000)
```

Replay can be accelerated by appending `block_log.index` file. Then node don't need to generate it again. This file will be automatically found and use in both above described methods. Index file should be placed in the same directory as block log file.
```
📂 ~
└─ 📂 blockchain
   ├─ block_log
   └─ block_log.index
```

If you don't have index file or just want to perform replay without it, set `include_index` to `False`. If you are getting block log from node in same test case:
```python
block_log = node.get_block_log(include_index=False)
```
or if block log comes from custom directory:
```python
from test_tools import BlockLog
block_log = BlockLog('~/blockchain/block_log', include_index=False)
```
