#!/bin/sh
set -eu
source_root=$1
scratch=$(mktemp -d "${TMPDIR:-/tmp}/wasm-hook-test.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
mkdir -p "$scratch/src" "$scratch/cmake"
cp "$source_root/cmake/Amalgamate.cmake" "$scratch/cmake/"
printf 'int original;\n' > "$scratch/src/amalgamation.h"
cd "$scratch"
git init -q
cmake -P cmake/Amalgamate.cmake
git add src cmake wasm.h
"$source_root/.githooks/pre-commit"
# Unstaged source changes must not affect validation of a matching index.
printf 'int unstaged;\n' >> src/amalgamation.h
"$source_root/.githooks/pre-commit"
# A staged source change with stale staged output must fail, even if the
# working tree header has already been regenerated.
git add src/amalgamation.h
cmake -P cmake/Amalgamate.cmake
before=$(git diff --cached --binary | git hash-object --stdin)
if "$source_root/.githooks/pre-commit"; then
    echo >&2 'Hook accepted a stale staged header'
    exit 1
fi
after=$(git diff --cached --binary | git hash-object --stdin)
test "$before" = "$after"
git add wasm.h
"$source_root/.githooks/pre-commit"
