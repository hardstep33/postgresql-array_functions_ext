#!/bin/bash
#
# PostgreSQL extension installer for array_functions_ext
# Usage: sudo ./install.sh [prefix]
#
# Without arguments searches pg_config in PATH.
# With prefix (e.g. /usr/local/postgresql or /usr/pgsql-15):
#   If <prefix>/bin/pg_config exists it will be used,
#   otherwise it will search pg_config in PATH.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${1:-}"

if [ -n "$PREFIX" ] && [ -x "$PREFIX/bin/pg_config" ]; then
    PG_CONFIG="$PREFIX/bin/pg_config"
else
    PG_CONFIG="${PG_CONFIG:-$(command -v pg_config || true)}"
fi

if [ -z "$PG_CONFIG" ] || [ ! -x "$PG_CONFIG" ]; then
    echo "ERROR: pg_config not found." >&2
    echo "Install postgresql-15-server or set PG_CONFIG environment variable." >&2
    exit 1
fi

echo "Using pg_config: $PG_CONFIG"

SHAREDIR="$($PG_CONFIG --sharedir)"
PKGLIBDIR="$($PG_CONFIG --pkglibdir)"
BITDIR="$PKGLIBDIR/bitcode"

echo "Install SQL/control to: $SHAREDIR/extension"
echo "Install shared library to: $PKGLIBDIR"

mkdir -p "$SHAREDIR/extension"
mkdir -p "$PKGLIBDIR"

cp "$SCRIPT_DIR/array_functions_ext.control" "$SHAREDIR/extension/"
cp "$SCRIPT_DIR/array_functions_ext--1.0.sql" "$SHAREDIR/extension/"
cp "$SCRIPT_DIR/array_functions_ext.so" "$PKGLIBDIR/"

# Optional: install bitcode for JIT
if [ -f "$SCRIPT_DIR/array_functions_ext.bc" ]; then
    mkdir -p "$BITDIR/array_functions_ext"
    cp "$SCRIPT_DIR/array_functions_ext.bc" "$BITDIR/array_functions_ext/"
    echo "Installed bitcode for JIT."
fi

echo "array_functions_ext installed successfully!"
echo "Run in psql: CREATE EXTENSION array_functions_ext;"
