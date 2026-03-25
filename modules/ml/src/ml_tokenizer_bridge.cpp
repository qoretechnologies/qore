/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_tokenizer_bridge.cpp

    Lazy dlsym bridge to the tokenizer module's C API

    Since Qore loads binary modules with RTLD_GLOBAL, the tokenizer
    module's extern "C" symbols are in the global symbol table when
    it's loaded. We use dlsym(RTLD_DEFAULT, ...) to find them lazily
    on first use.

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "ml_tokenizer_bridge.h"

#include <atomic>
#include <dlfcn.h>

// Function pointer types matching the tokenizer C API
typedef void* (*create_fn)(const QoreHashNode*, char*, size_t);
typedef int32_t (*count_fn)(void*, const char*, size_t);
typedef char** (*tokenize_fn)(void*, const char*, size_t, int32_t*);
typedef void (*free_tokens_fn)(char**, int32_t);
typedef void (*destroy_fn)(void*);

// Cached function pointers (resolved lazily)
static std::atomic<bool> resolved{false};
static std::atomic<bool> available{false};
static create_fn fn_create = nullptr;
static count_fn fn_count = nullptr;
static tokenize_fn fn_tokenize = nullptr;
static free_tokens_fn fn_free_tokens = nullptr;
static destroy_fn fn_destroy = nullptr;

static void resolve() {
    if (resolved.load(std::memory_order_acquire)) {
        return;
    }

    fn_create = (create_fn)dlsym(RTLD_DEFAULT, "qore_tokenizer_create");
    fn_count = (count_fn)dlsym(RTLD_DEFAULT, "qore_tokenizer_count_tokens");
    fn_tokenize = (tokenize_fn)dlsym(RTLD_DEFAULT, "qore_tokenizer_tokenize");
    fn_free_tokens = (free_tokens_fn)dlsym(RTLD_DEFAULT, "qore_tokenizer_free_tokens");
    fn_destroy = (destroy_fn)dlsym(RTLD_DEFAULT, "qore_tokenizer_destroy");

    available.store(fn_create && fn_count && fn_tokenize && fn_free_tokens && fn_destroy,
        std::memory_order_relaxed);
    resolved.store(true, std::memory_order_release);
}

bool ml_tokenizer_available() {
    resolve();
    return available.load(std::memory_order_relaxed);
}

void* ml_tokenizer_create(const QoreHashNode* config,
    char* error_buf, size_t error_buf_len) {
    resolve();
    if (!fn_create) {
        if (error_buf && error_buf_len > 0) {
            snprintf(error_buf, error_buf_len,
                "tokenizer module not loaded; load it with %%requires tokenizer");
        }
        return nullptr;
    }
    return fn_create(config, error_buf, error_buf_len);
}

int32_t ml_tokenizer_count_tokens(void* handle, const char* text, size_t text_len) {
    if (!fn_count) {
        return -1;
    }
    return fn_count(handle, text, text_len);
}

char** ml_tokenizer_tokenize(void* handle, const char* text, size_t text_len,
    int32_t* count_out) {
    if (!fn_tokenize) {
        if (count_out) { *count_out = 0; }
        return nullptr;
    }
    return fn_tokenize(handle, text, text_len, count_out);
}

void ml_tokenizer_free_tokens(char** tokens, int32_t count) {
    if (fn_free_tokens) {
        fn_free_tokens(tokens, count);
    }
}

void ml_tokenizer_destroy(void* handle) {
    if (fn_destroy) {
        fn_destroy(handle);
    }
}
