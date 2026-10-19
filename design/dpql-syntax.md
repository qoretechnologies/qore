# DPQL Syntax Reference

This document describes the syntax of DPQL (Data Provider Query Language), a domain-specific
language for filtering and querying data in the Qore DataProvider framework.

For integration guidance (callback registration, session lifecycle, expression evaluation),
see [DPQL Integration Guide](dpql-integration.md).

## Field References

Field references are prefixed with `@`:

```dpql
@name == "John"
@age > 18
```

### Field Paths

A field reference is a path: its parts are separated by dots, and each part is a key of the
value before it, starting with the record. Dots separate the parts in quoted and unquoted names
alike; quotes are only for characters that an unquoted name cannot hold. A dot that is part of a
key is escaped: `\.`.

```dpql
@user.email == "john@example.com"     # the key email of the key user
@"user.email" == "john@example.com"   # the same path
@"create.order-id" == "A-1"           # the key order-id of the key create
@"Unit\.Price" > 10                   # the key Unit.Price
```

### Unquoted Field Names

Unquoted field names can contain, in each part:
- Letters of any script, and `_`, to start the part
- Letters, combining marks, digits and `_` after the first character

```dpql
@user_name == "john"
@menge_bestätigt == 1
@user.email == "john@example.com"
```

### Quoted Field Names

Use double quotes for field names with parts that:
- Contain spaces
- Contain other characters, such as brackets, operators or `-`
- Start with a digit
- Contain a dot that is part of a key, which is escaped (`\.`)

```dpql
@"field with spaces" == "value"
@"items[0]" == 1             # the key items[0]: brackets in quotes are part of the name
@"123field" == "value"
@"order.order-id" == "A-1"   # the key order-id of the key order
@"Unit\.Price" > 10          # the key Unit.Price
@"prénom" == "José"          # Unicode field names and string values
```

A quoted name after an accessor (`@r[0].@"x.y"`) is one key, as `{"x.y"}` is: its dots are part of
the key, escaped or not.

### Escape Sequences

Within quoted field names, a backslash starts an escape:

| Sequence | Meaning |
|----------|---------|
| `\"` | Double quote |
| `\\` | Backslash |
| `\.` | A dot that is part of a key, not a separator of the path |
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |

Any other escape is an error (`DPQL-E003`, "Unknown escape '\x' in a quoted field name"). It is
not read as its character or as a backslash and its character, as either would give a name that
the author may not have meant: `\x` is `x` in a string literal, and a backslash in a field path
already has a meaning before a dot. `\\.` is read as `\.`, so `@"a\\.b"` is also the key `a.b`.

```dpql
@"field\"name" == "value"    # Field name contains a quote
@"path\\file" == "value"     # Field name contains a backslash
@"a\.b" == 1                 # the key a.b
```

In the expression form, the `field` of a field reference holds the path with its escaped dots:
`@"Unit\.Price"` is `{"field": "Unit\\.Price"}` (the text `Unit\.Price`), and
`AbstractDataProvider::getFieldPath()` gives its keys. The serializer writes a path as it is read
back: bare when every part is a bare name, otherwise quoted with each escaped dot written `\.`,
each other backslash `\\` and each quote `\"`; `DpqlSerializer::fieldReferenceForKey()` writes
the field reference to one key, such as the name of a field of a record type, with its dots
escaped.

## Field Reference Operators

Field reference operators allow extracting specific elements from field values.

### Index Operator

Access list elements by index (0-based, negative indices count from end):

```dpql
@items[0] == "first"     # First element
@items[-1] == "last"     # Last element
@items[2] == "third"     # Third element
```

Supports multiple indices and ranges in a single expression:

```dpql
@items[0..2, 5, 7]       # Elements 0-2, 5, and 7
```

### Slice Operator

Extract a range of elements (inclusive on both ends, Qore-style):

```dpql
@items[0..2]             # Elements 0, 1, 2 (inclusive)
@items[1..-1]            # All except first
@items[2..]              # From index 2 to end
@items[..5]              # From start through index 5
```

### Key Operator

Access hash values by key:

```dpql
@record{name}            # Single key
@record{name, age}       # Multiple keys (returns hash subset)
@record{"key with spaces"}  # Quoted key name
@record{123}             # Numeric key
```

### Dot Operator

Access hash member (equivalent to single-key operator):

```dpql
@record.name             # Equivalent to @record{name}
@user.address.city       # Nested access
```

