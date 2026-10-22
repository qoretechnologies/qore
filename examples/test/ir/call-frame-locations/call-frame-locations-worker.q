# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the call stack frames of exceptions three times, so that tiered execution runs native code
%modern
%requires LocChecks
for (int i = 0; i < 3; ++i) {
    print(LocChecks::run() + "\n");
}
