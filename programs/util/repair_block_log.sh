#! /bin/sh

if [ $# != 1 ]; then
  echo "Usage: $0 blockchain-directory"
  exit 1
fi

blockchain_directory=$1

if [ ! \( -f $blockchain_directory/block_log -a -f $blockchain_directory/block_log.index \) ]; then
  echo "Could not find block_log and block_log.index in the directory $blockchain_directory"
  exit 1
fi

block_log_size=`stat -c %s "$blockchain_directory/block_log"`
block_index_size=`stat -c %s "$blockchain_directory/block_log.index"`
block_index_entry_count=`expr $block_index_size / 8`
head_block_number=`expr $block_index_entry_count`
echo "Last indexed block in the block log: $head_block_number"

read_offset_from_block_index_by_file_offset () {
  od -An -l -j$1 -N6 "$blockchain_directory/block_log.index" | sed -r 's/\s+//g'
}

read_offset_from_block_index_by_block_number () {
  read_offset_from_block_index_by_file_offset $(expr \( $1 - 1 \) \* 8)
}

read_offset_from_block_log_by_file_offset () {
  od -An -l -j$1 -N6 "$blockchain_directory/block_log" | sed -r 's/\s+//g'
}

read_offset_and_flags_from_block_log_by_file_offset () {
  od -An -l -j$1 -N8 "$blockchain_directory/block_log" | sed -r 's/\s+//g'
}

delete_index () {
  echo ""
  echo "Deleting the block index.  The index will be regenerated the next time you"
  echo "launch hived"
  echo ""
  echo "Ready to execute:"
  echo "  rm -f $blockchain_directory/block_log.index"

  while true; do
    read -p "Confirm (y/n)? " yn
    case $yn in
      [Yy]* ) break;;
      [Nn]* ) echo "aborting"; exit 1;;
      * ) echo "Please answer y or n.";;
    esac
  done

  rm -f $blockchain_directory/block_log.index
  echo "Done"
  exit 0
}

truncate_to () {
  truncate_to_block_number=$1
  truncate_block_log=$2
  truncate_block_index=$3

  echo "Trying to truncate to $truncate_to_block_number"

  if [ $truncate_block_log = true ]; then
    size_of_block_log_after_truncate=$(read_offset_from_block_index_by_block_number $(expr $truncate_to_block_number + 1))
    if [ $size_of_block_log_after_truncate -gt $block_log_size ]; then
      echo "Unable to truncate to block $truncate_to_block_number, the block log does not appear to contain"
      echo "that block.  That block should end at offset $size_of_block_log_after_truncate, "
      echo "but the block log is only $block_log_size bytes"
      exit 1
    fi
  fi

  if [ $truncate_block_index = true ]; then
    new_size_of_block_index=`expr $truncate_to_block_number \* 8`
    if [ $new_size_of_block_index -gt $block_index_size ]; then
      echo "Unable to truncate to block $truncate_to_block_number, the block index does appear to contain"
      echo "that block.  That block should end at offset $new_size_of_block_index, "
      echo "but the block log is only $block_index_size bytes"
      exit 1
    fi
  fi

  echo ""
  echo "Ready to execute:"
  if [ $truncate_block_log = true ]; then
    echo "  truncate -s $size_of_block_log_after_truncate $blockchain_directory/block_log"
  fi
  if [ $truncate_block_index = true ]; then
    echo "  truncate -s $new_size_of_block_index $blockchain_directory/block_log.index"
  fi

  while true; do
    read -p "Confirm (y/n)? " yn
    case $yn in
      [Yy]* ) break;;
      [Nn]* ) echo "aborting"; exit 1;;
      * ) echo "Please answer y or n.";;
    esac
  done

  if [ $truncate_block_log = true ]; then
    truncate -s $size_of_block_log_after_truncate $blockchain_directory/block_log
  fi
  if [ $truncate_block_index = true ]; then
    truncate -s $new_size_of_block_index $blockchain_directory/block_log.index
  fi
  echo "Done"
  exit 0
}

