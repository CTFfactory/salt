#!/usr/bin/env python3
"""Report gcov block coverage for src/main.c and src/salt.c."""

import glob
import gzip
import json
import sys

# These functions are excluded because they are either thin wrappers that
# delegate immediately to tested paths, or are only reachable in non-test
# configurations (e.g. the real fread/argument-parser used in production main).
excluded_functions = {"salt_cli_fread_default", "salt_cli_parse_arguments", "salt_secure_release"}
covered = 0
total = 0
gcov_files = glob.glob("*.gcov.json.gz")
for path in gcov_files:
    with gzip.open(path, "rt", encoding="utf-8") as handle:
        data = json.load(handle)
    for file_entry in data.get("files", []):
        file_path = file_entry.get("file", "")
        if not (file_path.endswith("src/main.c") or file_path.endswith("src/salt.c")):
            continue
        for function in file_entry.get("functions", []):
            if function.get("name") in excluded_functions:
                continue
            total += int(function.get("blocks", 0))
            covered += int(function.get("blocks_executed", 0))
if total <= 0:
    if not gcov_files:
        print("FAIL: unable to determine block coverage (no *.gcov.json.gz files found)")
    else:
        print("FAIL: unable to determine block coverage (no blocks found in src/main.c or src/salt.c)")
    sys.exit(1)
print(f"blocks: {(covered * 100.0) / total:.1f}% ({covered} out of {total})")
