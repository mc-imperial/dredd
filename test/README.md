Dredd end-to-end tests
======================

There are two kinds of test:

- Single file tests, which checks that Dredd produces expected source code when applied to a single file, and that the mutated source code compiles. The mutated code is not executed.

- Execute tests, which check that a Dredd-mutated program produces given expected results when executed with and without mutants enabled. This is useful to ensure that certain outputs are always observed, no matter what optimisations are in place.

See README files in each subdirectory for more details.
