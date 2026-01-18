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

Use `ns=Qore` to place the class in the Qore namespace:
```cpp
qclass ClassName [dom=NETWORK; arg=ClassNamePriv* priv; ns=Qore];
```

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
- QPP emits an `init_enum_<EnumName>()` function; for system enums, register it in
  `lib/QoreNamespace.cpp` using `addSystemEnum(init_enum_<EnumName>(qns));`.

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
