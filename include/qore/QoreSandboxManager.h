/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxManager.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_QORE_SANDBOX_MANAGER_H
#define _QORE_QORE_SANDBOX_MANAGER_H

/** @file QoreSandboxManager.h
    Defines security and sandboxing classes for controlled code execution
*/

#include <qore/common.h>
#include <qore/AbstractPrivateData.h>

#include <sys/socket.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <utility>

// Forward declarations
class ExceptionSink;
class QoreString;
class QoreHashNode;

//! @defgroup sandbox_fs_access_modes Filesystem Access Mode Constants
//! Constants for specifying filesystem access modes in sandboxing
//@{
//! Read access to files
#define QSEC_READ    (1 << 0)
//! Write access to files
#define QSEC_WRITE   (1 << 1)
//! Execute access to files
#define QSEC_EXECUTE (1 << 2)
//! Delete access to files
#define QSEC_DELETE  (1 << 3)
//! Create new files
#define QSEC_CREATE  (1 << 4)
//! All filesystem access modes
#define QSEC_ALL     (QSEC_READ|QSEC_WRITE|QSEC_EXECUTE|QSEC_DELETE|QSEC_CREATE)
//@}

//! @defgroup sandbox_net_protocol_modes Network Protocol Constants
//! Constants for specifying network protocols in sandboxing
//@{
//! TCP protocol
#define QSEC_NET_TCP   (1 << 0)
//! UDP protocol
#define QSEC_NET_UDP   (1 << 1)
//! Unix domain sockets
#define QSEC_NET_UNIX  (1 << 2)
//! All network protocols
#define QSEC_NET_ALL   (QSEC_NET_TCP|QSEC_NET_UDP|QSEC_NET_UNIX)
//@}

//! Manages filesystem access restrictions for sandboxed programs
/** The QoreFilesystemSecurityManager class provides fine-grained control over
    which filesystem paths a sandboxed program can access. It supports:

    - Allow/deny lists with path patterns
    - Sandbox root directories (chroot-like behavior)
    - Access mode control (read, write, execute, delete, create)
    - Automatic path canonicalization and escape prevention

    @section fsm_security Security Features

    The security manager automatically:
    - Resolves all paths to canonical absolute form using realpath()
    - Follows and validates symlinks to prevent escapes
    - Blocks path traversal attacks (../)
    - Validates paths after environment variable expansion

    @par Example:
    @code{.cpp}
    QoreFilesystemSecurityManager fsm;
    fsm.setSandboxRoot("/home/sandbox", xsink);
    fsm.addAllowedPath("/home/sandbox/data", QSEC_READ | QSEC_WRITE, xsink);
    fsm.addDeniedPath("/home/sandbox/secrets", xsink);
    @endcode

    @since Qore 2.1
*/
class QoreFilesystemSecurityManager : public AbstractPrivateData {
public:
    //! Creates a new filesystem security manager with default deny policy
    DLLEXPORT QoreFilesystemSecurityManager();

    //! Destroys the filesystem security manager
    DLLEXPORT virtual ~QoreFilesystemSecurityManager();

    //! Adds an allowed path with specific access modes
    /** @param path The filesystem path to allow (will be canonicalized)
        @param mode Bitmask of QSEC_* flags specifying allowed access modes
        @param xsink Exception sink for errors

        @return 0 on success, -1 on error (exception raised)

        @par Example:
        @code{.cpp}
        fsm.addAllowedPath("/data/uploads", QSEC_READ | QSEC_WRITE, xsink);
        @endcode

        @note The path is canonicalized using realpath(). The path must exist
        for canonicalization to succeed, unless it's being created.
    */
    DLLEXPORT int addAllowedPath(const char* path, int mode, ExceptionSink* xsink);

    //! Adds a denied path (takes precedence over allowed paths)
    /** @param path The filesystem path to deny (will be canonicalized)
        @param xsink Exception sink for errors

        @return 0 on success, -1 on error (exception raised)

        @note Denied paths always take precedence over allowed paths.
        This allows for fine-grained "allow all except" patterns.
    */
    DLLEXPORT int addDeniedPath(const char* path, ExceptionSink* xsink);

