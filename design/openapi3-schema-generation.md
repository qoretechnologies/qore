# OpenAPI 3 Schema Generation

This guide explains how to generate OpenAPI 3.0.3 schemas from Qore source code using the `OpenApi3` module and the `qore-openapi3-gen` command-line tool.

## Overview

The OpenAPI 3 schema generator allows you to document your REST APIs directly in your Qore source code using special `@SCHEMA` comment blocks. The generator parses these blocks and produces a complete OpenAPI 3.0.3 specification in YAML or JSON format.

## @SCHEMA Block Format

Document REST endpoints using the `@SCHEMA` block format within Qore comments:

```qore
/** @REST GET /users/{id}
    @SCHEMA
    @summary Get user by ID
    @desc Retrieves a user record by their unique identifier.

    @params
    - id (int): The unique user identifier

    @return hash<UserInfo>
    - id (int): User ID
    - username (string): Username
    - email (string): Email address
    - created_at (date): Account creation date

    @error 404 User not found
    @error 401 Unauthorized

    @perms READ_USER
    @since 1.0.0
    @see updateUser, deleteUser
    @ENDSCHEMA
*/
hash<HttpHandlerResponseInfo> get(hash<auto> cx, *hash<auto> ah) {
    # Implementation
}
```

### Block Structure

