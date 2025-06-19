import filecmp
import os
import shutil
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_CLANG_LLVM_DIR = os.environ['DREDD_CLANG_LLVM_DIR']
DREDD_EXECUTABLE = Path(DREDD_REPO_ROOT, 'temp', 'build-Debug', 'src', 'dredd', 'dredd') if 'DREDD_EXECUTABLE' not in os.environ else os.environ['DREDD_EXECUTABLE']
QUERY_MUTANT_INFO_SCRIPT = Path(DREDD_REPO_ROOT, 'scripts', 'query_mutant_info.py')
CLANG_EXECUTABLE = Path(DREDD_CLANG_LLVM_DIR, 'bin', 'clang')
COMPILED_EXECUTABLE_FILENAME = 'a.exe' if os.name == 'nt' else './a.out'


def run_successfully(cmd):
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Command exited with error code {result.returncode}: {' '.join(cmd)}")
        print(result.stdout.decode('utf-8'))
        print(result.stderr.decode('utf-8'))
        sys.exit(1)
    return result


def main():
    # Run Dredd without tracking
    shutil.copyfile(src='example.c', dst='tomutate.c')
    run_successfully([DREDD_EXECUTABLE, '--mutation-info-file', 'info-mutate.json', 'tomutate.c', '--'])

    # Run Dredd with tracking
    shutil.copyfile(src='example.c', dst='tomutate.c')
    run_successfully([DREDD_EXECUTABLE,
                      '--mutation-info-file',
                      'info-track.json',
                      '--only-track-mutant-coverage',
                      'tomutate.c',
                      '--'])

    # Check that the JSON files produced with vs. without tracking are identical, because tracking should not affect the
    # JSON
    assert filecmp.cmp('info-mutate.json', 'info-track.json')

    # The mutant tracking version should compile successfully.
    run_successfully([CLANG_EXECUTABLE, 'tomutate.c'])

    # When executed with no command line arguments the program should return 0.
    dredd_env = os.environ.copy()
    dredd_env['DREDD_MUTANT_TRACKING_FILE'] = "1.mutants"
    result = subprocess.run([COMPILED_EXECUTABLE_FILENAME], env=dredd_env)
    assert result.returncode == 0

    # When executed with 3 command line arguments the program should return 40.
    dredd_env['DREDD_MUTANT_TRACKING_FILE'] = "2.mutants"
    result = subprocess.run([COMPILED_EXECUTABLE_FILENAME, '1', '2', '3'], env=dredd_env)
    assert result.returncode == 40

    # Get the largest mutant id
    largest_mutant_id = int(run_successfully(
        ["python", QUERY_MUTANT_INFO_SCRIPT, "--largest-mutant-id", "info-track.json"]).stdout.decode('utf-8'))

    # Get the mutants covered by each of the two tests, and the union and differences of these.
    covered_1 = set([int(line.strip()) for line in open("1.mutants", "r").readlines()])
    covered_2 = set([int(line.strip()) for line in open("2.mutants", "r").readlines()])
    all_covered = covered_1.union(covered_2)
    just_1 = covered_1.difference(covered_2)
    just_2 = covered_2.difference(covered_1)

    # Collectively, the tests should cover all of the available mutants.
    assert all_covered == set(range(0, largest_mutant_id + 1))
    # Each test should cover at least some distinct mutants.
    assert len(just_1) > 0
    assert len(just_2) > 0
    # Due to the way in which Dredd assigns mutant IDs and the way the test is constructed, the mutants unique to the
    # first test should have smaller IDs compared with the mutants unique to the second set.
    assert max(just_1) < min(just_2)


if __name__ == '__main__':
    sys.exit(main())
