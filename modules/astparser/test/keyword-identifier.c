/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Standalone grammar test for keywords that lib/scanner.lpp reads depending on the following
 * characters, also usable under Valgrind without loading the Qore runtime.
 */
#include <tree_sitter/api.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const TSLanguage* tree_sitter_qore(void);

static unsigned checks;

static TSNode find_node(TSNode node, const char* type) {
    if (!strcmp(ts_node_type(node), type)) {
        return node;
    }
    for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
        TSNode found = find_node(ts_node_named_child(node, i), type);
        if (!ts_node_is_null(found)) {
            return found;
        }
    }
    return (TSNode){0};
}

static bool has_text(const char* source, TSNode node, const char* text) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    return end - start == strlen(text) && !memcmp(source + start, text, end - start);
}

// Returns the number of nodes of the given type with the given text
static unsigned count_nodes(TSNode node, const char* type, const char* source, const char* text) {
    unsigned count = !strcmp(ts_node_type(node), type) && (!text || has_text(source, node, text));
    for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
        count += count_nodes(ts_node_named_child(node, i), type, source, text);
    }
    return count;
}

static void check_text(const char* source, TSNode node, const char* text) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    assert(start <= end && end <= strlen(source));
    if (end - start != strlen(text) || memcmp(source + start, text, end - start)) {
        fprintf(stderr, "Unexpected node text '%.*s' instead of '%s': %s\n", (int)(end - start), source + start,
            text, source);
        abort();
    }
}

static TSNode find_required_node(TSNode root, const char* type, const char* source) {
    TSNode node = find_node(root, type);
    if (ts_node_is_null(node)) {
        char* tree = ts_node_string(root);
        fprintf(stderr, "No %s node in %s: %s\n", type, tree, source);
        free(tree);
        abort();
    }
    return node;
}

static TSTree* parse(TSParser* parser, const char* source, bool valid) {
    size_t length = strlen(source);
    assert(length <= UINT32_MAX);
    TSTree* tree = ts_parser_parse_string(parser, NULL, source, (uint32_t)length);
    assert(tree);
    if (ts_node_has_error(ts_tree_root_node(tree)) == valid) {
        fprintf(stderr, "Unexpected parse result: %s\n", source);
        abort();
    }
    ++checks;
    return tree;
}

// Checks that the first node of the given type has the given text; a call is checked by its function name
static void check(TSParser* parser, const char* source, const char* type, const char* text) {
    TSTree* tree = parse(parser, source, true);
    TSNode node = find_required_node(ts_tree_root_node(tree), type, source);
    if (!strcmp(type, "call_expression")) {
        node = ts_node_child_by_field_name(node, "function", 8);
        assert(!ts_node_is_null(node));
        assert(!strcmp(ts_node_type(node), "identifier"));
    }
    check_text(source, node, text);
    ts_tree_delete(tree);
}

static void check_invalid(TSParser* parser, const char* source) {
    ts_tree_delete(parse(parser, source, false));
}

// Checks that the first node of the given type starts with the given anonymous keyword
static void check_keyword(TSParser* parser, const char* source, const char* type, const char* keyword,
        bool valid) {
    TSTree* tree = parse(parser, source, valid);
    TSNode node = find_required_node(ts_tree_root_node(tree), type, source);
    TSNode child = ts_node_child(node, 0);
    if (!strcmp(ts_node_type(child), "modifiers")) {
        child = ts_node_next_sibling(child);
    }
    assert(!strcmp(ts_node_type(child), keyword));
    assert(!ts_node_is_named(child));
    check_text(source, child, keyword);
    ts_tree_delete(tree);
}

static const char* const streaming_operators[] = {
    "streaming_terminal_expression", "streaming_limited_expression", "streaming_boundary_expression",
    "iterate_expression",
};

// Checks that each streaming operator keyword in the format is an identifier and that no operator is parsed
static void check_names(TSParser* parser, const char* format, const char* const* names, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char source[128];
        int size = snprintf(source, sizeof(source), format, names[i], names[i]);
        assert(size > 0 && (size_t)size < sizeof(source));
        TSTree* tree = parse(parser, source, true);
        TSNode root = ts_tree_root_node(tree);
        unsigned uses = 0;
        for (const char* p = strstr(source, names[i]); p; p = strstr(p + 1, names[i])) {
            ++uses;
        }
        if (count_nodes(root, "identifier", source, names[i]) != uses) {
            char* text = ts_node_string(root);
            fprintf(stderr, "Expected %u identifiers named %s in %s: %s\n", uses, names[i], text, source);
            free(text);
            abort();
        }
        for (size_t j = 0; j < sizeof(streaming_operators) / sizeof(*streaming_operators); ++j) {
            assert(!count_nodes(root, streaming_operators[j], source, NULL));
        }
        ts_tree_delete(tree);
    }
}

