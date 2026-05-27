EXTENSION = array_functions_ext
MODULES = array_functions_ext
DATA = array_functions_ext--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
