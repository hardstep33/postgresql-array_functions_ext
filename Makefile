EXTENSION = array_functions_ext
MODULES = array_functions_ext

# Source files
OBJS = array_functions_ext.o

# Extension SQL scripts
DATA = array_functions_ext--1.0.sql

# Include PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Compiler flags (optional, adjust as needed)
# PG_CFLAGS = -Wall -Wextra -O2
