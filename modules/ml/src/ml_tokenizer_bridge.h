/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_tokenizer_bridge.h

    Lazy dlsym bridge to the tokenizer module's C API

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_ML_TOKENIZER_BRIDGE_H
#define _QORE_MODULE_ML_ML_TOKENIZER_BRIDGE_H

#include "qore/Qore.h"

#include <cstdint>

//! Check if the tokenizer C API is available (module loaded)
DLLLOCAL bool ml_tokenizer_available();

//! Create a tokenizer from a parsed config hash
/** @return opaque handle, or nullptr on error (error_buf filled)
*/
DLLLOCAL void* ml_tokenizer_create(const QoreHashNode* config,
    char* error_buf, size_t error_buf_len);

//! Count tokens in text
/** @return token count, or -1 on error
*/
DLLLOCAL int32_t ml_tokenizer_count_tokens(void* handle,
    const char* text, size_t text_len);

//! Tokenize text to token strings
/** @return array of strings (caller frees with ml_tokenizer_free_tokens)
*/
DLLLOCAL char** ml_tokenizer_tokenize(void* handle,
    const char* text, size_t text_len, int32_t* count_out);

//! Free token array
DLLLOCAL void ml_tokenizer_free_tokens(char** tokens, int32_t count);

//! Destroy tokenizer handle
DLLLOCAL void ml_tokenizer_destroy(void* handle);

#endif
