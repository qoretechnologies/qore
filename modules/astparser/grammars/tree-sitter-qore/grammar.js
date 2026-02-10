/**
 * @file Qore grammar for tree-sitter
 * @author Qore Technologies
 * @license LGPL-2.1
 * @see {@link https://qore.org|Qore Programming Language}
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

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

  extras: $ => [
    $.comment,
    $.line_comment,
    $.conditional_directive,
    /\s/,
  ],

  conflicts: $ => [
    [$.hash_literal, $.block],
    [$.hash_literal, $._statement],
    [$.variable_declarator, $.primary_expression],
    [$.module_name, $.scoped_identifier],
    [$.parenthesized_expression, $.list_literal],
    [$.list_literal, $.paren_hash_literal],
    [$.function_declaration, $.closure_expression],
    [$.function_declaration, $.simple_type],
    [$.function_declaration, $.simple_type, $.scoped_identifier],
    [$.argument_list, $.parameter_list],
    [$.parameter, $.primary_expression],
    [$._statement, $._top_level_item],
    [$.module_spec],
    // list_assignment conflicts with expressions starting with '('
    [$.list_assignment, $.primary_expression],
    // foreach with optional type conflicts with local_variable_declaration
    [$.foreach_statement, $.local_variable_declaration],
    // call_expression with any expression as callable
    [$.call_expression, $.simple_type],
    // member declaration with constructor args conflicts with method declaration
    [$.member_declaration, $.method_declaration],
    // conditional declaration in if/while conflicts with expressions
    [$.conditional_declaration, $.primary_expression],
    // type keywords as expressions conflict with type usage
    [$._type_keyword, $.complex_type],
    [$._type_keyword, $.simple_type],
    [$.simple_type, $.complex_type],
    [$.simple_type, $.scoped_identifier],
    [$.scoped_identifier],
    [$._type_keyword, $.simple_type, $.complex_type],
    // case < identifier could be comparison value or start of <Type> cast
    [$.primary_expression, $.simple_type],
    // conditional_declaration in if/while vs as primary_expression
    [$.if_statement, $.primary_expression],
    [$.while_statement, $.primary_expression],
    // conditional_declaration vs local_variable_declaration
    [$.variable_declarator, $.conditional_declaration],
    [$.parameter, $.conditional_declaration],
    // @debug(type var) vs @debug (expression) and list_assignment
    [$.debug_statement, $.list_assignment, $.primary_expression],
    // typedef ambiguity: `typedef X = type` vs `typedef type name`
    [$.typedef_declaration, $.simple_type],
    [$.typedef_declaration, $.simple_type, $.scoped_identifier],
    // map/select optional filter comma vs enclosing comma (hash/list trailing comma)
    [$.map_expression],
    [$.select_expression],
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
      'module',
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
        seq('%requires', $.module_spec),
        seq('%requires', '(', 'reexport', ')', $.module_spec),
        seq('%try-module', $.module_spec),
        '%endtry',
        // Define directive for conditional compilation
        seq('%define', $.identifier),
        // Other common directives
        '%strict-args',
        '%allow-weak-references',
        '%no-global-vars',
        '%no-child-restrictions',
        '%no-typedef',
        '%no-enum',
        '%no-transient',
        '%no-new',
        '%lockdown',
        '%exec-class',
        '%modern',
        seq('%append-include-path', $.string),
        seq('%append-module-path', $.string),
        seq('%include', $.string),
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
    )),

    module_name: $ => choice(
      $.identifier,
      $.scoped_identifier,
    ),

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
      'class',
      field('name', choice($.identifier, $.scoped_identifier)),
      optional($.superclass_list),
      choice(
        seq('{', repeat($._class_item), '}'),
        ';',  // forward declaration
      ),
    ),

    superclass_list: $ => seq(
      'inherits',
      commaSep1($.superclass),
    ),

    superclass: $ => seq(
      optional($.access_modifier),
      choice($.scoped_identifier, $.identifier),
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
      repeat(choice($.member_declaration, $.parse_directive)),
      '}',
    ),

    member_declaration: $ => seq(
      optional($.modifiers),
      field('type', optional($.type)),
      field('name', $.identifier),
      optional(choice(
        seq('=', field('default', $._expression)),
        $.argument_list,  // constructor arguments: static Mutex m();
      )),
      ';',
    ),

    method_declaration: $ => prec.dynamic(1, seq(
      optional($.modifiers),
      optional(field('return_type', $.type)),
      field('name', $.identifier),
      $.parameter_list,
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
      optional(choice(
        seq(commaSep1($.parameter), optional(seq(',', '...'))),
        '...',
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
    function_declaration: $ => seq(
      optional($.modifiers),
      optional(field('return_type', $.type)),
      choice('sub', $.identifier, $.scoped_identifier),
      field('name', optional(choice($.identifier, $.scoped_identifier))),
      $.parameter_list,
      optional(seq('returns', field('returns', $.type))),
      choice(
        $.block,
        ';',
      ),
    ),

    parameter_list: $ => seq(
      '(',
      optional(choice(
        seq(commaSep1($.parameter), optional(seq(',', '...'))),
        '...',
      )),
      ')',
    ),

    parameter: $ => seq(
      optional($.modifiers),
      optional(field('type', $.type)),
      field('name', $.identifier),
      optional(seq('=', field('default', $._expression))),
    ),

    // ==================== Constants and Variables ====================
    constant_declaration: $ => seq(
      optional($.modifiers),
      'const',
      field('name', choice($.identifier, $.scoped_identifier)),
      '=',
      field('value', $._expression),
      ';',
    ),

    // 'our' / 'my' / 'thread_local' declarations (old-style and global scope)
    global_variable_declaration: $ => seq(
      choice('our', 'my', 'thread_local'),
      optional(field('type', $.type)),
      commaSep1($.variable_declarator),
      ';',
    ),

    variable_declarator: $ => choice(
      seq(
        field('name', $.variable_name),
        optional(seq(choice('=', '+='), field('value', $._expression))),
      ),
      // Object construction: identifier(args) or just identifier
      // Also: hash member init: hash sd.type = 'event'
      seq(
        field('name', $.identifier),
        optional(choice(
          seq(choice('=', '+='), field('value', $._expression)),
          $.argument_list,  // Constructor arguments
          // Hash member init via dot notation: hash sd.member = value
          // Also supports dynamic member: hash rv.(expr) = value
          seq(repeat1(seq('.', choice($.identifier, seq('(', $._expression, ')')))), '=', field('value', $._expression)),
        )),
      ),
    ),

    // ==================== Hashdecl ====================
    hashdecl_declaration: $ => seq(
      optional($.modifiers),
      'hashdecl',
      field('name', choice($.identifier, $.scoped_identifier)),
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
      $._expression,
      ';',
    ),

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
      field('variable', $.identifier),
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
        seq(choice('=~', '!~'), $.regex),  // case =~ /pattern/:
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

    try_statement: $ => seq(
      'try',
      field('body', $.block),
      repeat1($.catch_clause),
    ),

    catch_clause: $ => seq(
      'catch',
      '(',
      optional(field('type', $.type)),
      field('parameter', $.identifier),
      ')',
      field('body', $.block),
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

    local_variable_declaration: $ => prec.dynamic(2, seq(
      optional(field('type', $.type)),
      commaSep1($.variable_declarator),
      ';',
    )),

    // List destructuring assignment: (type1 var1, type2 var2) = expression;
    // Also: my (type1 var1, type2 var2) = expression;
    list_assignment: $ => seq(
      optional('my'),
      '(',
      commaSep1(seq(optional($.type), $.identifier)),
      ')',
      '=',
      $._expression,
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
      prec.left(PREC.NULL_COALESCE, seq($._expression, choice('??', '?*'), $._expression)),
      // Logical
      prec.left(PREC.LOGICAL_OR, seq($._expression, choice('||', 'or'), $._expression)),
      prec.left(PREC.LOGICAL_AND, seq($._expression, choice('&&', 'and'), $._expression)),
      // Bitwise
      prec.left(PREC.BITWISE_OR, seq($._expression, '|', $._expression)),
      prec.left(PREC.BITWISE_XOR, seq($._expression, '^', $._expression)),
      prec.left(PREC.BITWISE_AND, seq($._expression, '&', $._expression)),
      // Equality
      prec.left(PREC.EQUALITY, seq($._expression, choice('==', '!=', '===', '!==', '=~', '!~'), $._expression)),
      // Comparison
      prec.left(PREC.COMPARISON, seq($._expression, choice('<', '>', '<=', '>=', '<=>'), $._expression)),
      prec.left(PREC.COMPARISON, seq($._expression, 'instanceof', choice($.type, $._expression))),
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
      $.variable_name,
      $.scoped_identifier,
      $._type_keyword,  // type keywords used as variable names (data, hash, etc.)
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
      $.regex,
      // Higher-order functions
      $.map_expression,
      $.select_expression,
      $.foldl_expression,
      $.foldr_expression,
      // Multi-arg operators used as expressions
      $.push_expression,
      $.unshift_expression,
      $.splice_expression,
      $.extract_expression,
      $.new_expression,
      // Conditional declarations can appear in expression context: exists (*string x = expr)
      $.conditional_declaration,
    ),

    // Call expression: function(args) — supports identifiers, scoped names,
    // member access, and type keywords used as cast functions (string(), int(), etc.)
    // Any value can be callable: identifiers, member access, index access,
    // closures, call references, parenthesized expressions, etc.
    call_expression: $ => prec(PREC.CALL, seq(
      field('function', choice(
        $.identifier,
        $.scoped_identifier,
        $.member_expression,
        $.index_expression,     // factories{name}(), DataSerializationSupport{ct}(body)
        $.call_expression,      // chained calls: f()(x)
        $.parenthesized_expression,  // (closure)()
        $.variable_name,        // $func()
        $.implicit_argument,    // $1() — calling implicit arg as closure
        $._type_keyword,        // string(), int(), etc.
      )),
      $.argument_list,
    )),

    // Type keywords that can also be used as variable names or cast functions.
    // All simple type keywords are included so they can appear in expression context.
    _type_keyword: $ => choice(
      'string', 'int', 'float', 'number', 'bool', 'binary', 'date',
      'list', 'hash', 'softint', 'softfloat', 'softnumber', 'softbool',
      'softstring', 'softdate', 'softlist', 'timeout',
      'object', 'code', 'reference', 'nothing', 'any', 'auto', 'data',
    ),

    argument_list: $ => seq(
      '(',
      optional(seq(commaSep1($._expression), optional(','))),
      ')',
    ),

    member_expression: $ => prec.left(PREC.MEMBER, seq(
      field('object', $._expression),
      '.',
      field('member', choice($.identifier, $.string, $.implicit_argument, $.variable_name)),
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
    slice_expression: $ => prec.left(PREC.MEMBER, seq(
      field('object', $._expression),
      '.',
      '(',
      commaSep1($._expression),
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

    // new expression: new Type(args) — supports complex types
    new_expression: $ => prec.right(PREC.UNARY, seq(
      'new',
      field('type', $.type),
      optional($.argument_list),
    )),

    // extract var, offset, length — returns extracted portion, modifies var
    extract_expression: $ => prec.right(seq(
      'extract',
      field('list', $._expression),
      optional(seq(',', field('offset', $._expression),
        optional(seq(',', field('length', $._expression))))),
    )),

    parenthesized_expression: $ => seq('(', $._expression, ')'),

    closure_expression: $ => seq(
      optional(field('return_type', $.type)),
      'sub',
      $.parameter_list,
      $.block,
    ),

    // Higher-order functions
    // map expr, list[, filter] — no prec.right so that trailing commas
    // in enclosing hash/list literals are correctly handled
    map_expression: $ => seq(
      'map',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
      optional(prec.dynamic(-1, seq(',', field('filter', $._expression)))),
    ),

    select_expression: $ => seq(
      'select',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
      optional(prec.dynamic(-1, seq(',', field('filter', $._expression)))),
    ),

    foldl_expression: $ => prec.right(seq(
      'foldl',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
    )),

    foldr_expression: $ => prec.right(seq(
      'foldr',
      field('expression', $._expression),
      ',',
      field('list', $._expression),
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
      $.binary,
    ),

    integer: $ => token(choice(
      /[0-9]+/,
      /0x[0-9a-fA-F]+/,
      /0o[0-7]+/,
      /0b[01]+/,
    )),

    float: $ => token(/[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?/),

    number: $ => token(/[0-9]+(\.[0-9]+)?n/),

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
        /[0-9]{2}:[0-9]{2}:[0-9]{2}/,
        optional(/\.[0-9]+/),
        optional(choice(
          /[+-][0-9]{2}:?[0-9]{2}/,  // timezone offset
          'Z',                        // UTC timezone
        )),
      )),
    )),

    binary: $ => token(/<[0-9a-fA-F]*>/),

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
        /u[0-9a-fA-F]{4}/,
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
          repeat(seq(
            ',',
            repeat($.parse_directive),
            $._expression,
          )),
          optional(','),
          repeat($.parse_directive),
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
      field('key', $._hash_key),
      ':',
      field('value', $._expression),
    ),

    // Hash keys are more restricted than general expressions to avoid
    // ambiguity with ternary operator (a ? b : c vs hash key : value)
    _hash_key: $ => choice(
      $.string,
      $.identifier,
      $.variable_name,
      $.scoped_identifier,
      $.implicit_argument,  // $1, $2, etc. for map expressions
      $.member_expression,  // $1.key, obj.field for map expressions
      $.index_expression,   // Map{$1.key} etc. as computed key
      $.call_expression,    // sprintf(...) etc. as computed key
      seq('(', $._expression, ')'),  // computed key in parentheses
    ),

    // ==================== Regex ====================
    regex: $ => choice(
      $.regex_literal,
      $.regex_subst,
      $.regex_trans,
      $.regex_extract,
    ),

    regex_literal: $ => token(prec(-1, seq(
      '/',
      /([^\/\n\\]|\\.)*/,   // pattern
      '/',
      optional(/[gimxsun]+/),  // flags
    ))),

    regex_subst: $ => token(seq(
      's/',
      /([^\/\n\\]|\\.)*/,   // pattern
      '/',
      /([^\/\n\\]|\\.)*/,   // replacement
      '/',
      optional(/[gimxsun]+/),  // flags
    )),

    regex_trans: $ => token(seq(
      'tr/',
      /([^\/\n\\]|\\.)*/,   // pattern
      '/',
      /([^\/\n\\]|\\.)*/,   // replacement
      '/',
    )),

    // Regex extract: x/pattern/flags — returns list of captured groups
    regex_extract: $ => token(seq(
      'x/',
      /([^\/\n\\]|\\.)*/,   // pattern
      '/',
      optional(/[gimxsun]+/),  // flags
    )),

    // regex_pattern, regex_replacement, regex_flags are inlined into
    // the regex token() rules above to prevent extras (comments) from
    // being inserted between the / delimiters.

    // ==================== Types ====================
    type: $ => prec.right(seq(
      choice(
        $.simple_type,
        $.complex_type,
        $.nullable_type,
      ),
      optional('!'),  // non-null type modifier: hash<auto!> etc.
    )),

    simple_type: $ => choice(
      'int',
      'float',
      'number',
      'bool',
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
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    variable_name: $ => seq('$', /[a-zA-Z_][a-zA-Z0-9_]*/),

    scoped_identifier: $ => prec.left(seq(
      optional('::'),
      $.identifier,
      repeat1(seq('::', $.identifier)),
    )),

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
 * Creates a rule for comma-separated items (zero or more).
 */
function commaSep(rule) {
  return optional(commaSep1(rule));
}
