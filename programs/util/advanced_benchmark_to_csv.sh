#! /bin/bash
set -euo pipefail

ITEM_FIELDS="op_name context time count time_per_count"
NUMERIC_FIELDS="time count time_per_count"

print_help () {
cat <<EOF
Usage: $0 <command> [OPTION]... [FILE]

Converts advanced_benchmark.json, dumped into data dir by hived run with --advanced-benchmark, into CSV that can be
imported into a spreadsheet and sorted or charted there. Reads standard input when FILE is not given. Requires jq.

Each item of the file holds total time and count of executions of one part of code measured in one context:
evaluator of operation (empty context), validation of operation (validate), automatic processing (processing),
reaction of a plugin to notification (pre->PLUGIN, post->PLUGIN) etc. Times are in nanoseconds, time_per_count is
average time of single execution (rounded up). Time of nested measurement (e.g. plugin reaction to virtual operation
emitted by evaluator) is not included in time of the enclosing one (evaluator).

Commands:
  items                One row per item: name, context, time, count and time per count.
    --sort=FIELD       Sort items by given field (numbers descending, names ascending). Default: by name, then by
                       context.
  names                One row per item name, one column per context (in alphabetical order, so the first one, with
                       empty header, holds evaluators) with selected field of the item; empty when the name was not
                       measured in given context.
    --field=FIELD      Numeric item field to show (default: time_per_count).

Item fields: ${ITEM_FIELDS// /, }

Examples:
  $0 items advanced_benchmark.json > items.csv
  $0 items --sort=time advanced_benchmark.json | head -20
  $0 names advanced_benchmark.json > names.csv
EOF
}

require_field () {
  if [[ " $2 " != *" $1 "* ]]; then
    echo "ERROR: '$1' is not a valid field (${2// /, })" >&2
    exit 2
  fi
}

COMMAND=""
FIELD="time_per_count"
SORT=""
FILES=()

while [ $# -gt 0 ]; do
  case "$1" in
    --field=*)
        FIELD="${1#*=}"
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
  items)
    [ -z "$SORT" ] || require_field "$SORT" "$ITEM_FIELDS"
    jq -r --arg sort "$SORT" '
      def ordered:
        if $sort == "" then .
        elif $sort == "op_name" or $sort == "context" then sort_by( .[ $sort ] )
        else sort_by( -.[ $sort ] )
        end;
      [ "op_name", "context", "time", "count", "time_per_count" ] as $columns
      | ( $columns | @csv ), ( .items | ordered | .[] | [ .[ $columns[] ] ] | @csv )
    ' ${FILES[@]+"${FILES[@]}"}
    ;;
  names)
    require_field "$FIELD" "$NUMERIC_FIELDS"
    jq -r --arg field "$FIELD" '
      ( .items | map( .context ) | unique ) as $contexts
      | ( reduce .items[] as $item ( {}; .[ $item.op_name ][ $item.context ] = $item[ $field ] ) ) as $values
      | ( [ "op_name" ] + $contexts | @csv ),
        ( $values | keys[] as $name | [ $name ] + [ $values[ $name ][ $contexts[] ] ] | @csv )
    ' ${FILES[@]+"${FILES[@]}"}
    ;;
  *)
    print_help
    exit 2
    ;;
esac
