# open-addressing-schemes

## What is this?

A rudimentary hash table underlying a key-value store.
Implemented some different open-addressing schemes for comparison purposes.

- Linear probing
- Quadratic probing
- Ordered linear probing
- Graveyard hashing

## How to use

This is ultra rough code, just proof of concept for running some benchmarks.

Each directory contains a different version of the k/v store.
They are completely separate and don't refer to each other in any way.
Each has its own makefile.  Run `make` to build a version.

Then, run the program (provide -? as an argument to get a list of parameters).
`runtest` is a python script that run the same test multiple times, varying the table size.