// Checks that each streaming operator keyword in the format starts its operator
static void check_operators(TSParser* parser, const char* format, const char* const* names,
        const char* const* types, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char source[128];
        int size = snprintf(source, sizeof(source), format, names[i]);
        assert(size > 0 && (size_t)size < sizeof(source));
        TSTree* tree = parse(parser, source, true);
        TSNode node = find_required_node(ts_tree_root_node(tree), types[i], source);
        assert(ts_node_start_byte(node) == (uint32_t)(strstr(source, names[i]) - source));
        assert(!count_nodes(ts_tree_root_node(tree), "identifier", source, names[i]));
        ts_tree_delete(tree);
    }
}

// Checks that an edit changes a streaming keyword from an operator to a name
static void check_reparse(TSParser* parser, const char* before, const char* after, uint32_t offset) {
    TSTree* tree = parse(parser, before, true);
    assert(!ts_node_is_null(find_node(ts_tree_root_node(tree), "streaming_terminal_expression")));
    TSInputEdit edit = {
        .start_byte = offset,
        .old_end_byte = offset,
        .new_end_byte = offset + 1,
        .start_point = {0, offset},
        .old_end_point = {0, offset},
        .new_end_point = {0, offset + 1},
    };
    ts_tree_edit(tree, &edit);
    TSTree* edited = ts_parser_parse_string(parser, tree, after, (uint32_t)strlen(after));
    assert(edited);
    TSNode root = ts_tree_root_node(edited);
    assert(!ts_node_has_error(root));
    assert(ts_node_is_null(find_node(root, "streaming_terminal_expression")));
    assert(count_nodes(root, "identifier", after, "count") == 1);
    ++checks;
    ts_tree_delete(edited);
    ts_tree_delete(tree);
}

static void check_calls(TSParser* parser, const char* format, const char* const* names, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char source[64];
        int size = snprintf(source, sizeof(source), format, names[i]);
        assert(size > 0 && (size_t)size < sizeof(source));
        check(parser, source, "call_expression", names[i]);
    }
}

