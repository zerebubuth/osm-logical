MODULES = osm-logical
PGFILEDESC = "osm-logical - OSM logical replication"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
