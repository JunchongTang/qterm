#!/bin/bash
# Records a real command's terminal output as a reference case.
#
#   ./record_case.sh <case-name> <columns> <rows> <command...>
#
# Example:
#   ./record_case.sh ls-color 80 24 ls --color=always -la /usr/share
#
# Runs the command under a pty of the requested size so it emits exactly what it
# would to a real terminal, then writes input.bin and case.json. Generate the
# snapshot afterwards with:  QTERM_UPDATE_REFS=1 ./qterm_ref_tests
set -euo pipefail

if [ $# -lt 4 ]; then
    sed -n '2,12p' "$0"
    exit 1
fi

NAME="$1"; COLUMNS_="$2"; ROWS_="$3"; shift 3
DIR="$(cd "$(dirname "$0")" && pwd)/$NAME"
mkdir -p "$DIR"

# script(1) gives us a pty; stty inside it fixes the geometry so wrapping and
# any size queries match what the case declares.
if [ "$(uname)" = "Darwin" ]; then
    script -q /dev/null bash -c "stty columns $COLUMNS_ rows $ROWS_; $*" > "$DIR/input.bin" || true
else
    script -qc "stty columns $COLUMNS_ rows $ROWS_; $*" /dev/null > "$DIR/input.bin" || true
fi

cat > "$DIR/case.json" <<EOF
{
  "columns": $COLUMNS_,
  "rows": $ROWS_,
  "chunk": 4096,
  "recordedFrom": "$*"
}
EOF

printf '%s: %s bytes\n' "$NAME" "$(wc -c < "$DIR/input.bin" | tr -d ' ')"
echo "Now run: QTERM_UPDATE_REFS=1 ./qterm_ref_tests"