    //! Sets a sandbox root directory (chroot-like restriction)
    /** All path access will be restricted to within this directory.
        Symlinks pointing outside the sandbox root will be blocked.

        @param path The root directory for the sandbox
        @param xsink Exception sink for errors

        @return 0 on success, -1 on error (exception raised)

        @par Example:
        @code{.cpp}
        fsm.setSandboxRoot("/home/user/sandbox", xsink);
        @endcode

        @note When a sandbox root is set, all allowed paths must be within
        this root, and the root itself is implicitly allowed with QSEC_ALL.
    */
    DLLEXPORT int setSandboxRoot(const char* path, ExceptionSink* xsink);

    //! Clears the sandbox root directory
    DLLEXPORT void clearSandboxRoot();

    //! Sets whether paths not explicitly listed are allowed or denied
    /** @param allow If true, unlisted paths are allowed; if false (default), denied

        @note The default policy is "deny" for maximum security. Setting this
        to "allow" means only explicitly denied paths will be blocked.
    */
    DLLEXPORT void setDefaultPolicy(bool allow);

    //! Gets the current default policy
    /** @return true if default is allow, false if default is deny
    */
    DLLEXPORT bool getDefaultPolicy() const;

    //! Checks if access is allowed for the given path and mode
    /** This is the main enforcement method called from QoreFile, QoreDir, etc.

        @param path The path to check (will be canonicalized)
        @param mode The access mode requested (bitmask of QSEC_* flags)
        @param xsink Exception sink - raises FILESYSTEM-ACCESS-DENIED if denied

        @return true if access is allowed, false if denied (exception raised)

        @note This method:
        - Canonicalizes the path using realpath()
        - Checks denied paths first (they take precedence)
        - Checks sandbox root containment
        - Checks allowed paths
        - Falls back to default policy
    */
    DLLEXPORT bool checkAccess(const char* path, int mode, ExceptionSink* xsink);

    //! Returns the current configuration as a hash
    /** @return A hash containing:
        - \c "sandbox_root": The sandbox root path or NOTHING
        - \c "default_policy": "allow" or "deny"
        - \c "allowed_paths": List of hashes with "path" and "mode" keys
        - \c "denied_paths": List of denied path strings
    */
    DLLEXPORT QoreHashNode* getConfiguration(ExceptionSink* xsink) const;

    //! Creates a copy with the same restrictions (for child programs)
    /** @return A new QoreFilesystemSecurityManager with the same configuration

        @note Child programs can only make restrictions stricter, not looser.
    */
    DLLEXPORT QoreFilesystemSecurityManager* copy() const;

    //! Clears all rules and resets to default state
    DLLEXPORT void reset();

    //! Copies configuration from another security manager
    /** @param other The security manager to copy from
    */
    DLLEXPORT void copyFrom(const QoreFilesystemSecurityManager& other);

private:
    //! Private implementation
    struct qore_fs_security_private* priv;

    //! Disallow copying via assignment
    QoreFilesystemSecurityManager& operator=(const QoreFilesystemSecurityManager&) = delete;
};

//! Manages network access restrictions for sandboxed programs
/** The QoreNetworkSecurityManager class provides network access control
    for sandboxed programs including:

    - Host pattern matching with wildcards
    - CIDR-based IP range allow/deny lists
    - Port restrictions
    - Built-in private network blocking for SSRF prevention

    @section nsm_security Security Features

    The security manager:
    - Checks IP addresses AFTER DNS resolution to prevent SSRF
    - Supports both IPv4 and IPv6 addresses
    - Can block RFC 1918 private networks and cloud metadata endpoints
    - Prevents DNS rebinding attacks by checking resolved addresses

    @par Example:
    @code{.cpp}
    QoreNetworkSecurityManager nsm;
    nsm.blockPrivateNetworks();
    nsm.addAllowedHost("api.example.com", xsink);
    nsm.addAllowedPort(443, QSEC_NET_TCP);
    @endcode

    @since Qore 2.1
*/
class QoreNetworkSecurityManager : public AbstractPrivateData {
public:
    //! Creates a new network security manager with default deny policy
    DLLEXPORT QoreNetworkSecurityManager();

