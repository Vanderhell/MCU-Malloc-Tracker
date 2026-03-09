#!/bin/bash
# Decode MCU Malloc Tracker binary snapshot
# Usage: ./decode_snapshot.sh snapshot.bin [--filemap filemap.txt]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/mt_decode.py"

if [ ! -f "$PYTHON_SCRIPT" ]; then
    echo "Error: mt_decode.py not found at $PYTHON_SCRIPT"
    exit 1
fi

if [ $# -eq 0 ]; then
    echo "Usage: $0 <snapshot.bin> [--filemap <filemap.txt>] [--json <output.json>] [--top N]"
    echo ""
    echo "Example:"
    echo "  $0 snapshot_dump.bin"
    echo "  $0 snapshot_dump.bin --filemap filemap.txt"
    exit 1
fi

python3 "$PYTHON_SCRIPT" "$@"