int main(void) {
    TSParser* parser = ts_parser_new();
    assert(parser);
    bool language_set = ts_parser_set_language(parser, tree_sitter_qore());
    assert(language_set);

    const char* immediate[] = {
        "all", "any", "background", "case", "chomp", "count", "delete", "drop", "exists", "final", "find",
        "first", "foldl", "foldr", "inherits", "iterate", "map", "new", "pop", "private", "push", "select",
        "shift", "splice", "take", "takeuntil", "takewhile", "trim", "unshift",
    };
    const char* spaced[] = {"default", "deprecated", "module", "public", "returns", "static"};
    const char* unreserved[] = {"class", "module"};
    const size_t immediate_count = sizeof(immediate) / sizeof(*immediate);
    const size_t spaced_count = sizeof(spaced) / sizeof(*spaced);
    const size_t unreserved_count = sizeof(unreserved) / sizeof(*unreserved);

    // every name at a statement start and in an expression; the token excludes the parenthesis
    const char* formats[] = {"%s(1);", "x = %s(1, 2);", "sub f() { %s(); }", "x = \\%s();"};
    for (size_t i = 0; i < sizeof(formats) / sizeof(*formats); ++i) {
        check_calls(parser, formats[i], immediate, immediate_count);
        check_calls(parser, formats[i], spaced, spaced_count);
        check_calls(parser, formats[i], unreserved, unreserved_count);
    }
    check_calls(parser, "x = %s \t\r(1);", spaced, spaced_count);
    check_calls(parser, "%s \n /* comment */ (1);", unreserved, unreserved_count);
    check_calls(parser, "x = %s # comment\n(1);", unreserved, unreserved_count);

    // extras before the name
    check(parser, " \t\r\n\f\vselect(1);", "call_expression", "select");
    check(parser, "/* comment */select(1);", "call_expression", "select");
    check(parser, "# comment\nselect(1);", "call_expression", "select");

    // only complete keyword words are matched
    check(parser, "backgrounds(1);", "call_expression", "backgrounds");
    check(parser, "deprecatedx(1);", "call_expression", "deprecatedx");
    check(parser, "select_(1);", "call_expression", "select_");
    check(parser, "map2(1);", "call_expression", "map2");
    check(parser, "Select(1);", "call_expression", "Select");
    check(parser, "classes(1);", "call_expression", "classes");

    // class and module are identifiers unless a declaration name follows
    check(parser, "module = 1;", "assignment_expression", "module = 1");
    check(parser, "class.run();", "member_expression", "class.run");
    check(parser, "x = f(module: 1, class: 2);", "named_argument", "module: 1");
    check(parser, "x = module::X + class::Y;", "scoped_identifier", "module::X");
    check(parser, "x = module<int>::X;", "generic_scoped_identifier", "module<int>::X");
    check(parser, "sub f(any class, any module) {}", "parameter", "any class");
    check(parser, "any module = 1;", "variable_declarator", "module = 1");
    check(parser, "my (module, class) = (1, 2);", "list_assignment", "my (module, class) = (1, 2);");
    check_keyword(parser, "class Foo {}", "class_declaration", "class", true);
    check_keyword(parser, "public class Foo {}", "class_declaration", "class", true);
    check_keyword(parser, "module Foo {\n}", "module_declaration", "module", true);
    // a following declaration leaves a directive's line break to the newline token
    check(parser, "%modern\nclass Foo {}", "newline", "\n");
    check(parser, "%modern \r\nclass Foo {}", "newline", "\r\n");
    check(parser, "%modern\n  module Foo {\n}", "newline", "\n");
    check(parser, "%modern\nselect(1);", "newline", "\n");
    check_keyword(parser, "%modern\r\n\r\nclass Foo {}", "class_declaration", "class", true);
    // error recovery resumes at a declaration
    check_keyword(parser, "x = ;\nclass Foo {}", "class_declaration", "class", false);

    // operators, declarations and keywords that do not precede a parenthesis
    check(parser, "x = select (a), $1;", "select_expression", "select (a), $1");
    check(parser, "x = select\n(a), $1;", "select_expression", "select\n(a), $1");
    check(parser, "x = select /* comment */(a), $1;", "select_expression", "select /* comment */(a), $1");
    check(parser, "x = map $1, a;", "map_expression", "map $1, a");
    check(parser, "x = exists (a);", "unary_expression", "exists (a)");
    check(parser, "class += 1;", "assignment_expression", "class += 1");
    check(parser, "x = module + class;", "binary_expression", "module + class");
    check(parser, "class Foo {}", "class_declaration", "class Foo {}");
    check(parser, "class\t Foo {}", "class_declaration", "class\t Foo {}");
    check(parser, "public class Foo {}", "class_declaration", "public class Foo {}");
    check(parser, "module Foo {\n    version = \"1\";\n}", "module_declaration",
        "module Foo {\n    version = \"1\";\n}");

    // streaming operator keywords are names before the characters in SOFT_IDENTIFIER_FOLLOW
    const char* streaming[] = {
        "all", "any", "count", "drop", "first", "iterate", "take", "takeuntil", "takewhile",
    };
    const char* streaming_types[] = {
        "streaming_terminal_expression", "streaming_terminal_expression", "streaming_terminal_expression",
        "streaming_limited_expression", "streaming_terminal_expression", "iterate_expression",
        "streaming_limited_expression", "streaming_boundary_expression", "streaming_boundary_expression",
    };
    const size_t streaming_count = sizeof(streaming) / sizeof(*streaming);
    assert(streaming_count == sizeof(streaming_types) / sizeof(*streaming_types));
    const char* names[] = {
        "x = %s;", "x = f(%s - 1, 2);", "x = %s+1;", "x = %s-1;", "x = %s\n+ 1;", "x = %s + %s;", "%s++;",
        "%s--;", "x = %s ++ + 1;", "%s += 1;", "%s -= 1;", "%s *= 2;", "%s /= 2;", "%s %%= 2;", "%s &= 2;",
        "%s |= 2;", "%s ^= 2;", "%s >>= 2;", "%s <<= 2;", "x = %s == 1;", "x = %s\t!= 1;", "x = %s <> 1;",
        "x = %s <= 1;", "x = %s >= 1;", "x = %s <=> 1;", "x = %s && %s;", "x = %s || 1;", "x = %s * 2;",
        "x = %s / 2;", "x = %s %% 2;", "x = %s & 2;", "x = %s | 2;", "x = %s ^ 2;", "x = %s < 1;",
        "x = %s > 1;", "x = %s ? 1 : 0;", "x = (%s);", "x = l[%s];", "x = {\"a\": %s};", "x = %s.y;",
        "x = %s[0];", "x = %s{\"a\"};", "x = %s /* comment */ + 1;", "x = %s\r\n\tinstanceof C;",
        "x = %s --;",
        "x = f(%s: 1);", "x = f(a: 1, %s: 2);", "sub f(int %s) {}", "sub f(int %s = 1) { return %s; }",
        "foreach int %s in (l) {}", "foreach int %s\nin (l) {}", "int %s = 1;", "(int a, %s) = l;",
        "x = find first+%s in q where (1);",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        check_names(parser, names[i], streaming, streaming_count);
    }
    // a find value, except "first", which is the find mode
    for (size_t i = 0; i < streaming_count; ++i) {
        if (strcmp(streaming[i], "first")) {
            check_names(parser, "x = find %s in q where (1);", streaming + i, 1);
        }
    }
    check_invalid(parser, "x = find first in q where (1);");

    // other characters, whitespace before "-", "+" or "!", and words that start with "in" make operators
    const char* operators[] = {
        "x = %s -a, l;", "x = %s +a, l;", "x = %s !a, l;", "x = %s index, l;", "x = %s instances, l;",
        "x = %s\n$1, l;", "x = %s \"a\", l;", "x = %s # comment\n$1, l;", "x = %s (a), l;", "x = %s +- a, l;",
    };
    for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); ++i) {
        check_operators(parser, operators[i], streaming, streaming_types, streaming_count);
    }
    check_invalid(parser, "x = count");
    check_invalid(parser, "x = count in");

    // "first", "last" and "one" followed by whitespace after "find" are the find mode
    check(parser, "x = find first %name in q where (1);", "find_modifier", "first");
    check(parser, "x = find\nlast\n%name in q where (1);", "find_modifier", "last");
    check(parser, "x = find one a in q where (1);", "find_modifier", "one");
    check(parser, "x = find first(1) in q where (1);", "call_expression", "first");
    check(parser, "x = find last(1) in q where (1);", "call_expression", "last");
    check(parser, "x = last + one;", "binary_expression", "last + one");
    check_invalid(parser, "x = find first");

    // an incremental parse must reread the keyword when the characters after it change
    check_reparse(parser, "x = count -1, l;", "x = count - 1, l;", 11);
    check_reparse(parser, "x = count l, m;", "x = count ;l, m;", 10);

    // "<>" is an operator, and a generic type may have no arguments, with or without whitespace
    check(parser, "x = a <> b;", "binary_expression", "a <> b");
    check(parser, "Box<> b(1);", "generic_type", "Box<>");
    check(parser, "Box< \t> b(1);", "generic_type", "Box< \t>");
    check(parser, "hash<Result<>> r();", "generic_type", "Result<>");
    check(parser, "x = Static<>::echo(1);", "generic_scoped_identifier", "Static<>::echo");
    check(parser, "hashdecl H<T = string> inherits P<> { T a; }", "generic_type", "P<>");

    const char* invalid[] = {
        "x = select (1);", "x = select", "x = select(", "public\n(1);", "static\n(1);", "x = public ",
        "class\nFoo {}", "module\nFoo {\n}", "class :Foo {}", "x = select(1,;",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        check_invalid(parser, invalid[i]);
        check(parser, "select(1);", "call_expression", "select");
    }

    // an incremental parse must not reuse the name token once the parenthesis has moved
    const char* before = "x = (select(a), $1);";
    const char* after = "x = (select (a), $1);";
    TSTree* tree = parse(parser, before, true);
    assert(!ts_node_is_null(find_node(ts_tree_root_node(tree), "call_expression")));
    TSInputEdit edit = {
        .start_byte = 11,
        .old_end_byte = 11,
        .new_end_byte = 12,
        .start_point = {0, 11},
        .old_end_point = {0, 11},
        .new_end_point = {0, 12},
    };
    ts_tree_edit(tree, &edit);
    TSTree* edited = ts_parser_parse_string(parser, tree, after, (uint32_t)strlen(after));
    assert(edited);
    TSNode select = find_required_node(ts_tree_root_node(edited), "select_expression", after);
    check_text(after, select, "select (a), $1");
    assert(!ts_node_has_error(ts_tree_root_node(edited)));
    assert(ts_node_is_null(find_node(ts_tree_root_node(edited), "call_expression")));
    ++checks;
    ts_tree_delete(edited);
    ts_tree_delete(tree);

    ts_parser_delete(parser);
    printf("Passed %u keyword identifier grammar checks\n", checks);
    return 0;
}
