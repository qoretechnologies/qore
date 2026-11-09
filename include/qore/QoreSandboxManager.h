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
class QoreProgram;
class ResolvedCallReferenceNode;
class QoreSandboxMemoryAccount;

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

    @since Qore 3.0
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
        @param xsink Exception sink - raises FILESYSTEM-ACCESS-DENIED if denied by policy, or
        SANDBOX-PATH-ERROR if the path cannot be resolved

        @return true if access is allowed, false if denied (exception raised)

        @note This method:
        - Canonicalizes the path (see below)
        - Checks denied paths first (they take precedence)
        - Checks sandbox root containment
        - Checks allowed paths
        - Falls back to default policy

        @note The path does not have to exist; the deepest existing ancestor is resolved with
        realpath() and the remaining components are appended, with \c "." and \c ".." folded, so
        that paths naming files that have not been created yet can be checked.  Symbolic links
        are always resolved before the containment checks, including links whose target does not
        exist.  A path that cannot be resolved for any other reason (for example a component that
        cannot be searched) raises SANDBOX-PATH-ERROR; this is a resolution failure and not an
        access denial.
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

    friend class QoreSandboxComposition;
    friend class QoreSandboxManager;

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

    @since Qore 3.0
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

    //! Returns true if a connection to the port with the protocol is allowed by the port rules
    /** True also when no port rule is set, which allows every port
    */
    DLLLOCAL bool allowsPort(int port, int proto) const;

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

        @see checkConnect(const char*, const struct sockaddr*, socklen_t, int, ExceptionSink*) for the rules
    */
    DLLEXPORT bool checkConnect(const struct sockaddr* addr, socklen_t len,
                                int proto, ExceptionSink* xsink);

    //! Checks if a connection to a named host is allowed - called AFTER DNS resolution for each resolved address
    /** @param hostname The host name or address literal that was resolved; may be null
        @param addr The resolved socket address
        @param len Length of the address structure
        @param proto Protocol being used (QSEC_NET_TCP, QSEC_NET_UDP, QSEC_NET_UNIX)
        @param xsink Exception sink - raises NETWORK-ACCESS-DENIED if denied

        @return true if allowed, false if denied (exception raised)

        The rules are applied in this order:
        - an IPv4-mapped IPv6 address (<tt>::%ffff:a.b.c.d</tt>) is checked as the IPv4 address it carries, and an
          unspecified destination address (\c 0.0.0.0 or <tt>::</tt>), which the system connects to the local host, is
          checked as the loopback address
        - a denied IP range containing the address denies the connection, also for an allowed host name, so a host
          name that resolves to a blocked network (DNS rebinding) is denied
        - a port outside the allowed ports for the protocol denies the connection
        - a host name matching an allowed host pattern allows the connection
        - an allowed IP range containing the address allows the connection; if allowed IP ranges are configured, an
          address outside all of them denies the connection
        - otherwise the default policy decides
    */
    DLLEXPORT bool checkConnect(const char* hostname, const struct sockaddr* addr, socklen_t len,
                                int proto, ExceptionSink* xsink);

    //! Checks if binding a local address is allowed
    /** @param addr The address to bind to
        @param len Length of the address structure
        @param proto Protocol being used
        @param xsink Exception sink - raises NETWORK-ACCESS-DENIED if denied

        @return true if allowed, false if denied (exception raised)

        Binding is controlled like a connection to the local address, without host names: an IPv4-mapped address is
        checked as the IPv4 address it carries; a denied IP range containing the address denies the bind, unless
        the address is a loopback address (\c 127.0.0.0/8 or <tt>::1</tt>), because a socket bound there can only be
        reached from the local host, which is less than a bind on all interfaces exposes (denied ranges still deny
        connections to loopback addresses); a
        specific port outside the allowed ports for the protocol denies the bind (a system-assigned port, port 0, is
        not restricted); an address outside all configured allowed IP ranges denies the bind; otherwise the default
        policy decides.  A wildcard address (\c 0.0.0.0 or <tt>::</tt>) binds all interfaces, so it is allowed by an
        allowed IP range only if the range contains the wildcard address itself (\c 0.0.0.0/0 or <tt>::/0</tt>).
    */
    DLLEXPORT bool checkBind(const struct sockaddr* addr, socklen_t len,
                             int proto, ExceptionSink* xsink);

    //! Checks if a host name can be allowed before it is resolved
    /** @param hostname The hostname to check
        @param port The port number
        @param proto Protocol being used

        @return false if a connection to @a hostname is denied whatever address it resolves to: the port is not
        allowed for the protocol, or the host name matches no allowed host pattern while no allowed IP range and no
        default allow policy could allow its addresses; true otherwise

        @note This is a preliminary check. The final check using checkConnect(const char*, const struct sockaddr*,
        socklen_t, int, ExceptionSink*) MUST still be performed on every resolved address.
    */
    DLLEXPORT bool checkHostname(const char* hostname, int port, int proto) const;

    //! Returns true if the host name matches an allowed host pattern
    /** Host names are compared without regard to case and without a trailing dot; \c "*.example.com" matches
        \c "example.com" and a name with a single label before it, such as \c "api.example.com", but not
        \c "api.sub.example.com"; \c "*" matches every host name.
    */
    DLLEXPORT bool isHostAllowed(const char* hostname) const;

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

    friend class QoreSandboxComposition;

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
    sm.setMaxThreads(4);

    QoreProgram* pgm = new QoreProgram(PO_NEW_STYLE);
    pgm->setSandboxManager(&sm);
    @endcode

    @since Qore 3.0
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

    //! Sets maximum number of threads
    /** @param count Maximum number of threads (0 = unlimited)

        The limit is the number of threads that code governed by this sandbox may have running at the same time:
        threads started with the \c background operator, \c call_async(), the workers of \c parallel_map() and
        \c parallel_foreach(), and threads that modules start with q_start_thread() on the sandboxed code's
        behalf; these are exactly the threads that inherit this sandbox (see @ref QTF_NO_SANDBOX_INHERIT).  The
        thread that enters the sandboxed Program from outside is not counted, nor are shared threads that run work
        for many callers (for example the workers of a @ref Qore::Thread::ThreadPool "ThreadPool" and the I/O and
        callback threads of an async I/O controller).

        Starting a thread at the limit raises \c THREAD-CREATION-FAILURE in the starting code; a counted thread
        stops counting when it ends.  Lowering the limit does not affect threads that are already running.
    */
    DLLEXPORT void setMaxThreads(int count);

    //! Gets the max threads limit
    DLLEXPORT int getMaxThreads() const;

    //! Sets the wall-clock time limit of each call into the sandbox in milliseconds
    /** @param ms the limit in milliseconds; 0 (or a negative value) = unlimited

        A call starts when a thread enters code governed by this sandbox from code it does not govern: when a Program
        with this manager attached becomes current (QoreProgram::run(), QoreProgram::callFunction(), callbacks and
        method calls into the Program) or when code created under this sandbox (objects and call references with
        sandbox provenance) is run from outside it.  A thread that is already running a call of this sandbox does not
        start a new one when it re-enters it.  Work that the call continues on other threads (threads it starts,
        thread pool tasks, async I/O operations) shares the call's deadline.

        When the deadline passes, the threads of the call raise \c SANDBOX-TIMEOUT at their next cancellation point;
        a thread blocked in a cancellable wait wakes at once.  The exception is raised again at every cancellation
        point until the thread leaves the call, and clearing the thread's cancellation request does not clear it.
        Other calls into the same sandbox are not affected.

        Changing the limit affects calls that start afterwards.

        @see design/sandbox-resource-limits.md

        @since Qore 3.0
    */
    DLLEXPORT void setWallTimeLimit(int64 ms);

    //! Gets the wall-clock time limit of each call into the sandbox in milliseconds; 0 = unlimited
    DLLEXPORT int64 getWallTimeLimit() const;

    //! Sets the CPU time limit of each call into the sandbox in milliseconds
    /** @param ms the limit in milliseconds; 0 (or a negative value) = unlimited

        The limit applies to the CPU time used by all threads of a call together (see setWallTimeLimit() for what a
        call is and which threads continue it): the thread that entered the sandbox, threads the call starts, and
        thread pool tasks and async I/O operations that run for it, each from the moment it starts working for the
        call.  Time spent waiting (sleeping, blocked on a lock or on I/O) does not count.

        When the CPU time is used up, the threads of the call raise \c SANDBOX-TIMEOUT at their next cancellation
        point, as with the wall-time limit; the exception names the CPU time limit.  The CPU time is measured by the
        sandbox watchdog at the earliest point it could be used up (the remaining time divided by the number of the
        call's threads, at least 1 ms apart), so the call can exceed its limit by up to about 1 ms per thread plus
        the time to the next cancellation point.

        Where the platform has no per-thread CPU clock (\c pthread_getcpuclockid() or the Mach thread API), a call
        with a CPU time limit fails with \c SANDBOX-ERROR instead of running without it.  Changing the limit affects
        calls that start afterwards.

        @since Qore 3.0
    */
    DLLEXPORT void setCPUTimeLimit(int64 ms);

    //! Gets the CPU time limit of each call into the sandbox in milliseconds; 0 = unlimited
    DLLEXPORT int64 getCPUTimeLimit() const;

    //! Sets the stack size that each call into the sandbox may use, in bytes
    /** @param bytes the limit in bytes; 0 (or a negative value) = unlimited

        The stack is measured from the stack position where the call starts (see setWallTimeLimit() for what a call
        is); a thread that continues the call (a thread it starts, a pool task) measures from where it starts the
        work.  A re-entry into the sandbox does not start a new measurement.  The thread's own stack guard still
        applies: the sandbox limit can only make it tighter.

        Exceeding the limit raises \c STACK-LIMIT-EXCEEDED at the next stack check (every function and method call in
        all execution modes), which bounds recursion in sandboxed code.  Changing the limit affects calls that start
        afterwards.

        @note Only enforced on platforms where %Qore checks the stack (\c QORE_MANAGE_STACK)

        @since Qore 3.0
    */
    DLLEXPORT void setMaxStackSize(int64 bytes);

    //! Gets the stack size that each call into the sandbox may use, in bytes; 0 = unlimited
    DLLEXPORT int64 getMaxStackSize() const;

    //! Sets the memory limit of the sandbox in bytes
    /** @param bytes the limit in bytes; 0 (or a negative value) = unlimited

        The limit applies to the memory held by %Qore values (strings, lists, hashes, objects, binary data, and buffers
        that modules allocate with q_sb_malloc()) that code governed by the sandbox created and that are still alive,
        wherever they are now: data passed out of the sandbox stays charged to it until it is freed.  It does not
        count allocations made inside third-party libraries or by the interpreter itself.  With nested sandboxes, the
        memory is charged to every enclosing sandbox call with a memory limit as well.

        A call into the sandbox (see setWallTimeLimit()) charges the memory of the storage it creates if the sandbox
        has a memory limit when the call starts; a change of a non-zero limit takes effect at once.

        When the memory in use exceeds the limit, the thread that exceeded it raises \c SANDBOX-MEMORY-LIMIT at its
        next cancellation point, and again at every cancellation point until the memory in use is below the limit
        (code that catches the exception and frees memory can therefore go on).  An allocation never fails because of
        the limit; operations that know the size of a large allocation in advance check it first and raise
        \c SANDBOX-MEMORY-LIMIT instead of allocating.  The limit can be exceeded by at most one operation plus the
        memory that threads have reserved for their allocations (a small part of the limit near the limit).

        @since Qore 3.0
    */
    DLLEXPORT void setMemoryLimit(int64 bytes);

    //! Gets the memory limit of the sandbox in bytes; 0 = unlimited
    DLLEXPORT int64 getMemoryLimit() const;

    //! Returns the memory in bytes currently charged to the sandbox (see setMemoryLimit())
    /** Includes the memory that threads running calls into the sandbox have reserved for their allocations

        @since Qore 3.0
    */
    DLLEXPORT int64 getMemoryUsage() const;

    //! Returns the memory account of the sandbox; for internal use
    DLLLOCAL QoreSandboxMemoryAccount* getMemoryAccount() const {
        return mem_account;
    }

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

    //! Checks if a connection to a named host is allowed; called for each address the host name resolved to
    /** @param hostname The host name or address literal that was resolved; may be null
        @param addr The resolved address
        @param len Length of the address structure
        @param proto Protocol (QSEC_NET_TCP, QSEC_NET_UDP, etc.)
        @param xsink Exception sink for error reporting

        @return true if access is allowed, false if denied (exception raised)

        @see QoreNetworkSecurityManager::checkConnect(const char*, const struct sockaddr*, socklen_t, int,
        ExceptionSink*)
    */
    DLLEXPORT bool checkNetworkAccess(const char* hostname, const struct sockaddr* addr, socklen_t len,
                                       int proto, ExceptionSink* xsink);

    //! Checks if binding a local address is allowed
    /** @param addr The address to bind
        @param len Length of the address structure
        @param proto Protocol (QSEC_NET_TCP, QSEC_NET_UDP, QSEC_NET_UNIX)
        @param xsink Exception sink for error reporting

        @return true if the bind is allowed, false if denied (exception raised)

        @see QoreNetworkSecurityManager::checkBind()
    */
    DLLEXPORT bool checkNetworkBind(const struct sockaddr* addr, socklen_t len,
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
        - Wall time limit per call: 5000ms
        - CPU time limit per call: 1000ms
        - Stack size limit per call: 1MB
        - Memory limit: 10MB
        - Max threads: 1
    */
    DLLEXPORT static QoreSandboxManager* createLockdown();

    //! Creates a preset safe for web application sandboxing
    /** @return A new QoreSandboxManager with web-safe defaults

        Web-safe mode:
        - No filesystem access
        - Network allowed but private networks blocked
        - Wall time limit per call: 30000ms
        - CPU time limit per call: 10000ms
        - Stack size limit per call: 2MB
        - Memory limit: 100MB
        - Max threads: 4
    */
    DLLEXPORT static QoreSandboxManager* createWebSafe();

private:
    //! Filesystem security manager
    QoreFilesystemSecurityManager fs_mgr;

    friend class QoreSandboxComposition;

    //! Network security manager
    QoreNetworkSecurityManager net_mgr;

    //! Maximum threads (0 = unlimited)
    std::atomic<int> max_threads;

    //! Interrupt reason bit: requestInterrupt() was called; raises \c PROGRAM-INTERRUPTED
    static constexpr unsigned QSB_INTR_REQUESTED = (1u << 0);

    //! Interrupt reason bits (@ref QSB_INTR_REQUESTED); see "Reason codes" in design/sandbox-resource-limits.md
    std::atomic<unsigned> interrupt_flags;

    //! The wall-clock time limit of each call in milliseconds (0 = unlimited)
    std::atomic<int64> wall_time_limit;

    //! The CPU time limit of each call in milliseconds (0 = unlimited)
    std::atomic<int64> cpu_time_limit;

    //! The stack size limit of each call in bytes (0 = unlimited)
    std::atomic<int64> max_stack_size;

    //! The memory account of the sandbox, which holds its memory limit; referenced
    QoreSandboxMemoryAccount* mem_account;

    //! Mutex for cancel callbacks
    mutable std::mutex cancel_mutex;

    //! Registered cancel callbacks
    std::vector<std::pair<void*, cancel_callback_t>> cancel_callbacks;

    //! The number of running threads that this sandbox governs and counts against @ref max_threads
    std::atomic<int> active_threads;

    //! Disallow copying via assignment
    QoreSandboxManager& operator=(const QoreSandboxManager&) = delete;

    // reserves and releases thread slots for the threads that inherit this sandbox
    friend class QoreInheritedSandbox;
    // reserves and releases thread slots for the workers of a ThreadPool created by a Program with this sandbox
    friend class ThreadPool;

    //! Counts a new thread governed by this sandbox; raises THREAD-CREATION-FAILURE at the thread limit
    /** @return 0 if the thread is counted, -1 if the limit has been reached (an exception is raised)
    */
    DLLLOCAL int acquireThreadSlot(ExceptionSink* xsink);

    //! Counts a new thread like acquireThreadSlot() without waking the thread pools waiting for a slot
    /** For a ThreadPool taking a slot with its lock held.  A composition that cannot take a slot of every member
        releases the slots it took again; \a released is then set to true, and the caller must call
        notifyThreadSlotReleased() once it has released its lock.
    */
    DLLLOCAL int acquireThreadSlotQuiet(ExceptionSink* xsink, bool& released);

    //! Stops counting a thread counted with acquireThreadSlot() and wakes the thread pools waiting for a slot
    /** Must not be called with the lock of a ThreadPool held; see releaseThreadSlotQuiet()
    */
    DLLLOCAL void releaseThreadSlot();

    //! Stops counting a thread counted with acquireThreadSlot() without waking the thread pools waiting for a slot
    /** For a ThreadPool releasing a slot with its lock held, which notifies the waiting pools once it has released
        its lock
    */
    DLLLOCAL void releaseThreadSlotQuiet();

    //! Wakes the thread pools waiting for a slot of this manager or of a member of this composition
    /** Must not be called with the lock of a ThreadPool held

        @param except a pool not to wake: the pool that released slots itself, which retries only when another
        thread released one
    */
    DLLLOCAL void notifyThreadSlotReleased(const AbstractPrivateData* except = nullptr);
};