After an accessor, `.name` is a key (`@data[0].name` is `key(@data[0], "name")`); right
after `@field`, the dot continues the field name (`@record.name` is the field `record.name`).
The serializer writes accessors as they are read: a single key that is a name after an
accessor as `.name` (`@data[0]{name}` is written `@data[0].name`), any other key in braces
(`{"a b"}`, `{"true"}`, `{a, b}`), a key of a field reference in braces (`@record{name}`), and
an accessor whose index or key is not a literal, or whose value is not a field reference or
another accessor (a wildcard, a template reference, a call, an operation), as a call
(`index(@items, @i)`, `index(@orders[*].id, 0)`).

### Chained Operators

Field reference operators can be chained:

```dpql
@data[0]{user}.name      # First element's user's name
@records[-1]{addresses}[0]  # Last record's first address
@matrix[0][1]            # Nested list access
```

### Wildcard Operators

`[*]` and `{*}` take a value from each item of a list and each value of a hash. A wildcard
is the `map` operator written as an accessor: the accessors after it are the body that `map`
evaluates for each item, and it evaluates exactly as that `map` expression does.

| Wildcard | Same as | Result |
|----------|---------|--------|
| `@orders[*].id` | `map @id, @orders` | The `id` of each order |
| `@orders[*].customer.name` | `map @customer.name, @orders` | The `.name` steps continue the field |
| `@orders[*]{"order-id"}` | `map @"order-id", @orders` | A single key is a field too |
| `@orders[*].tags[0]` | `map @tags[0], @orders` | Ordinary accessors follow |
| `@orders[*].items[*].sku` | `map (map @sku, @items), @orders` | Nested: a list of lists, never flattened |
| `@prices{*}` | `map @value, @prices` | The values, in key order |
| `@prices{*}.amount` | `map @value.amount, @prices` | The accessors apply to `@value` |
| `@orders[*]` | `@orders` | A trailing `[*]` is the items: the list itself |
| `@matrix[*][0]` | `map @value[0], @matrix` | Accessors that do not start with a field apply to the item |

Missing fields, wrong source types (a non-list under `[*]`, a non-hash under `{*}`), empty
sources and no value give the same results and raise the same errors as the `map` expression.
`[*]` over a hash goes through its `key`/`value` entries (`@weights[*].key`), and a trailing
`[*]` over a hash gives those entries, as `select` does. `@data{"*"}` is the key `*`, not a
wildcard.

Expression form (`DataProviderExpression`): `list_wildcard` / `hash_wildcard` with the source
and the per-item expression as arguments (the per-item expression is the body of `map`; the
`.name` steps after `[*]`/`{*}` form one field reference such as `customer.name` or
`value.amount`); accessors on the item itself start from the zero-argument `wildcard_item`,
which is `internal` in the expression catalog (`DataProviderExpressionBaseInfo::internal`):
it is never offered for picking and is drawn only inside the wildcard that makes it.
The tokenizer gives the `*` its own token type (`DPQL_TOK_WILDCARD`), the serializer writes a
wildcard back as written, the expression type is `list<T>` when the type of each value is known,
and completion offers `[*]` after a list and `{*}` after a hash.

## Comparison Operators

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `>` | Greater than |
| `>=` | Greater than or equal |
| `<` | Less than |
| `<=` | Less than or equal |

## Arithmetic Operators

| Operator | Meaning |
|----------|---------|
| `+` | Addition (numbers, strings, dates, lists) |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulus (remainder) |

Arithmetic follows standard precedence: `*`, `/`, `%` bind tighter than `+`, `-`.
Use parentheses to override:

```dpql
@price * @qty + @tax         # multiplication first
(@price + @tax) * @qty       # addition first
@age % 2 == 0                # even ages
```

## Logical Operators

| Operator | Meaning |
|----------|---------|
| `&&` | Logical AND |
| `\|\|` | Logical OR |
| `!` | Logical NOT |

## Field Existence

A field reference to a field that the record does not have raises an error. `exists @field` is
`true` when the record has the field, also when its value is `null`, and `false` when it does
not, so records of different shapes can be told apart:

```dpql
exists @type && @type == "return"   # false for a record without type; no error
!exists @discount || @discount == 0
any exists @id, @orders              # each item is tested with any or all
```

| Expression | True when |
|------------|-----------|
| `exists @a` | the record has the key `a` (any value, also `null`) |
| `exists @a.b`, `exists @a{b}` | the record has `a`, which is a hash with the key `b` |
| `exists @a[0]`, `exists @a[-1]` | `a` is a list or a string with an element at that position (negative from the end) |
| `exists @a[0].b` | each step in turn, as above |
| `exists @"a\.b"` | the record has the key `a.b` (an escaped dot is part of the key) |