    //! Destroys the network security manager
    DLLEXPORT virtual ~QoreNetworkSecurityManager();

    //! Adds an allowed hostname pattern
    /** @param pattern Hostname pattern with optional wildcards (e.g., "*.example.com")
        @param xsink Exception sink for errors

        @return 0 on success, -1 on error (exception raised)

        @par Example:
        @code{.cpp}
        nsm.addAllowedHost("api.example.com", xsink);
        nsm.addAllowedHost("*.trusted-domain.org", xsink);
        @endcode

        @note Wildcards only match single domain components.
        "*.example.com" matches "api.example.com" but not "api.sub.example.com".
    */
    DLLEXPORT int addAllowedHost(const char* pattern, ExceptionSink* xsink);

    //! Adds an allowed IP range in CIDR notation
    /** @param cidr IP range in CIDR notation (e.g., "10.0.0.0/8" or "2001:db8::/32")
        @param xsink Exception sink for errors

        @return 0 on success, -1 on error (exception raised)

        @par Example:
        @code{.cpp}
        nsm.addAllowedIPRange("203.0.113.0/24", xsink);   // IPv4
        nsm.addAllowedIPRange("2001:db8::/32", xsink);    // IPv6
        @endcode
    */
    DLLEXPORT int addAllowedIPRange(const char* cidr, ExceptionSink* xsink);

    //! Adds a denied IP range (takes precedence over allowed)
    /** @param cidr IP range in CIDR notation
        @param xsink Exception sink for errors

        @return 0 on success, -1 on error (exception raised)
    */
    DLLEXPORT int addDeniedIPRange(const char* cidr, ExceptionSink* xsink);

    //! Adds an allowed port
    /** @param port Port number to allow (1-65535)
        @param proto Protocol flags (QSEC_NET_TCP, QSEC_NET_UDP, QSEC_NET_ALL)
    */
    DLLEXPORT void addAllowedPort(int port, int proto = QSEC_NET_ALL);

    //! Adds an allowed port range
    /** @param low Lower bound of port range (inclusive)
        @param high Upper bound of port range (inclusive)
        @param proto Protocol flags
    */
    DLLEXPORT void addAllowedPortRange(int low, int high, int proto = QSEC_NET_ALL);

    //! Preset: Blocks all private/internal networks
    /** Blocks the following ranges:
        - 127.0.0.0/8 (localhost)
        - 10.0.0.0/8 (RFC 1918 Class A)
        - 172.16.0.0/12 (RFC 1918 Class B)
        - 192.168.0.0/16 (RFC 1918 Class C)
        - 169.254.0.0/16 (link-local, includes cloud metadata 169.254.169.254)
        - ::1/128 (IPv6 localhost)
        - fe80::/10 (IPv6 link-local)
        - fc00::/7 (IPv6 unique local)

        @note This is essential for SSRF prevention in web applications.
    */
    DLLEXPORT void blockPrivateNetworks();

    //! Preset: Blocks localhost only
    /** Blocks 127.0.0.0/8 and ::1/128
    */
    DLLEXPORT void blockLocalhost();

    //! Preset: Allows only HTTPS connections (port 443)
    DLLEXPORT void allowHTTPSOnly();

    //! Sets default policy for unlisted destinations
    /** @param allow If true, unlisted destinations are allowed; if false (default), denied
    */
    DLLEXPORT void setDefaultPolicy(bool allow);

    //! Gets the current default policy
    DLLEXPORT bool getDefaultPolicy() const;

    //! Checks if a connection is allowed - called AFTER DNS resolution
    /** @param addr The resolved socket address
        @param len Length of the address structure
        @param proto Protocol being used (QSEC_NET_TCP, QSEC_NET_UDP, QSEC_NET_UNIX)
        @param xsink Exception sink - raises NETWORK-ACCESS-DENIED if denied

        @return true if allowed, false if denied (exception raised)

        @note CRITICAL: This method should be called AFTER DNS resolution to
        prevent SSRF attacks where hostnames resolve to internal IP addresses.
    */
    DLLEXPORT bool checkConnect(const struct sockaddr* addr, socklen_t len,
                                int proto, ExceptionSink* xsink);