//! RAII helper that acquires a ref'd sandbox manager for the current running program
/** Acquires a strong reference under lock to prevent use-after-free
    when the sandbox manager is cleared during program cleanup.

    Every helper names its purpose, because the two purposes resolve differently inside a system-policy
    capability (@ref QoreSandboxPolicyBarrierHelper, \c SandboxManager::createSystemPolicyCall()):
    - @ref Policy for a filesystem, network or process policy check
      (QoreSandboxManager::checkFilesystemAccess(), QoreSandboxManager::checkNetworkAccess(),
      QoreSandboxManager::checkNetworkBind(), and the policy of filesystem() and network()): the capability is
      subject to the system policy, not to its sandboxed caller's;
    - @ref All for the governing sandbox: interrupts and force-termination, resource limits such as nesting depth,
      and whether a sandbox governs the thread at all, none of which a capability suppresses.

    @par Example:
    @code{.cpp}
    QoreSandboxManagerHelper policy(QoreSandboxManagerHelper::Policy);
    if (policy && !policy->checkFilesystemAccess(path, QSEC_READ, xsink)) {
        return -1;
    }
    QoreSandboxManagerHelper governing(QoreSandboxManagerHelper::All);
    if (governing) {
        governing->checkIOInterrupt(xsink, "reading file");
    }
    @endcode

    @since Qore 3.0
*/
class DLLEXPORT QoreSandboxManagerHelper {
public:
    //! Selects which kind of check the resolved manager is for
    enum Purpose {
        //! the governing sandbox (interrupts, resource limits, whether a sandbox is active); resolution always
        //! walks out to enclosing callers, through any policy barrier
        All = 0,
        //! a filesystem, network or process POLICY check; honors an active policy barrier
        Policy = 1,
    };

