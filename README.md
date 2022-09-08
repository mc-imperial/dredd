# Dredd

Dredd is a tool that aims to enable [mutation testing and mutation
analysis](https://en.wikipedia.org/wiki/Mutation_testing) of large C++
code bases. Dredd is named after [Judge
Dredd](https://en.wikipedia.org/wiki/Judge_Dredd), because mutation
analysis can be used to judge the effectiveness of test suites, by
measuring test suite adequacy.

To apply Dredd to a C++ project, you must first generate a
[*compilation
database*](https://clang.llvm.org/docs/JSONCompilationDatabase.html)
for the project: a `compile_commands.json` file. Compilation databases
can be automatically generated by most modern build systems that
support C++ - [here is a great resource on compilation
databases](https://sarcasm.github.io/notes/dev/compilation-database.html).

You then point Dredd at (a) the C++ source files that you would like
to be mutated, and (b) the compilation database, which must provide
entries describing the compile commands that these source files
require.

Dredd will modify the source files in place, injecting a large number
of mutants. By default, each mutant is *disabled* and will have no
effect, so if you execute your project's executables or test suites in
the normal way they should behave normally (albeit running more slowly
due to the overhead of Dredd's instrumentation).

An environment variable, `DREDD_ENABLED_MUTATION`, can be used to
enable a mutant. Setting this environment variable to a non-negative
integer value $i$ will enable mutant number $i$.
Alternatively, a comma-separated list of non-negative integer values
can be used to enable multiple mutants, leading to a so-called
*higher-order mutant*.

In mutation testing parlance, rather than creating mutants, Dredd
creates a *mutant schema* or *meta-mutant*, simultaneously encoding
all the mutants that it *could* generate into the source code of the
program.

## Using Dredd

TODO(https://github.com/mc-imperial/dredd/issues/29): provide support
for Dredd releases (on Linux at least), so that folks can use Dredd
without having to build it from source.


We first show how to apply Dredd to a simple stand-alone program. We will then show how to apply it to a larger C++ CMake project.

Before following these instructions, make sure you have built Dredd and that the `dredd` executable is located under `<repository-root>/third_party/clang+llvm-13.0.1/bin`.

### Applying Dredd to a single file example

`pi.cc` is a small example in `examples/simple` that calculates an approximation of pi. To run this example, run the following from the root of the repository:
```
# This will modify pi.cc in-place.
third_party/clang+llvm-13.0.1/bin/dredd examples/simple/pi.cc
# clang++ can be replaced with your favourite C++ compiler.
clang++ examples/simple/pi.cc -o examples/simple/pi
```
Due to the changes made by Dredd, the compile command may lead to warnings being issued but this is expected.  The changes to `pi.cc` can be viewed by running `git diff` although, these changes are not meant to be human-readable.

Running the executable with no additional arguments via `./examples/simple/pi` will produce the expected result of 3.14159.  You can specify which mutants you want to use at runtime by running:
```
DREDD_ENABLED_MUTATION=$i$ ./examples/simple/pi
```
where $i$ is the id of the mutation in the program generated by Dredd. For example, running `DREDD_ENABLE_MUTATION=2 ./examples/simple/pi` will enable mutation number 2 in the modified program. 
This may print a different output to the original program and in some cases may not terminate. 
There is a possibility that equivalent mutants are generated which can lead to the printed value being unchanged.

You can also enable multiple mutants by setting the environment variable to a comma-separated list. For example, running `DREDD_ENABLE_MUTATION="0,2,4" ./examples/simple/pi` requests that mutants 0, 2 and 4 are enabled simultaneously.

To clean up and restore the file `pi.cc` to it's initial state, run
```
rm examples/simple/pi
git checkout HEAD examples/simple/pi.cc
```

### Applying Dredd to a CMake project

The `examples/math` directory contains a small math library to illustrate how to apply Dredd to a larger project.
To compile the math library, run the following from the root of the repository:
```
cd examples/math
mkdir build && cd build
cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
cmake --build .
```
The `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` argument causes CMake to output a compilation database called `compile_comands.json` to the `build` directory.
You can read more about how to generate compilation databases using various build systems [here](https://sarcasm.github.io/notes/dev/compilation-database.html).

You can check the output of the tests before mutations have been applied by running `./mathtest/mathtest` from the build directory.
All tests should pass.

To apply mutants to a single file in the library, run the following command:
```
/path/to/dredd/repo/third_party/clang+llvm-13.0.1/bin/dredd -p compile_commands.js <path-to-file>
``` 
For example, running `../../../third_party/clang+llvm-13.0.1/bin/dredd ../math/src/exp.cc` from the build directory will apply
mutants to the file `exp.cc`. 

To view the changes that Dredd has made you can do `git status` to see that `exp.cc` has changed, and `git diff` to see
the effect that Dredd has had on this file. These changes will be hard to understand as they are not intended to be
readable by humans.

The `-p` option allows the compilation database generated by CMake above to be passed to Dredd.
This is so that Dredd knows the correct compiler options to use when processing each source file.

If you wish to mutate multiple source files simultaneously, pass them all to a single invocation of Dredd. 
For the example project we provide a shell script to do this, `mutate.sh`.

To revert the changes that Dredd made to `exp.cc`, run:
```
git checkout HEAD ../math/src/exp.cc
# Confirm that git now regards the file exp.cc as having no modifications
git status ../math/src/exp.cc
```

Now, from the project's build directory, run:
```
../mutate.sh
```
This will mutate all of the `.cc` files in the library.

As with the single file above, you can run `git status` to see which files have changed, and `git diff` to see
the effect that Dredd has had on these files. Again, these changes will be hard to understand as they are not intended to be
readable by humans.

The project can then be recompiled with
```
cmake --build .
```
Due to the changes made by Dredd, it is likely that many warnings will be issued during compilation.
This is expected, because the mutants that Dredd introduces leads to code that violates many good
programing practices. If your project treats compiler warnings as errors then you will need to disable
this feature in your project's build configuration.

Running the test suite with `./mathtest/mathtest` will result in all the tests passing as before, because no mutants are enabled. 
You can specify a mutant to apply at runtime by setting the `DREDD_ENABLED_MUTATION` environment variable to a non-negative integer value.
```
DREDD_ENABLED_MUTATION=$i$ ./mathtest/mathtest
```
where $i$ is the of the mutation in the modified library generated by Dredd. For example, running 
`DREDD_ENABLED_MUTATION=1 ./mathtest/mathtest` will enable mutation number 1 in the modified library.
This may cause some tests to fail or in certain cases, not terminate. There is the possibility of equivalent mutants
being generated which can lead to all tests still passing.

You can also enable multiple mutants by setting `DREDD_ENABLED_MUTATION` to a comma-separated list of values. For example, running 
`DREDD_ENABLED_MUTATION="0,2,4" ./mathtest/mathtest` will enable mutants 0, 2 and 4 in the modified library.

To clean up the `examples/math` directory, run the following from the build directory:
```
cd .. && rm -rf build
git checkout HEAD .
```

## Building Dredd from source

The following instructions have been tested on Ubuntu 20.04.

### Prerequisites

- curl (used to fetch a Clang/LLVM release)
- CMake version 3.13 or higher
- ninja version 1.10.0 or higher

### Clone the repository and get submodules

Either do:

```
git clone --recursive https://github.com/mc-imperial/dredd.git
```

or:

```
git clone --recursive git@github.com:mc-imperial/dredd.git
```

The `--recursive` flag ensures that submodules are fetched.

### Downloading Clang/LLVM

Dredd builds against various Clang and LLVM libraries. Rather than including Clang/LLVM as a submodule of Dredd and building it from source, Dredd assumes that a built release of Clang/LLVM has been downloaded into `third_party`.

From the root of the repository, execute the following commands:

```
cd third_party
# The release file is pretty large, so this download may take a while
curl -Lo clang-13.0.1.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.1/clang+llvm-13.0.1-x86_64-linux-gnu-ubuntu-18.04.tar.xz
tar xf clang-13.0.1.tar.xz
mv clang+llvm-13.0.1-x86_64-linux-gnu-ubuntu-18.04 clang+llvm-13.0.1
rm clang-13.0.1.tar.xz
cd ..
```

### Build steps

From the root of the repository, execute the following commands.
Change `Debug` to `Release` for a release build.

```
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
# TODO: the following should be handled using a proper cmake install command
cp src/dredd/dredd ../third_party/clang+llvm-13.0.1/bin
```

## Guide for developers

This project uses the [Google C++ style guide](https://google.github.io/styleguide/cppguide.html). 

The `scripts` directory contains a number of commands that are useful for developers. To make use of these commands,
you must first run `./dev_shell.sh.template` from the root of the Dredd repo. This will ensure that the necessary
environment variables are set as well as building tools that are used in other check commands.

The last four scripts in this section assume that a build Dredd is under `temp/build-Debug`, as they copy the Dredd
binary to `third_party/clang+llvm-13.0.1/bin` to execute the test files. Therefore, to use these scripts, 
one should ensure that `check_build.sh` has been executed at least up to the point where a debug build has finished.

The commands that can be run in isolation in the `scripts` directory are:
- `check_all.sh` : This runs a combination of the commands below to ensure that Dredd is formatted and functioning as expected.
- `check_build.sh` : This checks that dredd can be build and that the unit tests included in the `test` directory still pass
- `check_clang_tidy.sh` : This runs `clang-tidy` on Dredd's source files.
- `check_clean_build.sh` : This checks that Dredd can be build from scratch by deleting any previous builds first and running
the provided unit tests.
- `check_cmakelint.sh` : This runs cmake-lint on Dredd's cmake files. 
- `check_cpplint.sh` : Runs a static check to ensure the code style is correct.
- `check_format.sh` : This uses `clang-format` and `cmake-format` to ensure that all source files and cmake files `src`
and `examples` are formatted correctly.
- `check_headers.py` : This checks that the necessary files contain a copyright header.
- `fix_format.sh` : This command formats all the Dredd source files and example files using `clang-format` and formats
  the Dredd cmake files and example cmake files using `cmake-format`.
- `check_one_single_file_test.sh` :This will run Dredd on a specific `.c` or `.cc` file in the `tests/single_file` directory
  and ensure the output is the same as the respective `.expected` file.
- `check_single_file_tests.sh` : This acts the same as the above commands, but will compare the output from Dredd for all
`.c` and `.cc` files in the `test/single_file` directory.
- `regenerate_one_single_file_expectation.sh` : This takes in a `.c` or `.cc` file from the `test/single_file` directory and
regenerates its respective `.expected` file. You should only do this if you are certain the changes you have made to Dredd are correct.
- `regenerate_single_file_expectations` : This acts the same as the above command but will regenerate all the `.expected`
files for the `.c` and `.cc` files in the `test/single_file` file directory.

The auxiliary commands in the `scripts` directory are:
- `check_cppcheck.sh` : This runs a static check to detect bugs and undefined behaviour.
- `check_iwyu.sh` : This runs include what you use to check that all the necessary header files are included.

### Working on Dredd from CLion

## Planned features

See [planned feature
issues](https://github.com/mc-imperial/dredd/issues?q=is%3Aissue+is%3Aopen+label%3Aplanned-feature)
in the Dredd GitHub repository.

If you would like to see a feature added to Dredd, please [open an
issue](https://github.com/mc-imperial/dredd/issues) and we will
consider it.
