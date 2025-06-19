import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_EXECUTABLE = Path(DREDD_REPO_ROOT, 'temp', 'build-Debug', 'src', 'dredd', 'dredd') if 'DREDD_EXECUTABLE' not in os.environ else os.environ['DREDD_EXECUTABLE']
QUERY_MUTANT_INFO_SCRIPT = Path(DREDD_REPO_ROOT, 'scripts', 'query_mutant_info.py')

FILENAME = f'math{os.sep}src{os.sep}exp.cc'

EXPECTED_MUTATION_1 = f'Remove statement at {FILENAME}, 83:3--94:4\n'\
    '\n'\
    'This is the removed statement:\n'\
    '\n'\
    '  if (y != 1) {\n'\
    '    for (int i = 0; i < 100; ++i) {\n'\
    '      int m = 0;\n'\
    '      while (!(2 <= z && z < 4)) {\n'\
    '        z *= z;\n'\
    '        ++m;\n'\
    '      }\n'\
    '      constant *= Pow(2, -m);\n'\
    '      result += constant;\n'\
    '      z /= 2;\n'\
    '    }\n'\
    '  }\n'

EXPECTED_MUTATION_2 = f'Remove statement at {FILENAME}, 84:5--93:6\n'\
    '\n'\
    'This is the removed statement:\n'\
    '\n'\
    '    for (int i = 0; i < 100; ++i) {\n'\
    '      int m = 0;\n'\
    '      while (!(2 <= z && z < 4)) {\n'\
    '        z *= z;\n'\
    '        ++m;\n'\
    '      }\n'\
    '      constant *= Pow(2, -m);\n'\
    '      result += constant;\n'\
    '      z /= 2;\n'\
    '    }\n'

EXPECTED_MUTATION_3 = f'Replace binary operator expression at {FILENAME}, 92:7--92:13\n'\
    '\n'\
    'Original binary operator expression:\n'\
    '\n'\
    'z /= 2\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    'z -= 2\n'

EXPECTED_MUTATION_4 = f'Replace expression at {FILENAME}, 92:12--92:13\n'\
    '\n'\
    'Original expression:\n'\
    '\n'\
    '2\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    '0.0\n'

EXPECTED_MUTATION_5 = f'Replace expression at {FILENAME}, 81:19--81:40\n'\
    '\n'\
    'Original expression:\n'\
    '\n'\
    'Pow(2, -logValue) * x\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    '- (Pow(2, -logValue) * x)\n'

EXPECTED_MUTATION_6 = f'Replace expression at {FILENAME}, 59:10--59:41\n'\
    '\n'\
    'Original expression:\n'\
    '\n'\
    '(exp < 0) ? 1 / result : result\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    '1.0\n'


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
    run_successfully([DREDD_EXECUTABLE,
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

    all_mutant_info = '\n'.join(all_mutant_info.splitlines())

    for expected_mutation in [EXPECTED_MUTATION_1,
                              EXPECTED_MUTATION_2,
                              EXPECTED_MUTATION_3,
                              EXPECTED_MUTATION_4,
                              EXPECTED_MUTATION_5,
                              EXPECTED_MUTATION_6,
                              ]:
        if expected_mutation not in all_mutant_info:
            print(f"Did not find expected mutation info:\n{expected_mutation}")
            sys.exit(1)


if __name__ == '__main__':
    sys.exit(main())
