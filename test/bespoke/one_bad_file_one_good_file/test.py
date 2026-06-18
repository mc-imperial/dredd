import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_INSTALLED_EXECUTABLE = Path(DREDD_REPO_ROOT, 'third_party', 'clang+llvm', 'bin', 'dredd')


def main():
    # Run Dredd
    cmd = [DREDD_INSTALLED_EXECUTABLE, 'bad.c', 'good.c', '--']
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode != 0
    error_output = result.stderr.decode('utf-8')
    pattern = r"Error while processing .*good\.c"
    assert re.search(pattern, error_output) is None


if __name__ == '__main__':
    sys.exit(main())