    //! Acquires a ref'd sandbox manager for the current program for the governing sandbox (@ref All)
    /** @deprecated name the purpose: a policy check made through this constructor is made with the policy of a
        system-policy capability's sandboxed caller instead of with the system policy; use
        QoreSandboxManagerHelper(Purpose) with @ref Policy for filesystem, network and process policy checks and
        @ref All for everything else (interrupts, cancel callbacks, resource limits, whether a sandbox is active)
    */
    QoreSandboxManagerHelper();

    //! Acquires a ref'd sandbox manager for the current program for the given purpose
    /** @param purpose @ref Policy for a filesystem/network policy check, which honors a
        policy barrier established by @ref QoreSandboxPolicyBarrierHelper; @ref All (the
        default behavior of the no-argument constructor) for anything else, notably
        interrupt/force-terminate checks, which must never be suppressed by a barrier

        @since Qore 3.0
    */
    QoreSandboxManagerHelper(Purpose purpose);

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


//! RAII barrier that stops a sandboxed CALLER's policy from being inherited
/** Sandbox policy normally propagates across program boundaries: code in a Qore-language
    module executes in the module's own QoreProgram, which has no manager of its own, so
    resolution walks out to the calling program and the module inherits the caller's sandbox.
    That is what stops a module from being used to bypass a sandbox, and it is the correct
    default.

    It is wrong, however, for trusted infrastructure that must use a SYSTEM resource while
    acting on a sandboxed caller's behalf -- reading a service-account credential to
    authenticate an outbound API call, for example.  There the caller must not be able to
    reach the resource, but the infrastructure must.

    Establishing this barrier around such a call stops policy resolution at the Program
    context that is current when the barrier is established, so the caller's policy is not
    inherited.  Two properties make it narrow:

    - it does not lift the managers of Programs entered inside the barrier, so if the barriered
      code calls back into sandboxed code (a callback), that code, and any module code it
      calls, is still checked against its sandbox
    - it applies to filesystem/network POLICY only.  Interrupt and force-terminate checks
      resolve unchanged, so a barriered call stays cancellable

    Object and callback provenance remains active through this barrier. Privileged operations that must mask
    provenance register a fixed capability during trusted startup with SandboxManager::createSystemPolicyCall().

    Scope it as tightly as possible: it is a sanctioned exception to sandbox inheritance, and
    everything executed inside it runs without the caller's policy.

    @par Example:
    @code{.cpp}
    {
        QoreSandboxPolicyBarrierHelper barrier;
        // system credential read on the caller's behalf; the caller cannot reach this path
        // except through this trusted entry point
        rv = read_system_credential(path, xsink);
    }
    @endcode

    @see design/module-sandboxing-audit-guide.md

    @since Qore 3.0
*/
class DLLEXPORT QoreSandboxPolicyBarrierHelper {
public:
    //! Establishes the barrier on the current thread
    DLLEXPORT QoreSandboxPolicyBarrierHelper();

