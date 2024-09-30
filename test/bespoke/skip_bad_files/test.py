import json
import os
import re
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_INSTALLED_EXECUTABLE = Path(DREDD_REPO_ROOT, 'third_party', 'clang+llvm', 'bin', 'dredd')


def run_successfully(cmd):
    return result


def main():
    cmd = [DREDD_INSTALLED_EXECUTABLE,
           '--mutation-info-file',
           'info.json',
           'good1.c',
           'bad1.c',
           'good2.c',
           'bad2.c',
           '--']
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    # Dredd should return non-zero, due to 'bad1.c' and 'bad2.c'
    assert result.returncode != 0
    error_output = result.stderr.decode('utf-8')
    pattern = "The following files were not mutated due to compile-time errors; see above for details:\s+[^\s]+bad1\.c\s+[^\s]+bad2\.c"
    match = re.search(pattern, error_output)
    assert match is not None
    dictionary = json.load(open('info.json'))
    info_for_files = dictionary['infoForFiles']
    assert len(info_for_files) == 2
    assert info_for_files[0]['filename'].endswith('good1.c')
    assert info_for_files[1]['filename'].endswith('good2.c')


if __name__ == '__main__':
    sys.exit(main())
