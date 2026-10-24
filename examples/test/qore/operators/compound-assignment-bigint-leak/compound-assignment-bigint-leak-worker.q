# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# applies each integer compound assignment operator to containers holding integers outside the inline range and
# prints the results and how many heap integers it left behind (dbg_get_live_bigint_count()), which
# compound-assignment-bigint-leak.qtest checks
%modern

class C {
    public {
        auto x = 1 << 60;
    }
}

list<int> sub apply(int rounds) {
    hash<auto> h = {"x": 1 << 60};
    list<auto> l = (1 << 60,);
    C c();
    for (int i = 0; i < rounds; ++i) {
        h.x += 1; h.x -= 1; h.x *= 1; h.x /= 1; h.x %= (1 << 62); h.x |= 0; h.x &= -1; h.x ^= 0; h.x <<= 0;
        h.x >>= 0; ++h.x; --h.x; h.x++; h.x--;
        l[0] += 1; l[0] -= 1; l[0] *= 1; l[0] /= 1; l[0] %= (1 << 62); l[0] |= 0; l[0] &= -1; l[0] ^= 0;
        l[0] <<= 0; l[0] >>= 0; ++l[0]; --l[0]; l[0]++; l[0]--;
        c.x += 1; c.x -= 1; c.x *= 1; c.x /= 1; c.x %= (1 << 62); c.x |= 0; c.x &= -1; c.x ^= 0; c.x <<= 0;
        c.x >>= 0; ++c.x; --c.x; c.x++; c.x--;
    }
    return (h.x, l[0], c.x);
}

*int live_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigint_count") : NOTHING;
list<int> result;
for (int i = 0; i < 3; ++i) {
    result = apply(10);
}
printf("%y\n", result);
if (exists live_before) {
    printf("live heap integers: %d\n", call_function("dbg_get_live_bigint_count") - live_before - 3);
}