    //! Releases the barrier
    DLLEXPORT ~QoreSandboxPolicyBarrierHelper();

    //! Returns true if the barrier is active
    DLLEXPORT operator bool() const {
        return active;
    }

private:
    bool active;

    // non-copyable
    QoreSandboxPolicyBarrierHelper(const QoreSandboxPolicyBarrierHelper&) = delete;
    QoreSandboxPolicyBarrierHelper& operator=(const QoreSandboxPolicyBarrierHelper&) = delete;
};

class QoreSandboxContext;

//! Applies the creation sandbox of an object or callback for the duration of a native invocation
/** Use this guard when a native bridge calls its implementation directly instead of
    ResolvedCallReferenceNode::invoke(). The object or callback must remain alive for construction of the guard.
    The context itself is retained until the guard is destroyed. No guard is needed around invoke().

    @par Example:
    @code{.cpp}
    QoreSandboxProvenanceHelper provenance(callback);
    return execute_native_callback(args, xsink);
    @endcode

    @since %Qore 3.0
*/
class DLLEXPORT QoreSandboxProvenanceHelper {
public:
    //! Creates an inactive guard
    QoreSandboxProvenanceHelper() = default;
    //! Applies an object's creation context; nullptr leaves the guard inactive
    /** @param object the receiver, or nullptr for no context */
    explicit QoreSandboxProvenanceHelper(const QoreObject* object);
    //! Applies a runtime call reference's creation context; nullptr leaves the guard inactive
    /** @param callback the runtime call reference, or nullptr for no context */
    explicit QoreSandboxProvenanceHelper(const ResolvedCallReferenceNode* callback);
    //! Restores the previous context and releases the retained creation context
    ~QoreSandboxProvenanceHelper();
    //! Applies an object's creation context to an inactive guard
    /** @param object the receiver, or nullptr for no context */
    void set(const QoreObject* object);

