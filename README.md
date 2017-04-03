# OSM Logical Replication

This is an experiment to see if it's possible to replace the current method of replication, which relies on unsupported features in PostgreSQL, with logical replication. See [operations/#154](https://github.com/openstreetmap/operations/issues/154) for background and more details.

## What is logical replication?

PostgreSQL (since 9.4?) supports the ability to register "logical decoding" plugins, which can access the stream of modifications to the database. This can be used for [replication of the full database](https://2ndquadrant.com/en-us/resources/pglogical/) between PostgreSQL instances, even of different versions. It can also be used to extract out the modifications that we're interested in for creating [OSM replication files](https://wiki.openstreetmap.org/wiki/Planet.osm/diffs) (a.k.a "diffs").

## Why use logical replication?

The main advantage of logical replication is that it requires no changes to the OSM schema and, to some extent, can be made insensitive to the parts of the schema it doesn't need to access. This means it can be the least invasive method of replication.

The main disadvantage is that it's extremely closely tied to PostgreSQL, and would increase the difficulty of moving to any other database.

## How does it work?

At the moment, it does very little. Dumping the replication data when inserting a few nodes into the `nodes` table gives something like:

```
openstreetmap=# SELECT * FROM pg_logical_slot_get_changes('replication_slot', NULL, NULL);
 location   |  xid  |     data
-------------+-------+---------------
59/DE3EF8C0 | 53776 | BEGIN
59/DE3EF8C0 | 53776 | NEW nodes 1 3
59/DE3F0150 | 53776 | NEW nodes 2 3
59/DE3F02D0 | 53776 | NEW nodes 3 4
59/DE3F0450 | 53776 | NEW nodes 4 3
59/DE3F07A8 | 53776 | COMMIT
(6 rows)
```

There will need to be an additional process which receives the log and:

1. Keeps a list of all new or redacted elements which have not yet been committed.
2. Decides on a `location` at which to break the stream into minutely chunks.
3. Fetches data versions from the database to build the `osmChange` document.

It would be possible to collect the data versions directly from the logical replication stream. However, this would increase the size of the state necessary to track the replication significantly, and the data in the nodes, ways and relations tables of the OSM database are mostly immutable. The one major exception to this is redactions, so this scheme does technically allow for "loss" of a redacted version if it is redacted before the additional process fetches that data. However, if it was desirable to redact that version anyway, this seems like an inconsequential loss.

## Building

Instructions below are for Ubuntu 16.04. Other Debian-based distros may be similar, but YMMV. This requires PostgreSQL at least version 9.4, but only tested with version 9.5.6-0ubuntu0.16.04.

```
sudo apt install build-essential postgresql postgresql-server-dev-all
make
sudo make install
```

You will need to make sure that your PostgreSQL settings have `wal_level=logical` and `max_replication_slots` set to at least 1.

You will then have to set up a logical replication slot. For example, these are the commands to set up a replication slot, get the latest changes and delete the replication slot:

```
select * from pg_create_logical_replication_slot('replication_slot', 'osm-logical');
SELECT * FROM pg_logical_slot_get_changes('replication_slot', NULL, NULL);
SELECT pg_drop_replication_slot('replication_slot');
```
