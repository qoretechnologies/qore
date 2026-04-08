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

### Unquoted Field Names

Unquoted field names can contain:
- Letters (a-z, A-Z)
- Digits (0-9)
- Underscores (_)
- Dots (.) for nested field access

```dpql
@user_name == "john"
@user.email == "john@example.com"
```

Note: Dots in unquoted field names represent nested field access syntax, not literal
dots in the field name.

### Quoted Field Names

Use double quotes for field names that:
- Contain spaces
- Contain special characters (brackets, operators, etc.)
- Start with a digit
- Contain literal dots (not for nested access)

```dpql
@"field with spaces" == "value"
@"items[0]" == 1
@"123field" == "value"
@"user.email.address" == "literal.field.name"
```

### Escape Sequences

Within quoted field names, use backslash to escape special characters:

| Sequence | Meaning |
|----------|---------|
| `\"` | Double quote |
| `\\` | Backslash |
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |

```dpql
@"field\"name" == "value"    # Field name contains a quote
@"path\\file" == "value"     # Field name contains a backslash
```

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

### Chained Operators

Field reference operators can be chained:

```dpql
@data[0]{user}.name      # First element's user's name
@records[-1]{addresses}[0]  # Last record's first address
@matrix[0][1]            # Nested list access
```

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

## Set Operators

```dpql
@status in ("active", "pending")
@status not in ("deleted", "archived")
```

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
concat(@first, " ", @last)   # Concatenate values as strings
split(@csv, ",")             # Split by separator
substr(@name, 0, 5)          # Substring extraction
```

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
```

### Collection Functions

```dpql
# Apply expression to each element, return list
map(@orders, @product.name)

# With filter
map(@orders, @product.name, @status == "active")

# Build hash from list
hash_map(@users, @id, @name)
```

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
@domain == omq             # Equivalent to @domain == "omq"
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

# Field name with special characters
@"response.body" != null && @"items[count]" > 0

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
