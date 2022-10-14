import argparse
import re

from pathlib import Path
from typing import List


def prepare(original_program: Path, prepared_program: Path, csmith_root: Path) -> None:
    include_files_to_follow: List[str] = ["csmith", "csmith_minimal", "random_inc", "platform_avr", "platform_generic", "platform_msp430"]
    content: str = open(original_program, 'r').read()
    for include_file in include_files_to_follow:
        pattern: str = f'(.*)(#include "{include_file}\\.h")(.*)'
        match = re.search(pattern, content, re.DOTALL)
        assert match is not None
        assert len(match.groups()) == 3
        content = match.group(1) + open(csmith_root / "runtime" / (include_file + ".h"), 'r').read() + match.group(3)
    with open(prepared_program, 'w') as outfile:
        outfile.write(content)


# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("original_program", help="Program to be prepared.", type=Path)
    parser.add_argument("prepared_program", help="File that prepared program will be written to.", type=Path)
    parser.add_argument("csmith_root", help="Path to a checkout of Csmith, assuming that it has been built under 'build' beneath this directory.", type=Path)
    args = parser.parse_args()
    prepare(original_program=args.original_program,
            prepared_program=args.prepared_program,
            csmith_root=args.csmith_root)