The field reference is a path as everywhere else in DPQL ([Field Paths](#field-paths)):
`exists @"x.y"` is the path `x`, `y`, as `exists @x.y` is.

- **Precedence**: `exists` binds tighter than every other operator: `exists @type && @type == "x"`
  is `(exists @type) && (@type == "x")`, `!exists @a` is `!(exists @a)`, and
  `exists @a == false` compares the result. The field reference can be in parentheses:
  `exists(@a)`, which the serializer writes `exists @a`.
- **Operand**: a field reference with accessors that each read one key or one position. Wildcards
  (`[*]`, `{*}`), ranges, several positions or keys, values and expressions are refused at parse
  time with `DPQL-E025`; an operand built without the parser is refused when evaluated
  (`DATA-PROVIDER-EXPRESSION-ERROR`) and when serialized (`DPQL-SERIALIZE-ERROR`).
- **Keyword**: `exists` is an identifier token, read as the operator only when a field reference or
  `(` follows it; `@a == exists` is still the string `"exists"`, and keys and members named
  `exists` need no quotes. A value after it (`exists 1`) is `DPQL-E025`. Highlighting gives it
  the `operator` span type where the parser reads it as the operator.
- **Expression form**: `{"exp": "exists", "args": (<field reference or accessor expression>,)}`
  (`DP_OP_EXISTS`); its catalog entry is a user-facing operator in the `Comparison` group that
  returns `bool`, so it types and verifies as a condition. Validation does not report the field as
  unknown (`DPQL-E010`), as it is looked for, not read.
- **Completion**: offered as a prefix operator where a condition starts (`exists ${1:Field}`);
  after it, only fields are offered.
- **Push-down**: SQL cannot express it (every row has every column, and a missing key is not a
  SQL concept), so it is never pushed down: `splitPushdownableExpression()` puts it in the client
  part, a database table evaluates it on the rows read with `allow_client_side_filter`, and a
  provider that searches only on its server reports `DPQL-E041`. In-memory providers, filters and
  DataFrame blocks evaluate it per record.

## Set Operators

```dpql
@status in ("active", "pending")
@status not in ("deleted", "archived")
```

### findIn

`findIn(value, list, location)` returns the first element of `list` whose value at `location` equals
`value`; without a location, or when no element matches by location, it returns `value` if it is an
element of the list, otherwise no value.  It gives the same results as Qorus's `findIn`:

```dpql
findIn(2, @orders, "id")              # the order whose id is 2
findIn(2, (1, 2, 3), "id")            # 2: no element has an id, but 2 is an element
findIn("vip", @orders, "tags[-1]")    # the order whose last tag is "vip"
findIn(2, @rows, "a\\.b")              # reads the key "a.b"
findIn(1, @rows, "'a b'")             # reads the key "a b" (also written a\\ b)
findIn((1, 2), @rows, "o.v")          # o is a list of hashes: the list of their v values
findIn(1, null)                       # no list: no value
```

The location is a dot-separated path; `\\.` is a dot in a key, `[n]` or `[-n]` after a key reads an
element of a list, quotes around a key or `\\ ` give a key with a space, and a key that reads a list of
more than one hash before the last step gives the list of the values that the rest of the path reads in
each of them.

## Range Operators

```dpql
@age between 18 and 65            # inclusive on both bounds
@age not between 18 and 65        # negated range
```

## Pattern Matching

### Regex

```dpql
@name =~ /^John/         # Regex match
@name =~ /john/i         # Case-insensitive regex
```

Supported regex flags:
- `i` — case-insensitive
- `s` — dot matches newlines
- `m` — multiline mode
- `x` — extended syntax (ignore whitespace)
- `u` — Unicode

### LIKE

SQL-style pattern matching with `%` (any sequence) and `_` (single character) wildcards:

```dpql
@name like "%John%"
@email like "%@example.com"
@code like "A_C"
```

## Built-in Functions

### Math Functions

```dpql
abs(@value)                  # Absolute value
round(@score, 2)             # Round to 2 decimal places
floor(@price)                # Round down to integer
ceil(@price)                 # Round up to integer
```

### String Functions

```dpql
trim(@name)                  # Remove leading/trailing whitespace
ltrim(@name)                 # Remove leading whitespace
rtrim(@name)                 # Remove trailing whitespace
unaccent(@name)              # Replace accented characters with unaccented equivalents
lwr(@name)                   # Convert to lower case
upr(@name)                   # Convert to upper case
concat(@first, " ", @last)   # Concatenate values as strings
split(@csv, ",")             # Split by separator
substr(@name, 0, 5)          # Substring extraction
```

#### Accent-Insensitive Matching

`unaccent(string)` returns a string using the same character mapping as Qore's
`<string>::unaccent()`. It preserves case and the input encoding. Use function-call
syntax in DPQL:

```dpql
unaccent(@name) == "Jose"                 # Matches "José" and "Jose"
lwr(unaccent(@name)) == "jose"            # Also matches "JOSÉ"
unaccent("příliš žluťoučký kůň") == "prilis zlutoucky kun"
```

For nullable fields, provide an empty-string default explicitly:

```dpql
lwr(unaccent(coalesce(@name, ""))) == "jose"
```

`unaccent` is included in `DataProviderGenericExpressions`. Availability in a
provider-specific search depends on that provider's expression support; generic
evaluation is described in the [integration guide](dpql-integration.md#accent-removal).

### Null Handling Functions

```dpql
coalesce(@nickname, @name, "unknown")  # First non-null value
nullif(@status, "inactive")            # Null if equal
```

### Date/Time Functions

```dpql
now()                        # Current date/time

# Duration constructors (for date arithmetic with + and -)
@created + days(30)
@deadline - hours(12)
years(1) + months(6)
weeks(2)
minutes(30) + seconds(15)
milliseconds(500)
microseconds(1000)

# Date component extraction
get_year(@created)           # Year (e.g. 2026)
get_month(@created)          # Month (1-12)
get_day(@created)            # Day (1-31)
get_hour(@created)           # Hour (0-23)
get_minute(@created)         # Minute (0-59)
get_second(@created)         # Second (0-59)

# Date formatting
format_date(@created, "YYYY-MM-DD")
format_number(@price, ",", ".", 2)
format_number(@price)        # The same: missing or null arguments take the defaults "," "." 2
format_number(@price, "?")   # "?" or "": no thousands separator
```

`format_number` separators are single characters and its precision is not negative; anything
else raises `FORMAT-NUMBER-ERROR`.  With a precision of `0` there is no decimal separator.

### Collection and Streaming Operators

Collection operators use Qore-style prefix syntax rather than function-call syntax. Arguments
are separated with commas.

```dpql
# Apply expression to each element, return list
map @product.name, @orders

# With filter
map @product.name, @orders, @status == "active"

# Build hash from list
hash_map @id, @name, @users
```

The body, key, value, and predicate expressions are evaluated in the context of the current
source item. When a list item is a hash, its fields are available directly, such as
`@product.name` or `@status`. Scalar list items are exposed as `@value`. Hash sources expose
`@key` and `@value`.

```dpql
# Scalar list source
select @scores, @value >= 90

# Hash source
hash_map @key, @value, @weights, @value > 0
```

Supported prefix operator forms:

| Operator | Syntax | Result |
|----------|--------|--------|
| `map` | `map body, source[, filter]` | List of evaluated body results |
| `hash_map` | `hash_map key, value, source[, filter]` | Hash built from evaluated keys and values |
| `select` | `select source, predicate` | List of matching source items |
| `first` | `first source` or `first predicate, source` | First item, first matching item, or `nothing` |
| `any` | `any source` or `any predicate, source` | `true` if any item or predicate result is truthy |
| `all` | `all predicate, source` | `true` if all predicate results are truthy |
| `count` | `count source` or `count predicate, source` | Number of source items or matching items |
| `take` | `take count, source` | First `count` items |
| `drop` | `drop count, source` | Items after the first `count` items |
| `takewhile` | `takewhile predicate, source` | Items until the predicate first becomes false |
| `takeuntil` | `takeuntil predicate, source` | Items until the predicate first becomes true |

```dpql
select @orders, @amount > 100
first @orders
first @amount > 100, @orders
any @value >= 90, @scores
any @flags
all @active, @users
count @status == "active", @orders
take 10, @orders
drop 10, @orders
takewhile @status != "closed", @events
takeuntil @status == "closed", @events
```

For hash sources, `select`, `first`, `take`, `drop`, `takewhile`, and `takeuntil` return
`key`/`value` records. `takeuntil` stops before the matching boundary item. `all` returns
`true` for an empty source. Each item or body result is one element of the result, also when it
is a list: `map @items, @orders` is a list of lists, and so is a slice of a list of lists.

While a body, filter, key, value or predicate (or the expression after a wildcard) is evaluated,
the template context holds the current item as `item`, its zero-based position as `index` and the
source size as `size` (`$local:item`, `$local:index`, `$local:size` where the template callback reads
`$local` from the template context, as Qorus does); a hash source's item is its `key`/`value` entry.
Each item gets a new context hash (`dp_item_context()`), so a nested operator shadows the outer values
only while it runs, the caller's other values stay visible, and nothing needs restoring after an error.

The field references in a body, filter or predicate (and after a wildcard) are fields of each
item, not of the record. Code that collects or validates the fields of a record must skip those
arguments with `DpqlParser::isItemScopedArg(exp, pos)` rather than keep its own operator table;
DPQL validation does so.

A field reference path (`@customer.name`) has no value when a value on the path is not a hash
or an object (a plain value, a list or no value); only the first key must be in the record.

## Values

### String Literals

Double-quoted or single-quoted:

```dpql
@name == "John"
@name == 'John'
@desc == "Line 1\nLine 2"  # Escape sequences work
```

### Numeric Literals

```dpql
@age == 25
@score >= 75.5
@temp < -10
@rate == 1.5e-3            # Scientific notation
```

### Boolean Literals

```dpql
@active == true
@deleted == false
```

### Null

```dpql
@value != null
```

### Date Literals

ISO-8601 format:

```dpql
@created < 2026-01-15
@updated >= 2026-01-02T15:20:11.123+01:00
```

### Binary Literals

Hex-encoded:

```dpql
@checksum == <deadbeef>
```

### List Literals

```dpql
@tags == (one, two, three)
@ids == (1, 2, 3)
```

### Hash Literals

```dpql
@meta == {a=1, b=two}
```

### Unquoted Identifiers

Unquoted identifiers on the right-hand side of an expression are treated as string values:

```dpql
@domain == billing         # Equivalent to @domain == "billing"
@status == active          # Equivalent to @status == "active"
```

## Expressions

Expressions can be grouped with parentheses:

```dpql
(@status == "active" || @status == "pending") && @age >= 18
!(@deleted == true)
```

## Template References

Template references allow DPQL expressions to reference contextual values that are resolved
at runtime via registered callbacks. This enables expressions like configuration values,
environment variables, and dynamic context without hardcoding values.

### Syntax

| Form | Example | Description |
|------|---------|-------------|
| Simple | `$static:account.id` | Context `static`, value `account.id` |
| Bracketed | `$qore-expr:{1 + 2}` | Bracketed value (allows special characters) |
| Fallback | `$static:A??{$static:B}` | Fallback syntax (raw passthrough) |

### Template Contexts

Template contexts identify the source of the value. Well-known contexts include:

| Context | Description |
|---------|-------------|
| `static` | Static context values |
| `dynamic` | Dynamic runtime values |
| `config` | Configuration values |
| `var` | Variable values |
| `transient` | Transient context values |
| `env` | Environment variables |
| `timestamp` | Timestamp values |
| `qore-expr` | Qore expression evaluation |
| `rest` | REST context values |

### Usage in Expressions

Template references can appear anywhere a value is expected:

```dpql
# As a comparison operand
@name == $static:expected_name

# In logical expressions
@age > $config:min_age::int && @status == $static:required_status

# As a standalone expression
$static:name

# Mixed with field references
@price > $config:threshold && @category in ($static:allowed_categories)
```

### Resolution

Template references are resolved at runtime via callbacks registered with
`DataProvider::setTemplateCallbacks()`. If no callback is registered and a template
reference is evaluated, a `TEMPLATE-RESOLUTION-ERROR` is thrown. See
[DPQL Integration Guide — Template Resolution](dpql-integration.md#template-resolution)
for callback implementation details.

Array indexing, fallback syntax (`??{}`), and dot navigation within template values
are passed as raw strings to the callback for handling.

## Examples

```dpql
# Simple equality
@name == "John"

# Compound condition
@status == "active" && @age >= 18 && @role in ("admin", "editor")

# With field reference operators
@users[0].name == "Admin" && @config{timeout} > 30

# Field names with special characters: the key body of the key response, and the key items[count]
@"response.body" != null && @"items[count]" > 0

# A key with a dot
@"Unit\.Price" > 10

# Field name with escaped quote
@"field\"with\"quotes" == "value"

# Date comparison
@created >= 2026-01-01 && @created < 2026-02-01

# Date arithmetic with duration functions
@created + days(30) > now()
@deadline - hours(12) < now()

# Arithmetic in comparisons
@total == @price * @qty + @tax
@score - 80 > 0

# Pattern matching
@email =~ /example\.com$/i
@name like "%Smith%"

# Null handling
coalesce(@nickname, @name) != "unknown"
@status not in ("deleted", "archived")

# Math functions
abs(@balance) > 1000 && round(@rate, 2) == 3.14

# String functions
trim(@name) == "Alice" && concat(@first, " ", @last) == @full_name

# Date extraction
get_year(@created) == 2026 && get_month(@created) >= 6

# Chained field reference operators
@orders[-1]{items}[0].product_id == 123
```
