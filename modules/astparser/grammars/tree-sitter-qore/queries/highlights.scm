; Syntax highlighting queries for Qore
;
; Copyright (C) 2026 Qore Technologies, s.r.o.
;
; Capture names follow the common tree-sitter conventions (as used by Neovim, Helix and Zed), so editors can use
; this file directly.  The astparser module embeds it for AstTree::getHighlights(), which Qdx uses to highlight the
; Qore code in the HTML documentation that doxygen generates.
;
; Precedence: when several patterns capture the same node, the FIRST pattern in this file wins, as in
; tree-sitter-highlight; a capture of a node inside a captured node takes precedence over the outer capture.  Patterns
; for tokens that have another meaning in a particular context (type names that are also operators, such as "any",
; or keywords used as member names, such as ".first") therefore come before the general keyword patterns.
;
; Predicates supported by astparser: #eq?, #not-eq?, #any-of?, #not-any-of?, #match? and #not-match?

; ==================== Comments and directives ====================

((comment) @comment.documentation
  (#match? @comment.documentation "^/\\*\\*[^/]"))
((line_comment) @comment.documentation
  (#match? @comment.documentation "^#!"))
[(comment) (line_comment)] @comment

[(parse_directive) (conditional_directive)] @keyword.directive

; ==================== Types ====================

(simple_type
  ["int" "float" "number" "bool" "char" "string" "date" "binary" "hash" "list" "object" "code" "reference"
   "nothing" "any" "auto" "data" "softint" "softfloat" "softnumber" "softbool" "softchar" "softstring"
   "softdate" "softlist" "timeout"] @type.builtin)

(complex_type
  ["hash" "list" "softlist" "enum" "reference" "object" "date" "absolute" "relative" "union" "code"]
  @type.builtin)

(simple_type [(identifier) (scoped_identifier)] @type)
(generic_type base: _ @type)
(nullable_type "*" @type.builtin)
(type "!" @type.builtin)
(wildcard_type ["?" "extends" "super"] @type.builtin)

(class_declaration name: _ @type)
(hashdecl_declaration name: _ @type)
(typedef_declaration name: _ @type)
(enum_declaration name: _ @type)
(namespace_declaration name: _ @module)
(superclass [(identifier) (scoped_identifier)] @type)
(type_parameter name: (identifier) @type)
(module_declaration name: (identifier) @module)

; a type name called as a function converts its argument: string(x), int(x)
(call_expression
  function: (identifier) @type.builtin
  (#any-of? @type.builtin
    "int" "float" "number" "bool" "string" "date" "binary" "hash" "list" "softint" "softfloat" "softnumber"
    "softbool" "softstring" "softdate" "softlist"))

; ==================== Keywords used as names ====================

; streaming operator names used as members, as in "x.first"
(streaming_keyword_identifier
  ["iterate" "first" "any" "all" "count" "take" "drop" "takewhile" "takeuntil"] @property)

; ==================== Literals ====================

[(integer) (float) (number) (binary)] @number
(special_float) @number.float
[(relative_date) (date) (time)] @number.date
[(boolean) (null) (nothing)] @constant.builtin
(char_literal) @character

(escape_sequence) @string.escape
(string) @string
(regex) @string.regexp
(backquote_expression) @string.special

; ==================== Variables ====================

[(implicit_argument) (last_element_expression) (context_reference)] @variable.builtin
((identifier) @variable.builtin
  (#eq? @variable.builtin "self"))
(variable_name) @variable

(parameter name: (identifier) @variable.parameter)
(named_argument name: (identifier) @variable.parameter)

; ==================== Functions ====================

(function_declaration name: (identifier) @function)
(function_declaration name: (scoped_identifier (identifier) @function .))
(method_declaration name: (identifier) @function.method)
(call_expression function: (identifier) @function.call)
(call_expression function: (scoped_identifier (identifier) @function.call .))
(call_expression function: (member_expression member: (identifier) @function.method.call))

(member_expression member: (identifier) @property)
(hashdecl_member name: (identifier) @property)
(enum_member name: (identifier) @constant)
((identifier) @constant
  (#match? @constant "^[A-Z][A-Z0-9_]*[A-Z0-9]$"))

(class_compatibility_attribute) @attribute
(callable_attribute name: (identifier) @attribute)
(callable_attribute value: (identifier) @constant)

; ==================== Keywords ====================

["if" "else" "switch" "case" "default"] @keyword.conditional

["while" "do" "for" "foreach" "break" "continue"] @keyword.repeat

["return" "thread_exit"] @keyword.return

["try" "catch" "throw" "rethrow" "on_exit" "on_success" "on_error"] @keyword.exception

["sub" "returns"] @keyword.function

["class" "namespace" "hashdecl" "typedef" "enum" "module" "inherits"] @keyword.type

["abstract" "final" "static" "synchronized" "deprecated" "transient" "public" "private" "private:internal"
 "private:hierarchy" "const" "our" "my" "thread_local" "reexport"] @keyword.modifier

["new" "cast" "background" "delete" "remove" "exists" "elements" "keys" "shift" "pop" "chomp" "trim" "push"
 "unshift" "splice" "extract" "map" "select" "foldl" "foldr" "find" "in" "where" "iterate" "instanceof" "not"
 "and" "or" "first" "any" "all" "count" "take" "drop" "takewhile" "takeuntil"] @keyword.operator
(find_modifier) @keyword.operator

["constructor" "destructor" "copy" "context" "summarize" "by" "sortBy" "sortDescendingBy" "@debug" "@assert"
 "@discard"] @keyword

; ==================== Operators ====================

["=" "+=" "-=" "*=" "/=" "%=" "&=" "|=" "^=" "<<=" ">>=" ":=" "@=" "==" "!=" "<>" "===" "!==" "<=" ">="
 "<=>" "=~" "!~" "+" "-" "/" "%" "++" "--" "&&" "||" "!" "~" "&" "|" "^" "<<" ">>" "??" "?*" "?:" ".."
 "\\"] @operator