| Tag | Description | Required |
|-----|-------------|----------|
| `@REST` | HTTP method and path (e.g., `GET /users/{id}`) | Yes |
| `@SCHEMA` | Marks the start of schema documentation | Yes |
| `@summary` | Brief one-line description | No |
| `@desc` | Detailed description (can be multi-line with `\` continuation) | No |
| `@params` | Parameter definitions | No |
| `@return` | Return type and field descriptions | No |
| `@error` | Error response codes and descriptions | No |
| `@perms` | Required permissions | No |
| `@since` | Version when endpoint was added | No |
| `@see` | Related endpoints or references | No |
| `@examples` | Usage examples as a bullet list (output as `x-examples` extension) | No |
| `@note` | Behavioral notes/caveats (appended to description; multi-line with `\`) | No |
| `@x-ai-summary` | AI-specific summary override for embedding text (output as `x-ai-summary` extension) | No |
| `@ENDSCHEMA` | Marks the end of schema documentation | Yes |

### Parameter Definitions

Parameters are defined in the `@params` section using the format:
```
- name (type): description
```

Parameter location is inferred from context:
- Parameters in the path (e.g., `{id}`) are path parameters
- Other parameters are query parameters by default

Examples:
```
@params
- id (int): User ID (path parameter if in URL)
- limit (*int): Maximum results to return (optional)
- offset (int): Pagination offset
- filter (*string): Optional search filter
```

### Type Specifications

The generator supports all Qore types:

| Qore Type | OpenAPI Type | Format |
|-----------|--------------|--------|
| `string` | string | - |
| `int` | integer | int64 |
| `float` | number | double |
| `bool` | boolean | - |
| `date` | string | date-time |
| `binary` | string | binary |
| `hash` | object | - |
| `hash<TypeName>` | $ref to schema | - |
| `list<T>` | array of T | - |
| `*type` | type (nullable) | - |

### Return Type Definitions

Specify return types with optional field descriptions:
```
@return hash<OrderResponse>
- order_id (int): Unique order identifier
- items (list<hash<OrderItem>>): Order line items
- total (*float): Order total (nullable)
- status (string): Order status
```

## Command-Line Tool

### Basic Usage

```bash
qore-openapi3-gen [options] <source-files...>
```

### Options

| Option | Description |
|--------|-------------|
| `-o, --output=FILE` | Output file (default: stdout) |
| `-f, --format=FORMAT` | Output format: `yaml` or `json` (default: yaml) |
| `-t, --title=TITLE` | API title |
| `-V, --api-version=VER` | API version (default: 1.0.0) |
| `-d, --description=DESC` | API description |
| `-s, --server=URL` | Server URL (can be specified multiple times) |
| `-b, --base-path=PATH` | Base path prefix for all endpoints |
| `-v, --verbose` | Increase verbosity |
| `-h, --help` | Show help message |

### Examples

Generate schema from all REST handler classes:
```bash
qore-openapi3-gen -t "My API" -V "2.0.0" -o api.yaml src/*.qclass
```

Generate JSON with multiple servers:
```bash
qore-openapi3-gen \
    -t "Production API" \
    -V "1.0.0" \
    -f json \
    -s "https://api.example.com" \
    -s "https://staging.example.com" \
    -o api.json \
    src/handlers/*.qclass
```

Generate with base path:
```bash
qore-openapi3-gen \
    -t "User Service" \
    -b "/api/v2" \
    -o user-api.yaml \
    src/UserHandler.qclass
```

## Programmatic Usage

### Basic Example

```qore
%requires OpenApi3

# Create generator with API metadata
OpenApi3::OpenApi3SchemaGenerator gen({
    "title": "My REST API",
    "version": "1.0.0",
    "description": "API for managing resources",
});

# Parse source file
string content = ReadOnlyFile::readTextFile("src/MyHandler.qclass");
list<hash<OpenApi3::RestMethodInfo>> methods =
    OpenApi3::OpenApi3SchemaTokenizer::parseSourceContent(content);

# Add methods to generator
foreach hash<OpenApi3::RestMethodInfo> method in (methods) {
    gen.addMethod(method.path, method);
}

# Register custom types used in return values
gen.registerSchema("UserInfo", {
    "type": "object",
    "properties": {
        "id": {"type": "integer"},
        "username": {"type": "string"},
        "email": {"type": "string"},
    },
    "required": ("id", "username"),
});

# Add server definitions
gen.addServer({
    "url": "https://api.example.com",
    "description": "Production server",
});

# Generate output
string yaml = gen.toYaml();
print(yaml);
```

### Processing Multiple Files

```qore
%requires OpenApi3

OpenApi3::OpenApi3SchemaGenerator gen({
    "title": "Complete API",
    "version": "2.0.0",
});

# Process all handler files
list<string> files = glob("src/handlers/*.qclass");
foreach string filepath in (files) {
    string content = ReadOnlyFile::readTextFile(filepath);
    list<hash<OpenApi3::RestMethodInfo>> methods =
        OpenApi3::OpenApi3SchemaTokenizer::parseSourceContent(content);

    foreach hash<OpenApi3::RestMethodInfo> method in (methods) {
        gen.addMethod(method.path, method);
    }
}

# Write to file
File f();
f.open("api-spec.yaml", O_CREAT | O_WRONLY | O_TRUNC);
f.write(gen.toYaml());
f.close();
```

## Complete Source Code Example

Here's a complete example of a documented REST handler:

```qore
%requires RestHandler

/** @file UserHandler.qclass
    REST handler for user management operations
*/

class UserHandler inherits AbstractRestHandler {
    /** @REST GET /users
        @SCHEMA
        @summary List all users
        @desc Returns a paginated list of users with optional filtering.

        @params
        - limit (*int): Maximum number of results (default: 20, max: 100)
        - offset (*int): Pagination offset (default: 0)
        - status (*string): Filter by status (active, inactive, pending)

        @return hash<UserListResponse>
        - users (list<hash<UserInfo>>): List of user objects
        - total (int): Total number of users matching filter
        - has_more (bool): Whether more results are available

        @error 401 Authentication required
        @perms LIST_USERS
        @since 1.0.0
        @ENDSCHEMA
    */
    hash<HttpHandlerResponseInfo> get(hash<auto> cx, *hash<auto> ah) {
        # Implementation
    }

    /** @REST POST /users
        @SCHEMA
        @summary Create a new user
        @desc Creates a new user account with the provided information.

        @params
        - username (string): Unique username (3-50 characters)
        - email (string): Valid email address
        - password (string): Password (min 8 characters)
        - role (*string): User role (default: "user")

        @return hash<UserInfo>
        - id (int): Assigned user ID
        - username (string): Username
        - email (string): Email address
        - role (string): Assigned role
        - created_at (date): Creation timestamp

        @error 400 Invalid input data
        @error 409 Username or email already exists
        @perms CREATE_USER
        @since 1.0.0
        @ENDSCHEMA
    */
    hash<HttpHandlerResponseInfo> post(hash<auto> cx, *hash<auto> ah) {
        # Implementation
    }

    /** @REST GET /users/{id}
        @SCHEMA
        @summary Get user by ID
        @desc Retrieves detailed information about a specific user.

        @params
        - id (int): User ID

        @return hash<UserInfo>
        - id (int): User ID
        - username (string): Username
        - email (string): Email address
        - role (string): User role
        - created_at (date): Creation timestamp
        - last_login (*date): Last login timestamp

        @error 404 User not found
        @error 401 Authentication required
        @perms READ_USER
        @since 1.0.0
        @ENDSCHEMA
    */
    hash<HttpHandlerResponseInfo> getUser(hash<auto> cx, *hash<auto> ah) {
        # Implementation
    }

    /** @REST PUT /users/{id}
        @SCHEMA
        @summary Update user
        @desc Updates an existing user's information.

        @params
        - id (int): User ID
        - email (*string): New email address
        - role (*string): New role

        @return hash<UserInfo>
        - id (int): User ID
        - username (string): Username
        - email (string): Updated email
        - role (string): Updated role
        - updated_at (date): Update timestamp

        @error 404 User not found
        @error 400 Invalid input
        @error 409 Email already in use
        @perms UPDATE_USER
        @since 1.0.0
        @ENDSCHEMA
    */
    hash<HttpHandlerResponseInfo> putUser(hash<auto> cx, *hash<auto> ah) {
        # Implementation
    }

    /** @REST DELETE /users/{id}
        @SCHEMA
        @summary Delete user
        @desc Permanently deletes a user account.

        @params
        - id (int): User ID

        @return hash<StatusResponse>
        - success (bool): Whether deletion succeeded
        - message (string): Status message

        @error 404 User not found
        @error 403 Cannot delete admin users
        @perms DELETE_USER
        @since 1.0.0
        @ENDSCHEMA
    */
    hash<HttpHandlerResponseInfo> deleteUser(hash<auto> cx, *hash<auto> ah) {
        # Implementation
    }
}
```

Generate the schema:
```bash
qore-openapi3-gen -t "User Management API" -V "1.0.0" -o users-api.yaml UserHandler.qclass
```

## Generated Output Example

The above handler produces an OpenAPI 3.0.3 specification like:

```yaml
openapi: "3.0.3"
info:
  title: "User Management API"
  version: "1.0.0"
paths:
  /users:
    get:
      summary: "List all users"
      description: "Returns a paginated list of users with optional filtering."
      parameters:
        - name: limit
          in: query
          schema:
            type: integer
            nullable: true
          description: "Maximum number of results (default: 20, max: 100)"
        - name: offset
          in: query
          schema:
            type: integer
            nullable: true
          description: "Pagination offset (default: 0)"
      responses:
        "200":
          description: "Successful response"
          content:
            application/json:
              schema:
                $ref: "#/components/schemas/UserListResponse"
        "401":
          description: "Authentication required"
    post:
      summary: "Create a new user"
      # ... additional endpoints
  /users/{id}:
    get:
      summary: "Get user by ID"
      parameters:
        - name: id
          in: path
          required: true
          schema:
            type: integer
          description: "User ID"
      # ... responses
components:
  schemas:
    UserInfo:
      type: object
      properties:
        id:
          type: integer
        username:
          type: string
        email:
          type: string
        # ... additional properties
```

## Best Practices

1. **Document all public endpoints** - Include `@SCHEMA` blocks for every REST endpoint that should appear in the API documentation.

2. **Use descriptive summaries** - The `@summary` should be a concise one-liner; use `@desc` for detailed explanations.

3. **Define all parameters** - Document every parameter including optional ones (prefixed with `*`).

4. **Include error responses** - Use `@error` to document all possible error codes and their meanings.

5. **Register complex types** - When using `hash<TypeName>` return types, register the schema using `gen.registerSchema()`.

6. **Version your API** - Use `@since` to track when endpoints were introduced.

7. **Keep documentation in sync** - Update `@SCHEMA` blocks whenever you change endpoint behavior.

8. **Add usage examples** - Use `@examples` to provide concrete request examples that help
   both human readers and AI tools understand how to use the endpoint.

9. **Use notes for caveats** - Use `@note` for behavioral nuances, side effects, or edge
   cases that users should be aware of. Multiple `@note` blocks are supported.

10. **Use AI summary for embeddings** - If `@summary` is constrained by backward
    compatibility but doesn't provide good semantic signal for AI tool selection, add
    `@x-ai-summary` with a richer description optimized for embedding similarity search.

## Examples and Notes Tags

### @examples

Provide concrete usage examples as a bullet list. Output as the `x-examples` OpenAPI
extension on the operation:

```qore
/** @REST GET /services
    @SCHEMA
    @summary List services with runtime status, threads, and configuration

    @examples
    - GET /api/v9/services — list all services
    - GET /api/v9/services?status=loaded — only loaded/running services
    - GET /api/v9/services?search=http&limit=10 — search by name with pagination
    - GET /api/v9/services?details=true — include full metadata per service

    @ENDSCHEMA
*/
```

### @note

Add behavioral notes that are appended to the operation description. Supports multi-line
with backslash (`\`) continuation. Multiple `@note` blocks accumulate:

```qore
/** @REST PUT /services/{id}/enable
    @SCHEMA
    @summary Enable a service

    @note Enabling a service does not automatically load it. Use the load \
    action to also load the service into memory.
    @note This action is idempotent — enabling an already-enabled service returns success.

    @ENDSCHEMA
*/
```

### @x-ai-summary

Override the summary text used for AI embedding generation. The standard `@summary` appears
in human documentation; `@x-ai-summary` provides a richer description optimized for
semantic search without changing the public API docs:

```qore
/** @REST GET /system
    @SCHEMA
    @summary Returns system information

    @x-ai-summary Returns Qorus system health, cluster status, resource counts, \
    instance key, version, and node information for monitoring and diagnostics

    @ENDSCHEMA
*/
```
