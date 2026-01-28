# DPQL Syntax Reference

This document describes the syntax of DPQL (Data Provider Query Language), a domain-specific
language for filtering and querying data in the Qore DataProvider framework.

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

## Field Accessors

Accessors allow extracting specific elements from fields.

### Index Accessor

Access list elements by index (0-based, negative indices count from end):

```dpql
@items[0] == "first"     # First element
@items[-1] == "last"     # Last element
@items[2] == "third"     # Third element
```

### Slice Accessor

Extract a range of elements:

```dpql
@items[0..2]             # Elements 0, 1, 2 (inclusive)
@items[1..-1]            # All except first
```

### Key Accessor

Access hash keys:

```dpql
@record{name}            # Single key
@record{name, age}       # Multiple keys (subset)
@record{"key with spaces"}  # Quoted key name
```

### Dot Accessor

Access hash member:

```dpql
@record.name             # Equivalent to @record{name}
@user.address.city       # Nested access
```

### Chained Accessors

Accessors can be chained:

```dpql
@data[0]{user}.name      # First element's user's name
@records[-1]{addresses}[0]  # Last record's first address
```

## Operators

### Comparison Operators

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `>` | Greater than |
| `>=` | Greater than or equal |
| `<` | Less than |
| `<=` | Less than or equal |

### Logical Operators

| Operator | Meaning |
|----------|---------|
| `&&` | Logical AND |
| `\|\|` | Logical OR |
| `!` | Logical NOT |

### Set Operators

```dpql
@status in ("active", "pending")
@role not in ("guest", "anonymous")
```

### Range Operators

```dpql
@age between 18 and 65
```

### Pattern Matching

```dpql
@name =~ /^John/         # Regex match
@email like "%@example.com"  # SQL-like pattern
```

## Values

### String Literals

Use double quotes:

```dpql
@name == "John"
@desc == "Line 1\nLine 2"  # Escape sequences work
```

### Numeric Literals

```dpql
@age == 25
@score >= 75.5
@temp < -10
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

## Expressions

Expressions can be grouped with parentheses:

```dpql
(@status == "active" || @status == "pending") && @age >= 18
!(@deleted == true)
```

## Examples

```dpql
# Simple equality
@name == "John"

# Compound condition
@status == "active" && @age >= 18 && @role in ("admin", "editor")

# With accessors
@users[0].name == "Admin" && @config{timeout} > 30

# Field name with special characters
@"response.body" != null && @"items[count]" > 0

# Field name with escaped quote
@"field\"with\"quotes" == "value"
```
