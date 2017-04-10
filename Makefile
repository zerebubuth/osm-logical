MODULES = osm-logical
PGFILEDESC = "osm-logical - OSM logical replication"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# test cases
check: test.log

.PHONY: test.log
test.log:
	ruby tests/test.rb 2>&1 | tee test.log