    QoreSandboxProvenanceHelper(const QoreSandboxProvenanceHelper&) = delete;
    QoreSandboxProvenanceHelper& operator=(const QoreSandboxProvenanceHelper&) = delete;

    //! Internal access to the immutable active context
    /** @return the retained context, or nullptr if inactive */
    const QoreSandboxContext* getContext() const { return context; }
    //! Internal access to the preceding active guard
    /** @return the preceding guard, or nullptr */
    const QoreSandboxProvenanceHelper* getPrevious() const { return previous; }
private:
    QoreSandboxContext* context = nullptr;
    const QoreSandboxProvenanceHelper* previous = nullptr;
    void apply(QoreSandboxContext* captured);

    // applies the sandbox of the caller of a parse or a module load in another Program
    friend class QoreCallerSandboxHelper;
};

//! The sandbox that governs the current thread, captured so that it can govern work run on another thread
/** A thread started with q_start_thread() or the \c background operator inherits the sandbox of the code that
    starts it.  A thread that runs work for many callers - a module's worker pool, or a thread that a native library
    owns and that runs callbacks registered by Qore code - must instead govern each piece of work by the sandbox of
    the code that submitted or registered it: capture a QoreSandboxCapture when the work is submitted (in the
    submitting thread) and apply it with QoreSandboxCaptureHelper around the work on the worker thread.

    Without this, the work of a sandboxed caller runs with no sandbox at all, or under the sandbox of whichever
    caller happened to start the worker.

    The object is immutable after construction and may be shared between threads.

    @par Example:
    @code{.cpp}
    // when the work is submitted, in the submitting thread
    task->sandbox.reset(new QoreSandboxCapture);
    // on the worker thread
    QoreSandboxCaptureHelper sch(*task->sandbox);
    task->callback->invoke(args, xsink);
    @endcode

    @see @ref QTF_NO_SANDBOX_INHERIT

    @since %Qore 3.0
*/
class DLLEXPORT QoreSandboxCapture {
public:
    //! Captures the sandbox that governs the current thread
    DLLEXPORT QoreSandboxCapture();

