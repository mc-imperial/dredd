import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_EXECUTABLE = Path(DREDD_REPO_ROOT, 'temp', 'build-Debug', 'src', 'dredd', 'dredd') if 'DREDD_EXECUTABLE' not in os.environ else os.environ['DREDD_EXECUTABLE']


def main():
    # Run Dredd
    cmd = [DREDD_EXECUTABLE, 'bad.c', 'good.c', '--']
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode != 0
    error_output = result.stderr.decode('utf-8')
    pattern = r"Error while processing .*good\.c"
    assert re.search(pattern, error_output) is None


if __name__ == '__main__':
    sys.exit(main())
