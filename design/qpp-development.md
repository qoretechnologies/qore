# QPP Development Guide

This document describes how to develop Qore classes using the QPP (Qore Pre-Processor) system, including class creation, common patterns, and useful techniques.

## Overview

QPP files (`.qpp`) are processed by the `qpp` tool to generate C++ code that implements Qore classes. The generated code handles argument parsing, type checking, and integration with the Qore runtime.

## File Structure

### 1. Create the QPP File

Create `lib/QC_ClassName.qpp` with the following structure:

```cpp
/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_ClassName.qpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.
    [license text...]
*/

#include <qore/Qore.h>
// Additional includes as needed

// Private data class (optional)
class ClassNamePriv : public AbstractPrivateData {
public:
    // Implementation details
};

//! Brief description of the class
/** Detailed description of the class.

    @par Example:
    @code{.py}
ClassName obj();
obj.method();
    @endcode

    @since %Qore X.Y
*/
qclass ClassName [dom=DOMAIN; arg=ClassNamePriv* priv; ns=Qore];

/** Constructor documentation
*/
ClassName::constructor() {
    // Implementation
}

/** Method documentation
*/
returntype ClassName::methodName(type param) {
    // Implementation
}
```

### 2. Create the Header File

Create `include/qore/intern/QC_ClassName.h`:

```cpp
/* -*- mode: c++; indent-tabs-mode: nil -*- */
#ifndef _QORE_CLASS_CLASSNAME_H
#define _QORE_CLASS_CLASSNAME_H

#include <qore/Qore.h>

DLLLOCAL QoreClass* initClassNameClass(QoreNamespace& ns);

#endif
```

### 3. Register in CMakeLists.txt

Add the QPP file to the build:

```cmake
set(QPP_SOURCES
    # ... existing files ...
    lib/QC_ClassName.qpp
)
```

### 4. Register in QoreNamespace.cpp

Add the include and initialization:

```cpp
// In includes section (around line 45-100):
#include "qore/intern/QC_ClassName.h"

// In StaticSystemNamespace::StaticSystemNamespace() (around line 1100-1300):
qns.addSystemClass(initClassNameClass(qns));
```

## Important Guidelines

### Reserved Words

**Do not use Qore reserved words as method names.** Common reserved words include:
- `remove` - use `del` instead
- `delete` - use `del` instead
- `new` - use `create` instead
- `throw` - use `raise` instead
- `switch`, `case`, `default`
- `if`, `else`, `while`, `for`, `foreach`
- `return`, `break`, `continue`
- `class`, `namespace`, `public`, `private`

### Object Parameter Handling

When methods accept Qore objects (like `Socket`), the QPP processor references the private data. You must dereference it when the method exits:

```cpp
nothing ClassName::methodWithSocket(Socket[QoreSocketObject] socket, int events) {
    // REQUIRED: Use ReferenceHolder to ensure proper cleanup
    ReferenceHolder<QoreSocketObject> holder(socket, xsink);

    // Now use socket...
    if (!socket->isOpen()) {
        xsink->raiseException("ERROR", "Socket not open");
        return QoreValue();
    }
    // ...
}
```

The pattern for different object types:
- `Socket[QoreSocketObject]` -> `ReferenceHolder<QoreSocketObject>`
- `ReadOnlyFile[File]` -> `ReferenceHolder<File>`
- `Mutex[AbstractSmartLock]` -> `ReferenceHolder<AbstractSmartLock>`

### Accessing the QoreObject* for Object Parameters

When you have an object parameter like `Socket[QoreSocketObject] socket`, the QPP macro `HARD_QORE_VALUE_OBJ_DATA` (defined in `include/qore/params.h`) creates two variables:

1. `socket` - the private data pointer (`QoreSocketObject*`)
2. `obj_socket` - the `QoreObject*` wrapper (prefixed with `obj_`)

This is useful when you need the `QoreObject*` rather than just the private data:

```cpp
nothing EventLoop::del(Socket[QoreSocketObject] socket) {
    ReferenceHolder<QoreSocketObject> holder(socket, xsink);

    // Use obj_socket (the QoreObject*) for operations that need the object pointer
    // For example, looking up by object identity in a map
    el->removeByObject(const_cast<QoreObject*>(obj_socket), xsink);
}
```

The naming pattern is `obj_<parameter_name>`:
- `Socket[QoreSocketObject] socket` -> `obj_socket` is the `QoreObject*`
- `ReadOnlyFile[File] file` -> `obj_file` is the `QoreObject*`
- `Mutex[AbstractSmartLock] mutex` -> `obj_mutex` is the `QoreObject*`

**Use case example**: When maintaining a map of registered objects (e.g., an event loop), you may need to look up by `QoreObject*` to handle cases where the underlying resource (socket fd, file handle) has been closed but the object is still registered.