    //! Releases the captured sandbox
    DLLEXPORT ~QoreSandboxCapture();

    //! Returns true if a sandbox governed the thread that captured the object
    DLLEXPORT bool isSandboxed() const;

    //! Returns true if both objects captured the same sandbox (or both captured none)
    /** Work captured under the same sandbox can be combined, for example into one batch, and run under either
        capture.
    */
    DLLEXPORT bool sameSandbox(const QoreSandboxCapture& other) const;

private:
    QoreSandboxContext* ctx;

    friend class QoreSandboxCaptureHelper;

    // non-copyable
    QoreSandboxCapture(const QoreSandboxCapture&) = delete;
    QoreSandboxCapture& operator=(const QoreSandboxCapture&) = delete;
};

//! Governs the current thread by a captured sandbox while the object exists
/** The captured sandbox governs the work as its outermost caller, exactly as if the work ran on a thread that the
    capturing code had started: a Program that the work enters and that has a sandbox manager of its own is still
    governed by that manager.

    @since %Qore 3.0
*/
class DLLEXPORT QoreSandboxCaptureHelper {
public:
    //! Applies the captured sandbox; the capture must remain valid while the helper exists
    DLLEXPORT QoreSandboxCaptureHelper(const QoreSandboxCapture& capture);

    //! Restores the sandbox that governed the thread before
    DLLEXPORT ~QoreSandboxCaptureHelper();

private:
    const QoreSandboxContext* old_ctx = nullptr;
    bool active = false;

