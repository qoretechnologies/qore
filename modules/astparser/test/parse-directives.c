/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Standalone grammar test for parse directive arguments, including %define lines, which AstParser evaluates
 * before parsing; also usable under Valgrind without loading the Qore runtime.
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

// Appends the text of each node of the given type to buf, separated by '|'
static void collect(TSNode node, const char* type, const char* source, char* buf, size_t size) {
    if (!strcmp(ts_node_type(node), type)) {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        size_t len = strlen(buf);
        int n = snprintf(buf + len, size - len, "%s%.*s", len ? "|" : "", (int)(end - start), source + start);
        assert(n >= 0 && (size_t)n < size - len);
    }
    for (uint32_t i = 0; i < ts_node_named_child_count(node); ++i) {
        collect(ts_node_named_child(node, i), type, source, buf, size);
    }
}

// Checks the parse result and the '|'-separated texts of the nodes of the given type
static void check(TSParser* parser, const char* source, bool valid, const char* type, const char* texts) {
    size_t length = strlen(source);
    assert(length <= UINT32_MAX);
    TSTree* tree = ts_parser_parse_string(parser, NULL, source, (uint32_t)length);
    assert(tree);
    TSNode root = ts_tree_root_node(tree);
    if (ts_node_has_error(root) == valid) {
        char* s = ts_node_string(root);
        fprintf(stderr, "Unexpected parse result %s: %s\n", s, source);
        free(s);
        abort();
    }
    if (type) {
        char buf[256] = "";
        collect(root, type, source, buf, sizeof(buf));
        if (strcmp(buf, texts)) {
            fprintf(stderr, "Unexpected %s nodes '%s' instead of '%s': %s\n", type, buf, texts, source);
            abort();
        }
    }
    ts_tree_delete(tree);
    ++checks;
}

int main(void) {
    TSParser* parser = ts_parser_new();
    assert(parser);
    bool language_set = ts_parser_set_language(parser, tree_sitter_qore());
    assert(language_set);

    // module paths and names
    check(parser, "%requires ../../qlib/QUnit.qm\nint x;", true, "module_path", "../../qlib/QUnit.qm");
    check(parser, "%requires /usr/lib/Foo.qm\n", true, "module_path", "/usr/lib/Foo.qm");
    check(parser, "%requires(reexport) ./Local.qm\n", true, "module_path", "./Local.qm");
    check(parser, "%requires QUnit.qm\n", true, "module_path", "QUnit.qm");
    check(parser, "%requires json >= 1.3\n", true, "module_path", "");
    check(parser, "%requires Foo::Bar\n", true, "scoped_identifier", "Foo::Bar");
    check(parser, "%try-module (ex) ../Mod.qm >= 1.0\n%endtry\n", true, "module_path", "../Mod.qm");
    check(parser, "%try-module($ex) json\n%endtry\n", true, "variable_name", "$ex");
    check(parser, "%try-reexport-module yaml\n%endtry\n", true, "module_name", "yaml");
    check(parser, "%try-child-module ./Child.qm\n", true, "module_path", "./Child.qm");

    // directive arguments consume the rest of their line, without trailing blanks
    check(parser, "%include ../x/inc.q \t\nint x;", true, "directive_argument", "../x/inc.q");
    check(parser, "%include \"inc.q\"\n", true, "directive_argument", "");
    check(parser, "%append-module-path /opt/a:$DIR/b\n", true, "directive_argument", "/opt/a:$DIR/b");
    check(parser, "%append-include-path ../include\r\nint x;", true, "directive_argument", "../include");
    check(parser, "%set-time-zone   Europe/Prague\nint x;", true, "directive_argument", "Europe/Prague");
    check(parser, "%set-time-zone +01:00", true, "directive_argument", "+01:00");

    // %define values, and the following line
    check(parser, "%define VALUE 42\nint x = 1;", true, "directive_argument", "42");
    check(parser, "%define VALUE \"a b\"  \n\"x\";", true, "directive_argument", "\"a b\"");
    check(parser, "%define VALUE bar\nbar();", true, "directive_argument", "bar");
    check(parser, "%define VALUE\nint x = 1;", true, "local_variable_declaration", "int x = 1;");
    check(parser, "%define VALUE # comment\nint x = 1;", true, "directive_argument", "");
    check(parser, "%define VALUE\r\nint x = 1;", true, "directive_argument", "");

    // parse options without arguments
    check(parser, "%broken-logic-precedence\n%strong-encapsulation\n%no-reflection\nclass C {}", true,
        "class_declaration", "class C {}");

    // an argument must be on the directive's line
    const char* invalid[] = {
        "%include\nfile.q\n", "%include", "%set-time-zone\nEurope/Prague\n", "%requires\njson\n",
        "%define\nFOO\n", "%try-module\njson\n%endtry\n", "%append-module-path\n\"x\"\n",
        "%prepend-module-path\n\"x\"\n",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        check(parser, invalid[i], false, NULL, NULL);
        check(parser, "%include inc.q\n", true, "directive_argument", "inc.q");
    }

    ts_parser_delete(parser);
    printf("Passed %u parse directive grammar checks\n", checks);
    return 0;
}
