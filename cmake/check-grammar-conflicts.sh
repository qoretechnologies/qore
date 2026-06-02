#!/bin/sh
#
# check-grammar-conflicts.sh — enforce the parser grammar conflict budget.
#
# Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.
#
# The Qore grammar (lib/parser.ypp) must stay within a strict conflict budget so that ambiguity cannot
# silently creep back in and mis-parse valid source.  A regression here previously caused
# qore-extract-qm-metadata to fail with "PARSE-EXCEPTION: syntax error, unexpected quoted string": a
# feature merge turned a conflict-free grammar into one with 70 shift/reduce + 83 reduce/reduce conflicts
# (including conflicts on the quoted-string token), and bison's default conflict resolution then rejected
# valid input in some build/input combinations.
#
# Budget (see the comment near %error-verbose in lib/parser.ypp):
#   shift/reduce  : 0  (must always be zero)
#   reduce/reduce : 1  (one inherent, documented, correctly-resolved generic-call-vs-method-def case)
#
# Usage: check-grammar-conflicts.sh /path/to/parser.ypp
# Exits non-zero (failing the build) if the actual counts exceed the budget.

set -eu

YPP="${1:-lib/parser.ypp}"
MAX_SR=0
MAX_RR=1

if ! command -v bison >/dev/null 2>&1; then
    echo "check-grammar-conflicts: bison not found; skipping conflict check" >&2
    exit 0
fi

# bison writes the conflict summary to stderr; -o /dev/null discards the generated parser.
ERR="$(bison -o /dev/null "$YPP" 2>&1 || true)"

# Extract counts (lines like "N shift/reduce conflicts" / "N reduce/reduce conflict[s]").
# Absent line == 0 conflicts of that kind.
sr="$(printf '%s\n' "$ERR" | sed -n 's/.*[^0-9]\([0-9][0-9]*\) shift\/reduce conflict.*/\1/p' | head -n1)"
rr="$(printf '%s\n' "$ERR" | sed -n 's/.*[^0-9]\([0-9][0-9]*\) reduce\/reduce conflict.*/\1/p' | head -n1)"
sr="${sr:-0}"
rr="${rr:-0}"

echo "check-grammar-conflicts: $YPP -> shift/reduce=$sr (max $MAX_SR), reduce/reduce=$rr (max $MAX_RR)"

status=0
if [ "$sr" -gt "$MAX_SR" ]; then
    echo "ERROR: $sr shift/reduce conflicts exceed budget of $MAX_SR" >&2
    status=1
fi
if [ "$rr" -gt "$MAX_RR" ]; then
    echo "ERROR: $rr reduce/reduce conflicts exceed budget of $MAX_RR" >&2
    status=1
fi

if [ "$status" -ne 0 ]; then
    echo "Grammar conflict budget exceeded — see the conflict comment in lib/parser.ypp and rerun" >&2
    echo "  bison -Wcounterexamples -o /dev/null $YPP" >&2
    echo "to see the offending derivations." >&2
fi

exit "$status"