### Documentation Format

Use Doxygen-style comments:
- Class description: `//!` brief line followed by `/** detailed */`
- Method documentation: `/** ... */` before the method
- Use `@par Example:` for code examples
- Use `@param`, `@return`, `@throw` for parameter/return/exception docs
- Use `@since %Qore X.Y` for version information

### Domain Flags

Common domain flags for the `qclass` declaration:
- `NETWORK` - Network operations (sockets)
- `FILESYSTEM` - File system operations
- `THREAD_CONTROL` - Thread management
- `PROCESS` - Process control
- `DEFAULT` - No restrictions

### Namespace

The `ns=` attribute in the `qclass` declaration controls the namespace path used for documentation
and the value returned by `Class::getPathName()` in the reflection API.

For classes directly in the `Qore` namespace:
```cpp
qclass ClassName [dom=NETWORK; arg=ClassNamePriv* priv; ns=Qore];
```

For classes in a sub-namespace of `Qore`, use the **full path** including `Qore::`:
```cpp
qclass ClassName [arg=ClassNamePriv* priv; ns=Qore::ML; flags=final];
```

**Important:** The `ns=` value must match the full namespace path as registered in the module's
`ns_init` function. If the `QoreNamespace` is created with `"Qore::ML"` and added to `qns`, then
`ns=` must be `Qore::ML` (not just `ML`). A mismatch causes `getPathName()` to return an
incomplete path, which breaks tools like `qjar` (JNI bytecode generation) that rely on absolute
namespace paths for class resolution.

Examples from existing modules:
- Core classes: `ns=Qore` (e.g., `Socket`, `File`)
- Thread classes: `ns=Qore::Thread` (e.g., `Mutex`, `Queue`)
- SQL classes: `ns=Qore::SQL` (e.g., `Datasource`, `SQLStatement`)
- Reflection classes: `ns=Qore::Reflection` (e.g., `Class`, `Method`)
- ML classes: `ns=Qore::ML` (e.g., `IsolationForest`, `DBSCAN`)

### Enum Declarations

QPP supports Qore enum declarations. Add a documentation comment and a line starting with `enum`:

```cpp
//! HTTP/2 mode enum
/** @since %Qore 2.3 */
enum HTTP2Mode : int {
    //! HTTP/2 disabled
    Disabled = 0,
    //! HTTP/2 auto (default)
    Auto = 1,
    //! HTTP/2 required
    Required = 2,
};
```

Notes:
- Base types: `int` (default), `string`, `float`, or `number`.
- String enums require explicit values for all members.
- Enums are placed in the Qore namespace unless you include a namespace prefix in the name (for example,
  `Qore::HTTP2Mode`).
- Use `enum<HTTP2Mode>` or `*enum<HTTP2Mode>` in QPP type signatures.
- QPP emits an `init_enum_<EnumName>()` function; for system enums, call it from
  `lib/QoreNamespace.cpp` (it registers itself via `addSystemEnum()` internally).
- When used in QPP method signatures, enum parameters are passed to C++ as the enum's base type
  (`int64`, `const QoreStringNode*`, `double`, or `const QoreNumberNode*`).

#### Using Enums in Hashdecl Fields

When a hashdecl field references an enum type using `enum<EnumName>` or `*enum<EnumName>`, the
generated C++ code calls `enumPointer->getTypeInfo()`. This requires:

1. **Global enum pointer**: Declare and define a global `QoreEnumDecl*` pointer:
   ```cpp
   // In header file (e.g., yaml-module.h):
   DLLLOCAL extern QoreEnumDecl* enumMyEnumType;

   // In source file (e.g., yaml-module.cpp):
   QoreEnumDecl* enumMyEnumType = nullptr;
   ```

2. **Initialization order**: Initialize enums **before** hashdecls that reference them:
   ```cpp
   // In module init function:
   enumMyEnumType = init_enum_MyEnumType(ns);      // Initialize enum first
   hashdeclMyHashDecl = init_hashdecl_MyHashDecl(ns);  // Then hashdecl
   ```

3. **Hashdecl field syntax**: Use typed enum references in hashdecl fields:
   ```cpp
   hashdecl MyEvent {
       //! Required enum field
       enum<MyEnumType> type;

       //! Optional enum field
       *enum<MyEnumType> style;
   }
   ```

Example from the YAML module:
```cpp
// Enum definitions
enum Qore::YAML::YamlSaxEventType : int {
    StreamStart = 1,
    Scalar = 6,
    // ...
};

enum Qore::YAML::YamlScalarStyle : string {
    Plain = "plain",
    Literal = "literal",
    // ...
};

// Hashdecl using enum types
hashdecl YamlSaxEvent {
    enum<YamlSaxEventType> type;      // Required int enum
    *enum<YamlScalarStyle> style;     // Optional string enum
}
```

