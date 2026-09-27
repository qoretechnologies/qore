/* Copyright 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 * Verify that the public C headers and the linked runtime agree about TSLogger,
 * and exercise allocation/reset/destruction through the installed library.
 */
#include <tree_sitter/api.h>
#include <stdio.h>

static void log_message(void *payload, TSLogType type, const char *message) {
    (void)payload;
    (void)type;
    (void)message;
}

int main(void) {
    int payload = 42;
    TSParser *parser = ts_parser_new();
    if (!parser) {
        return 1;
    }
    TSLogger logger = {&payload, log_message};
    ts_parser_set_logger(parser, logger);
    TSLogger result = ts_parser_logger(parser);
    int failed = result.payload != &payload || result.log != log_message;
    ts_parser_reset(parser);
    ts_parser_delete(parser);
    if (failed) {
        fputs("tree-sitter public C ABI smoke test failed\n", stderr);
    }
    return failed;
}
