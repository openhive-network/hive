# Documentation for auxiliary programs useful for hived nodes

## compress_block_log: compresses/decompresses block_log files
### Example usage for compress_block_log
Create compressed version of blocklog in blockchain dir in blockchain2 dir:
`compress_block_log ./datadir/blockchain ./datadir/blockchain2`

Same as above, with explicitly specified arguments (non-positional):
`compress_block_log -i ./datadir/blockchain -o ./datadir/blockchain2`

Created compressed blocklog with 5000 blocks  using 8 threads:
`compress_block_log -j8 -n5000 ./datadir/blockchain ./datadir/blockchain2`

Decompress blocklog in blockchain dir to blockchain2 dir:
`compress_block_log --decompress ./datadir/blockchain ./datadir/blockchain2`


### Allowed options for compress_block_log
```
  --decompress                          Instead of compressing the block log,
                                        decompress it
  --zstd-level arg (=15)                The zstd compression level to use
                                        (-131072 - 22) 
  --benchmark-decompression             decompress each block and report the
                                        decompression times at the end
  -j [ --jobs ] arg (=1)                The number of threads to use for
                                        compression/decompression
  -i [ --input-block-log ] arg          The directory containing the input
                                        block log (required)
  -o [ --output-block-log ] arg         The directory to contain the compressed
                                        block log (required)
  --dump-raw-blocks arg                 A directory in which to dump raw,
                                        uncompressed blocks (one block per
                                        file
  -s [ --starting-block-number ] arg (=1)
                                        Start at the given block number (for
                                        benchmarking only, values > 1 will
                                        generate an unusable block log)
  -n [ --block-count ] arg              Stop after this many blocks
  -h [ --help ]                         Print usage instructions
```
### Overview of compress_block_log
The compress_block_log tool requires a valid block_log.index file.
If no block_log.index file is present, it will build one first before
beginning the compression/decompression step.

If the block_log.index is valid but too long, the block_log.index will be
truncated to the number of blocks in the blocklog. If the block_log.index is
too short, the block_log.index will be extended to match the size of the blocklog.

When using positional arguments, explicitly specified options must come first.

When compressing a block_log, it is recommended to use -j set to the number
of cpus available to avoid long compression times.

This tool also replaces the previous `truncate_block_log` utility. To truncate
a blocklog, see the third example above using the -n option.
