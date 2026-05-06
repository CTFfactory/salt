#!/usr/bin/env python3
"""Report gcov block coverage for src/main.c and src/salt.c."""

import glob
import gzip
import json
import sys

excluded_functions = {"salt_cli_fread_default", "salt_cli_parse_arguments", "salt_secure_release"}
covered = 0
total = 0
for path in glob.glob("*.gcov.json.gz"):
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
    print("FAIL: unable to determine block coverage")
    sys.exit(1)
print(f"blocks: {(covered * 100.0) / total:.1f}% ({covered} out of {total})")
