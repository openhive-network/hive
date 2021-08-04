## Snapshot

In following tutorial you will learn what snapshot is, how to create and use it.

### What is snapshot?

Imagine following situation. You have live node (node synchronized with network, containing actual blockchain state). You waited all day yesterday to bring him to this state. Now your collegue wants to run his node. Does he also have to wait all day to replay all operations in blockchain? If only you could share state from your node with his node... but you can! With snapshots.

Snapshot is a group of files which contains state of node. You can start node and load state from snapshot to avoid replaying all previous transactions. Your node will continue to work from state loaded from snapshot. It will replay only new transactions and will be live.

### How to create snapshot?

To create snapshot you have to call node method `dump_snapshot`. It will return snapshot object, which can be passed to another node, to start from its state.

```python
snapshot = node.dump_snapshot()
```

Note, that in automatic tests all unneeded files are removed when test is completed. Snapshot files then are also removed. You can change this behavior with clean up policies ([read more](../clean_up_policies.md)).

### How to run node using snapshot?

You can run node using snapshot with `load_snapshot_from` parameter of `Node.run` method.

If you want to run node with snapshot from another node is same test case:
```python
snapshot = first_node.dump_snapshot()
second_node.run(load_snapshot_from=snapshot)
```

If you have external snapshot, which was generated outside of test case, you can pass path to snapshot files in `load_snapshot_from` parameter (but don't forget that blocklog files are also needed, read below code example):
```python
second_node.run(load_snapshot_from='~/snapshot')
```
To start from snapshot node requires, besides snapshot files, block log (and optionally block log index). TestTools will try to find these files in same directory, where snapshot is located, in `blockchain` subdirectory. So in above example you need to provide following files structure:
```
📂 ~
├─ 📂 blockchain
│  ├─ block_log
│  └─ block_log.index
└─ 📂 snapshot
```