    //! Checks if binding is allowed
    /** @param addr The address to bind to
        @param len Length of the address structure
        @param proto Protocol being used
        @param xsink Exception sink

        @return true if allowed, false if denied
    */
    DLLEXPORT bool checkBind(const struct sockaddr* addr, socklen_t len,
                             int proto, ExceptionSink* xsink);

    //! Checks if a hostname is in the allowed list (before DNS resolution)
    /** @param hostname The hostname to check
        @param port The port number
        @param proto Protocol being used

        @return true if hostname matches an allowed pattern, false otherwise

        @note This is a preliminary check. The final check using checkConnect()
        MUST still be performed on the resolved IP address.
    */
    DLLEXPORT bool checkHostname(const char* hostname, int port, int proto) const;

    //! Returns the current configuration as a hash
    DLLEXPORT QoreHashNode* getConfiguration(ExceptionSink* xsink) const;

    //! Creates a copy with the same restrictions
    DLLEXPORT QoreNetworkSecurityManager* copy() const;

    //! Clears all rules and resets to default state
    DLLEXPORT void reset();

    //! Copies configuration from another security manager
    /** @param other The security manager to copy from
    */
    DLLEXPORT void copyFrom(const QoreNetworkSecurityManager& other);

private:
    //! Private implementation
    struct qore_net_security_private* priv;

    //! Disallow copying via assignment
    QoreNetworkSecurityManager& operator=(const QoreNetworkSecurityManager&) = delete;
};

//! Unified sandbox manager combining filesystem, network, and resource controls
/** The QoreSandboxManager class provides a unified interface for all
    sandboxing features including:

    - Filesystem access control via QoreFilesystemSecurityManager
    - Network access control via QoreNetworkSecurityManager
    - Resource limits (memory, CPU time, wall time, threads)
    - Safe interruption of sandboxed code

    @par Example:
    @code{.cpp}
    QoreSandboxManager sm;
    sm.filesystem().setSandboxRoot("/sandbox", xsink);
    sm.network().blockPrivateNetworks();
    sm.setMemoryLimit(100 * 1024 * 1024);  // 100MB
    sm.setWallTimeLimit(30000);            // 30 seconds

    QoreProgram* pgm = new QoreProgram(PO_NEW_STYLE);
    pgm->setSandboxManager(&sm);
    @endcode

    @since Qore 2.1
*/
class QoreSandboxManager : public AbstractPrivateData {
public:
    //! Creates a new sandbox manager with default settings
    DLLEXPORT QoreSandboxManager();

    //! Destroys the sandbox manager
    DLLEXPORT virtual ~QoreSandboxManager();

    //! Gets the filesystem security manager
    /** @return Reference to the filesystem security manager
    */
    DLLEXPORT QoreFilesystemSecurityManager& filesystem();

    //! Gets the filesystem security manager (const version)
    DLLEXPORT const QoreFilesystemSecurityManager& filesystem() const;

    //! Gets the network security manager
    /** @return Reference to the network security manager
    */
    DLLEXPORT QoreNetworkSecurityManager& network();

    //! Gets the network security manager (const version)
    DLLEXPORT const QoreNetworkSecurityManager& network() const;

    //! Sets maximum memory usage in bytes
    /** @param bytes Maximum memory allocation in bytes (0 = unlimited)

        @note This limit is checked when allocating memory in sandboxed code.
        Exceeding the limit raises a SANDBOX-MEMORY-LIMIT exception.
    */
    DLLEXPORT void setMemoryLimit(size_t bytes);

    //! Gets the memory limit
    DLLEXPORT size_t getMemoryLimit() const;

    //! Sets maximum CPU time in milliseconds
    /** @param ms Maximum CPU time in milliseconds (0 = unlimited)

        @note CPU time is checked periodically during execution.
        Exceeding the limit raises a SANDBOX-TIMEOUT exception.
    */
    DLLEXPORT void setCPUTimeLimit(int64 ms);

    //! Gets the CPU time limit
    DLLEXPORT int64 getCPUTimeLimit() const;

