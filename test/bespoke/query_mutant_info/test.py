import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_INSTALLED_EXECUTABLE = Path(DREDD_REPO_ROOT, 'third_party', 'clang+llvm', 'bin', 'dredd')
QUERY_MUTANT_INFO_SCRIPT = Path(DREDD_REPO_ROOT, 'scripts', 'query_mutant_info.py')

EXPECTED_MUTATION_1 = """
Remove statement at math/src/exp.cc, 83:3--94:4

This is the removed statement:

  if (y != 1) {
    for (int i = 0; i < 100; ++i) {
      int m = 0;
      while (!(2 <= z && z < 4)) {
        z *= z;
        ++m;
      }
      constant *= Pow(2, -m);
      result += constant;
      z /= 2;
    }
  }
"""

EXPECTED_MUTATION_2 = """
Remove statement at math/src/exp.cc, 84:5--93:6

This is the removed statement:

    for (int i = 0; i < 100; ++i) {
      int m = 0;
      while (!(2 <= z && z < 4)) {
        z *= z;
        ++m;
      }
      constant *= Pow(2, -m);
      result += constant;
      z /= 2;
    }
"""

EXPECTED_MUTATION_3 = """
Replace binary operator expression at math/src/exp.cc, 92:7--92:13

Original binary operator expression:

z /= 2

Replacement expression:

z -= 2
"""

EXPECTED_MUTATION_4 = """
Replace expression at math/src/exp.cc, 92:12--92:13

Original expression:

2

Replacement expression:

0.0
"""

EXPECTED_MUTATION_5 = """
Replace expression at math/src/exp.cc, 81:19--81:40

Original expression:

Pow(2, -logValue) * x

Replacement expression:

- (Pow(2, -logValue) * x)
"""

EXPECTED_MUTATION_6 = """
Replace expression at math/src/exp.cc, 59:10--59:41

Original expression:

(exp < 0) ? 1 / result : result

Replacement expression:

1.0
"""


def run_successfully(cmd):
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Command exited with error code {result.returncode}: {' '.join([str(c) for c in cmd])}")
        print(result.stdout.decode('utf-8'))
        print(result.stderr.decode('utf-8'))
        sys.exit(1)
    return result


def main():
    path_to_original_code = Path(DREDD_REPO_ROOT, 'examples', 'math')

    # Make a copy of the "math" example
    shutil.copytree(src=path_to_original_code,
                    dst='math')

    # Configure the math example (with tests disabled)
    run_successfully(['cmake',
                      '-S',
                      'math',
                      '-B',
                      'build',
                      '-G',
                      'Ninja',
                      '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                      '-DDREDD_EXAMPLES_MATH_BUILD_TESTS=OFF',
                      ])

    # Mutate the source files of the math example
    run_successfully([DREDD_INSTALLED_EXECUTABLE,
                      '-p',
                      'build',
                      '--mutation-info-file',
                      'mutant-info.json',
                      'math/math/src/exp.cc'])

    # Get the largest mutant id
    largest_mutant_id = int(run_successfully(
        ["python", QUERY_MUTANT_INFO_SCRIPT, "--largest-mutant-id", "mutant-info.json"]).stdout.decode('utf-8'))

    all_mutant_info = ""
    for mutant in range(0, largest_mutant_id + 1):
        all_mutant_info += str(run_successfully(
            ["python",
             QUERY_MUTANT_INFO_SCRIPT,
             "--show-info-for-mutant",
             str(mutant),
             "--path-prefix-replacement",
             os.path.abspath(os.curdir + os.sep + "math"),
             str(path_to_original_code),
             "mutant-info.json"]).stdout.decode('utf-8'))

    for expected_mutation in [EXPECTED_MUTATION_1,
                              EXPECTED_MUTATION_2,
                              EXPECTED_MUTATION_3,
                              EXPECTED_MUTATION_4,
                              EXPECTED_MUTATION_5,
                              EXPECTED_MUTATION_6,
                              ]:
        if '\n'.join(expected_mutation.splitlines()) not in all_mutant_info:
            print("=======")
            print(expected_mutation)
            print("=======")
            print('\n'.join(expected_mutation.splitlines()))
            print("=======")
            print(all_mutant_info[0:1000])
            print("=======")
            print('\n'.join(all_mutant_info[0:1000].splitlines()))
            
            


            
            print(f"Did not find expected mutation info:\n{expected_mutation}")
            sys.exit(1)


if __name__ == '__main__':
    sys.exit(main())
