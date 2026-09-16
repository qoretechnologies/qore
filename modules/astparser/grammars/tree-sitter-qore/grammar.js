/**
 * @file Qore grammar for tree-sitter
 * Copyright (C) 2026 Qore Technologies, s.r.o.
 * @author Qore Technologies
 * @license LGPL-2.1
 * @see {@link https://qore.org|Qore Programming Language}
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

// lib/scanner.lpp has no reserved type names: these are identifiers that name built-in types
const TYPE_NAMES = [
  'string', 'int', 'float', 'number', 'bool', 'binary', 'date',
  'char', 'list', 'hash', 'softint', 'softfloat', 'softnumber', 'softbool',
  'softstring', 'softchar', 'softdate', 'softlist', 'timeout',
  'object', 'code', 'reference', 'nothing', 'any', 'auto', 'data',
  'union',
];

const PREC = {
  // Operator precedence (based on Qore's parser)
  COMMA: 1,
  ASSIGN: 2,
  TERNARY: 3,
  NULL_COALESCE: 4,
  LOGICAL_OR: 5,
  LOGICAL_AND: 6,
  BITWISE_OR: 7,
  BITWISE_XOR: 8,
  BITWISE_AND: 9,
  EQUALITY: 10,
  COMPARISON: 11,
  SHIFT: 12,
  ADD: 13,
  MULTIPLY: 14,
  UNARY: 15,
  POSTFIX: 16,
  CALL: 17,
  MEMBER: 18,
};

module.exports = grammar({
  name: 'qore',

  externals: $ => [
    $._brace_regex_match,
    $._brace_regex_subst,
    $._brace_regex_trans,
    $._brace_regex_extract,
    $._keyword_identifier,
    $._class_keyword,
    $._module_keyword,
    // a streaming operator keyword that lib/scanner.lpp reads as a name because of the characters that follow it,
    // as in "count - 1" or "first;"
    $._soft_identifier,
    // "first", "last" or "one" followed by whitespace after "find", which lib/scanner.lpp reads as the find mode
    $._find_modifier,
    // matched by the newline rule; listed so that src/scanner.c knows when a
    // newline is valid and leaves it to the generated lexer
    $.newline,
    // the rest of a directive line, which lib/scanner.lpp reads as the
    // argument; a quoted argument is a string, and a comment is not an argument
    $.directive_argument,
    // the rest of a %define line, which may be a quoted string
    $._define_value,
    // an empty token that requires a directive's argument on the same line
    $._same_line,
  ],

  extras: $ => [
    $.comment,
    $.line_comment,
    $.conditional_directive,
    /\s/,
  ],

  conflicts: $ => [
    [$.hash_literal, $.block],
    [$.hash_literal, $._statement],
    [$.list_literal, $.paren_hash_literal],
    [$.function_declaration, $.primary_expression, $.generic_type],
    [$.argument_list, $.parameter_list],
    [$._statement, $._top_level_item],
    [$.module_spec],
    // type keywords as expressions conflict with type usage
    [$._type_keyword, $.complex_type],
    [$._type_keyword, $.simple_type],
    [$.simple_type, $.complex_type],
    [$.simple_type, $.generic_type],
    [$.type_parameter, $.simple_type],
    // case < identifier could be comparison value or start of <Type> cast
    [$.primary_expression, $.simple_type],
    [$.primary_expression, $.generic_type],
    [$.generic_type, $.generic_scoped_identifier],
    [$.generic_scoped_identifier, $.scoped_identifier],
    // conditional_declaration in if/while vs as primary_expression
    [$.if_statement, $.primary_expression],
    [$.while_statement, $.primary_expression],
    // map/select optional filter comma vs enclosing comma (hash/list trailing comma)
    [$.map_expression],
    [$.select_expression],
    [$.simple_type, $.streaming_terminal_expression],
    [$._type_keyword, $.streaming_terminal_expression],
    // a declared name vs an expression: local declarations, parameters and lists of declarations, such as
    // (int a, b) = ..., vs expressions starting with a name or "("
    [$.primary_expression, $._declared_name],
    [$.conditional_declaration, $._declared_name],
    // @debug(type var) vs @debug (expression) and a list of declarations
    [$.debug_statement, $._declared_name],
    [$.debug_statement, $.primary_expression, $._declared_name],
    // a type name as an expression, the name of an untyped declaration or the start of a type
    [$._type_keyword, $._declared_name],
    [$._declared_name, $.generic_type],
    [$._declared_name, $.complex_type],
  ],

  rules: {
    // Entry point
    source_file: $ => repeat($._top_level_item),

    _top_level_item: $ => choice(
      $.parse_directive,
      $.module_declaration,
      $.namespace_declaration,
      $.class_declaration,
      $.function_declaration,
      $.constant_declaration,
      $.global_variable_declaration,
      $.hashdecl_declaration,
      $.typedef_declaration,
      $.enum_declaration,
      $._statement,
    ),

    // Module declaration block
    // e.g., module Swagger { version = "1.0"; author = "..."; }
    module_declaration: $ => seq(
      // lib/scanner.lpp reads module as a keyword only when a name follows
      alias($._module_keyword, 'module'),
      field('name', $.identifier),
      '{',
      repeat(choice($.module_attribute, $.parse_directive)),
      '}',
    ),

    module_attribute: $ => seq(
      field('name', $.identifier),
      '=',
      field('value', $._expression),
      ';',
    ),

    // ==================== Parse Directives ====================
    // Note: Conditional compilation directives (%ifdef, %ifndef, %if, %elif,
    // %else, %endif) and position-flexible directives (%disable-warning, etc.)
    // are handled as extras so they can appear anywhere in the source.
    parse_directive: $ => seq(
      choice(
        // Style directives
        '%new-style',
        '%old-style',
        // Type checking
        '%require-types',
        '%strict-types',
        '%require-prototypes',
        // Variable handling
        '%require-our',
        '%assume-local',
        '%assume-global',
        '%allow-bare-refs',
        // Boolean evaluation
        '%perl-bool-eval',
        '%strict-bool-eval',
        // Debugging
        '%enable-debug',
        '%disable-debug',
        '%no-debugging',
        '%allow-debugger',
        '%allow-injection',
        '%no-database',
        '%no-external-process',
        '%no-filesystem',
        '%no-gui',
        '%no-io',
        '%no-locale-control',
        '%no-modules',
        '%no-network',
        '%no-process-control',
        '%no-thread-classes',
        '%no-thread-control',
        '%no-thread-info',
        '%no-threads',
        '%no-top-level',
        '%no-uncontrolled-apis',
        '%no-terminal-io',
        '%no-external-access',
        // Warning controls (all-warnings variants stay structured)
        '%enable-all-warnings',
        '%disable-all-warnings',
        // Module directives
        seq('%requires', $._same_line, $.module_spec),
        seq('%requires', $._same_line, '(', 'reexport', ')', $.module_spec),
        seq('%try-module', $._same_line, optional($._try_module_variable), $.module_spec),
        seq('%try-reexport-module', $._same_line, optional($._try_module_variable), $.module_spec),
        seq('%try-child-module', $._same_line, $.module_spec),
        '%endtry',
        // Define directive for conditional compilation, with an optional value
        seq('%define', $._same_line, $.identifier, optional(alias($._define_value, $.directive_argument))),
        seq('%set-time-zone', $.directive_argument),
        // Other common directives
        '%strict-args',
        '%allow-weak-references',
        '%no-global-vars',
        '%no-child-restrictions',
        '%no-typedef',
        '%no-enum',
        '%no-transient',
        '%no-new',
        '%no-iterate',
        '%no-first',
        '%no-any-operator',
        '%no-all-operator',
        '%no-count',
        '%no-take',
        '%no-drop',
        '%no-takewhile',
        '%no-takeuntil',
        '%no-find-modifiers',
        '%no-stream-fusion',
        '%streaming-any',
        '%no-streaming-operators',
        '%no-char-type',
        '%no-string-index-char',
        '%negative-offsets',
        '%no-negative-offsets',
        '%lockdown',
        '%modern',
        // the include path and legacy module path forms are unquoted
        seq('%append-include-path', $._same_line, choice($.string, $.directive_argument)),
        seq('%append-module-path', $._same_line, choice($.string, $.directive_argument)),
        seq('%prepend-module-path', $._same_line, $.string),
        seq('%include', $._same_line, choice($.string, $.directive_argument)),
        // Parse options without arguments that lib/scanner.lpp accepts
        '%allow-reparse',
        '%allow-returns',
        '%allow-statement-no-effect',
        '%broken-auto-cast',
        '%broken-cast',
        '%broken-int-assignments',
        '%broken-list-parsing',
        '%broken-list-range',
        '%broken-logic-precedence',
        '%broken-loop-statement',
        '%broken-namespace-resolution',
        '%broken-narrowed-types',
        '%broken-operators',
        '%broken-range',
        '%broken-references',
        '%broken-soft-types',
        '%broken-sprintf',
        '%broken-varargs',
        '%correct-cast',
        '%correct-int-assignments',
        '%correct-list-parsing',
        '%correct-list-range',
        '%correct-logic-precedence',
        '%correct-loop-statement',
        '%correct-namespace-resolution',
        '%correct-narrowed-types',
        '%correct-operators',
        '%correct-range',
        '%correct-references',
        '%correct-soft-types',
        '%correct-sprintf',
        '%correct-varargs',
        '%fp-fast-math',
        '%lock-options',
        '%lock-warnings',
        '%loose-args',
        '%loose-types',
        '%no-class-defs',
        '%no-constant-defs',
        '%no-external-info',
        '%no-module-path-directives',
        '%no-namespace-defs',
        '%no-reflection',
        '%no-subroutine-defs',
        '%no-summarize',
        '%require-dollar',
        '%strict-warnings',
        '%strong-encapsulation',
      ),
      optional($.newline),
    ),

    // Preprocessor-like directives — placed in extras so they can appear
    // between any two tokens (like comments).  Each directive consumes from the
    // % sign through to the end of the line.
    conditional_directive: $ => token(choice(
      // Conditional compilation
      seq('%ifdef', /[^\n]*/),
      seq('%ifndef', /[^\n]*/),
      seq('%if', /\s/, /[^\n]*/),
      seq('%elif', /\s/, /[^\n]*/),
      seq('%else', /[^\n]*/),
      seq('%endif', /[^\n]*/),
      // Warning control (can appear between any declarations)
      seq('%disable-warning', /[^\n]*/),
      seq('%enable-warning', /[^\n]*/),
      // Parse option stack (can appear between any declarations)
      seq('%push-parse-options', /[^\n]*/),
      seq('%pop-parse-options', /[^\n]*/),
      // Module command directive (consumes entire line as opaque token)
      seq('%module-cmd', /[^\n]*/),
      // Exec-class with class name argument
      seq('%exec-class', /[^\n]*/),
    )),

    module_name: $ => choice(
      $.identifier,
      $.scoped_identifier,
      $.module_path,
    ),

    // A module file given by a relative or absolute path, such as
    // ../../qlib/QUnit.qm
    module_path: $ => /[^\s()<>=]*[.\/][^\s()<>=]*/,

    // %try-module (ex) module: the variable that receives a load exception,
    // followed by the module on the same line
    _try_module_variable: $ => seq('(', choice($.variable_name, $.identifier), ')', $._same_line),

    // Module specification with optional version constraint
    // e.g., "qore >= 1.0.3", "json", "xml <= 2.0"
    module_spec: $ => seq(
      $.module_name,
      optional($.version_constraint),
    ),

    version_constraint: $ => seq(
      field('operator', $.comparison_operator),
      field('version', $.version_number),
    ),

    comparison_operator: $ => choice('>=', '<=', '>', '<', '=='),

    version_number: $ => token(/[0-9]+(\.[0-9]+)*/),

    // ==================== Namespace ====================
    namespace_declaration: $ => seq(
      optional($.modifiers),
      'namespace',
      field('name', choice($.identifier, $.scoped_identifier)),
      choice(
        seq('{', repeat($._namespace_item), '}'),
        ';',
      ),
    ),

    _namespace_item: $ => choice(
      $.parse_directive,
      $.namespace_declaration,
      $.class_declaration,
      $.function_declaration,
      $.constant_declaration,
      $.global_variable_declaration,
      $.hashdecl_declaration,
      $.typedef_declaration,
      $.enum_declaration,
    ),

    // ==================== Class ====================
    class_declaration: $ => seq(
      optional($.modifiers),
      // lib/scanner.lpp reads class as a keyword only when a name follows
      alias($._class_keyword, 'class'),
      field('name', choice($.identifier, $.scoped_identifier)),
      optional(field('type_parameters', $.type_parameter_list)),
      optional($.class_compatibility_attributes),
      optional($.superclass_list),
      choice(
        seq('{', repeat($._class_item), '}'),
        ';',  // forward declaration
      ),
    ),

    class_compatibility_attributes: $ => repeat1($.class_compatibility_attribute),

    class_compatibility_attribute: $ => seq(
      '[',
      field('name', $.identifier),
      ']',
    ),

    type_parameter_list: $ => seq(
      '<',
      commaSep1($.type_parameter),
      '>',
    ),

    type_parameter: $ => seq(
      field('name', $.identifier),
      optional(seq(':', field('bound', $.type))),
      optional(seq('=', field('default', $.type))),
    ),

    superclass_list: $ => seq(
      'inherits',
      commaSep1($.superclass),
    ),

    superclass: $ => seq(
      optional($.access_modifier),
      choice($.generic_type, $.scoped_identifier, $.identifier),
    ),

    _class_item: $ => choice(
      $.parse_directive,
      $.member_declaration,
      $.method_declaration,
      $.constructor_declaration,
      $.destructor_declaration,
      $.copy_method,
      $.constant_declaration,
      $.member_group,
    ),

    member_group: $ => seq(
      $.access_modifier,
      '{',
      repeat(choice($.member_declaration, $.constant_declaration, $.parse_directive)),
      '}',
    ),

    member_declaration: $ => seq(
      optional($.modifiers),
      field('type', optional($.type)),
      field('name', $._declared_name),
      optional(choice(
        seq('=', field('default', $._expression)),
        $.argument_list,  // constructor arguments: static Mutex m();
      )),
      ';',
    ),

    method_declaration: $ => prec.dynamic(1, seq(
      optional($.modifiers),
      optional(field('return_type', $.type)),
      field('name', $._declared_name),
      optional(field('type_parameters', $.type_parameter_list)),
      $.parameter_list,
      optional($.method_qualifier),
      optional(seq('returns', field('returns', $.type))),
      choice(
        $.block,
        ';',
      ),
    )),

    constructor_declaration: $ => seq(
      optional($.modifiers),
      'constructor',
      $.parameter_list,
      optional($.base_class_constructor_calls),
      choice(
        $.block,
        ';',
      ),
    ),

    destructor_declaration: $ => seq(
      optional($.modifiers),
      'destructor',
      '(',
      ')',
      choice(
        $.block,
        ';',
      ),
    ),

    copy_method: $ => seq(
      optional($.modifiers),
      'copy',
      '(',
      optional(seq(
        choice(
          seq(commaSep1($.parameter), optional(seq(',', '...'))),
          '...',
        ),
        optional(','),
      )),
      ')',
      choice(
        $.block,
        ';',
      ),
    ),

    base_class_constructor_calls: $ => seq(
      ':',
      commaSep1($.base_class_constructor_call),
    ),

    // Base class constructor call: ClassName(args) or Namespace::ClassName(args)
    base_class_constructor_call: $ => seq(
      choice($.scoped_identifier, $.identifier),
      $.argument_list,
    ),

    // ==================== Function ====================
    // lib/parser.ypp: sub_def and scoped_sub_def, which require a block, and outofline_methoddef, which may end
    // with ";" only after method modifiers
    function_declaration: $ => choice(
      seq(
        optional($.modifiers),
        optional(field('return_type', $.type)),
        'sub',
        field('name', choice($.identifier, $.scoped_identifier)),
        optional(field('type_parameters', $.type_parameter_list)),
        $.parameter_list,
        optional(seq('returns', field('returns', $.type))),
        $.block,
      ),
      seq(
        optional($.modifiers),
        optional(field('return_type', $.type)),
        field('name', $.scoped_identifier),
        optional(field('type_parameters', $.type_parameter_list)),
        $.parameter_list,
        optional($.method_qualifier),
        optional(seq('returns', field('returns', $.type))),
        $.block,
      ),
      seq(
        $.modifiers,
        optional(field('return_type', $.type)),
        field('name', $.scoped_identifier),
        optional(field('type_parameters', $.type_parameter_list)),
        $.parameter_list,
        optional($.method_qualifier),
        optional(seq('returns', field('returns', $.type))),
        ';',
      ),
    ),

    method_qualifier: $ => 'const',

    // lib/parser.ypp reads the parameters as a list expression, which may end with a comma
    parameter_list: $ => seq(
      '(',
      optional(seq(
        choice(
          seq(commaSep1($.parameter), optional(seq(',', '...'))),
          '...',
        ),
        optional(','),
      )),
      ')',
    ),

    parameter: $ => seq(
      optional($.modifiers),
      optional('const'),
      optional(field('type', $.type)),
      field('name', $._declared_name),
      optional(seq('=', field('default', $._expression))),
    ),

    // ==================== Constants and Variables ====================
    constant_declaration: $ => prec(3, seq(
      optional($.modifiers),
      'const',
      choice(
        seq(field('type', $.type), field('name', choice($._declared_name, $.scoped_identifier))),
        field('name', choice($._declared_name, $.scoped_identifier)),
      ),
      '=',
      field('value', $._expression),
      ';',
    )),

    // 'our' / 'my' / 'thread_local' declarations (old-style and global scope)
    global_variable_declaration: $ => seq(
      choice('our', 'my', 'thread_local'),
      optional(field('type', $.type)),
      commaSep1(choice($.variable_declarator, alias($._scoped_variable_declarator, $.variable_declarator))),
      ';',
    ),

    // A global variable in a namespace path; lib/parser.ypp gvardecl accepts
    // SCOPED_REF names for our and thread_local
    _scoped_variable_declarator: $ => seq(
      field('name', $.scoped_identifier),
      optional(seq(choice('=', '+=', ':='), field('value', $._expression))),
    ),

    variable_declarator: $ => variableDeclarator($, $._declared_name),


    // ==================== Hashdecl ====================
    hashdecl_declaration: $ => seq(
      optional($.modifiers),
      'hashdecl',
      field('name', choice($.identifier, $.scoped_identifier)),
      optional(field('type_parameters', $.type_parameter_list)),
      optional($.superclass_list),
      '{',
      repeat(choice($.hashdecl_member, $.parse_directive)),
      '}',
    ),

    hashdecl_member: $ => seq(
      optional(field('type', $.type)),
      field('name', $.identifier),
      optional(choice(
        seq('=', field('default', $._expression)),
        $.argument_list,  // constructor arguments: list<X> items();
      )),
      ';',
    ),

    // ==================== Typedef ====================
    // Supports both forms:
    //   typedef <type> <name>;          (e.g., typedef int MyInt;)
    //   typedef <name> = <type>;        (e.g., typedef MyInt = int;)
    typedef_declaration: $ => seq(
      optional($.modifiers),
      'typedef',
      choice(
        seq(field('type', $.type), field('name', choice($.identifier, $.scoped_identifier))),
        seq(field('name', choice($.identifier, $.scoped_identifier)), '=', field('type', $.type)),
      ),
      ';',
    ),

    // ==================== Enum ====================
    enum_declaration: $ => seq(
      optional($.modifiers),
      'enum',
      field('name', choice($.identifier, $.scoped_identifier)),
      optional(seq(':', field('base_type', $.type))),
      '{',
      optional(commaSep1($.enum_member)),
      optional(','),  // trailing comma allowed
      '}',
    ),

    enum_member: $ => seq(
      field('name', $.identifier),
      optional(seq('=', field('value', $._expression))),
    ),

    // ==================== Statements ====================
    _statement: $ => choice(
      $.parse_directive,
      $.empty_statement,
      $.expression_statement,
      $.block,
      $.if_statement,
      $.while_statement,
      $.do_while_statement,
      $.for_statement,
      $.foreach_statement,
      $.switch_statement,
      $.try_statement,
      $.return_statement,
      $.throw_statement,
      $.rethrow_statement,
      $.break_statement,
      $.continue_statement,
      $.on_exit_statement,
      $.debug_statement,
      $.assert_statement,
      $.context_statement,
      $.summarize_statement,
      $.local_variable_declaration,
      $.global_variable_declaration,  // my/our declarations inside functions
      $.list_assignment,
      $.thread_exit_statement,
    ),

    // Empty statement: just a semicolon (e.g. the ; in "};")
    empty_statement: $ => ';',

    expression_statement: $ => seq(
      choice($.sequence_expression, $._expression),
      ';',
    ),

    // Comma-separated expression list at statement level: remove x, remove y;
    // Not part of _expression to avoid conflicts with commaSep1 in argument lists
    sequence_expression: $ => prec.left(PREC.COMMA, seq(
      $._expression,
      ',',
      choice($.sequence_expression, $._expression),
    )),

    block: $ => seq(
      '{',
      repeat($._statement),
      '}',
    ),

    if_statement: $ => prec.right(seq(
      'if',
      '(',
      field('condition', choice($.conditional_declaration, $._expression)),
      ')',
      field('consequence', $._statement),
      optional(seq(
        'else',
        field('alternative', $._statement),
      )),
    )),

    while_statement: $ => seq(
      'while',
      '(',
      field('condition', choice($.conditional_declaration, $._expression)),
      ')',
      field('body', $._statement),
    ),

    // Variable declaration inside if/while condition: if (*string val = expr) { ... }
    conditional_declaration: $ => prec.dynamic(1, seq(
      field('type', $.type),
      field('name', $.identifier),
      '=',
      field('value', $._expression),
    )),

    do_while_statement: $ => seq(
      'do',
      field('body', $._statement),
      'while',
      '(',
      field('condition', $._expression),
      ')',
      ';',
    ),

    // Each slot in for(;;) is a general expression; commas create list expressions
    // Variable declarations use conditional_declaration (already in primary_expression)
    // e.g. for (int i = 0, int e = n; i < e; ++i)
    for_statement: $ => seq(
      'for',
      '(',
      field('init', optional(commaSep1($._expression))),
      ';',
      field('condition', optional(commaSep1($._expression))),
      ';',
      field('update', optional(commaSep1($._expression))),
      ')',
      field('body', $._statement),
    ),

    // foreach supports optional type: foreach int x in (list) { ... }
    // Also supports old-style: foreach my string s in (\l)
    // The content in () can be a single expression or comma-separated list
    foreach_statement: $ => seq(
      'foreach',
      optional('my'),
      optional(field('type', $.type)),
      field('variable', $._declared_name),
      'in',
      '(',
      commaSep1($._expression),
      ')',
      field('body', $._statement),
    ),

    switch_statement: $ => seq(
      'switch',
      '(',
      field('value', $._expression),
      ')',
      '{',
      repeat($.switch_case),
      optional($.default_case),
      '}',
    ),

    switch_case: $ => seq(
      'case',
      field('value', choice(
        $._expression,
        alias($._match_regex, $.regex),
        seq(choice('=~', '!~'), alias($._match_regex, $.regex)),  // case =~ /pattern/:
        $.case_comparison,  // case < 0: case > 0: etc.
      )),
      ':',
      repeat($._statement),
    ),

    // Switch case with comparison operator: case < 0: case >= 1:
    // Note: bare < and > conflict with cast syntax <Type>expr, so they need
    // dynamic precedence to prefer comparison in case context
    case_comparison: $ => prec.dynamic(2, seq(
      field('operator', choice('<', '>', '<=', '>=', '==', '!=')),
      field('value', $._expression),
    )),

    default_case: $ => seq(
      'default',
      ':',
      repeat($._statement),
    ),

    // lib/parser.ypp: TOK_TRY statement_or_block TOK_CATCH '(' myexp ')' statement_or_block
    try_statement: $ => seq(
      'try',
      field('body', $._statement),
      $.catch_clause,
    ),

    // the exception parameter is optional and may be declared with my and a type
    catch_clause: $ => seq(
      'catch',
      '(',
      optional(seq(
        optional('my'),
        optional(field('type', $.type)),
        field('parameter', choice($._declared_name, $.variable_name)),
      )),
      ')',
      field('body', $._statement),
    ),

    return_statement: $ => seq(
      'return',
      optional(choice(
        // return Type name(args); — inline object construction and return
        seq($.type, $.identifier, $.argument_list),
        $._expression,
      )),
      ';',
    ),

    throw_statement: $ => seq(
      'throw',
      field('error', $._expression),
      optional(seq(',', field('description', $._expression),
        optional(seq(',', field('arg', $._expression))))),
      ';',
    ),

    rethrow_statement: $ => seq(
      'rethrow',
      optional(seq(
        field('error', $._expression),
        optional(seq(',', field('description', $._expression),
          optional(seq(',', field('arg', $._expression))))),
      )),
      ';',
    ),

    break_statement: $ => seq('break', ';'),
    continue_statement: $ => seq('continue', ';'),

    thread_exit_statement: $ => seq('thread_exit', ';'),

    // on_exit, on_success, on_error statements
    on_exit_statement: $ => seq(
      choice('on_exit', 'on_success', 'on_error'),
      $._statement,
    ),

    // @debug { ... } — debug conditional block
    // @debug(type var) — debug variable declaration
    debug_statement: $ => choice(
      seq('@debug', $._statement),
      prec.dynamic(3, seq('@debug', '(', optional($.type), $.identifier, ')', ';')),  // @debug(int b0);
    ),

    // @assert(expr) and @assert(expr, msg) — assertion check
    assert_statement: $ => seq(
      '@assert',
      $.argument_list,
      ';',
    ),

    context_statement: $ => seq(
      'context',
      optional(field('name', $.identifier)),
      '(',
      field('expression', $._expression),
      ')',
      optional($.context_modifiers),
      field('body', $._statement),
    ),

    context_modifiers: $ => repeat1(choice(
      $.where_clause,
      $.sortby_clause,
    )),

    where_clause: $ => seq('where', '(', $._expression, ')'),
    sortby_clause: $ => seq(choice('sortBy', 'sortDescendingBy'), '(', $._expression, ')'),

    summarize_statement: $ => seq(
      'summarize',
      '(',
      field('expression', $._expression),
      ')',
      'by',
      '(',
      commaSep1($._expression),
      ')',
      field('body', $._statement),
    ),

    // a read-only local variable requires a type in lib/parser.ypp; without a type or a scope keyword, a name is an
    // expression, as in "data = 1;" or "h.key = 1;"
    local_variable_declaration: $ => prec.dynamic(2, seq(
      optional('const'), field('type', $.type), commaSep1($.variable_declarator),
      ';',
    )),

    // A list of declarations: (type1 var1, type2 var2) = expression;
    // with my, our or thread_local, the assignment is optional, and old-style variables may be declared:
    // our (type1 var1, $var2);
    list_assignment: $ => seq(
      choice(
        seq(
          choice('my', 'our', 'thread_local'),
          '(',
          commaSep1(seq(optional($.type), choice($._declared_name, $.variable_name))),
          ')',
          optional(seq('=', $._expression)),
        ),
        seq('(', commaSep1(seq(optional($.type), $._declared_name)), ')', '=', $._expression),
      ),
      ';',
    ),

    // ==================== Expressions ====================
    _expression: $ => choice(
      $.assignment_expression,
      $.ternary_expression,
      $.binary_expression,
      $.unary_expression,
      $.postfix_expression,
      $.primary_expression,
    ),

    assignment_expression: $ => prec.right(PREC.ASSIGN, seq(
      field('left', $._expression),
      field('operator', choice(
        '=', '+=', '-=', '*=', '/=', '%=',
        '&=', '|=', '^=', '<<=', '>>=',
        ':=',
      )),
      field('right', $._expression),
    )),

    ternary_expression: $ => prec.right(PREC.TERNARY, seq(
      field('condition', $._expression),
      '?',
      field('consequence', $._expression),
      ':',
      field('alternative', $._expression),
    )),

    binary_expression: $ => choice(
      // Null coalescing
      prec.left(PREC.NULL_COALESCE, seq($._expression, choice('??', '?*', '?:'), $._expression)),
      // Logical
      prec.left(PREC.LOGICAL_OR, seq($._expression, choice('||', 'or'), $._expression)),
      prec.left(PREC.LOGICAL_AND, seq($._expression, choice('&&', 'and'), $._expression)),
      // Bitwise
      prec.left(PREC.BITWISE_OR, seq($._expression, '|', $._expression)),
      prec.left(PREC.BITWISE_XOR, seq($._expression, '^', $._expression)),
      prec.left(PREC.BITWISE_AND, seq($._expression, '&', $._expression)),
      // Equality
      prec.left(PREC.EQUALITY, seq($._expression, choice('==', '!=', '<>', '===', '!=='), $._expression)),
      // Regex prefixes are reserved only after a regex operator (or case).
      // In ordinary expressions, m{key}, s{key}, tr{key} and x{key} are hash lookups.
      prec.left(PREC.EQUALITY, seq($._expression, '=~', $.regex)),
      prec.left(PREC.EQUALITY, seq($._expression, '!~', alias($._match_regex, $.regex))),
      // Comparison
      prec.left(PREC.COMPARISON, seq($._expression, choice('<', '>', '<=', '>=', '<=>'), $._expression)),
      // lib/parser.ypp: exp TOK_INSTANCEOF uncqtypedef
      prec.left(PREC.COMPARISON, seq($._expression, 'instanceof', $.type)),
      // Shift
      prec.left(PREC.SHIFT, seq($._expression, choice('<<', '>>'), $._expression)),
      // Arithmetic
      prec.left(PREC.ADD, seq($._expression, choice('+', '-'), $._expression)),
      prec.left(PREC.MULTIPLY, seq($._expression, choice('*', '/', '%'), $._expression)),
      // Range
      prec.left(PREC.ADD, seq($._expression, '..', $._expression)),
    ),

    unary_expression: $ => prec.right(PREC.UNARY, seq(
      field('operator', choice(
        '!', 'not', '~', '-', '+', '\\',
        '++', '--',
        'background',
        'delete', 'remove',
        'exists', 'elements', 'keys',
        'shift', 'pop',
        'chomp', 'trim',
      )),
      field('operand', $._expression),
    )),

    postfix_expression: $ => prec.left(PREC.POSTFIX, seq(
      field('operand', $._expression),
      field('operator', choice('++', '--')),
    )),

    primary_expression: $ => choice(
      $.identifier,
      alias($._soft_identifier, $.identifier),
      $.variable_name,
      $.generic_scoped_identifier,
      $.scoped_identifier,
      // type keywords used as variable names (data, hash, etc.); an alias makes the name visible
      alias($._type_keyword, $.identifier),
      $.literal,
      $.string,
      $.string_concatenation,
      $.list_literal,
      $.paren_hash_literal,
      $.hash_literal,
      $.closure_expression,
      $.call_expression,
      $.member_expression,
      $.index_expression,
      $.slice_expression,
      $.cast_expression,
      $.parenthesized_expression,
      $.implicit_argument,
      $.last_element_expression,
      $.context_reference,
      // Higher-order functions
      $.map_expression,
      $.select_expression,
      $.foldl_expression,
      $.foldr_expression,
      $.find_expression,
      $.iterate_expression,
      $.streaming_terminal_expression,
      $.streaming_limited_expression,
      $.streaming_boundary_expression,
      // Multi-arg operators used as expressions
      $.push_expression,
      $.unshift_expression,
      $.splice_expression,
      $.extract_expression,
      $.new_expression,
      // Conditional declarations can appear in expression context: exists (*string x = expr)
      $.conditional_declaration,
      // Shell command execution: `command`
      $.backquote_expression,
    ),

    // Call expression: function(args) — supports identifiers, scoped names,
    // member access, and type keywords used as cast functions (string(), int(), etc.)
    // Any value can be callable: identifiers, member access, index access,
    // closures, call references, parenthesized expressions, etc.
    call_expression: $ => prec(PREC.CALL, seq(
      field('function', choice(
        $.identifier,
        // A keyword that lib/scanner.lpp returns as a name before '(': select(x)
        // is a call, while select (x), y is the select operator. See
        // design/astparser-keyword-identifiers.md.
        alias($._keyword_identifier, $.identifier),
        $.generic_scoped_identifier,
        $.scoped_identifier,
        $.member_expression,
        $.index_expression,     // factories{name}(), DataSerializationSupport{ct}(body)
        $.call_expression,      // chained calls: f()(x)
        $.parenthesized_expression,  // (closure)()
        $.closure_expression,   // sub() { ... }() — immediate closure invocation
        $.variable_name,        // $func()
        $.implicit_argument,    // $1() — calling implicit arg as closure
        alias($._type_keyword, $.identifier),  // string(), int(), etc.
      )),
      $.argument_list,
    )),

    // Type names used as variable names or cast functions in expressions
    _type_keyword: $ => choice(...TYPE_NAMES),

    // The name of a declaration, which may be a type name, as in "our int;"
    _declared_name: $ => choice(
      $.identifier,
      alias(choice(...TYPE_NAMES), $.identifier),
      alias($._soft_identifier, $.identifier),
    ),

    streaming_keyword_identifier: $ => choice(
      'iterate', 'first', 'any', 'all', 'count',
      'take', 'drop', 'takewhile', 'takeuntil',
    ),

    argument_list: $ => seq(
      '(',
      optional($._argument_list_body),
      ')',
    ),

    _argument_list_body: $ => seq(
      choice(
        commaSep1($.named_argument),
        seq(commaSep1($._expression), optional(seq(',', commaSep1($.named_argument)))),
      ),
      optional(','),
    ),

    named_argument: $ => seq(
      field('name', choice($.identifier, alias($._soft_identifier, $.identifier),
        alias($._type_keyword, $.identifier))),
      ':',
      field('value', $._expression),
    ),

    member_expression: $ => prec.left(PREC.MEMBER, seq(
      field('object', $._expression),
      '.',
      field('member', choice($.identifier, $.streaming_keyword_identifier, $.string, $.implicit_argument,
        $.variable_name)),
    )),

    index_expression: $ => prec.left(PREC.MEMBER, seq(
      field('object', $._expression),
      choice(
        seq('[', field('index', $._expression), ']'),
        // Range subscript: list[start..end], list[start..], list[..end]
        seq('[', optional(field('range_start', $._expression)), '..', optional(field('range_end', $._expression)), ']'),
        seq('{', commaSep1(field('key', $._expression)), optional(','), '}'),  // multi-key hash access
      ),
    )),

    // Hash/object member slice: obj.('key1', 'key2') returns a hash subset
    // A trailing comma is accepted, as in the core grammar's `list: exp ','` rule
    slice_expression: $ => prec.left(PREC.MEMBER, seq(
      field('object', $._expression),
      '.',
      '(',
      commaSep1($._expression),
      optional(','),
      ')',
    )),

    // Qore cast: cast<Type>(expr) or typed hash creation: <Type>expr
    cast_expression: $ => choice(
      seq('cast', '<', field('type', $.type), '>', '(', field('value', $._expression), ')'),
      seq('<', field('type', $.type), '>', field('value', $._expression)),
    ),

    // Multi-argument operators: push list, value; unshift list, value; splice list, offset, ...
    push_expression: $ => prec.right(seq(
      'push',
      field('list', $._expression),
      ',',
      field('value', $._expression),
    )),

    unshift_expression: $ => prec.right(seq(
      'unshift',
      field('list', $._expression),
      ',',
      field('value', $._expression),
    )),

    splice_expression: $ => prec.right(seq(
      'splice',
      field('list', $._expression),
      optional(seq(',', field('offset', $._expression),
        optional(seq(',', field('length', $._expression),
          optional(seq(',', field('replacement', $._expression))))))),
    )),

    // new expression: new Type(args) — parentheses are required in Qore
    new_expression: $ => prec.right(PREC.UNARY, seq(
      'new',
      field('type', $.type),
      $.argument_list,
    )),

    // extract var, offset, length — returns extracted portion, modifies var
    extract_expression: $ => prec.right(seq(
      'extract',
      field('list', $._expression),
      optional(seq(',', field('offset', $._expression),
        optional(seq(',', field('length', $._expression))))),
    )),

    parenthesized_expression: $ => seq('(', $._expression, ')'),

    closure_expression: $ => choice(
      // lib/scanner.lpp reads a return type such as hash<auto> before "sub" as a type, not as comparisons
      prec.dynamic(1, seq(
        field('return_type', $.type),
        'sub',
        $.parameter_list,
        $.block,
      )),
      seq(
        'sub',
        $.parameter_list,
        $.block,
      ),
    ),

    // Higher-order functions
    // map expr, list[, filter] — Qore's LALR grammar uses `list: exp ','` to
    // support trailing commas globally.  In the tree-sitter grammar we handle
    // this locally: a trailing comma alternative (prec.dynamic -2) is only
    // chosen when the enclosing context cannot consume the comma itself
    // (e.g. inside a ternary expression).
    map_expression: $ => seq(
      'map',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
      // as in lib/parser.ypp, the operator takes all of the following comma-separated expressions
      optional(choice(
        prec.dynamic(1, seq(',', field('filter', $._expression), optional(','))),
        ',',
      )),
    ),

    select_expression: $ => seq(
      'select',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
      // as in lib/parser.ypp, the operator takes all of the following comma-separated expressions
      optional(choice(
        prec.dynamic(1, seq(',', field('filter', $._expression), optional(','))),
        ',',
      )),
    ),

    foldl_expression: $ => prec.right(seq(
      'foldl',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
      optional(prec.dynamic(-2, ',')),
    )),

    foldr_expression: $ => prec.right(seq(
      'foldr',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
      optional(prec.dynamic(-2, ',')),
    )),

    find_expression: $ => prec.right(PREC.UNARY, choice(
      seq(
        'find',
        field('mode', alias($._find_modifier, $.find_modifier)),
        field('value', $._expression),
        'in',
        field('source', $._expression),
        'where',
        '(',
        field('condition', $._expression),
        ')',
      ),
      seq(
        'find',
        field('value', $._expression),
        'in',
        field('source', $._expression),
        'where',
        '(',
        field('condition', $._expression),
        ')',
      ),
    )),

    iterate_expression: $ => prec.right(PREC.UNARY, seq(
      'iterate',
      field('source', $._expression),
    )),

    streaming_terminal_expression: $ => prec.right(seq(
      field('operator', choice('first', 'any', 'all', 'count')),
      field('expression', $._expression),
      // as in lib/parser.ypp, the operator takes all of the following comma-separated expressions
      optional(choice(
        prec.dynamic(1, seq(',', field('source', $._expression), optional(','))),
        ',',
      )),
    )),

    streaming_limited_expression: $ => prec.right(seq(
      field('operator', choice('take', 'drop')),
      field('count', $._expression),
      ',',
      field('source', $._expression),
      optional(prec.dynamic(-2, ',')),
    )),

    streaming_boundary_expression: $ => prec.right(seq(
      field('operator', choice('takewhile', 'takeuntil')),
      field('predicate', $._expression),
      ',',
      field('source', $._expression),
      optional(prec.dynamic(-2, ',')),
    )),

    implicit_argument: $ => /\$\d+/,

    // $# — last element index (e.g., list[$#])
    // Needs to be a single token so the # isn't consumed as a comment start
    last_element_expression: $ => token(prec(2, '$#')),

    // Implicit string concatenation: adjacent string literals are concatenated
    // e.g., "hello " "world" → "hello world"
    string_concatenation: $ => prec.left(seq(
      choice($.string, $.string_concatenation),
      $.string,
    )),

    context_reference: $ => choice(
      seq('%', $.identifier),
      '%%',  // current row reference in context/summarize
    ),

    // ==================== Literals ====================
    literal: $ => choice(
      $.relative_date,  // Must be before integer/float to get longest match
      $.integer,
      $.float,
      $.number,
      $.boolean,
      $.null,
      $.nothing,
      $.date,
      $.time,
      $.binary,
      $.char_literal,
      $.special_float,
    ),

    integer: $ => token(choice(
      /[0-9]+/,
      /0x[0-9a-fA-F]+/,
      /0o[0-7]+/,
      /0b[01]+/,
    )),

    float: $ => token(choice(
      /[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?/,
      /[0-9]+[eE][+-]?[0-9]+/,
    )),

    number: $ => token(choice(
      /[0-9]+(\.[0-9]+)?n/,
      /[0-9]+[eE][+-]?[0-9]+n/,
      /[0-9]+\.[0-9]+[eE][+-]?[0-9]+n/,
    )),

    // Relative date literals: 30s, 5m, 1h, 500ms, 100us, P1DT2H3M
    relative_date: $ => token(prec(1, choice(
      // Integer/float with time suffix
      seq(/[0-9]+(\.[0-9]+)?/, choice('ms', 'us', 's', 'm', 'h', 'D', 'M', 'Y')),
      // ISO 8601 duration: P[nY][nM][nD][T[nH][nM][nS]]
      // Requires at least one digit to avoid matching identifiers like PS_ABORTED
      /PT?[0-9][0-9YMWDTHS.]*/,
    ))),

    boolean: $ => choice('True', 'False'),

    null: $ => 'NULL',
    nothing: $ => 'NOTHING',

    date: $ => token(seq(
      /[0-9]{4}-[0-9]{2}-[0-9]{2}/,
      optional(seq(
        /[T ]/,
        /[0-9]{2}:[0-9]{2}(:[0-9]{2}(\.[0-9]+)?)?/,
      )),
      // Timezone applies to both date-only and date+time forms
      // (Qore scanner: {YEAR}-{MONTH}-{DAY}?{TZ}?)
      optional(choice(
        /[+-][0-9]{2}(:?[0-9]{2}(:?[0-9]{2})?)?/,  // timezone offset: +HH, +HHMM, +HH:MM, etc.
        'Z',                                          // UTC timezone
      )),
    )),

    // Standalone time literal: HH:MM:SS with optional fractional seconds and timezone
    // e.g., 10:20:30, 10:20:30Z, 10:20:30.123+01:00
    time: $ => token(seq(
      /[0-9]{1,2}:[0-9]{2}:[0-9]{2}/,
      optional(/\.[0-9]+/),
      optional(choice(
        /[+-][0-9]{2}(:?[0-9]{2})?/,
        'Z',
      )),
    )),

    // IEEE 754 special float constants
    special_float: $ => token(choice('@inf@', '@nan@')),

    binary: $ => token(/<[0-9a-fA-F]*>/),

    char_literal: $ => token(seq(
      'c',
      choice(
        seq("'", /([^'\\]|\\.)*/, "'"),
        seq('"', /([^"\\]|\\.)*/, '"'),
      ),
    )),

    // ==================== Strings ====================
    string: $ => choice(
      $.single_quoted_string,
      $.double_quoted_string,
    ),

    // Single-quoted strings: only \\ is a recognized escape sequence.
    // A lone \ (not followed by another \ or closing ') is consumed with the next char.
    // A lone \ before closing ' ends with just the \ as literal content.
    // Qore does NOT support \' in single-quoted strings.
    single_quoted_string: $ => seq(
      "'",
      repeat(choice(
        token.immediate('\\\\'),      // \\ → literal backslash
        token.immediate(/[^'\\]+/),   // any non-quote, non-backslash chars
        token.immediate(/\\[^'\\]/),  // \ + non-quote, non-backslash: literal
        token.immediate(/\\/),        // lone \ (before closing '): literal
      )),
      "'",
    ),

    double_quoted_string: $ => seq(
      '"',
      repeat(choice(
        $.escape_sequence,
        $.string_interpolation,
        token.immediate(prec(1, /[^"\\$]+/)),
        token.immediate(prec(1, '$')),  // bare $ not followed by identifier/{/(
      )),
      '"',
    ),

    // Escape sequences for double-quoted strings (full set)
    escape_sequence: $ => token.immediate(seq(
      '\\',
      choice(
        /x[0-9a-fA-F]{1,2}/,
        /u\{[0-9a-fA-F]{1,6}\}/,
        /u[0-9a-fA-F]{4}/,
        /U[0-9a-fA-F]{8}/,
        /[0-7]{1,3}/,  // octal escape
        /[^xu0-7]/,    // any other char: \n, \t, \a, \e, \,, etc.
      ),
    )),

    string_interpolation: $ => seq(
      token.immediate('$'),
      choice(
        $.identifier,
        seq('{', $._expression, '}'),
        seq('(', $._expression, ')'),
      ),
    ),

    // ==================== Collections ====================
    // Note: Parse directives are allowed before and after elements.
    list_literal: $ => seq(
      '(',
      optional(seq(
        repeat($.parse_directive),
        optional(seq(
          $._expression,
          choice(
            // a single element is a list only with a trailing comma, as (x)
            // is a parenthesized expression
            seq(',', repeat($.parse_directive)),
            seq(
              repeat1(seq(
                ',',
                repeat($.parse_directive),
                $._expression,
              )),
              optional(','),
              repeat($.parse_directive),
            ),
          ),
        )),
      )),
      ')',
    ),

    paren_hash_literal: $ => seq(
      '(',
      optional(seq(
        repeat($.parse_directive),
        optional(seq(
          $.hash_entry,
          repeat(seq(
            ',',
            repeat($.parse_directive),
            $.hash_entry,
          )),
          optional(','),
          repeat($.parse_directive),
        )),
      )),
      ')',
    ),

    hash_literal: $ => seq(
      '{',
      optional(seq(
        repeat($.parse_directive),
        optional(seq(
          $.hash_entry,
          repeat(seq(
            ',',
            repeat($.parse_directive),
            $.hash_entry,
          )),
          optional(','),
          repeat($.parse_directive),
        )),
      )),
      '}',
    ),

    hash_entry: $ => seq(
      field('key', $._expression),
      ':',
      field('value', $._expression),
    ),

    // ==================== Regex ====================
    _match_regex: $ => seq($.regex_literal),

    regex: $ => choice(
      $.regex_literal,
      $.regex_subst,
      $.regex_trans,
      $.regex_extract,
    ),

    // lib/scanner.lpp reads line breaks in slash-delimited expressions as part of the expression
    regex_literal: $ => choice(token(prec(-1, seq(
      '/',
      /([^\/\\]|\\.)*/,
      '/',
      optional(/[gimxsun]+/),
    ))), seq('m{', $._brace_regex_match)),

    regex_subst: $ => choice(token(seq(
      's/',
      /([^\/\\]|\\.)*/,
      '/',
      /([^\/\\]|\\.)*/,
      '/',
      optional(/[gimxsun]+/),
    )), seq('s{', $._brace_regex_subst)),

    regex_trans: $ => choice(token(seq(
      'tr/',
      /([^\/\\]|\\.)*/,
      '/',
      /([^\/\\]|\\.)*/,
      '/',
    )), seq('tr{', $._brace_regex_trans)),

    regex_extract: $ => choice(token(seq(
      'x/',
      /([^\/\\]|\\.)*/,
      '/',
      optional(/[gimxsun]+/),
    )), seq('x{', $._brace_regex_extract)),

    // Each brace prefix is a reserved lexical token. The scanner consumes the
    // balanced body, remaining parts and modifiers as one token, so comments
    // and whitespace within the expression cannot become grammar extras.

    // Backquote (shell command) expression: `command`
    // Returns the stdout of the command as a string
    backquote_expression: $ => token(seq(
      '`',
      /[^`]*/,
      '`',
    )),

    // ==================== Types ====================
    type: $ => prec.right(seq(
      choice(
        $.generic_type,
        $.simple_type,
        $.complex_type,
        $.nullable_type,
        $.wildcard_type,
      ),
      optional('!'),  // non-null type modifier: hash<auto!> etc.
    )),

    simple_type: $ => choice(
      'int',
      'float',
      'number',
      'bool',
      'char',
      'string',
      'date',
      'binary',
      'hash',
      'list',
      'object',
      'code',
      'reference',
      'nothing',
      'any',
      'auto',
      'data',
      'softint',
      'softfloat',
      'softnumber',
      'softbool',
      'softchar',
      'softstring',
      'softdate',
      'softlist',
      'timeout',
      $.identifier,
      $.scoped_identifier,
    ),

    complex_type: $ => choice(
      // hash<type> or hash<key_type, value_type>
      seq(
        'hash',
        '<',
        $.type,
        optional(seq(',', $.type)),
        '>',
      ),
      // list<type>, softlist<type>, enum<type>, reference<type>
      seq(
        choice('list', 'softlist', 'enum', 'reference'),
        '<',
        $.type,
        '>',
      ),
      // object<ClassType>
      seq(
        'object',
        '<',
        $.type,
        '>',
      ),
      // date<absolute> or date<relative>
      seq(
        'date',
        '<',
        choice('absolute', 'relative'),
        '>',
      ),
      // union<type1, type2, ...>
      seq(
        'union',
        '<',
        commaSep1($.type),
        '>',
      ),
      // code<return_type(param_types...)> or code<return_type()>
      seq(
        'code',
        '<',
        $.type,  // return type
        '(',
        optional($.code_param_types),
        ')',
        '>',
      ),
    ),

    generic_type: $ => seq(
      field('base', choice($.identifier, $.scoped_identifier)),
      choice(
        seq('<', commaSep1($.type), '>'),
        // lib/scanner.lpp: TYPE_ARGS may be empty, which uses the defaults of all type parameters
        seq('<', '>'),
        '<>',
      ),
    ),

    // lib/scanner.lpp matches the longest word<args>::word<args> sequence as
    // one scoped name, so each generic component takes precedence over a
    // comparison chain such as (a < b) > ::c(d), which requires whitespace
    // before the separator
    generic_scoped_identifier: $ => prec.left(seq(
      prec.dynamic(1, $.generic_type),
      repeat1(seq(token.immediate('::'), choice($.identifier, prec.dynamic(1, $.generic_type)))),
    )),

    wildcard_type: $ => seq(
      '?',
      optional(choice(
        seq('extends', field('bound', $.type)),
        seq('super', field('bound', $.type)),
      )),
    ),

    // Parameter types for code<> signature, supporting varargs
    code_param_types: $ => choice(
      // Just varargs: code<int(...)>
      '...',
      // Types optionally followed by varargs: code<int(string, ...)>
      seq(
        commaSep1($.type),
        optional(seq(',', '...')),
      ),
    ),

    nullable_type: $ => seq('*', $.type),

    // ==================== Modifiers ====================
    modifiers: $ => repeat1($.modifier),

    modifier: $ => choice(
      $.access_modifier,
      'abstract',
      'final',
      'static',
      'synchronized',
      'deprecated',
      'transient',
    ),

    access_modifier: $ => choice(
      'public',
      'private',
      'private:internal',
      'private:hierarchy',
    ),

    // ==================== Identifiers ====================
    // identifier is a rule rather than a token so that error recovery can insert a missing name: tree-sitter
    // inserts a missing token only if the next token then reduces a rule
    identifier: $ => $._identifier_token,

    _identifier_token: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    // lib/scanner.lpp matches a variable reference as one VAR_REF token: \${WORD}
    variable_name: $ => seq('$', token.immediate(/[a-zA-Z_][a-zA-Z0-9_]*/)),

    // lib/scanner.lpp matches a scoped name as one SCOPED_REF token:
    // ({WORD}::)+{WORD} or (::{WORD})+. A separator after a name is therefore
    // immediate, while a leading separator may follow whitespace, so `A ::b`
    // is the name `A` followed by the root namespace name `::b`, as in
    // `our Type ::name`.
    scoped_identifier: $ => choice(
      seq('::', $.identifier, repeat(seq(token.immediate('::'), $.identifier))),
      seq($.identifier, repeat1(seq(token.immediate('::'), $.identifier))),
    ),

    // ==================== Comments ====================
    comment: $ => token(seq(
      '/*',
      /[^*]*\*+([^/*][^*]*\*+)*/,
      '/',
    )),

    // NOTE: comment and regex_literal both match /* ... */ at the same length
    // (19 bytes). Without explicit precedence, rule order breaks the tie.
    // regex_literal has prec(-1) so comment wins the tiebreak.

    line_comment: $ => token(seq('#', /.*/)),

    newline: $ => /\r?\n/,
  },
});

/**
 * Creates a rule for comma-separated items with at least one item.
 */
function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

/**
 * Creates the rule for a variable declarator with the given name rule.
 */
function variableDeclarator($, name) {
  return choice(
    seq(
      field('name', $.variable_name),
      optional(seq(choice('=', '+=', ':='), field('value', $._expression))),
    ),
    // Object construction: identifier(args) or just identifier
    // Also: hash member init: hash sd.type = 'event'
    seq(
      field('name', name),
      optional(choice(
        seq(choice('=', '+=', ':='), field('value', $._expression)),
        $.argument_list,  // Constructor arguments
        // Hash member init via dot notation: hash sd.member = value
        // Also supports dynamic member: hash rv.(expr) = value
        // Also supports string keys: hash res."DAV:href" = value
        seq(repeat1(seq('.', choice($.identifier, $.string, seq('(', $._expression, ')')))), '=',
          field('value', $._expression)),
      )),
    ),
  );
}

/**
 * Creates a rule for comma-separated items (zero or more).
 */
function commaSep(rule) {
  return optional(commaSep1(rule));
}