    //! Sets maximum wall clock time in milliseconds
    /** @param ms Maximum wall clock time in milliseconds (0 = unlimited)

        @note Wall time is checked periodically during execution.
        Exceeding the limit raises a SANDBOX-TIMEOUT exception.
    */
    DLLEXPORT void setWallTimeLimit(int64 ms);

    //! Gets the wall time limit
    DLLEXPORT int64 getWallTimeLimit() const;

    //! Sets maximum number of threads
    /** @param count Maximum number of threads (0 = unlimited)

        @note This limits threads created by the background statement.
        Exceeding the limit raises a SANDBOX-THREAD-LIMIT exception.
    */
    DLLEXPORT void setMaxThreads(int count);

    //! Gets the max threads limit
    DLLEXPORT int getMaxThreads() const;

    //! Sets maximum recursion depth
    /** @param depth Maximum recursion depth (0 = unlimited)

        @note This limits function call stack depth.
        Exceeding the limit raises a SANDBOX-RECURSION-LIMIT exception.
    */
    DLLEXPORT void setMaxRecursionDepth(int depth);

    //! Gets the max recursion depth
    DLLEXPORT int getMaxRecursionDepth() const;

    //! Requests graceful interruption of sandboxed code
    /** This sets an interrupt flag that is checked at various points
        during execution (statement boundaries, blocking operations).

        Interrupted code receives a PROGRAM-INTERRUPTED exception.

        When interrupt is requested, all registered cancel callbacks are invoked
        to cancel any ongoing blocking operations (e.g., database queries).

        @note This is the preferred way to stop runaway sandboxed code.
        It allows for clean shutdown without corrupting state.
    */
    DLLEXPORT void requestInterrupt();

    //! Clears the interrupt request
    DLLEXPORT void clearInterrupt();

    //! Cancel callback function type
    /** Cancel callbacks are called when requestInterrupt() is invoked.
        They should attempt to cancel any ongoing blocking operation.

        @return true if a cancel was attempted, false otherwise
    */
    typedef std::function<bool()> cancel_callback_t;

    //! Registers a cancel callback for interruptible operations
    /** Call this before starting a blocking operation that should be cancellable.
        The callback will be invoked when requestInterrupt() is called.

        @param context Unique context pointer (used to unregister)
        @param callback The callback function to invoke on interrupt

        @note Thread-safe. The callback may be invoked from any thread.
        @note The callback should be safe to call even if the operation
              has already completed.
    */
    DLLEXPORT void registerCancelCallback(void* context, cancel_callback_t callback);

    //! Unregisters a cancel callback
    /** Call this after the blocking operation completes.

        @param context The context pointer used during registration
    */
    DLLEXPORT void unregisterCancelCallback(void* context);

    //! Checks if interrupt has been requested
    /** @return true if an interrupt has been requested
    */
    DLLEXPORT bool isInterruptRequested() const;

    //! Checks if interrupt has been requested and raises exception if so
    /** This is a convenience method for I/O operations that need to check
        for interrupts and raise an exception if interrupted.

        @param xsink Exception sink for error reporting
        @param operation Description of the operation being interrupted (e.g., "reading file")

        @return true if interrupted (exception raised), false otherwise
    */
    DLLEXPORT bool checkIOInterrupt(ExceptionSink* xsink, const char* operation = "I/O operation") const;

    //! Returns the full configuration as a hash
    /** @return A hash containing all sandbox configuration including
        filesystem, network, and resource limit settings
    */
    DLLEXPORT QoreHashNode* getConfiguration(ExceptionSink* xsink) const;

    //! Checks if filesystem access is allowed
    /** @param path The path to check
        @param mode The access mode (QSEC_READ, QSEC_WRITE, etc.)
        @param xsink Exception sink for error reporting

        @return true if access is allowed, false if denied (exception raised)
    */
    DLLEXPORT bool checkFilesystemAccess(const char* path, int mode, ExceptionSink* xsink);

    //! Checks if network access is allowed
    /** @param addr The address to check
        @param len Length of the address structure
        @param proto Protocol (QSEC_NET_TCP, QSEC_NET_UDP, etc.)
        @param xsink Exception sink for error reporting

        @return true if access is allowed, false if denied (exception raised)
    */
    DLLEXPORT bool checkNetworkAccess(const struct sockaddr* addr, socklen_t len,
                                       int proto, ExceptionSink* xsink);

