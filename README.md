# open-addressing-schemes

Rudimentary hash table underlying a key-value store.
Implemented some different open-addressing schemes for comparison purposes.

- Linear probing
- Ordered linear probing
- Graveyard hashing

Hash tables in the `hashtables` directory.  Instantiate with key and
value types (default int key, int value).

The `testers` directory contains some header only test benches.
Instantiate using one of the table types found in `hashtables`.
