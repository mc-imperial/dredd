import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_EXECUTABLE = Path(DREDD_REPO_ROOT, 'temp', 'build-Debug', 'src', 'dredd', 'dredd') if 'DREDD_EXECUTABLE' not in os.environ else os.environ['DREDD_EXECUTABLE']
QUERY_MUTANT_INFO_SCRIPT = Path(DREDD_REPO_ROOT, 'scripts', 'query_mutant_info.py')

EXPECTED_MUTATION_1 = f'Replace binary operator expression at example.c, 2:10--2:16\n'\
    '\n'\
    'Original binary operator expression:\n'\
    '\n'\
    'a && b\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    'a || b\n'

EXPECTED_MUTATION_2 = f'Replace binary operator expression at example.c, 2:10--2:16\n'\
    '\n'\
    'Original binary operator expression:\n'\
    '\n'\
    'a && b\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    'a\n'

EXPECTED_MUTATION_3 = f'Replace binary operator expression at example.c, 2:10--2:16\n'\
    '\n'\
    'Original binary operator expression:\n'\
    '\n'\
    'a && b\n'\
    '\n'\
    'Replacement expression:\n'\
    '\n'\
    'b\n'

def run_successfully(cmd):
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Command exited with error code {result.returncode}: {' '.join([str(c) for c in cmd])}")
        print(result.stdout.decode('utf-8'))
        print(result.stderr.decode('utf-8'))
        sys.exit(1)
    return result


def main():
    os.makedirs("original")
    os.makedirs("mutated")
    shutil.copyfile(src='example.c', dst='original' + os.sep + 'example.c')
    shutil.copyfile(src='example.c', dst='mutated' + os.sep + 'example.c')
    
    run_successfully([DREDD_EXECUTABLE, '--mutation-info-file', 'mutant-info.json', 'mutated' + os.sep + 'example.c', '--'])

    # Get the largest mutant id
    largest_mutant_id = int(run_successfully(
        ["python", QUERY_MUTANT_INFO_SCRIPT, "--largest-mutant-id", "mutant-info.json"]).stdout.decode('utf-8'))

    all_mutant_info = ""
    for mutant in range(0, largest_mutant_id + 1):
        cmd = ["python",
             QUERY_MUTANT_INFO_SCRIPT,
             "--show-info-for-mutant",
             str(mutant),
             "--path-prefix-replacement",
             os.path.abspath(os.curdir + os.sep + "mutated"),
             os.path.abspath(os.curdir + os.sep + "original"),
             "mutant-info.json"]
        all_mutant_info += str(run_successfully(cmd).stdout.decode('utf-8'))

    all_mutant_info = '\n'.join(all_mutant_info.splitlines())

    for expected_mutation in [EXPECTED_MUTATION_1,
                              EXPECTED_MUTATION_2,
                              EXPECTED_MUTATION_3,
                              ]:
        if expected_mutation not in all_mutant_info:
            print(f"Did not find expected mutation info:\n{expected_mutation}")
            sys.exit(1)


if __name__ == '__main__':
    sys.exit(main())
