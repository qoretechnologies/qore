// Copyright 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: MIT
#include <assert.h>
#include <llhttp.h>
#include <string.h>

struct result {
    char body[64];
    size_t length;
    int completed;
};

static int body_cb(llhttp_t* parser, const char* data, size_t length) {
    struct result* result = parser->data;
    assert(length <= sizeof(result->body) - result->length);
    memcpy(result->body + result->length, data, length);
    result->length += length;
    return 0;
}

static int complete_cb(llhttp_t* parser) {
    struct result* result = parser->data;
    ++result->completed;
    return 0;
}

static void parse(llhttp_type_t type, const char* message, const char* expected, size_t chunk) {
    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_body = body_cb;
    settings.on_message_complete = complete_cb;
    llhttp_t parser;
    llhttp_init(&parser, type, &settings);
    struct result result = {{0}, 0, 0};
    parser.data = &result;
    size_t remaining = strlen(message);
    while (remaining) {
        size_t size = remaining < chunk ? remaining : chunk;
        assert(llhttp_execute(&parser, message, size) == HPE_OK);
        message += size;
        remaining -= size;
    }
    assert(llhttp_finish(&parser) == HPE_OK);
    assert(result.completed == 1);
    assert(result.length == strlen(expected));
    assert(memcmp(result.body, expected, result.length) == 0);
}

int main(void) {
    const char request[] = "POST /api HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nbody";
    const char response[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
    parse(HTTP_REQUEST, request, "body", sizeof(request));
    parse(HTTP_REQUEST, request, "body", 1);
    parse(HTTP_RESPONSE, response, "Wikipedia", sizeof(response));
    parse(HTTP_RESPONSE, response, "Wikipedia", 1);
    llhttp_t parser;
    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    llhttp_init(&parser, HTTP_REQUEST, &settings);
    const char invalid[] = "POST / HTTP/1.1\r\nContent-Length: invalid\r\n\r\n";
    assert(llhttp_execute(&parser, invalid, strlen(invalid)) == HPE_INVALID_CONTENT_LENGTH);
    return 0;
}