    //! Creates a copy with the same or stricter restrictions
    /** @return A new QoreSandboxManager with the same configuration

        @note Used when creating child programs. Children can only
        make restrictions stricter, never looser.
    */
    DLLEXPORT QoreSandboxManager* copy() const;

    //! Creates a preset for maximum restrictions ("lockdown" mode)
    /** @return A new QoreSandboxManager configured for maximum security

        Lockdown mode:
        - No filesystem access (empty allow list)
        - No network access (empty allow list)
        - Memory limit: 10MB
        - CPU time limit: 1000ms
        - Wall time limit: 5000ms
        - Max threads: 1
        - Max recursion: 100
    */
    DLLEXPORT static QoreSandboxManager* createLockdown();

    //! Creates a preset safe for web application sandboxing
    /** @return A new QoreSandboxManager with web-safe defaults

        Web-safe mode:
        - No filesystem access
        - Network allowed but private networks blocked
        - Memory limit: 100MB
        - CPU time limit: 10000ms
        - Wall time limit: 30000ms
        - Max threads: 4
        - Max recursion: 200
    */
    DLLEXPORT static QoreSandboxManager* createWebSafe();

private:
    //! Filesystem security manager
    QoreFilesystemSecurityManager fs_mgr;

    //! Network security manager
    QoreNetworkSecurityManager net_mgr;

    //! Memory limit in bytes (0 = unlimited)
    size_t memory_limit;

    //! CPU time limit in milliseconds (0 = unlimited)
    int64 cpu_time_limit;

    //! Wall time limit in milliseconds (0 = unlimited)
    int64 wall_time_limit;

    //! Maximum threads (0 = unlimited)
    int max_threads;

    //! Maximum recursion depth (0 = unlimited)
    int max_recursion;

    //! Interrupt flag
    std::atomic<bool> interrupt_requested;

    //! Mutex for cancel callbacks
    mutable std::mutex cancel_mutex;

    //! Registered cancel callbacks
    std::vector<std::pair<void*, cancel_callback_t>> cancel_callbacks;

    //! Disallow copying via assignment
    QoreSandboxManager& operator=(const QoreSandboxManager&) = delete;
};

//! RAII helper that acquires a ref'd sandbox manager for the current running program
/** Acquires a strong reference under lock to prevent use-after-free
    when the sandbox manager is cleared during program cleanup.

    @par Example:
    @code{.cpp}
    QoreSandboxManagerHelper smh;
    if (smh) {
        smh->checkIOInterrupt(xsink, "reading file");
    }
    @endcode

    @since Qore 2.1
*/
class DLLEXPORT QoreSandboxManagerHelper {
public:
    //! Acquires a ref'd sandbox manager for the current program
    QoreSandboxManagerHelper();

    //! Acquires a ref'd sandbox manager for the given program
    /** @param pgm the program to get the sandbox manager from; if nullptr, no reference is acquired
    */
    QoreSandboxManagerHelper(QoreProgram* pgm);

    //! Releases the reference if held
    ~QoreSandboxManagerHelper();

    //! Returns true if a valid reference is held
    operator bool() const {
        return ptr != nullptr;
    }

    //! Provides access to the sandbox manager
    QoreSandboxManager* operator->() const {
        return ptr;
    }

    //! Returns the raw pointer
    QoreSandboxManager* get() const {
        return ptr;
    }

private:
    QoreSandboxManager* ptr;

    // non-copyable
    QoreSandboxManagerHelper(const QoreSandboxManagerHelper&) = delete;
    QoreSandboxManagerHelper& operator=(const QoreSandboxManagerHelper&) = delete;
};


//! Polling interval for blocking I/O operations in milliseconds
/** This constant defines how frequently blocking I/O operations check for
    interrupt requests. It matches the polling interval used for threading
    primitives (500ms).
*/
#define QORE_IO_POLL_INTERVAL_MS 500

#endif // _QORE_QORE_SANDBOX_MANAGER_H
