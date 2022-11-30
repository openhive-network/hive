## block_log_util
`block_log_util` is a program for doing simple tests and repairs of a block log
from the command line.  It's primary feature right now is comparing block logs.
After HF26, it is no longer possible to verify whether two block logs are
functionally identical by using simple unix tools like `cmp`, `sum`, or
comparing file sizes.  This is because block compression is done on a per-block
basis, and different nodes may use different compression settings or have
different mixtures of old uncompressed blocks and new compressed blocks.

_Note 1_: when `block_log_util` needs a filename for a block log, it expects
the path to the file named `block_log`, not the directory containing the
`block_log` file (so: `/home/luser/.hive/blockchain/block_log`)

_Note 2_: in general, `block_log_util` is designed to operate on a pair of
`block_log` and `block_log.artifacts` files, just like `hived` does.  If the
`block_log.artifacts` file is missing or doesn't match the `block_log` file,
you will need to generate/repair it first.

`block_log_util` has serveral sub-commands:

### sha256sum
The `sha256sum` subcommand computes the checksum of the block log as if it were
uncompressed.  If you run this on two block logs and they generate the same
checksum then they are functionally identical, even if they are not the same
byte-for-byte.

You can also compare this checksum to the regular `sha256sum(1)` result for an
uncompressed (pre-HF26) block log file.

##### Example: generating a checksum
```
$ block_log_util sha256sum blockchain/block_log
54f0a42f8738de8d23a904433f2c9e37efa5f2003735c37089fa2b1a22ec0536  blockchain/block_log
```
You can also redirect the output to a file, then use that file to check another
block log.
```
$ cd ~/known-good-data-dir
$ block_log_util sha256sum blockchain/block_log > /tmp/sha256sum.txt
$ cd ~/dubious-data-dir
$ block_log_util sha256sum -c /tmp/sha256sum.txt
blockchain/block_log: FAILED
WARNING: 1 block logs had checksums that did NOT match
```

##### Example: using checkpoints
You can use the `--checkpoint` function to generate a series of sums at regular
block intervals which can help determine where two block logs differ.  To
generate a series of checksums every million blocks:
```
$ cd ~/known-good-data-dir
$ block_log_util sha256sum --checkpoint 1000000 blockchain/block_log | tee /tmp/sha256sum.txt
42cc908f4af1d8a91d9bb64ce637705208c19e7a2f73cb839ae24c0be3a56ddf  blockchain/block_log@1000000
445769d430ab28df46fe1547b43f4e54bb904a5b3ef18059f1c106b129514f68  blockchain/block_log@2000000
c0c93e986d514f7c1656c76b8a9223326de1c0224241254dacbcf228e6571e4f  blockchain/block_log@3000000
...
675cdc8cb5d7266070a221cdddcc372c8e815317509dc0542d1fd2f394251625  blockchain/block_log@67000000
cd537406de604cce03704fb18875262a1a987bbf177beec38df92f31eb294833  blockchain/block_log@68000000
0858288291b328316c302bfb32e9fc277533c330c1d0f6fd27b94df72661a964  blockchain/block_log@68477710
$ cd ~/dubious-data-dir
$ block_log_util sha256sum -c /tmp/sha256sum.txt
blockchain/block_log@1000000: OK
blockchain/block_log@2000000: OK
blockchain/block_log@3000000: OK
...
blockchain/block_log@66000000: OK
blockchain/block_log@67000000: OK
blockchain/block_log@68000000: FAILED (block log only contains 67999999 blocks)
blockchain/block_log@68477710: FAILED (block log only contains 67999999 blocks)
WARNING: 1 block logs had checksums that did NOT match
```
Which tells us that the `dubious-data-dir` block log is functionally identical
through block 67000000.  After that, it may have different blocks, or it may
have identical blocks, just missing a few at the end.

### cmp
You can directly compare two block logs using the `cmp` command.  If both logs
are on the same machine, this will be at least as fast as generating checksums
of both logs and comparing them.
```
$ block_log_util cmp ~/known-good-data-dir/blockchain/block_log ~/dubious-data-dir/blockchain/block_log
~/known-good-data-dir/blockchain/block_log has 223649 more blocks than ~/dubious-data-dir/blockchain/block_log.  Both logs are equivalent through block 67999999, which is the last block in ~/dubious-data-dir/blockchain/block_log
```
You can start the comparison anywhere in the blockchain.  Going back to our
previous example, we know from the hashes that the two block logs are
functionally identical through block 67000000, so we can use `cmp` to find out
exactly where the difference is.  Running this command will give you the same
results, but much quicker:
```
$ block_log_util cmp --start-at-block 67000001 ~/known-good-data-dir/blockchain/block_log ~/dubious-data-dir/blockchain/block_log
```

### truncate
You can shorten a block log using the `truncate` command.  This alters the
existing block log in-place, and executes quickly.  So, if you have used the
`sha256sum` tool to verify that your block log is good up through block
67000000, you can trim off the possibly-bad blocks:
```
$ block_log_util truncate ~/dubious-data-dir/blockchain/block_log 67000000
Really truncate /storage1/datadir-compression-testing/compressed-blockchain/blockchain/block_log to 67000000 blocks? [yes/no] yes
```
If you don't want the prompt, use `-f`.

### find-end
The `find-end` command attempts to recover a block log that is incomplete
(either by an interrupted file transfer, or a unclean shutdown).  When the
block log ends with a partially-written block, `hived` is unable to open it.
This will quickly scan the end of the block log to find where the last complete
block ends.  If it finds a valid end, it will offer to remove the
partially-written block from the end.

_Note_: this subcommand operates only on the `block_log` file, and ignores the
`block_log.artifacts` file.  There's a good chance the `block_log.artifacts`
file will need to be repaired after trimming the end off the `block_log`.  This
should happen automatically the next time you run `hived`.  

```
$ block_log_util find-end blockchain/block_log
Likely end detected, block log size should be reduced to 386095191649,
which will reduce the block log size by 2463 bytes

Do you want to truncate blockchain/block_log now? [yes/no] yes
done
```

### get-head-block-number
Just want to know how many blocks are in your block log?
```
$ block_log_util get-head-block-number blockchain/block_log
67000000
```

### get-block-ids
Or want to check the id of a given block number?
```
$ block_log_util get-block-ids blockchain/block_log 67000000
03fe56c0685d81371f5df86f7cb783b8a5e065c3
```
Give it two block numbers, and it will print ids for all blocks in that range.

### get-block
To print the whole block from the block log:
```
$ block_log_util get-block --pretty blockchain/block_log 67000000 | head
{
  "previous": "03fe56bf1b40292d9df42b4789e53b560273d9be",
  "timestamp": "2022-08-14T03:29:12",
  "witness": "therealwolf",
  "transaction_merkle_root": "ac806cc65408ea6a76c08611e7a39c22008037cc",
  "extensions": [],
  "witness_signature": "2007f38f985ac3e89140425280b21fe5e152db92540c26c43fe858bf18b2570a1c120554d9c58ec0f4af216783e4d726848a592aa16f1c6217532ea96aa8473fdb",
  "transactions": [{
      "ref_block_num": 22188,
      "ref_block_prefix": 92816311,
```
Other options exist to dump the block in binary form, or just print the block header.
