# array_functions_ext

PostgreSQL extension with array utility functions: `array_map_concat`, `array_sum`, `array_exists`, `array_match`.

## Requirements

- PostgreSQL 15 (matching the ABI of the compiled `.so`)
- Standard postgresql-server package (only for `pg_config`)

## Install

Copy all files from this folder to the target server and execute:

```bash
sudo ./install.sh
```

If PostgreSQL is installed in a custom prefix:
```bash
sudo ./install.sh /usr/local/postgresql
```

Or use an explicit `pg_config`:
```bash
PG_CONFIG=/usr/pgsql-15/bin/pg_config sudo -E ./install.sh
```

Then in psql:
```sql
CREATE EXTENSION array_functions_ext;
```

## Files

| File | Destination |
|------|-------------|
| `array_functions_ext.control` | `SHAREDIR/extension/` |
| `array_functions_ext--1.0.sql` | `SHAREDIR/extension/` |
| `array_functions_ext.so` | `PKGLIBDIR/` |
| `array_functions_ext.bc` | `PKGLIBDIR/bitcode/array_functions_ext/` (optional, for JIT) |

