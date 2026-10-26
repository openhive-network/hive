#! /bin/bash
set -euo pipefail

INDEX_FIELDS="index_name index_size item_sizeof item_additional_allocation additional_container_allocation total_index_mem_usage"

print_help () {
cat <<EOF
Usage: $0 <command> [OPTION]... [FILE]

Converts replay_benchmark.jsonl, dumped by hived run with --dump-memory-details (one json record per line), into CSV
that can be imported into a spreadsheet and sorted or charted there. Reads standard input when FILE is not given.
Requires jq.

Commands:
  records              One row per record: block number, summary type (empty for regular measurements), times,
                       memory and comment archive stats.
  indexes              One row per record, one column per index (in natural order) holding selected field.
    --field=FIELD      Index field to show (default: total_index_mem_usage).
  record               One row per index of single record.
    --select=SELECTOR  Summary type (initial, replay, sync, live) or block number of the record; when more records
                       match, the last one is taken. Default: the last record.
    --sort=FIELD       Sort indexes by given field (numbers descending, names ascending). Default: natural order.

Index fields: ${INDEX_FIELDS// /, }

Examples:
  $0 records replay_benchmark.jsonl > records.csv
  $0 indexes --field=index_size replay_benchmark.jsonl > objects.csv
  $0 record --select=replay --sort=total_index_mem_usage replay_benchmark.jsonl
EOF
}

require_index_field () {
  if [[ " $INDEX_FIELDS " != *" $1 "* ]]; then
    echo "ERROR: '$1' is not an index field (${INDEX_FIELDS// /, })" >&2
    exit 2
  fi
}

COMMAND=""
FIELD="total_index_mem_usage"
SELECT=""
SORT=""
FILES=()

while [ $# -gt 0 ]; do
  case "$1" in
    --field=*)
        FIELD="${1#*=}"
        ;;
    --select=*)
        SELECT="${1#*=}"
        ;;
    --sort=*)
        SORT="${1#*=}"
        ;;
    --help|-h|-?)
        print_help
        exit 0
        ;;
    -*)
        echo "ERROR: '$1' is not a valid option" >&2
        exit 2
        ;;
    *)
        if [ -z "$COMMAND" ]; then
          COMMAND="$1"
        else
          FILES+=("$1")
        fi
        ;;
  esac
  shift
done

case "$COMMAND" in
  records)
    jq -nr '
      input as $first
      | ( $first.comment_archive_stats | keys_unsorted ) as $stats
      | ( [ "block_number", "summary", "real_ms", "cpu_ms", "current_mem", "peak_mem", "shm_free" ]
          + [ $stats[] | ( . + "_count", . + "_time_ns" ) ] | @csv ),
        ( $first, inputs
          | [ .block_number, .summary // "", .real_ms, .cpu_ms, .current_mem, .peak_mem, .shm_free ]
            + [ .comment_archive_stats[ $stats[] ] | ( .count, .time_ns ) ]
          | @csv )
    ' ${FILES[@]+"${FILES[@]}"}
    ;;
  indexes)
    require_index_field "$FIELD"
    jq -nr --arg field "$FIELD" '
      input as $first
      | ( $first.index_memory_details_cntr | map( .index_name ) ) as $names
      | ( [ "block_number", "summary" ] + $names | @csv ),
        ( $first, inputs
          | ( .index_memory_details_cntr | map( { key: .index_name, value: .[ $field ] } ) | from_entries ) as $values
          | [ .block_number, .summary // "" ] + [ $values[ $names[] ] ]
          | @csv )
    ' ${FILES[@]+"${FILES[@]}"}
    ;;
  record)
    [ -z "$SORT" ] || require_index_field "$SORT"
    jq -nr --arg select "$SELECT" --arg sort "$SORT" '
      def selected:
        if $select == "" then true
        elif ( $select | test( "^[0-9]+$" ) ) then .block_number == ( $select | tonumber )
        else .summary == $select
        end;
      def ordered:
        if $sort == "" then .
        elif $sort == "index_name" then sort_by( .index_name )
        else sort_by( -.[ $sort ] )
        end;
      reduce ( inputs | select( selected ) ) as $record ( null; $record )
      | if . == null then error( "no record matching \"" + $select + "\"" ) else . end
      | [ "index_name", "index_size", "item_sizeof", "item_additional_allocation", "additional_container_allocation",
          "total_index_mem_usage" ] as $columns
      | ( $columns | @csv ), ( .index_memory_details_cntr | ordered | .[] | [ .[ $columns[] ] ] | @csv )
    ' ${FILES[@]+"${FILES[@]}"}
    ;;
  *)
    print_help
    exit 2
    ;;
esac
