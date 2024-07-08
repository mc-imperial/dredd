This provides a way of confirming that Dredd behaves as expected on a
set of single-file test cases.

Whenever a new feature is added to Dredd, or a defect is discovered
when using Dredd, a test case should be added to this test set in
response. The test case should ensure that Dredd produces the expected
output on a given input program.

A test case consists of a `.c` or `.cc` file and a corresponding `.expected`
file.

The `check_single_file_tests.sh` script can be used to run Dredd on
all single-file test cases, confirming that results are as expected
and that every mutated file compiles.

A change in Dredd may require many of the `.expected` file to be
updated. If you are confident that the change is a good one, the
`regenerate_single_file_expectations.sh` can be used to update all
such files based on Dredd's current behaviour.
