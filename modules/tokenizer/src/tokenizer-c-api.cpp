/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    tokenizer-c-api.cpp

    C-linkage API for cross-module tokenizer access via dlsym()

    Qore loads all binary modules with RTLD_GLOBAL, so these symbols are
    automatically visible to all subsequently loaded modules. Consumers
    call dlsym(RTLD_DEFAULT, "qore_tokenizer_count_tokens") etc.

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "HFTokenizer.h"

#include <cstring>

using namespace QoreTokenizer;

extern "C" {

//! Create a tokenizer from a parsed Qore hash (tokenizer.json config)
/** @param config parsed tokenizer.json as a QoreHashNode*
    @param error_buf buffer for error message (filled on failure)
    @param error_buf_len size of error buffer
    @return opaque tokenizer handle, or nullptr on error
*/
DLLEXPORT void* qore_tokenizer_create(const QoreHashNode* config,
    char* error_buf, size_t error_buf_len) {
    if (!config) {
        if (error_buf && error_buf_len > 0) {
            snprintf(error_buf, error_buf_len, "config hash is null");
        }
        return nullptr;
    }

    ExceptionSink xsink;
    auto* tokenizer = new QoreHFTokenizer(config, &xsink);
    if (xsink) {
        if (error_buf && error_buf_len > 0) {
            const QoreValue desc = xsink.getExceptionDesc();
            if (desc.getType() == NT_STRING) {
                snprintf(error_buf, error_buf_len, "%s",
                    desc.get<const QoreStringNode>()->c_str());
            } else {
                snprintf(error_buf, error_buf_len, "tokenizer initialization failed");
            }
        }
        delete tokenizer;
        return nullptr;
    }

    return static_cast<void*>(tokenizer);
}

//! Count tokens in text
/** @param handle tokenizer handle from qore_tokenizer_create()
    @param text UTF-8 text to tokenize
    @param text_len length of text in bytes
    @return number of tokens, or -1 on error
*/
DLLEXPORT int32_t qore_tokenizer_count_tokens(void* handle,
    const char* text, size_t text_len) {
    if (!handle || !text) {
        return -1;
    }

    auto* tokenizer = static_cast<QoreHFTokenizer*>(handle);
    ExceptionSink xsink;

    SimpleRefHolder<QoreStringNode> qtext(new QoreStringNode(text, text_len, QCS_UTF8));
    ReferenceHolder<QoreHashNode> result(tokenizer->encode(qtext.release(),
        nullptr, true, &xsink), &xsink);

    if (xsink || !result) {
        return -1;
    }

    QoreValue ids_val = result->getKeyValue("input_ids");
    if (ids_val.getType() != NT_LIST) {
        return -1;
    }

    return (int32_t)ids_val.get<const QoreListNode>()->size();
}

//! Tokenize text to token strings
/** @param handle tokenizer handle
    @param text UTF-8 text
    @param text_len length in bytes
    @param count_out receives the number of tokens
    @return array of token strings (caller frees with qore_tokenizer_free_tokens), or nullptr on error
*/
DLLEXPORT char** qore_tokenizer_tokenize(void* handle,
    const char* text, size_t text_len, int32_t* count_out) {
    if (!handle || !text || !count_out) {
        if (count_out) { *count_out = 0; }
        return nullptr;
    }

    auto* tokenizer = static_cast<QoreHFTokenizer*>(handle);
    ExceptionSink xsink;

    SimpleRefHolder<QoreStringNode> qtext(new QoreStringNode(text, text_len, QCS_UTF8));
    ReferenceHolder<QoreHashNode> result(tokenizer->encode(qtext.release(),
        nullptr, true, &xsink), &xsink);

    if (xsink || !result) {
        *count_out = 0;
        return nullptr;
    }

    QoreValue tokens_val = result->getKeyValue("tokens");
    if (tokens_val.getType() != NT_LIST) {
        *count_out = 0;
        return nullptr;
    }

    const QoreListNode* tokens = tokens_val.get<const QoreListNode>();
    int32_t n = (int32_t)tokens->size();

    char** token_array = (char**)malloc(sizeof(char*) * n);
    if (!token_array) {
        *count_out = 0;
        return nullptr;
    }

    for (int32_t i = 0; i < n; ++i) {
        QoreValue tv = tokens->retrieveEntry(i);
        if (tv.getType() == NT_STRING) {
            const QoreStringNode* s = tv.get<const QoreStringNode>();
            token_array[i] = strdup(s->c_str());
        } else {
            token_array[i] = strdup("");
        }
    }

    *count_out = n;
    return token_array;
}

//! Free token string array from qore_tokenizer_tokenize()
DLLEXPORT void qore_tokenizer_free_tokens(char** tokens, int32_t count) {
    if (!tokens) {
        return;
    }
    for (int32_t i = 0; i < count; ++i) {
        free(tokens[i]);
    }
    free(tokens);
}

//! Destroy tokenizer handle
DLLEXPORT void qore_tokenizer_destroy(void* handle) {
    if (handle) {
        auto* tokenizer = static_cast<QoreHFTokenizer*>(handle);
        ExceptionSink xsink;
        tokenizer->deref(&xsink);
    }
}

} // extern "C"
