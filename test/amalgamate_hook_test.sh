#!/bin/sh
set -eu
source_root=$1
scratch=$(mktemp -d "${TMPDIR:-/tmp}/wasm-hook-test.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
mkdir -p "$scratch/src" "$scratch/tools"
cp "$source_root/tools/amalgamate.c" "$scratch/tools/"
printf 'int original;\n' > "$scratch/src/amalgamation.h"
cd "$scratch"
git init -q
${CC:-cc} -o amalgamate tools/amalgamate.c
./amalgamate
git add src tools wasm.h
"$source_root/.githooks/pre-commit"
# Unstaged source changes must not affect validation of a matching index.
printf 'int unstaged;\n' >> src/amalgamation.h
"$source_root/.githooks/pre-commit"
# A staged source change with stale staged output must fail, even if the
# working tree header has already been regenerated.
git add src/amalgamation.h
./amalgamate
before=$(git diff --cached --binary | git hash-object --stdin)
if "$source_root/.githooks/pre-commit"; then
    echo >&2 'Hook accepted a stale staged header'
    exit 1
fi
after=$(git diff --cached --binary | git hash-object --stdin)
test "$before" = "$after"
git add wasm.h
"$source_root/.githooks/pre-commit"
