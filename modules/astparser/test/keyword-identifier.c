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
    check(parser, "module = 1;", "variable_declarator", "module = 1");
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
    check(parser, "class += 1;", "variable_declarator", "class += 1");
    check(parser, "x = module + class;", "binary_expression", "module + class");
    check(parser, "class Foo {}", "class_declaration", "class Foo {}");
    check(parser, "class\t Foo {}", "class_declaration", "class\t Foo {}");
    check(parser, "public class Foo {}", "class_declaration", "public class Foo {}");
    check(parser, "module Foo {\n    version = \"1\";\n}", "module_declaration",
        "module Foo {\n    version = \"1\";\n}");

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
