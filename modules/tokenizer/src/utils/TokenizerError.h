/* Copyright (C) 2026 Qore Technologies, s.r.o. — MIT License */
#ifndef _QORE_TOKENIZER_ERROR_H
#define _QORE_TOKENIZER_ERROR_H

#include <stdexcept>
#include <string>

namespace QoreTokenizer {

//! Component error translated into a Qore exception at an encode or decode entry point
class TokenizerError : public std::runtime_error {
public:
    const char* const err;

    TokenizerError(const char* err, const std::string& desc) : std::runtime_error(desc), err(err) {
    }
};

} // namespace QoreTokenizer

#endif
