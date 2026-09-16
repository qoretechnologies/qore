/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Brace-delimited regex and keyword tokens follow lib/scanner.lpp.
 */
#include "tree_sitter/parser.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum TokenType {
    BRACE_REGEX_MATCH,
    BRACE_REGEX_SUBST,
    BRACE_REGEX_TRANS,
    BRACE_REGEX_EXTRACT,
    KEYWORD_IDENTIFIER,
    CLASS_KEYWORD,
    MODULE_KEYWORD,
    // matched by the generated lexer
    NEWLINE,
    TOKEN_COUNT,
};

typedef enum {
    // KW_IDENTIFIER_OPENPAREN when "(" follows immediately
    KEYWORD_CALL,
    // KW_IDENTIFIER_OPENPAREN when {WS}* and "(" follow
    KEYWORD_SPACED_CALL,
    // a class declaration when {WS}+ and {WORD} or (::{WORD})+ follow; otherwise IDENTIFIER
    KEYWORD_CLASS,
    // a module declaration when {WS}+ and {WORD} follow; otherwise IDENTIFIER
    KEYWORD_MODULE,
} KeywordKind;

typedef struct {
    const char* name;
    KeywordKind kind;
} Keyword;

// The keywords that lib/scanner.lpp reads depending on the following characters, sorted by name
static const Keyword keywords[] = {
    {"all", KEYWORD_CALL},
    {"any", KEYWORD_CALL},
    {"background", KEYWORD_CALL},
    {"case", KEYWORD_CALL},
    {"chomp", KEYWORD_CALL},
    {"class", KEYWORD_CLASS},
    {"count", KEYWORD_CALL},
    {"default", KEYWORD_SPACED_CALL},
    {"delete", KEYWORD_CALL},
    {"deprecated", KEYWORD_SPACED_CALL},
    {"drop", KEYWORD_CALL},
    {"exists", KEYWORD_CALL},
    {"final", KEYWORD_CALL},
    {"find", KEYWORD_CALL},
    {"first", KEYWORD_CALL},
    {"foldl", KEYWORD_CALL},
    {"foldr", KEYWORD_CALL},
    {"inherits", KEYWORD_CALL},
    {"iterate", KEYWORD_CALL},
    {"map", KEYWORD_CALL},
    {"module", KEYWORD_MODULE},
    {"new", KEYWORD_CALL},
    {"pop", KEYWORD_CALL},
    {"private", KEYWORD_CALL},
    {"public", KEYWORD_SPACED_CALL},
    {"push", KEYWORD_CALL},
    {"returns", KEYWORD_SPACED_CALL},
    {"select", KEYWORD_CALL},
    {"shift", KEYWORD_CALL},
    {"splice", KEYWORD_CALL},
    {"static", KEYWORD_SPACED_CALL},
    {"take", KEYWORD_CALL},
    {"takeuntil", KEYWORD_CALL},
    {"takewhile", KEYWORD_CALL},
    {"trim", KEYWORD_CALL},
    {"unshift", KEYWORD_CALL},
};

enum {
    KEYWORD_COUNT = sizeof(keywords) / sizeof(keywords[0]),
    // strlen("background") and strlen("deprecated")
    KEYWORD_MAX = 10,
};

