// Copyright 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: MIT
#include <ada_c.h>
#include <assert.h>
#include <string.h>

static void equal(ada_string actual, const char* expected) {
    assert(actual.length == strlen(expected));
    assert(memcmp(actual.data, expected, actual.length) == 0);
}

int main(void) {
    const char input[] = "HTTPS://B\xc3\xbc" "cher.example:443/a/../b?q=one#two";
    ada_url url = ada_parse(input, strlen(input));
    assert(ada_is_valid(url));
    equal(ada_get_href(url), "https://xn--bcher-kva.example/b?q=one#two");
    equal(ada_get_hostname(url), "xn--bcher-kva.example");
    equal(ada_get_port(url), "");
    ada_owned_string origin = ada_get_origin(url);
    equal((ada_string){origin.data, origin.length}, "https://xn--bcher-kva.example");
    ada_free_owned_string(origin);
    ada_url copy = ada_copy(url);
    ada_free(url);
    ada_clear_hash(copy);
    equal(ada_get_href(copy), "https://xn--bcher-kva.example/b?q=one");
    ada_free(copy);

    const char base[] = "https://example.org/a/b/";
    const char relative[] = "../c";
    url = ada_parse_with_base(relative, strlen(relative), base, strlen(base));
    assert(ada_is_valid(url));
    equal(ada_get_href(url), "https://example.org/a/c");
    ada_free(url);
    const char invalid[] = "http://[broken";
    assert(!ada_can_parse(invalid, strlen(invalid)));
    url = ada_parse(invalid, strlen(invalid));
    assert(!ada_is_valid(url));
    ada_free(url);
    return 0;
}
