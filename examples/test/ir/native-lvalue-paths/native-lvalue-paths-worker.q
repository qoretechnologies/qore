# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the results of LvChecks::run() three times, so that tiered execution runs native code
%modern
%requires LvChecks
for (int i = 0; i < 3; ++i) {
    print(LvChecks::run().join("#") + "\n");
}