### Typed Code Types

QPP supports typed code types (`code<ReturnType(ParamTypes...)>`) for function/closure parameters and return
types. This enables type-safe callbacks and higher-order functions.

#### Basic Usage

```cpp
//! Applies a transformation function to each element
list<auto> ClassName::map(list<auto> input, code<auto(auto)> transformer) {
    // transformer is guaranteed to accept one argument and return a value
    QoreListNode* result = new QoreListNode(autoTypeInfo);
    // ...
}

//! Returns a typed closure
code<int(int)> ClassName::getMultiplier(int factor) {
    // Return type ensures the closure signature matches
}
```

#### Complex Type Parameters

Typed code types fully support complex nested types:

```cpp
// Code with complex parameter types
nothing ClassName::processData(code<int(hash<string, int>)> processor) {
    // processor takes a hash<string, int> and returns int
}

// Code with complex return types
code<hash<string, list<int>>()> ClassName::getDataFactory() {
    // Returns a closure that produces hash<string, list<int>>
}

// Deeply nested types
nothing ClassName::transform(code<list<hash<string, int>>(list<string>, int)> transformer) {
    // transformer: (list<string>, int) -> list<hash<string, int>>
}
```

#### Or-Nothing Code Types

Use `*code<...>` for optional typed code parameters:

```cpp
nothing ClassName::setCallback(*code<nothing(string)> callback) {
    // callback can be NOTHING or a closure matching the signature
}
```

#### Notes

- The return type in `code<ReturnType(...)>` can be any valid Qore type including `nothing`
- Parameter types support all complex types: `hash<K,V>`, `list<T>`, `softlist<T>`, etc.
- Varargs are supported: `code<int(string, ...)>` for closures accepting variable arguments
- Type checking happens at parse time; incompatible closures will cause parse errors

## Conditional Compilation (`#ifdef`) in QPP Files

The QPP processor **strips `#ifdef`/`#endif` blocks that wrap entire function or class declarations**.
This means you cannot conditionally compile away entire QPP functions or classes.

### What does NOT work

```cpp
// WRONG: qpp strips the #ifdef, so the function is always generated
#ifdef HAVE_FEATURE
string ClassName::optionalMethod() {
    return do_something();
}
#endif
```

### Correct pattern: always define the API, use `#ifdef` inside the body

To maintain a consistent API surface regardless of compile-time features, always define the
function/class and use `#ifdef` inside the body to throw `MISSING-FEATURE-ERROR` when the
feature is unavailable:

```cpp
string ClassName::optionalMethod() {
#ifdef HAVE_FEATURE
    return do_something();
#else
    xsink->raiseException("MISSING-FEATURE-ERROR",
        "this method requires feature X; check Capabilities before calling");
    return QoreValue();
#endif
}
```

This pattern is used throughout the codebase:
- `ql_crypto.qpp`: OpenSSL feature checks with `missing_openssl_feature()`
- `QC_OnnxModel.qpp`: ONNX Runtime availability check

### Stub classes for conditional features

When a class depends on a conditional library, provide a stub private data class in the header
so the class is always registered. Use `#ifdef` inside constructors to throw
`MISSING-FEATURE-ERROR`:

```cpp
// In QC_FeatureClass.h
#ifdef HAVE_FEATURE
class QoreFeatureClass : public AbstractPrivateData {
    // full implementation
};
#else
class QoreFeatureClass : public AbstractPrivateData {
    // stub - methods return nullptr / empty values
};
#endif
```

## Class Creation Checklist

1. [ ] Create `lib/QC_ClassName.qpp`
2. [ ] Create `include/qore/intern/QC_ClassName.h` with `initClassNameClass` declaration
3. [ ] Add to `CMakeLists.txt` QPP sources
4. [ ] Add `#include "qore/intern/QC_ClassName.h"` to `QoreNamespace.cpp`
5. [ ] Add `qns.addSystemClass(initClassNameClass(qns));` to `QoreNamespace.cpp`
6. [ ] Verify no reserved words used as method names
7. [ ] Verify object parameters use `ReferenceHolder` pattern
8. [ ] Build and test

## Common Errors

### "expression has no effect as a statement"
Usually means a reserved word was used as a method name. Rename the method.

### "reference to undefined type"
The class is not registered in `QoreNamespace.cpp`. Add the include and `addSystemClass` call.

### Memory leaks with object parameters
Missing `ReferenceHolder` for object parameters. Add the holder pattern.

## Example: Complete EventLoop Class

See `lib/QC_EventLoop.qpp` for a complete example including:
- Private data class
- Constructor/destructor
- Methods with object parameters
- Proper documentation