    // non-copyable
    QoreSandboxCaptureHelper(const QoreSandboxCaptureHelper&) = delete;
    QoreSandboxCaptureHelper& operator=(const QoreSandboxCaptureHelper&) = delete;
};

//! Polling interval for blocking I/O operations in milliseconds
/** The interval at which a blocking wait that cannot be woken directly checks for a cancellation request or program
    interrupt, for example a wait inside a third-party library that exposes neither its descriptors nor an event loop.

    @deprecated a wait on descriptors should use qore_cancellable_poll(), and a library event loop should watch the
    descriptor from qore_cancel_wakeup_register(); both end as soon as cancellation is requested, without polling.
    %Qore itself no longer uses this constant.
*/
#define QORE_IO_POLL_INTERVAL_MS 500

//! Allocates memory that counts against the memory limit of the sandbox the calling code runs in
/** For modules that allocate large buffers for sandboxed code (compression, parsers, machine learning, ...): the
    memory is charged to the sandbox call the current thread runs, like the memory of %Qore values (see
    QoreSandboxManager::setMemoryLimit()), and released when it is freed with q_sb_free(), on any thread.  Outside a
    sandbox call with a memory limit it is a plain allocation.

    The memory must be freed with q_sb_free() and resized with q_sb_realloc() (it has a 16-byte header; the pointer
    returned has the alignment of malloc()).

    @param size the number of bytes to allocate
    @param xsink if not null, the request is checked against the memory limit first, and \c SANDBOX-MEMORY-LIMIT is
    raised instead of allocating if it does not fit; an allocation failure raises an out-of-memory error

    @return the memory, or nullptr if the request does not fit the limit or cannot be allocated (or \a size is 0)

    @since Qore 3.0
*/
DLLEXPORT void* q_sb_malloc(size_t size, ExceptionSink* xsink);

//! Resizes memory allocated with q_sb_malloc(); the change in size is charged or released
/** @param ptr the memory, or nullptr to allocate
    @param size the new size; 0 frees the memory and returns nullptr
    @param xsink if not null, growth is checked against the memory limit first, and \c SANDBOX-MEMORY-LIMIT is raised
    (and \a ptr left unchanged) if it does not fit; an allocation failure raises an out-of-memory error

    @return the resized memory, or nullptr on error, in which case \a ptr is still valid

    @since Qore 3.0
*/
DLLEXPORT void* q_sb_realloc(void* ptr, size_t size, ExceptionSink* xsink);

//! Frees memory allocated with q_sb_malloc() or q_sb_realloc() and releases its charge; nullptr is ignored
/** @since Qore 3.0
*/
DLLEXPORT void q_sb_free(void* ptr);

#endif // _QORE_QORE_SANDBOX_MANAGER_H
