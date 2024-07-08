Dredd *execute* tests
=====================

There is one subdirectory per test.

Each subdirectory must contain:

- `harness.c(c)` - a C or C++ file with a `main`, that should print a single line of output.

- `tomutate.c(c)` - a C or C++ file that will be mutated. The harness should call functions of this file.

- `original.txt` - A file containing the single line of output that should be printed by the harness when no mutants are enabled.

- `mutants.txt` - A file containing various outputs that should be
  expected when mutants are used. Each line can feature a comment, of
  the form `;` - the content of the line from the semi-colon onward
  will be ignored. This allows one to document why particular outputs
  are expected for particular mutants.

See the script that runs `execute` tests for precise details of how they work.