static bool whitespace(int32_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// The grammar's /\s/ extras, as compiled into the generated lexer
static bool extra_space(int32_t c) {
    return c == ' ' || ('\t' <= c && c <= '\r');
}

/* Skips extras before a word. Returns false before a line break that the generated lexer must read as a
 * newline token instead.
 */
static bool skip_extras(TSLexer* lexer, bool newline) {
    while (extra_space(lexer->lookahead)) {
        if (newline && lexer->lookahead == '\n') {
            return false;
        }
        bool cr = lexer->lookahead == '\r';
        lexer->advance(lexer, true);
        if (newline && cr && lexer->lookahead == '\n') {
            return false;
        }
    }
    return true;
}

// {WS} in lib/scanner.lpp
static bool keyword_space(int32_t c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static bool word_start(int32_t c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool word_char(int32_t c) {
    return word_start(c) || ('0' <= c && c <= '9');
}

static const Keyword* find_keyword(const char* word) {
    size_t low = 0;
    size_t high = KEYWORD_COUNT;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int cmp = strcmp(word, keywords[mid].name);
        if (!cmp) {
            return &keywords[mid];
        }
        if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return NULL;
}

static void skip_keyword_space(TSLexer* lexer) {
    while (keyword_space(lexer->lookahead)) {
        lexer->advance(lexer, false);
    }
}

// Returns true if {WS}+ and a declaration name follow
static bool declaration_follows(TSLexer* lexer, bool scoped) {
    if (!keyword_space(lexer->lookahead)) {
        return false;
    }
    skip_keyword_space(lexer);
    if (scoped && lexer->lookahead == ':') {
        lexer->advance(lexer, false);
        if (lexer->lookahead != ':') {
            return false;
        }
        lexer->advance(lexer, false);
    }
    return word_start(lexer->lookahead);
}

/* Matches a complete keyword word, as flex's longest match does, when lib/scanner.lpp reads it as the
 * valid token given the following characters. The token ends with the word, so that the grammar reads
 * the following characters normally. Otherwise the generated lexer reads the word: class and module
 * as identifiers, the other keywords as keywords where they are valid.
 */
static bool keyword_token(TSLexer* lexer, const bool* valid_symbols) {
    if (!skip_extras(lexer, valid_symbols[NEWLINE]) || !word_start(lexer->lookahead)) {
        return false;
    }
    char word[KEYWORD_MAX + 1];
    size_t len = 0;
    do {
        if (len == KEYWORD_MAX) {
            return false;
        }
        word[len++] = (char)lexer->lookahead;
        lexer->advance(lexer, false);
    } while (word_char(lexer->lookahead));
    word[len] = '\0';

    const Keyword* keyword = find_keyword(word);
    if (!keyword) {
        return false;
    }
    lexer->mark_end(lexer);
    switch (keyword->kind) {
        case KEYWORD_CALL:
        case KEYWORD_SPACED_CALL:
            if (!valid_symbols[KEYWORD_IDENTIFIER]) {
                return false;
            }
            if (keyword->kind == KEYWORD_SPACED_CALL) {
                skip_keyword_space(lexer);
            }
            if (lexer->lookahead != '(') {
                return false;
            }
            lexer->result_symbol = KEYWORD_IDENTIFIER;
            return true;
        case KEYWORD_CLASS:
            if (!valid_symbols[CLASS_KEYWORD] || !declaration_follows(lexer, true)) {
                return false;
            }
            lexer->result_symbol = CLASS_KEYWORD;
            return true;
        case KEYWORD_MODULE:
            if (!valid_symbols[MODULE_KEYWORD] || !declaration_follows(lexer, false)) {
                return false;
            }
            lexer->result_symbol = MODULE_KEYWORD;
            return true;
    }
    return false;
}

/* The grammar has already consumed the opening brace. No allocation or shared
 * state is needed; every successful iteration consumes source input.
 */
static bool body(TSLexer* lexer) {
    size_t depth = 1;
    while (!lexer->eof(lexer)) {
        int32_t c = lexer->lookahead;
        lexer->advance(lexer, false);
        if (c == '\\') {
            if (lexer->eof(lexer)) {
                return false;
            }
            lexer->advance(lexer, false);
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            if (!--depth) {
                return true;
            }
        }
    }
    return false;
}

void* tree_sitter_qore_external_scanner_create(void) {
    return NULL;
}

void tree_sitter_qore_external_scanner_destroy(void* payload) {
    (void)payload;
}

unsigned tree_sitter_qore_external_scanner_serialize(void* payload, char* buffer) {
    (void)payload;
    (void)buffer;
    return 0;
}

void tree_sitter_qore_external_scanner_deserialize(void* payload, const char* buffer, unsigned length) {
    (void)payload;
    (void)buffer;
    (void)length;
}

bool tree_sitter_qore_external_scanner_scan(void* payload, TSLexer* lexer, const bool* valid_symbols) {
    (void)payload;
    enum TokenType type = TOKEN_COUNT;
    for (unsigned i = BRACE_REGEX_MATCH; i <= BRACE_REGEX_EXTRACT; ++i) {
        if (valid_symbols[i]) {
            if (type != TOKEN_COUNT) {
                // Tree-sitter enables every token during error recovery; matching declaration keywords there
                // lets recovery resume at a class or module declaration
                bool recovery_symbols[TOKEN_COUNT] = {false};
                recovery_symbols[CLASS_KEYWORD] = valid_symbols[CLASS_KEYWORD];
                recovery_symbols[MODULE_KEYWORD] = valid_symbols[MODULE_KEYWORD];
                recovery_symbols[NEWLINE] = valid_symbols[NEWLINE];
                return keyword_token(lexer, recovery_symbols);
            }
            type = (enum TokenType)i;
        }
    }
    if (type == TOKEN_COUNT) {
        return keyword_token(lexer, valid_symbols);
    }
    if (!body(lexer)) {
        return false;
    }
    if (type == BRACE_REGEX_SUBST || type == BRACE_REGEX_TRANS) {
        while (whitespace(lexer->lookahead)) {
            lexer->advance(lexer, false);
        }
        if (lexer->lookahead != '{') {
            return false;
        }
        lexer->advance(lexer, false);
        if (!body(lexer)) {
            return false;
        }
    }
    if (type != BRACE_REGEX_TRANS) {
        while (lexer->lookahead == 'i' || lexer->lookahead == 'm' || lexer->lookahead == 's'
                || lexer->lookahead == 'x' || lexer->lookahead == 'u' || lexer->lookahead == 'U'
                || (type != BRACE_REGEX_MATCH && lexer->lookahead == 'g')) {
            lexer->advance(lexer, false);
        }
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = type;
    return true;
}