truncate_to_prompt_for_block () {
  max_block=$1

  while true; do
    echo ""
    echo "What block number do you want to truncate the log and index to?"
    read -p "Enter a block number less than $max_block or 'q' to quit: " truncate_to_block_num

    case $truncate_to_block_num in
      [Qq]*) echo "aborting"; exit 1;;
      ''|*[!0-9]*) echo "Invalid block number.  Please answer with a number or 'q'.";;
      *) if [ $truncate_to_block_num -lt $max_block ]; then
            truncate_to $truncate_to_block_num true true
          else
            echo "You must enter a number smaller than ${max_block}"
          fi;;
    esac
  done
}

last_block_start_offset_from_index=$(read_offset_from_block_index_by_block_number $head_block_number)
#echo "Last block start offset from index: $last_block_start_offset_from_index"
head_block_position_offset_in_block_log=`expr $block_log_size - 8`
last_block_start_offset_from_block_log=$(read_offset_from_block_log_by_file_offset $head_block_position_offset_in_block_log)
#echo "Last block start offset from block_log: $last_block_start_offset_from_block_log"
echo ""

if [ $last_block_start_offset_from_index -gt $last_block_start_offset_from_block_log ]; then
  echo "The index contains more blocks than the block log."
  echo "Trying to find the last number in the block log by searching the index."
  echo "This will take a few seconds..."
  offset_of_head_block_with_flags=$(read_offset_and_flags_from_block_log_by_file_offset $head_block_position_offset_in_block_log)
  found=$(od -Ad -w8 -l "$blockchain_directory/block_log.index" | grep "^ *[0-9][0-9]*  *${offset_of_head_block_with_flags}$")

  if [ $? -eq 0 ]; then
    offset_in_index=$(echo "$found" | cut -d ' ' -f 1)
    block_according_to_index=$(expr $offset_in_index / 8 + 1)
    echo "Found the block log's head block block number using index: $block_according_to_index"
    echo ""
    echo "To fix this, you can:"
    echo "1. Do nothing.  The next time you launch hived, it will automatically truncate the index"
    echo "   to match the block log."
    echo "2. Use this script to truncate the index to ${block_according_to_index} blocks to match the block log"
    echo "3. truncate both the block log and index to any number of blocks less than ${block_according_to_index}"
    while true; do
      read -p "What do you want to do: 1, 2, 3, or q to quit? " answer
      case $answer in
        1 ) echo "Exiting without making any changes"; exit 0;;
        2 ) truncate_to ${block_according_to_index} false true;;
        3 ) truncate_to_prompt_for_block ${block_according_to_index};;
        [Qq] ) echo "aborting"; exit 1;;
        * ) echo "Please answer 1, 2, 3, or q to quit.";;
      esac
    done
  else
    echo "Unable to find the head block number using the index"
    echo "This could mean one of two things:"
    echo " - your index is corrupted and doesn't match the block log, or"
    echo " - your block log is corrupted and ends in a partial block"
    echo ""
    echo "To fix this, you can:"
    echo "1. delete the index.  It will be recreated from the block log at hived startup."
    echo "   if the block log is not corrupted, this will restore you to a working state."
    echo "   Rebuilding the index can take on the order of an hour for a full block log on"
    echo "   a magnetic disk"
    echo "2. Use this script to truncate some blocks off the end of both the block log"
    echo "   and index to try to remove the corrupt blocks. (i.e., truncate to a block count"
    echo "   less than ${head_block_number} and see if it removes the corruption)."
    echo "   This is fine if you're planning to initiate a replay, but it will likely cause "
    echo "   problems if you're trying to load a shared_memory file that refers to the "
    echo "   corrupted blocks you're removing"
    while true; do
      read -p "What do you want to do: 1, 2, or q to quit? " answer
      case $answer in
        1 ) delete_index;;
        2 ) truncate_to_prompt_for_block ${head_block_number};;
        [Qq] ) echo "aborting"; exit 1;;
        * ) echo "Please answer 1, 2, or q.";;
      esac
    done
  fi
