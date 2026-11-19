#!/usr/bin/env bash
#
# Generates a clangd-friendly compile_commands.json at the project root.
#
# On NixOS clangd runs its own clang and, without --query-driver, cannot see
# the gcc/libstdc++ include dirs that the build actually uses, so it reports
# errors like "include file 'string' not found". This script takes the real
# compile database from the cmake build and augments every C++ entry with the
# compiler's system include dirs (-isystem), which clangd then honors.

set -euo pipefail
cd "$(dirname "$0")/.."

DB=build/compile_commands.json
OUT=compile_commands.json

# Use a generator like cmake (already configured) but re-export just in case.
if [[ ! -f "$DB" ]]; then
  echo "error: $DB not found; configure with cmake first" >&2
  exit 1
fi

# Capture the include search path of the compiler used for the build.
CXX=${CXX:-g++}
INC_DIRS=$("$CXX" -E -x c++ -v /dev/null 2>&1 \
  | sed -n '/^#include <...> search starts here:/,/End of search list\./p' \
  | sed '1d;$d' | sed 's/^[[:space:]]*//')
if [[ -z "$INC_DIRS" ]]; then
  echo "error: could not extract include dirs from '$CXX'" >&2
  exit 1
fi

python3 - "$DB" "$OUT" "$INC_DIRS" <<'PY'
import json
import sys

db_path, out_path, inc_dirs = sys.argv[1], sys.argv[2], sys.argv[3]

def is_cpp(f):
    return f.endswith((".cpp", ".cc", ".cxx", ".hpp", ".hxx"))

entries = json.load(open(db_path, "r"))
flags = " ".join("-isystem " + d for d in inc_dirs.splitlines())

for e in entries:
    file = e.get("file", "")
    if is_cpp(file) and e.get("command"):
        # Insert system include flags right before the '-o <object>' part.
        cmd = e["command"]
        idx = cmd.find(" -o ")
        e["command"] = cmd[:idx] + " " + flags + cmd[idx:]

json.dump(entries, open(out_path, "w"), indent=2)
print(f"wrote {out_path} with system includes for {len(entries)} entries")
PY
