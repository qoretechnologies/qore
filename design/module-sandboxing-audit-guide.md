# Module Sandboxing Audit and Implementation Guide

## Purpose

This document supports:
1. **Auditing** existing Qore modules (C++ and Qore-language) for sandboxing compliance
2. **Implementing** missing or incomplete sandboxing functionality

## Security Context

Qore's sandboxing system enables **safe execution of untrusted or partially trusted code**. Use cases include:

- **Multi-tenant SaaS platforms**: Users upload and execute custom Qore code
- **Plugin/extension systems**: Third-party code runs within a host application
- **Web applications**: User-provided scripts execute server-side
- **IDE/notebook environments**: Interactive code execution with safety constraints
- **CI/CD systems**: Running user-defined build/test scripts

### Threat Model

The sandbox defends against malicious or buggy user code attempting to:

| Threat | Mitigation |
|--------|------------|
| **Read sensitive files** (credentials, configs, other users' data) | Filesystem security manager |
| **Write/delete files** (corruption, defacement, persistence) | Filesystem security manager |
| **SSRF attacks** (accessing internal services, cloud metadata) | Network security manager |
| **Data exfiltration** (sending data to external servers) | Network security manager |
| **Denial of service** (infinite loops, memory exhaustion, fork bombs) | Resource limits |
| **Hanging the system** (blocking operations that never complete) | Interrupt mechanism + timeouts |

### Security Principle

**Every module operation that accesses external resources must respect sandbox restrictions.** A single unprotected operation creates a sandbox escape vulnerability that compromises the entire security model.

---

## Sandboxing Architecture Overview

Qore's sandboxing system consists of four subsystems, all managed through `QoreSandboxManager`:

| Subsystem | Class | Purpose |
|-----------|-------|---------|
| Filesystem Security | `QoreFilesystemSecurityManager` | Controls file/directory access |
| Network Security | `QoreNetworkSecurityManager` | Controls network connections |
| Resource Limits | `QoreSandboxManager` | Memory, CPU, threads, recursion |
| Interrupt Mechanism | `QoreSandboxManager` | Graceful termination of operations |

### How Sandboxing is Activated

1. A `QoreSandboxManager` is created and configured with policies
2. It's attached to a `QoreProgram` via `setSandboxManager()`
3. Code executing in that program context is subject to all configured restrictions
4. Modules access the sandbox manager via `runtime_get_sandbox_manager()`

### Module Loading in Sandboxed Environments

When a module is loaded into a sandboxed program, all operations performed by that module on behalf of user code must respect sandbox restrictions. This is why module auditing is critical:

- **User code calls module function** → Module performs I/O → **Sandbox must be checked**
- If the module bypasses sandbox checks, malicious user code can exploit this to escape the sandbox

Modules that have not been audited and verified for sandbox compliance should not be made available in sandboxed environments.

### Key Principle: Sandbox Manager May Be Absent

When `runtime_get_sandbox_manager()` returns `nullptr`, no sandboxing is active. Modules must:
- Check for this and use fast paths when no sandbox is present
- Never fail or behave incorrectly when sandboxing is disabled

---

## Part 1: Filesystem Security

### 1.1 How It Works

`QoreFilesystemSecurityManager` controls access to files and directories:

- **Allow/Deny Lists**: Paths can be explicitly allowed or denied
- **Access Modes**: `QSEC_READ`, `QSEC_WRITE`, `QSEC_EXECUTE`, `QSEC_DELETE`, `QSEC_CREATE`
- **Sandbox Root**: Optional chroot-like restriction to a directory tree
- **Default Policy**: Deny-by-default (configurable)
- **Path Canonicalization**: All paths resolved via `realpath()` to prevent traversal attacks

### 1.2 Audit Checklist - Filesystem Operations

Search for these patterns that require filesystem security checks:

#### C++ Modules

| Pattern to Find | Required Check |
|-----------------|----------------|
| `fopen`, `open`, `creat` | `checkAccess()` with appropriate mode |
| `stat`, `lstat`, `fstat`, `access` | `checkAccess()` with `QSEC_READ` |
| `unlink`, `remove` | `checkAccess()` with `QSEC_DELETE` |
| `rename` | `checkAccess()` on both paths |
| `mkdir`, `mkdtemp` | `checkAccess()` with `QSEC_CREATE` on parent |
| `rmdir` | `checkAccess()` with `QSEC_DELETE` |
| `opendir`, `readdir` | `checkAccess()` with `QSEC_READ` |
| `chmod`, `chown`, `utime` | `checkAccess()` with `QSEC_WRITE` |
| `link`, `symlink` | `checkAccess()` with `QSEC_CREATE` on target |
| `readlink` | `checkAccess()` with `QSEC_READ` |
| `truncate`, `ftruncate` | `checkAccess()` with `QSEC_WRITE` |
| `mmap` with file | `checkAccess()` based on mmap flags |
| `execve`, `execl`, `system` | `checkAccess()` with `QSEC_EXECUTE` |
| `dlopen` | `checkAccess()` with `QSEC_EXECUTE` |
| `QoreFile::open` | Should already check (verify) |
| `QoreDir` operations | Should already check (verify) |

#### Qore-Language Modules

| Pattern to Find | Required Check |
|-----------------|----------------|
| `File::open()`, `open_file()` | Uses QoreFile (verify checks) |
| `ReadOnlyFile` constructor | Uses QoreFile (verify checks) |
| `Dir` operations | Uses QoreDir (verify checks) |
| `stat()`, `lstat()`, `hstat()` | Verify sandbox check |
| `unlink()`, `remove_file()` | Verify sandbox check |
| `rename()`, `move_file()` | Verify sandbox check |
| `mkdir()`, `rmdir()` | Verify sandbox check |
| `glob()` | Verify sandbox check |
| `system()`, `backquote` | Verify sandbox check on executable |

### 1.3 Implementation Pattern - Filesystem Check

```cpp
#include <qore/QoreSandboxManager.h>

int myFileOperation(const char* path, ExceptionSink* xsink) {
    // Get sandbox manager (may be nullptr if no sandbox active)
    QoreSandboxManager* sm = runtime_get_sandbox_manager();

    if (sm) {
        QoreFilesystemSecurityManager* fs = sm->getFilesystemSecurityManager();
        if (fs) {
            // Check access before operation
            // Mode should match what operation actually does
            if (!fs->checkAccess(path, QSEC_READ, xsink)) {
                return -1;  // Exception raised: FILESYSTEM-ACCESS-DENIED
            }
        }
    }

    // Proceed with operation
    return do_actual_file_operation(path);
}
```

### 1.4 Common Filesystem Audit Findings

1. **Missing checks on helper functions**: Internal helpers that open files may bypass checks
2. **Temporary files**: `/tmp` operations may not be checked
3. **Configuration files**: Module config loading may not respect sandbox
4. **Logging**: File-based logging may bypass sandbox
5. **Caching**: File-based caches may not be checked
6. **Path construction**: Concatenating paths without re-checking final path

---

## Part 2: Network Security

### 2.1 How It Works

`QoreNetworkSecurityManager` controls network access:

- **IP Allow/Deny Lists**: CIDR notation for IPv4 and IPv6
- **Hostname Patterns**: Wildcards supported (e.g., `*.example.com`)
- **Port Restrictions**: Per-protocol port ranges
- **Protocol Control**: TCP, UDP, Unix sockets independently controlled
- **SSRF Prevention**: `blockPrivateNetworks()` blocks RFC 1918, link-local, metadata endpoints
- **Default Policy**: Deny-by-default (configurable)

### 2.2 Audit Checklist - Network Operations

Search for these patterns that require network security checks:

#### C++ Modules

| Pattern to Find | Required Check |
|-----------------|----------------|
| `connect()` | `checkConnect()` |
| `bind()` | `checkBind()` |
| `socket()` + connection | Check before connect |
| `getaddrinfo()` + connect | `checkHostname()` before DNS, `checkConnect()` after |
| `gethostbyname()` + connect | `checkHostname()` before DNS, `checkConnect()` after |
| `QoreSocket::connect` | Should already check (verify) |
| `QoreHttpClientObject` | Should already check (verify) |
| HTTP client libraries (curl, etc.) | Must wrap with checks |
| Database client libraries | Must check connection endpoints |
| Message queue clients | Must check broker endpoints |
| `sendto()` with address | `checkConnect()` on destination |

#### Qore-Language Modules

| Pattern to Find | Required Check |
|-----------------|----------------|
| `Socket::connect()` | Uses QoreSocket (verify checks) |
| `HTTPClient` constructor/connect | Verify sandbox check |
| `FtpClient` | Verify sandbox check |
| Database connections (`Datasource`) | Verify sandbox check |
| Any URL-based operations | Verify sandbox check |

### 2.3 Implementation Pattern - Network Check

```cpp
#include <qore/QoreSandboxManager.h>

int myConnectOperation(const char* hostname, int port, ExceptionSink* xsink) {
    QoreSandboxManager* sm = runtime_get_sandbox_manager();

    if (sm) {
        QoreNetworkSecurityManager* net = sm->getNetworkSecurityManager();
        if (net) {
            // Pre-DNS check (catches obvious violations early)
            if (!net->checkHostname(hostname, port, QSEC_NET_TCP, xsink)) {
                return -1;  // Exception raised: NETWORK-ACCESS-DENIED
            }
        }
    }

    // Perform DNS resolution
    struct addrinfo* result = resolve_hostname(hostname, port);

    // Post-DNS check (prevents DNS rebinding attacks)
    if (sm) {
        QoreNetworkSecurityManager* net = sm->getNetworkSecurityManager();
        if (net) {
            if (!net->checkConnect(result->ai_addr, result->ai_addrlen,
                                   QSEC_NET_TCP, xsink)) {
                freeaddrinfo(result);
                return -1;  // Exception raised: NETWORK-ACCESS-DENIED
            }
        }
    }

    // Proceed with connection
    return do_actual_connect(result);
}
```

### 2.4 DNS Rebinding Prevention

**Critical**: Network checks must happen AFTER DNS resolution, not just before.

Attack scenario without post-resolution check:
1. Attacker controls DNS for `evil.com`
2. First DNS query returns allowed IP (passes `checkHostname`)
3. Attacker changes DNS to return `169.254.169.254` (cloud metadata)
4. Connection goes to metadata endpoint

**Solution**: Always call `checkConnect()` on the resolved `sockaddr` before connecting.

### 2.5 Common Network Audit Findings

1. **Missing post-DNS checks**: Only checking hostname, not resolved IP
2. **Redirect following**: HTTP redirects may go to blocked destinations
3. **Proxy connections**: SOCKS/HTTP proxy may bypass checks
4. **Unix sockets**: Often forgotten in network security checks
5. **UDP operations**: May be checked differently than TCP
6. **Connection pooling**: Reused connections may bypass checks on subsequent use
7. **Library callbacks**: HTTP libraries with callbacks may connect without checks

---

## Part 3: Resource Limits

### 3.1 How It Works

Resource limits are enforced by the Qore runtime, but modules can contribute to or bypass them:

| Limit | Enforcement Point | Module Responsibility |
|-------|-------------------|----------------------|
| Memory (`memory_limit`) | Allocation tracking | Report large allocations |
| CPU Time (`cpu_time_limit`) | Periodic runtime check | Yield periodically in tight loops |
| Wall Time (`wall_time_limit`) | Periodic runtime check | Don't block indefinitely |
| Threads (`max_threads`) | Thread creation | Use Qore's threading APIs |
| Recursion (`max_recursion`) | Call stack tracking | Avoid deep native recursion |

### 3.2 Audit Checklist - Resource Consumption

#### C++ Modules

| Pattern to Find | Concern |
|-----------------|---------|
| `malloc`, `new`, large allocations | May bypass memory tracking |
| `mmap` for large regions | May bypass memory tracking |
| Tight loops without yields | May bypass CPU time checks |
| `sleep()`, blocking syscalls | May bypass wall time checks |
| `pthread_create` | May bypass thread limits |
| Deep recursion | May bypass recursion limits |
| Caching large data structures | Memory growth over time |

#### Qore-Language Modules

| Pattern to Find | Concern |
|-----------------|---------|
| Building large data structures | Memory consumption |
| Infinite loops | CPU time |
| `background` statements | Thread creation |
| Recursive functions | Recursion depth |

### 3.3 Implementation Pattern - Resource Awareness

```cpp
// For tight loops that may run long, periodically check for timeout
void processLargeDataset(ExceptionSink* xsink) {
    QoreSandboxManager* sm = runtime_get_sandbox_manager();

    int iteration = 0;
    for (auto& item : large_dataset) {
        // Check every N iterations to avoid overhead
        if (sm && (++iteration % 1000) == 0) {
            if (sm->isInterruptRequested()) {
                xsink->raiseException("PROGRAM-INTERRUPTED",
                    "operation interrupted");
                return;
            }
            // Also gives runtime a chance to check time limits
        }

        process_item(item);
    }
}
```

---

## Part 4: Interrupt Mechanism

### 4.1 How It Works

The interrupt mechanism allows graceful termination of operations:

1. External code calls `sm->requestInterrupt()`
2. `interrupt_requested` flag is set (atomic)
3. Running code checks flag and raises `PROGRAM-INTERRUPTED`
4. Operations clean up and return

### 4.2 Where Interrupts Must Be Checked

Interrupts should be checked:

1. **Before** potentially blocking operations
2. **During** long-running operations (via polling)
3. At **loop boundaries** in iterative algorithms

### 4.3 Audit Checklist - Blocking Operations

#### C++ Modules

| Pattern to Find | Required Interrupt Support |
|-----------------|---------------------------|
| `read()`, `recv()`, `recvfrom()` | Poll with timeout, check interrupt |
| `write()`, `send()`, `sendto()` | Poll with timeout, check interrupt |
| `select()`, `poll()`, `epoll_wait()` | Use short timeout, check interrupt |
| `accept()` | Poll with timeout, check interrupt |
| `flock()`, `lockf()` | Poll with timeout, check interrupt |
| `waitpid()` | Use `WNOHANG` + poll, check interrupt |
| `sem_wait()`, `pthread_mutex_lock()` | Use timed variants, check interrupt |
| `sleep()`, `usleep()`, `nanosleep()` | Break into chunks, check interrupt |
| Database queries | Use async API or cancel callback |
| HTTP requests | Use timeouts, check interrupt |
| Any blocking library call | Wrap with polling/timeout |

#### Qore-Language Modules

| Pattern to Find | Required Interrupt Support |
|-----------------|---------------------------|
| `sleep()`, `usleep()` | Runtime handles (verify) |
| `Mutex::lock()` | Runtime handles (verify) |
| `Queue::get()` | Runtime handles (verify) |
| `Counter::waitForZero()` | Runtime handles (verify) |
| Blocking I/O | Uses Qore I/O classes (verify) |

### 4.4 Implementation Patterns - Interrupt Support

#### Pattern A: Pre-Operation Check

```cpp
int quickOperation(ExceptionSink* xsink) {
    // Check before starting
    if (qore_check_io_interrupt(xsink)) {
        return -1;  // Exception already raised
    }

    // Operation completes quickly, no further checks needed
    return do_quick_thing();
}
```

#### Pattern B: Polling During Blocking Operation

```cpp
ssize_t blockingRead(int fd, void* buf, size_t len, int timeout_ms,
                     ExceptionSink* xsink) {
    QoreSandboxManager* sm = runtime_get_sandbox_manager();

    // Fast path: no sandbox
    if (!sm) {
        return blocking_read_with_timeout(fd, buf, len, timeout_ms);
    }

    // Check before starting
    if (sm->isInterruptRequested()) {
        xsink->raiseException("PROGRAM-INTERRUPTED", "I/O interrupted");
        return -1;
    }

    // Poll with short timeouts
    int remaining = timeout_ms;
    while (remaining > 0 || timeout_ms < 0) {
        if (sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "I/O interrupted");
            return -1;
        }

        int chunk = QORE_IO_POLL_INTERVAL_MS;  // 500ms
        if (timeout_ms >= 0 && remaining < chunk) {
            chunk = remaining;
        }

        struct pollfd pfd = {fd, POLLIN, 0};
        int rv = poll(&pfd, 1, chunk);

        if (rv > 0) {
            return read(fd, buf, len);
        }
        if (rv < 0 && errno != EINTR) {
            return -1;  // Real error
        }

        if (timeout_ms >= 0) {
            remaining -= chunk;
        }
    }

    errno = ETIMEDOUT;
    return -1;
}
```

#### Pattern C: Cancel Callback for Library Operations

```cpp
// For libraries that support async cancellation
class DatabaseQuery {
    db_handle_t* handle;

public:
    int execute(const char* sql, ExceptionSink* xsink) {
        QoreSandboxManager* sm = runtime_get_sandbox_manager();

        if (!sm) {
            return db_query_sync(handle, sql);  // Fast path
        }

        if (sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "query interrupted");
            return -1;
        }

        // Register cancel callback
        sm->registerCancelCallback(this, [](void* ctx) {
            auto* self = static_cast<DatabaseQuery*>(ctx);
            db_cancel_query(self->handle);
        });

        // Execute query
        int result = db_query_sync(handle, sql);

        // Unregister callback
        sm->unregisterCancelCallback(this);

        // Check if we were interrupted
        if (sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "query interrupted");
            return -1;
        }

        return result;
    }
};
```

### 4.5 Common Interrupt Audit Findings

1. **No interrupt checks**: Operations block indefinitely
2. **Check only at start**: Long operations don't check during execution
3. **Missing cleanup on interrupt**: Resources leaked when interrupted
4. **Polling interval too long**: Poor responsiveness
5. **Polling interval too short**: Excessive overhead
6. **No cancel callback**: Libraries that support cancellation not utilizing it
7. **Swallowed exceptions**: `PROGRAM-INTERRUPTED` caught and not re-raised

---

## Part 5: Audit Methodology

### 5.1 Static Analysis Approach

1. **Identify entry points**: All public functions/methods in the module
2. **Trace to system calls**: Follow code paths to I/O and blocking operations
3. **Check for sandbox awareness**: Verify `runtime_get_sandbox_manager()` is called
4. **Verify correct checks**: Ensure appropriate security manager method is called
5. **Check error handling**: Verify exceptions are propagated correctly

### 5.2 Search Patterns for C++ Modules

```bash
# Find potential filesystem operations
grep -rn "fopen\|open(\|creat\|unlink\|remove\|rename\|mkdir\|rmdir\|stat\|chmod\|chown" src/

# Find potential network operations
grep -rn "connect(\|bind(\|socket(\|accept(\|getaddrinfo\|gethostbyname" src/

# Find potential blocking operations
grep -rn "read(\|write(\|recv\|send\|select\|poll\|epoll\|sleep\|usleep\|wait\|lock" src/

# Find existing sandbox checks (should appear near above patterns)
grep -rn "runtime_get_sandbox_manager\|QoreSandboxManager\|checkAccess\|checkConnect" src/

# Find exception sinks (entry points)
grep -rn "ExceptionSink" src/*.h
```

### 5.3 Search Patterns for Qore Modules

```bash
# Find potential filesystem operations
grep -rn "File\|Dir\|open\|stat\|unlink\|rename\|mkdir\|rmdir\|glob" src/*.qm src/*.q

# Find potential network operations
grep -rn "Socket\|HTTPClient\|FtpClient\|Datasource\|connect" src/*.qm src/*.q

# Find potential blocking operations
grep -rn "sleep\|Mutex\|lock\|Queue\|Counter\|wait" src/*.qm src/*.q
```

### 5.4 Dynamic Testing Approach

1. **Create restrictive sandbox**:
   ```qore
   QoreSandboxManager sm = QoreSandboxManager::createLockdown();
   ```

2. **Exercise all module functions**: Each should either:
   - Succeed (if no restricted operations)
   - Raise appropriate sandbox exception
   - Never block indefinitely

3. **Test interrupt responsiveness**:
   ```qore
   background sub() {
       sleep(100ms);
       sm.requestInterrupt();
   }();

   # This should fail within ~600ms (500ms poll + overhead)
   module_blocking_operation();
   ```

### 5.5 Audit Report Template

**Note**: Gaps identified in this audit represent security vulnerabilities. Modules with unmitigated gaps should not be loaded in sandbox environments until remediated.

```markdown
# Module Sandboxing Audit Report

## Module Information
- **Name**:
- **Version**:
- **Type**: C++ / Qore
- **Audit Date**:
- **Safe for Sandbox Use**: Yes / No / Conditional

## Filesystem Security
- [ ] All file operations checked
- [ ] Path canonicalization used
- [ ] Temporary file operations checked
- **Gaps Found**:
- **Severity**: Critical / High / Medium / Low / None

## Network Security
- [ ] All connections checked
- [ ] Post-DNS resolution checks present
- [ ] Redirect following checked
- **Gaps Found**:
- **Severity**: Critical / High / Medium / Low / None

## Resource Limits
- [ ] Large allocations tracked
- [ ] Tight loops yield periodically
- [ ] No unbounded native threads
- **Gaps Found**:
- **Severity**: Critical / High / Medium / Low / None

## Interrupt Support
- [ ] Pre-operation checks present
- [ ] Polling during blocking operations
- [ ] Cancel callbacks registered where applicable
- [ ] Cleanup on interrupt
- **Gaps Found**:
- **Severity**: Critical / High / Medium / Low / None

## Summary
- **Compliance Level**: Full / Partial / None
- **Highest Severity Finding**:
- **Recommendation**: Safe to use / Do not use in sandbox / Use with restrictions

## Specific Findings

| # | Location | Description | Severity | Remediation |
|---|----------|-------------|----------|-------------|
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
```

---

## Part 6: Implementation Priorities

**Every gap in sandboxing support is a potential security vulnerability.** When untrusted code can bypass sandbox restrictions, the entire security model is compromised.

When implementing sandboxing support, prioritize in this order:

### Priority 1: Security-Critical (Must Fix Immediately)

These gaps allow untrusted code to directly compromise the system or access unauthorized data:

- **Network SSRF vectors**: Any unprotected outbound connection enables attackers to access internal services, cloud metadata endpoints (169.254.169.254), or exfiltrate data
- **Filesystem escape**: Access outside sandbox root exposes credentials, other users' data, system files
- **Arbitrary file write**: Can lead to code execution, configuration tampering, or data destruction
- **Credential exposure**: Access to API keys, database passwords, certificates

### Priority 2: Denial of Service (Should Fix)

These gaps allow untrusted code to degrade or disable the system:

- **Indefinite blocking**: Operations that never timeout can hang worker threads/processes
- **Resource exhaustion**: Unbounded memory/thread usage can crash the host
- **Non-interruptible operations**: Malicious code that can't be stopped
- **Fork/exec without limits**: Process creation outside sandbox control

### Priority 3: Completeness (Fix When Possible)

These are lower risk but should still be addressed for defense in depth:

- **Minor filesystem operations**: stat, readdir on non-sensitive paths (information disclosure)
- **Localhost-only network**: Operations limited to loopback (still risky in containerized environments)
- **Timing/existence leaks**: Can reveal information about system configuration

---

## Appendix A: Exception Reference

| Exception | Subsystem | Meaning |
|-----------|-----------|---------|
| `FILESYSTEM-ACCESS-DENIED` | Filesystem | Path access blocked by policy |
| `NETWORK-ACCESS-DENIED` | Network | Connection blocked by policy |
| `PROGRAM-INTERRUPTED` | Interrupt | Operation interrupted by request |
| `SANDBOX-MEMORY-LIMIT` | Resources | Memory allocation would exceed limit |
| `SANDBOX-TIMEOUT` | Resources | CPU or wall time limit exceeded |
| `SANDBOX-THREAD-LIMIT` | Resources | Thread creation would exceed limit |
| `SANDBOX-RECURSION-LIMIT` | Resources | Call depth would exceed limit |
| `SANDBOX-PATH-ERROR` | Filesystem | Path canonicalization failed |

## Appendix B: Header Reference

```cpp
// Main sandbox header
#include <qore/QoreSandboxManager.h>

// Key functions
QoreSandboxManager* runtime_get_sandbox_manager();
bool qore_check_io_interrupt(ExceptionSink* xsink = nullptr);

// Constants
#define QORE_IO_POLL_INTERVAL_MS 500

// Access mode flags
#define QSEC_READ    (1 << 0)
#define QSEC_WRITE   (1 << 1)
#define QSEC_EXECUTE (1 << 2)
#define QSEC_DELETE  (1 << 3)
#define QSEC_CREATE  (1 << 4)
#define QSEC_ALL     (QSEC_READ | QSEC_WRITE | QSEC_EXECUTE | QSEC_DELETE | QSEC_CREATE)

// Network protocol flags
#define QSEC_NET_TCP  (1 << 0)
#define QSEC_NET_UDP  (1 << 1)
#define QSEC_NET_UNIX (1 << 2)
#define QSEC_NET_ALL  (QSEC_NET_TCP | QSEC_NET_UDP | QSEC_NET_UNIX)
```

## Appendix C: Related Documentation

- `design/interruptible-io-module-guide.md` - Detailed patterns for interrupt support in binary modules
- `doxygen/lang/222_sandboxing.dox.tmpl` - User-facing sandboxing documentation
- `examples/test/qore/security/sandbox-manager.qtest` - Test cases demonstrating expected behavior