elif [ $last_block_start_offset_from_index -lt $last_block_start_offset_from_block_log ]; then
  echo "The block log contains more blocks than the index."
  echo "To fix this, you can:"
  echo "1. Do nothing.  The next time you launch hived, it will reconstruct the missing index"
  echo "   entries at startup. This will be fast if the number of missing index entries is small"
  echo "2. delete the index.  It will be recreated from scratch using the block log at hived"
  echo "   startup. Rebuilding the index can take on the order of an hour for a full block log on"
  echo "   a magnetic disk"
  echo "3. truncate both the block log and index to any number of blocks less than ${head_block_number}"
  echo "   This is fine if you're planning to initiate a replay, but it will likely"
  echo "   cause problems if you're trying to load a shared_memory file that refers to"
  echo "   blocks at or above ${head_block_number}"

  while true; do
    read -p "What do you want to do: 1, 2, 3, or q to quit? " answer
    case $answer in
      1 ) echo "Exiting without making any changes"; exit 0;;
      2 ) delete_index ;;
      3 ) truncate_to_prompt_for_block ${head_block_number} ;;
      [Qq] ) echo "aborting"; exit 1;;
      * ) echo "Please answer 1, 2, 3, or q.";;
    esac
  done
else
  echo "The block log and index are consistent with each other.  You don't need to"
  echo "do anything to fix them."
  echo ""
  echo "If you want to truncate the log to a smaller size, you can do that now."
  echo "Your options are:"
  echo "1. Do nothing"
  echo "2. Truncate the block log"

  while true; do
    read -p "What do you want to do: 1, 2, or q to quit? " answer
    case $answer in
      1|[Qq] ) echo "Exiting without making any changes"; exit 0;;
      2 ) truncate_to_prompt_for_block ${head_block_number};;
      * ) echo "Please answer 1, 2, or q.";;
    esac
  done
fi

# Notes on how to extract a possibly-compressed block.
#
# Let's say you want to extract block number 63,000,000:
# Read the offset of the start of the block from the index:
# ```
# $ od -An -l -j $(expr \( 63000000 - 1 \) \* 8) -N6 block_log.index | sed -r 's/\s+//g'
# 294494568365
# ```
# And the start of the next block, so we know when this block ends (corner case: if it's the head block, use the block_log's size instead):
# ```
# $ od -An -l -j $(expr 63000000 \* 8) -N6 block_log.index | sed -r 's/\s+//g'
# 294494582094
# ```
# Compute the block length:
# ```
# $ expr 294494582094 - 294494568365 - 8
# 13721
# ```
# And extract it to a file
# ```
# $ dd if=block_log of=/tmp/block.raw bs=1 skip=294494568365 count=13721
# 13721+0 records in
# 13721+0 records out
# 13721 bytes (14 kB, 13 KiB) copied, 0.0154765 s, 887 kB/s
# ```
# Now read the block's flags:
# ```
# $ od -An -l -j $(expr \( 63000000 - 1 \) \* 8 + 7) -N1 block_log.index | sed -r 's/\s+//g'
# 129
# ```
# If the high bit is set, the block is compressed with zstd.  If the low bit is set, it uses a custom dictionary.
# If you read a 0, you're done, /tmp/block.raw is already uncompressed.
# If you read 128, it's compressed, but without a custom dictionar.  Skip to the last step, and omit the `-D` parameter.
# In our case, both the high and low bits are set.  To decompress, we need to know which dicitonary was used, so read it:
# ```
# $ od -An -l -j $(expr \( 63000000 - 1 \) \* 8 + 6) -N1 block_log.index | sed -r 's/\s+//g'
# 63
# ```
# We store our dictionaries in compressed form in the respository, so decompress the required dictionary:
# ```
# $ zstd -d -o /tmp/063M.dict libraries/chain/compression_dictionaries/063M.dict.zst
# ../../libraries/chain/compression_dictionaries/063M.dict.zst: 225280 bytes
# ```
# Now use it to decompress our block:
# (note: the zstd command-line utility requres the compressed file to start with a magic number.
#  when we store the blocks in the block log, we omit the magic number to save space, so we must
#  add it back here)
# ```
# $ zstd -d -o /tmp/block -D /tmp/063M.dict <(echo -n -e '\x28\xb5\x2f\xfd'; cat /tmp/block.raw)
# /dev/fd/63          : 27108 bytes
# ```
