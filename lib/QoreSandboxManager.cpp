/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxManager.cpp

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

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include <qore/TypedHashDecl.h>
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreSandboxProvenance.h"
#include "qore/intern/QoreSandboxMemory.h"
#include "qore/intern/ThreadPool.h"
#include "qore/intern/QoreForkHandlers.h"

#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// Filesystem Security Manager - Private Implementation
//------------------------------------------------------------------------------

struct PathRule {
    std::string path;
    int mode;

    PathRule(const std::string& p, int m) : path(p), mode(m) {}
};

//! the maximum number of symbolic links resolved when canonicalizing a path that does not exist
/** matches the traditional kernel limit (SYMLOOP_MAX / MAXSYMLINKS)
*/
static constexpr unsigned qore_sandbox_max_symlinks = 40;

struct QoreSandboxMembers {
    const std::vector<QoreSandboxManager*> managers;

    explicit QoreSandboxMembers(std::vector<QoreSandboxManager*> managers) : managers(std::move(managers)) {
        for (auto* manager : this->managers) {
            manager->ref();
        }
    }

    ~QoreSandboxMembers() {
        for (auto* manager : managers) {
            manager->deref(nullptr);
        }
    }
};

struct qore_fs_security_private {
    std::shared_ptr<const QoreSandboxMembers> composition;
    std::vector<PathRule> allowed_paths;
    std::vector<std::string> denied_paths;
    std::string sandbox_root;
    bool default_allow = false;

    //! Splits an absolute path into its parent directory and its final component
    /** The parent of a path directly below the root is the root itself; a trailing separator
        yields an empty final component.
    */
    static void splitPath(const std::string& path, std::string& parent, std::string& leaf) {
        size_t pos = path.rfind('/');
        assert(pos != std::string::npos);
        leaf = path.substr(pos + 1);
        parent = pos ? path.substr(0, pos) : "/";
    }

    //! Resolves a path to an absolute, symlink-free canonical path for security checks
    /** realpath() requires every component of the path to exist, but a path naming a file that
        has not been created yet is a legitimate target for stat(), open(O_CREAT), mkdir(), etc,
        so the deepest existing ancestor is resolved with realpath() and the remaining components
        are appended.

        Components below the deepest existing ancestor do not exist, so they cannot be symbolic
        links; "." and ".." in that unresolved tail are therefore folded lexically, which is
        exactly what the OS would do with them.  A ".." that pops past the resolved ancestor is
        applied to the ancestor with realpath() instead, so symbolic links in the existing prefix
        are always resolved by the OS.

        A dangling symbolic link - one that exists but whose target does not - makes realpath()
        fail with ENOENT, so it is resolved here with readlink(); otherwise the link path rather
        than the link target would be subjected to the containment checks, and a link inside the
        sandbox root pointing outside it could be used to create files outside the root.

        Only a missing component (ENOENT, ENOTDIR) is resolved this way; any other realpath()
        error means that the path cannot be resolved safely, and guessing at a canonical form
        that the containment checks would then trust could place a path outside the sandbox
        inside it.
    */
    std::string canonicalize(const char* path, ExceptionSink* xsink) {
        if (!path || !*path) {
            xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize an empty path");
            return "";
        }

        // make the path absolute the same way realpath() would
        std::string probe;
        if (*path == '/') {
            probe = path;
        } else {
            std::unique_ptr<char, decltype(&free)> cwd(realpath(".", nullptr), &free);
            if (!cwd) {
                xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize relative path "
                    "'%s': the current directory cannot be resolved: %s", path, strerror(errno));
                return "";
            }
            probe = cwd.get();
            if (probe != "/") {
                probe += '/';
            }
            probe += path;
        }

        // the components below the deepest existing ancestor, in path order
        std::vector<std::string> tail;
        // the deepest existing ancestor of the path
        std::string base;
        // the number of symbolic links resolved manually, to break symbolic link loops
        unsigned links = 0;

        while (true) {
            std::unique_ptr<char, decltype(&free)> real(realpath(probe.c_str(), nullptr), &free);
            if (real) {
                base = real.get();
                break;
            }
            // save errno before any other system call can overwrite it
            int err = errno;
            if (err != ENOENT && err != ENOTDIR) {
                xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize path '%s': "
                    "'%s': %s", path, probe.c_str(), strerror(err));
                return "";
            }

#if defined(HAVE_LSTAT) && defined(HAVE_READLINK)
            // a dangling symbolic link exists but cannot be resolved by realpath(); resolve it
            // here so that its target is subject to the containment checks
            struct stat sb;
            if (!lstat(probe.c_str(), &sb) && S_ISLNK(sb.st_mode)) {
                if (++links > qore_sandbox_max_symlinks) {
                    xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize path '%s': "
                        "'%s': %s", path, probe.c_str(), strerror(ELOOP));
                    return "";
                }
                char buf[QORE_PATH_MAX + 1];
                ssize_t len = readlink(probe.c_str(), buf, QORE_PATH_MAX);
                if (len < 0) {
                    xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize path '%s': "
                        "readlink('%s') failed: %s", path, probe.c_str(), strerror(errno));
                    return "";
                }
                assert(len <= QORE_PATH_MAX);
                buf[len] = '\0';
                if (*buf == '/') {
                    probe = buf;
                } else {
                    // a relative link target is resolved against the directory holding the link
                    std::string parent, leaf;
                    splitPath(probe, parent, leaf);
                    probe = parent == "/" ? parent + buf : parent + "/" + buf;
                }
                continue;
            }
#endif

            // realpath() cannot fail on the root, so the loop always terminates before the root
            // is consumed; this is defensive only
            if (probe == "/") {
                xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize path '%s': %s",
                    path, strerror(err));
                return "";
            }

            std::string parent, leaf;
            splitPath(probe, parent, leaf);
            // empty components (from "//" or a trailing separator) and "." are dropped
            if (!leaf.empty() && leaf != ".") {
                tail.insert(tail.begin(), leaf);
            }
            probe = parent;
        }

        // fold the unresolved components onto the resolved ancestor
        std::vector<std::string> comps;
        for (const std::string& c : tail) {
            if (c != "..") {
                comps.push_back(c);
                continue;
            }
            if (!comps.empty()) {
                comps.pop_back();
                continue;
            }
            // ".." at the root is the root
            if (base == "/") {
                continue;
            }
            // the tail has popped back into the existing prefix; let the OS resolve the parent so
            // that symbolic links in the prefix are followed
            std::string up = base + "/..";
            std::unique_ptr<char, decltype(&free)> real(realpath(up.c_str(), nullptr), &free);
            if (!real) {
                xsink->raiseException("SANDBOX-PATH-ERROR", "cannot canonicalize path '%s': the "
                    "parent of '%s' cannot be resolved: %s", path, base.c_str(), strerror(errno));
                return "";
            }
            base = real.get();
        }

        std::string result = base == "/" ? std::string() : base;
        for (const std::string& c : comps) {
            result += '/';
            result += c;
        }
        return result.empty() ? "/" : result;
    }

    // Check if path starts with prefix (for directory containment)
    bool pathStartsWith(const std::string& path, const std::string& prefix) const {
        if (prefix.empty()) return true;
        if (path.size() < prefix.size()) return false;

        // Must match prefix exactly
        if (path.compare(0, prefix.size(), prefix) != 0) return false;

        // If prefix is exactly the path, or prefix ends at a directory boundary
        if (path.size() == prefix.size()) return true;
        if (prefix.back() == '/') return true;
        return path[prefix.size()] == '/';
    }

    // Check if path is in denied list
    bool isDenied(const std::string& canon_path) const {
        for (const auto& denied : denied_paths) {
            if (pathStartsWith(canon_path, denied)) {
                return true;
            }
        }
        return false;
    }

    // Check if path is in allowed list with required mode
    bool isAllowed(const std::string& canon_path, int mode) const {
        for (const auto& rule : allowed_paths) {
            if (pathStartsWith(canon_path, rule.path)) {
                // Check if all required modes are allowed
                if ((rule.mode & mode) == mode) {
                    return true;
                }
            }
        }
        return false;
    }

    // Check sandbox root containment
    bool isInSandboxRoot(const std::string& canon_path) const {
        if (sandbox_root.empty()) return true;
        return pathStartsWith(canon_path, sandbox_root);
    }
};

//------------------------------------------------------------------------------
// Filesystem Security Manager - Public Implementation
//------------------------------------------------------------------------------

QoreFilesystemSecurityManager::QoreFilesystemSecurityManager() {
    priv = new qore_fs_security_private;
}

QoreFilesystemSecurityManager::~QoreFilesystemSecurityManager() {
    delete priv;
}

int QoreFilesystemSecurityManager::addAllowedPath(const char* path, int mode, ExceptionSink* xsink) {
    std::string canon = priv->canonicalize(path, xsink);
    if (*xsink) return -1;

    // If sandbox root is set, verify path is within it
    if (!priv->sandbox_root.empty() && !priv->isInSandboxRoot(canon)) {
        xsink->raiseException("SANDBOX-CONFIG-ERROR",
            "Allowed path '%s' is outside sandbox root '%s'",
            canon.c_str(), priv->sandbox_root.c_str());
        return -1;
    }

    priv->allowed_paths.emplace_back(canon, mode);
    return 0;
}

int QoreFilesystemSecurityManager::addDeniedPath(const char* path, ExceptionSink* xsink) {
    std::string canon = priv->canonicalize(path, xsink);
    if (*xsink) return -1;

    priv->denied_paths.push_back(canon);
    return 0;
}

int QoreFilesystemSecurityManager::setSandboxRoot(const char* path, ExceptionSink* xsink) {
    std::string canon = priv->canonicalize(path, xsink);
    if (*xsink) return -1;

    priv->sandbox_root = canon;

    // Implicitly allow the sandbox root with all access
    priv->allowed_paths.emplace_back(canon, QSEC_ALL);
    return 0;
}

void QoreFilesystemSecurityManager::clearSandboxRoot() {
    priv->sandbox_root.clear();
}

void QoreFilesystemSecurityManager::setDefaultPolicy(bool allow) {
    priv->default_allow = allow;
}

bool QoreFilesystemSecurityManager::getDefaultPolicy() const {
    return priv->default_allow;
}

bool QoreFilesystemSecurityManager::checkAccess(const char* path, int mode, ExceptionSink* xsink) {
    if (priv->composition) {
        unsigned checked = 0;
        for (auto* manager : priv->composition->managers) {
            if (!(checked++ % 100) && qore_check_cancel(xsink, "sandbox policy intersection")) {
                return false;
            }
            if (!manager->filesystem().checkAccess(path, mode, xsink)) {
                return false;
            }
        }
        return true;
    }
    // Canonicalize the path; a path that cannot be resolved is a resolution failure and not a
    // policy decision, so SANDBOX-PATH-ERROR is propagated rather than reported as a denial
    std::string canon = priv->canonicalize(path, xsink);
    if (*xsink) {
        return false;
    }

    // Check denied paths first (they always take precedence)
    if (priv->isDenied(canon)) {
        xsink->raiseException("FILESYSTEM-ACCESS-DENIED",
            "Access to '%s' denied by security policy (path is in denied list)", path);
        return false;
    }

    // Check sandbox root containment
    if (!priv->isInSandboxRoot(canon)) {
        xsink->raiseException("FILESYSTEM-ACCESS-DENIED",
            "Access to '%s' denied: path is outside sandbox root '%s'",
            path, priv->sandbox_root.c_str());
        return false;
    }

    // Check allowed paths
    if (priv->isAllowed(canon, mode)) {
        return true;
    }

    // Fall back to default policy
    if (priv->default_allow) {
        return true;
    }

    // Build mode string for error message
    std::string mode_str;
    if (mode & QSEC_READ) mode_str += "read,";
    if (mode & QSEC_WRITE) mode_str += "write,";
    if (mode & QSEC_EXECUTE) mode_str += "execute,";
    if (mode & QSEC_DELETE) mode_str += "delete,";
    if (mode & QSEC_CREATE) mode_str += "create,";
    if (!mode_str.empty()) mode_str.pop_back();  // Remove trailing comma

    xsink->raiseException("FILESYSTEM-ACCESS-DENIED",
        "Access to '%s' denied: %s access not permitted by security policy",
        path, mode_str.c_str());
    return false;
}

QoreHashNode* QoreFilesystemSecurityManager::getConfiguration(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> config(new QoreHashNode(hashdeclFilesystemSecurityConfigInfo, xsink), xsink);

    // Sandbox root
    if (!priv->sandbox_root.empty()) {
        config->setKeyValue("sandbox_root", new QoreStringNode(priv->sandbox_root.c_str()), xsink);
    }

    // Default policy
    config->setKeyValue("default_policy",
        new QoreStringNode(priv->default_allow ? "allow" : "deny"), xsink);

    // Allowed paths
    ReferenceHolder<QoreListNode> allowed(new QoreListNode(hashdeclFilesystemPathInfo->getTypeInfo()), xsink);
    for (const auto& rule : priv->allowed_paths) {
        ReferenceHolder<QoreHashNode> entry(new QoreHashNode(hashdeclFilesystemPathInfo, xsink), xsink);
        entry->setKeyValue("path", new QoreStringNode(rule.path.c_str()), xsink);
        entry->setKeyValue("mode", rule.mode, xsink);
        allowed->push(entry.release(), xsink);
    }
    config->setKeyValue("allowed_paths", allowed.release(), xsink);

    // Denied paths
    ReferenceHolder<QoreListNode> denied(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& path : priv->denied_paths) {
        denied->push(new QoreStringNode(path.c_str()), xsink);
    }
    config->setKeyValue("denied_paths", denied.release(), xsink);

    return config.release();
}

QoreFilesystemSecurityManager* QoreFilesystemSecurityManager::copy() const {
    QoreFilesystemSecurityManager* result = new QoreFilesystemSecurityManager();
    result->priv->composition = priv->composition;
    result->priv->allowed_paths = priv->allowed_paths;
    result->priv->denied_paths = priv->denied_paths;
    result->priv->sandbox_root = priv->sandbox_root;
    result->priv->default_allow = priv->default_allow;
    return result;
}

void QoreFilesystemSecurityManager::reset() {
    priv->allowed_paths.clear();
    priv->denied_paths.clear();
    priv->sandbox_root.clear();
    priv->default_allow = false;
}

void QoreFilesystemSecurityManager::copyFrom(const QoreFilesystemSecurityManager& other) {
    priv->composition = other.priv->composition;
    priv->allowed_paths = other.priv->allowed_paths;
    priv->denied_paths = other.priv->denied_paths;
    priv->sandbox_root = other.priv->sandbox_root;
    priv->default_allow = other.priv->default_allow;
}

//------------------------------------------------------------------------------
// Network Security Manager - Private Implementation
//------------------------------------------------------------------------------

struct CIDRRange {
    bool is_ipv6;
    union {
        struct {
            uint32_t network;
            uint32_t mask;
        } v4;
        struct {
            uint8_t network[16];
            uint8_t mask[16];
        } v6;
    };

    bool matches(const struct sockaddr* addr) const {
        if (addr->sa_family == AF_INET && !is_ipv6) {
            const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(addr);
            uint32_t ip = ntohl(sin->sin_addr.s_addr);
            return (ip & v4.mask) == v4.network;
        } else if (addr->sa_family == AF_INET6 && is_ipv6) {
            const struct sockaddr_in6* sin6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
            for (int i = 0; i < 16; ++i) {
                if ((sin6->sin6_addr.s6_addr[i] & v6.mask[i]) != v6.network[i]) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
};

struct PortRange {
    int low;
    int high;
    int proto;

    bool matches(int port, int p) const {
        return port >= low && port <= high && (proto & p);
    }
};

struct qore_net_security_private {
    std::shared_ptr<const QoreSandboxMembers> composition;
    std::vector<std::string> allowed_hosts;
    std::vector<CIDRRange> allowed_ranges;
    std::vector<CIDRRange> denied_ranges;
    std::vector<PortRange> allowed_ports;
    bool default_allow = false;

    // Parse CIDR notation into CIDRRange
    bool parseCIDR(const char* cidr, CIDRRange& range, ExceptionSink* xsink) {
        std::string str(cidr);
        size_t slash = str.find('/');

        std::string addr_str;
        int prefix_len;

        if (slash != std::string::npos) {
            addr_str = str.substr(0, slash);
            prefix_len = std::stoi(str.substr(slash + 1));
        } else {
            addr_str = str;
            prefix_len = -1;  // Will be set based on address type
        }

        // Try IPv4 first
        struct in_addr addr4;
        if (inet_pton(AF_INET, addr_str.c_str(), &addr4) == 1) {
            range.is_ipv6 = false;
            if (prefix_len < 0) prefix_len = 32;
            if (prefix_len > 32) {
                xsink->raiseException("SANDBOX-CONFIG-ERROR",
                    "Invalid CIDR prefix length %d for IPv4 address", prefix_len);
                return false;
            }

            range.v4.mask = prefix_len == 0 ? 0 : (0xFFFFFFFF << (32 - prefix_len));
            range.v4.network = ntohl(addr4.s_addr) & range.v4.mask;
            return true;
        }

        // Try IPv6
        struct in6_addr addr6;
        if (inet_pton(AF_INET6, addr_str.c_str(), &addr6) == 1) {
            range.is_ipv6 = true;
            if (prefix_len < 0) prefix_len = 128;
            if (prefix_len > 128) {
                xsink->raiseException("SANDBOX-CONFIG-ERROR",
                    "Invalid CIDR prefix length %d for IPv6 address", prefix_len);
                return false;
            }

            // Build mask
            memset(range.v6.mask, 0, 16);
            int full_bytes = prefix_len / 8;
            int remaining_bits = prefix_len % 8;

            for (int i = 0; i < full_bytes; ++i) {
                range.v6.mask[i] = 0xFF;
            }
            if (remaining_bits > 0 && full_bytes < 16) {
                range.v6.mask[full_bytes] = 0xFF << (8 - remaining_bits);
            }

            // Apply mask to get network
            for (int i = 0; i < 16; ++i) {
                range.v6.network[i] = addr6.s6_addr[i] & range.v6.mask[i];
            }
            return true;
        }

        xsink->raiseException("SANDBOX-CONFIG-ERROR",
            "Invalid IP address in CIDR notation: '%s'", cidr);
        return false;
    }

    // Returns the host name in the form that is compared: DNS names are case-insensitive (RFC 4343), and a
    // trailing dot only marks the name as fully qualified
    static std::string normalizeHostName(const char* hostname) {
        std::string host(hostname);
        if (!host.empty() && host.back() == '.') {
            host.pop_back();
        }
        for (char& c : host) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        return host;
    }

    // Check if host pattern matches hostname
    bool hostMatches(const std::string& pattern, const char* hostname) const {
        // Simple wildcard matching
        if (pattern == "*") return true;

        std::string host = normalizeHostName(hostname);
        std::string pat = normalizeHostName(pattern.c_str());

        if (pat.size() > 2 && pat[0] == '*' && pat[1] == '.') {
            // *.example.com pattern: the domain itself or a name with one more label
            std::string domain = pat.substr(2);  // example.com
            if (host == domain) {
                return true;
            }
            size_t label_len = host.size() - domain.size() - 1;
            return host.size() > domain.size() + 1
                && host.compare(host.size() - domain.size(), domain.size(), domain) == 0
                && host[label_len] == '.'
                && host.find('.') == label_len;
        }

        // Exact match
        return pat == host;
    }

    // Check if the host name matches an allowed host pattern
    bool isHostAllowed(const char* hostname) const {
        if (!hostname || !*hostname) {
            return false;
        }
        for (const auto& pattern : allowed_hosts) {
            if (hostMatches(pattern, hostname)) {
                return true;
            }
        }
        return false;
    }

    // Returns the address that the rules are applied to: an IPv4-mapped IPv6 address carries an IPv4 address, and a
    // connection to an unspecified address (0.0.0.0 or ::) goes to the local host
    static const struct sockaddr* normalizeAddr(const struct sockaddr* addr, struct sockaddr_storage& storage,
            bool unspecified_as_loopback) {
        if (addr->sa_family == AF_INET6) {
            const struct sockaddr_in6* sin6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
            if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
                storage = {};
                struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(&storage);
                sin->sin_family = AF_INET;
                sin->sin_port = sin6->sin6_port;
                memcpy(&sin->sin_addr, &sin6->sin6_addr.s6_addr[12], sizeof(sin->sin_addr));
                addr = reinterpret_cast<const struct sockaddr*>(&storage);
            } else if (unspecified_as_loopback && IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
                storage = {};
                struct sockaddr_in6* out = reinterpret_cast<struct sockaddr_in6*>(&storage);
                out->sin6_family = AF_INET6;
                out->sin6_port = sin6->sin6_port;
                out->sin6_addr = in6addr_loopback;
                return reinterpret_cast<const struct sockaddr*>(&storage);
            }
        }
        if (unspecified_as_loopback && addr->sa_family == AF_INET) {
            const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(addr);
            if (sin->sin_addr.s_addr == htonl(INADDR_ANY)) {
                struct sockaddr_in out = {};
                out.sin_family = AF_INET;
                out.sin_port = sin->sin_port;
                out.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                storage = {};
                memcpy(&storage, &out, sizeof(out));
                return reinterpret_cast<const struct sockaddr*>(&storage);
            }
        }
        return addr;
    }

    // Returns true if the address is in an allowed range
    bool inAllowedRange(const struct sockaddr* addr) const {
        for (const auto& range : allowed_ranges) {
            if (range.matches(addr)) {
                return true;
            }
        }
        return false;
    }

    // Returns true if the address is a loopback address (127.0.0.0/8 or ::1)
    static bool isLoopback(const struct sockaddr* addr) {
        if (addr->sa_family == AF_INET) {
            return (ntohl(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_addr.s_addr) >> 24) == 127;
        }
        if (addr->sa_family == AF_INET6) {
            return IN6_IS_ADDR_LOOPBACK(&reinterpret_cast<const struct sockaddr_in6*>(addr)->sin6_addr);
        }
        return false;
    }

    // Check if address is in denied ranges
    bool isDenied(const struct sockaddr* addr) const {
        for (const auto& range : denied_ranges) {
            if (range.matches(addr)) {
                return true;
            }
        }
        return false;
    }

    // Check if port is allowed
    bool isPortAllowed(int port, int proto) const {
        if (allowed_ports.empty()) return true;  // No port restrictions

        for (const auto& range : allowed_ports) {
            if (range.matches(port, proto)) {
                return true;
            }
        }
        return false;
    }

    // Get port from sockaddr
    int getPort(const struct sockaddr* addr) const {
        if (addr->sa_family == AF_INET) {
            return ntohs(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_port);
        } else if (addr->sa_family == AF_INET6) {
            return ntohs(reinterpret_cast<const struct sockaddr_in6*>(addr)->sin6_port);
        }
        return 0;
    }

    // Get IP address string from sockaddr
    std::string getAddrString(const struct sockaddr* addr) const {
        char buf[INET6_ADDRSTRLEN];
        if (addr->sa_family == AF_INET) {
            inet_ntop(AF_INET, &reinterpret_cast<const struct sockaddr_in*>(addr)->sin_addr,
                      buf, sizeof(buf));
        } else if (addr->sa_family == AF_INET6) {
            inet_ntop(AF_INET6, &reinterpret_cast<const struct sockaddr_in6*>(addr)->sin6_addr,
                      buf, sizeof(buf));
#ifndef _Q_WINDOWS
        } else if (addr->sa_family == AF_UNIX) {
            const struct sockaddr_un* sun = reinterpret_cast<const struct sockaddr_un*>(addr);
            return std::string("UNIX socket ") + std::string(sun->sun_path,
                strnlen(sun->sun_path, sizeof(sun->sun_path)));
#endif
        } else {
            return "<unknown>";
        }
        return buf;
    }
};

//------------------------------------------------------------------------------
// Network Security Manager - Public Implementation
//------------------------------------------------------------------------------

QoreNetworkSecurityManager::QoreNetworkSecurityManager() {
    priv = new qore_net_security_private;
}

QoreNetworkSecurityManager::~QoreNetworkSecurityManager() {
    delete priv;
}

int QoreNetworkSecurityManager::addAllowedHost(const char* pattern, ExceptionSink* xsink) {
    if (!pattern || !*pattern) {
        xsink->raiseException("SANDBOX-CONFIG-ERROR", "Empty hostname pattern");
        return -1;
    }
    priv->allowed_hosts.push_back(pattern);
    return 0;
}

int QoreNetworkSecurityManager::addAllowedIPRange(const char* cidr, ExceptionSink* xsink) {
    CIDRRange range;
    if (!priv->parseCIDR(cidr, range, xsink)) {
        return -1;
    }
    priv->allowed_ranges.push_back(range);
    return 0;
}

int QoreNetworkSecurityManager::addDeniedIPRange(const char* cidr, ExceptionSink* xsink) {
    CIDRRange range;
    if (!priv->parseCIDR(cidr, range, xsink)) {
        return -1;
    }
    priv->denied_ranges.push_back(range);
    return 0;
}

void QoreNetworkSecurityManager::addAllowedPort(int port, int proto) {
    priv->allowed_ports.push_back({port, port, proto});
}

void QoreNetworkSecurityManager::addAllowedPortRange(int low, int high, int proto) {
    priv->allowed_ports.push_back({low, high, proto});
}

void QoreNetworkSecurityManager::blockPrivateNetworks() {
    ExceptionSink xsink;

    // IPv4 private networks
    addDeniedIPRange("127.0.0.0/8", &xsink);      // Localhost
    addDeniedIPRange("10.0.0.0/8", &xsink);       // RFC 1918 Class A
    addDeniedIPRange("172.16.0.0/12", &xsink);    // RFC 1918 Class B
    addDeniedIPRange("192.168.0.0/16", &xsink);   // RFC 1918 Class C
    addDeniedIPRange("169.254.0.0/16", &xsink);   // Link-local (includes cloud metadata)

    // IPv6 private networks
    addDeniedIPRange("::1/128", &xsink);          // Localhost
    addDeniedIPRange("fe80::/10", &xsink);        // Link-local
    addDeniedIPRange("fc00::/7", &xsink);         // Unique local addresses

    assert(!xsink);  // These should never fail
}

void QoreNetworkSecurityManager::blockLocalhost() {
    ExceptionSink xsink;
    addDeniedIPRange("127.0.0.0/8", &xsink);
    addDeniedIPRange("::1/128", &xsink);
    assert(!xsink);
}

void QoreNetworkSecurityManager::allowHTTPSOnly() {
    addAllowedPort(443, QSEC_NET_TCP);
}

bool QoreNetworkSecurityManager::allowsPort(int port, int proto) const {
    return priv->isPortAllowed(port, proto);
}

void QoreNetworkSecurityManager::setDefaultPolicy(bool allow) {
    priv->default_allow = allow;
}

bool QoreNetworkSecurityManager::getDefaultPolicy() const {
    return priv->default_allow;
}

bool QoreNetworkSecurityManager::checkConnect(const struct sockaddr* addr, socklen_t len,
                                               int proto, ExceptionSink* xsink) {
    return checkConnect(nullptr, addr, len, proto, xsink);
}

bool QoreNetworkSecurityManager::checkConnect(const char* hostname, const struct sockaddr* addr, socklen_t len,
                                               int proto, ExceptionSink* xsink) {
    if (priv->composition) {
        unsigned checked = 0;
        for (auto* manager : priv->composition->managers) {
            if (!(checked++ % 100) && qore_check_cancel(xsink, "sandbox policy intersection")) {
                return false;
            }
            if (!manager->checkNetworkAccess(hostname, addr, len, proto, xsink)) {
                return false;
            }
        }
        return true;
    }
    struct sockaddr_storage storage;
    addr = qore_net_security_private::normalizeAddr(addr, storage, true);

    // Check denied ranges first (always take precedence, also over an allowed host name)
    if (priv->isDenied(addr)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Connection to %s denied by security policy (private/blocked network)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    // Check port restrictions
    int port = priv->getPort(addr);
    if (port > 0 && !priv->isPortAllowed(port, proto)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Connection to port %d denied by security policy", port);
        return false;
    }

    // An allowed host name allows the connection
    if (priv->isHostAllowed(hostname)) {
        return true;
    }

    // Check allowed ranges
    if (!priv->allowed_ranges.empty()) {
        if (priv->inAllowedRange(addr)) {
            return true;
        }
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Connection to %s denied by security policy (not in allowed list)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    // Fall back to default policy
    if (!priv->default_allow) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Connection to %s denied by security policy (default deny)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    return true;
}

bool QoreNetworkSecurityManager::checkBind(const struct sockaddr* addr, socklen_t len,
                                            int proto, ExceptionSink* xsink) {
    if (priv->composition) {
        unsigned checked = 0;
        for (auto* manager : priv->composition->managers) {
            if (!(checked++ % 100) && qore_check_cancel(xsink, "sandbox policy intersection")) {
                return false;
            }
            if (!manager->checkNetworkBind(addr, len, proto, xsink)) {
                return false;
            }
        }
        return true;
    }
    struct sockaddr_storage storage;
    addr = qore_net_security_private::normalizeAddr(addr, storage, false);

    // denied ranges keep code from reaching networks; a socket bound to a loopback address can only be reached from
    // the local host, which is less than a bind on all interfaces exposes, so denied ranges do not apply to it
    if (!qore_net_security_private::isLoopback(addr) && priv->isDenied(addr)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Binding to %s denied by security policy (private/blocked network)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    // a system-assigned port (port 0) is not restricted
    int port = priv->getPort(addr);
    if (port > 0 && !priv->isPortAllowed(port, proto)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Binding to port %d denied by security policy", port);
        return false;
    }

    if (!priv->allowed_ranges.empty()) {
        if (priv->inAllowedRange(addr)) {
            return true;
        }
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Binding to %s denied by security policy (not in allowed list)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    if (!priv->default_allow) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Binding to %s denied by security policy (default deny)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    return true;
}

bool QoreNetworkSecurityManager::checkHostname(const char* hostname, int port, int proto) const {
    if (port > 0 && !priv->isPortAllowed(port, proto)) {
        return false;
    }
    // a matching host name is allowed unless it resolves to a denied range; any other name can only be allowed
    // by the address it resolves to
    return priv->isHostAllowed(hostname) || !priv->allowed_ranges.empty() || priv->default_allow;
}

bool QoreNetworkSecurityManager::isHostAllowed(const char* hostname) const {
    return priv->isHostAllowed(hostname);
}

QoreHashNode* QoreNetworkSecurityManager::getConfiguration(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> config(new QoreHashNode(hashdeclNetworkSecurityConfigInfo, xsink), xsink);

    // Default policy
    config->setKeyValue("default_policy",
        new QoreStringNode(priv->default_allow ? "allow" : "deny"), xsink);

    // Allowed hosts
    ReferenceHolder<QoreListNode> hosts(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& host : priv->allowed_hosts) {
        hosts->push(new QoreStringNode(host.c_str()), xsink);
    }
    config->setKeyValue("allowed_hosts", hosts.release(), xsink);

    // Note: For security, we don't expose the full CIDR range details
    config->setKeyValue("allowed_ranges_count", (int64)priv->allowed_ranges.size(), xsink);
    config->setKeyValue("denied_ranges_count", (int64)priv->denied_ranges.size(), xsink);

    // Allowed ports
    ReferenceHolder<QoreListNode> ports(new QoreListNode(hashdeclPortRangeInfo->getTypeInfo()), xsink);
    for (const auto& pr : priv->allowed_ports) {
        ReferenceHolder<QoreHashNode> entry(new QoreHashNode(hashdeclPortRangeInfo, xsink), xsink);
        entry->setKeyValue("low", pr.low, xsink);
        entry->setKeyValue("high", pr.high, xsink);
        entry->setKeyValue("proto", pr.proto, xsink);
        ports->push(entry.release(), xsink);
    }
    config->setKeyValue("allowed_ports", ports.release(), xsink);

    return config.release();
}

QoreNetworkSecurityManager* QoreNetworkSecurityManager::copy() const {
    QoreNetworkSecurityManager* result = new QoreNetworkSecurityManager();
    result->priv->composition = priv->composition;
    result->priv->allowed_hosts = priv->allowed_hosts;
    result->priv->allowed_ranges = priv->allowed_ranges;
    result->priv->denied_ranges = priv->denied_ranges;
    result->priv->allowed_ports = priv->allowed_ports;
    result->priv->default_allow = priv->default_allow;
    return result;
}

void QoreNetworkSecurityManager::reset() {
    priv->allowed_hosts.clear();
    priv->allowed_ranges.clear();
    priv->denied_ranges.clear();
    priv->allowed_ports.clear();
    priv->default_allow = false;
}

void QoreNetworkSecurityManager::copyFrom(const QoreNetworkSecurityManager& other) {
    priv->composition = other.priv->composition;
    priv->allowed_hosts = other.priv->allowed_hosts;
    priv->allowed_ranges = other.priv->allowed_ranges;
    priv->denied_ranges = other.priv->denied_ranges;
    priv->allowed_ports = other.priv->allowed_ports;
    priv->default_allow = other.priv->default_allow;
}

//------------------------------------------------------------------------------
// Sandbox Manager - Public Implementation
//------------------------------------------------------------------------------

QoreSandboxManager::QoreSandboxManager()
    : max_threads(0), interrupt_flags(0), wall_time_limit(0), cpu_time_limit(0), max_stack_size(0),
    mem_account(new QoreSandboxMemoryAccount), active_threads(0) {
}

namespace {
//! the managers that code that is not trusted created, with the ID of the Program it ran in (issue #5659)
/** Kept apart from QoreSandboxManager, so that its layout, which is part of the C++ API, does not change
*/
std::mutex untrusted_creators_lock;
std::unordered_map<const QoreSandboxManager*, qore_program_id_t> untrusted_creators;

//! returns the ID of the current Program, or QORE_NO_PROGRAM_ID
qore_program_id_t qore_sandbox_caller_pgm_id() {
    QoreProgram* pgm = getProgram();
    return pgm ? pgm->getProgramId() : QORE_NO_PROGRAM_ID;
}
}

void qore_sandbox_manager_record_creator(const QoreSandboxManager* sm) {
    if (!sm || !qore_calling_code_is_untrusted()) {
        return;
    }
    qore_program_id_t id = qore_sandbox_caller_pgm_id();
    std::lock_guard<std::mutex> lck(untrusted_creators_lock);
    untrusted_creators[sm] = id;
}

int qore_sandbox_manager_check_change(const QoreSandboxManager* sm, const char* what, ExceptionSink* xsink) {
    if (!qore_calling_code_is_untrusted()) {
        return 0;
    }
    qore_program_id_t id = qore_sandbox_caller_pgm_id();
    {
        std::lock_guard<std::mutex> lck(untrusted_creators_lock);
        auto i = untrusted_creators.find(sm);
        if (i != untrusted_creators.end() && i->second == id) {
            return 0;
        }
    }
    xsink->raiseException("SANDBOX-ACCESS-ERROR", "%s: sandboxed code, or code whose functional domains are "
        "restricted, can lift a restriction only of a SandboxManager that its own Program created; not of one that "
        "governs it or another Program", what);
    return -1;
}

QoreSandboxManager::~QoreSandboxManager() {
    {
        std::lock_guard<std::mutex> lck(untrusted_creators_lock);
        untrusted_creators.erase(this);
    }
    // the account lives on while data charged to it is alive
    mem_account->deref();
}

QoreFilesystemSecurityManager& QoreSandboxManager::filesystem() {
    return fs_mgr;
}

const QoreFilesystemSecurityManager& QoreSandboxManager::filesystem() const {
    return fs_mgr;
}

QoreNetworkSecurityManager& QoreSandboxManager::network() {
    return net_mgr;
}

const QoreNetworkSecurityManager& QoreSandboxManager::network() const {
    return net_mgr;
}

// Enforcement views report the tightest positive constituent limit; zero means unlimited.
template <typename T>
static T sandbox_intersection_limit(const QoreSandboxManager* manager, T own,
        T (QoreSandboxManager::*get)() const) {
    if (const auto* members = QoreSandboxComposition::members(manager)) {
        for (const auto* member : *members) {
            T limit = (member->*get)();
            if (limit > 0 && (own <= 0 || limit < own)) {
                own = limit;
            }
        }
    }
    return own;
}

void QoreSandboxManager::setMaxThreads(int count) {
    max_threads = count;
    // a raised limit frees slots for the thread pools waiting for one
    notifyThreadSlotReleased();
}

int QoreSandboxManager::getMaxThreads() const {
    return sandbox_intersection_limit<int>(this, max_threads.load(), &QoreSandboxManager::getMaxThreads);
}

void QoreSandboxManager::setWallTimeLimit(int64 ms) {
    wall_time_limit = ms > 0 ? ms : 0;
}

int64 QoreSandboxManager::getWallTimeLimit() const {
    return sandbox_intersection_limit<int64>(this, wall_time_limit.load(), &QoreSandboxManager::getWallTimeLimit);
}

void QoreSandboxManager::setCPUTimeLimit(int64 ms) {
    cpu_time_limit = ms > 0 ? ms : 0;
}

int64 QoreSandboxManager::getCPUTimeLimit() const {
    return sandbox_intersection_limit<int64>(this, cpu_time_limit.load(), &QoreSandboxManager::getCPUTimeLimit);
}

void QoreSandboxManager::setMemoryLimit(int64 bytes) {
    mem_account->limit.store(bytes > 0 ? bytes : 0, std::memory_order_relaxed);
}

int64 QoreSandboxManager::getMemoryLimit() const {
    return sandbox_intersection_limit<int64>(this, mem_account->limit.load(std::memory_order_relaxed),
        &QoreSandboxManager::getMemoryLimit);
}

int64 QoreSandboxManager::getMemoryUsage() const {
    return mem_account->used.load(std::memory_order_relaxed);
}

void QoreSandboxManager::setMaxStackSize(int64 bytes) {
    max_stack_size = bytes > 0 ? bytes : 0;
}

int64 QoreSandboxManager::getMaxStackSize() const {
    return sandbox_intersection_limit<int64>(this, max_stack_size.load(), &QoreSandboxManager::getMaxStackSize);
}

int QoreSandboxManager::acquireThreadSlot(ExceptionSink* xsink) {
    bool released = false;
    int rc = acquireThreadSlotQuiet(xsink, released);
    if (released) {
        // the slots of a composition taken before a member refused one were released again
        notifyThreadSlotReleased();
    }
    return rc;
}

int QoreSandboxManager::acquireThreadSlotQuiet(ExceptionSink* xsink, bool& released) {
    if (const auto* managers = QoreSandboxComposition::members(this)) {
        size_t acquired = 0;
        for (auto* manager : *managers) {
            if ((!(acquired % 100) && qore_check_cancel(xsink, "sandbox thread reservation"))
                    || manager->acquireThreadSlotQuiet(xsink, released)) {
                while (acquired) {
                    (*managers)[--acquired]->releaseThreadSlotQuiet();
                    released = true;
                }
                return -1;
            }
            ++acquired;
        }
        return 0;
    }
    int count = active_threads.load();
    while (true) {
        int max = max_threads.load();
        if (max > 0 && count >= max) {
            xsink->raiseException("THREAD-CREATION-FAILURE", "cannot start a thread: sandbox thread limit of %d "
                "reached", max);
            return -1;
        }
        if (active_threads.compare_exchange_weak(count, count + 1)) {
            return 0;
        }
    }
}

//------------------------------------------------------------------------------
// Thread pools waiting for a thread slot
//------------------------------------------------------------------------------


//! The thread pools waiting for a thread slot of a sandbox manager
/** Pools register with their own lock held and are notified without it: a notifier takes this lock, references the
    waiting pools, releases this lock and then takes the lock of each pool to wake it, so a pool lock and this lock are
    never taken in the opposite order.
*/
static std::mutex slot_waiters_lock;
static std::multimap<QoreSandboxManager*, ThreadPool*> slot_waiters;
//! The number of registered pools, so that a release can skip the registry when no pool waits
/** Sequentially consistent with the slot counts: a pool registers before it tries to take a slot, and a release
    reads this count after releasing the slot, so either the pool takes the slot or the release sees the pool.
*/
static std::atomic<int> slot_waiter_count{0};

void ThreadPool::registerSlotWaiter(ThreadPool* tp) {
    assert(tp->sbx);
    std::lock_guard<std::mutex> lck(slot_waiters_lock);
    slot_waiters.emplace(tp->sbx, tp);
    slot_waiter_count.fetch_add(1, std::memory_order_seq_cst);
}

void ThreadPool::unregisterSlotWaiter(ThreadPool* tp) {
    std::lock_guard<std::mutex> lck(slot_waiters_lock);
    auto range = slot_waiters.equal_range(tp->sbx);
    for (auto i = range.first; i != range.second; ++i) {
        if (i->second == tp) {
            slot_waiters.erase(i);
            slot_waiter_count.fetch_sub(1, std::memory_order_seq_cst);
            return;
        }
    }
    assert(false);
}

void ThreadPool::notifySlotReleased(QoreSandboxManager* sm, const AbstractPrivateData* except) {
    if (!slot_waiter_count.load(std::memory_order_seq_cst)) {
        return;
    }
    // the waiting pools, referenced so that they stay valid after the registry lock is released
    std::vector<ThreadPool*> pools;
    {
        std::lock_guard<std::mutex> lck(slot_waiters_lock);
        auto range = slot_waiters.equal_range(sm);
        for (auto i = range.first; i != range.second; ++i) {
            // a pool being destroyed (no references left) cannot use a slot; never revive it
            if (static_cast<const AbstractPrivateData*>(i->second) != except && i->second->optRef()) {
                pools.push_back(i->second);
            }
        }
    }
    ExceptionSink xsink;
    for (ThreadPool* tp : pools) {
        {
            AutoLocker al(tp->m);
            tp->slot_notified = true;
            tp->cond.signal();
        }
        tp->deref(&xsink);
    }
}

void QoreSandboxManager::releaseThreadSlot() {
    releaseThreadSlotQuiet();
    notifyThreadSlotReleased();
}

void QoreSandboxManager::notifyThreadSlotReleased(const AbstractPrivateData* except) {
    // a thread pool waiting for a slot of this manager, or of a member of this composition, can start a worker now
    ThreadPool::notifySlotReleased(this, except);
    if (const auto* managers = QoreSandboxComposition::members(this)) {
        for (auto* manager : *managers) {
            ThreadPool::notifySlotReleased(manager, except);
        }
    }
}

void QoreSandboxManager::releaseThreadSlotQuiet() {
    if (const auto* managers = QoreSandboxComposition::members(this)) {
        for (auto* manager : *managers) {
            manager->releaseThreadSlotQuiet();
        }
        return;
    }
    int prev = active_threads.fetch_sub(1);
    assert(prev > 0);
    (void)prev;
}

void QoreSandboxManager::requestInterrupt() {
    // seq_cst pairs with seq_cst on the waiter side (register waiting_on, then check interrupt) to
    // defeat the lost-wakeup race in QoreCondition::waitWithInterrupt
    interrupt_flags.fetch_or(QSB_INTR_REQUESTED, std::memory_order_seq_cst);

    // Wake any threads currently blocked in QoreCondition::waitWithInterrupt — they re-check their
    // cancel/interrupt state on wakeup.  We don't filter by program (no back-ref from manager to
    // program); spurious wakeups for threads in other programs are harmless — those threads recheck
    // their own program's interrupt state, find it not set, and resume waiting.
    thread_list.wakeAllWaiters();

    // Copy callbacks under lock, then invoke outside lock to avoid deadlock
    // (callbacks may call unregisterCancelCallback() which needs the lock)
    std::vector<cancel_callback_t> callbacks_copy;
    {
        std::lock_guard<std::mutex> lock(cancel_mutex);
        callbacks_copy.reserve(cancel_callbacks.size());
        for (const auto& cb : cancel_callbacks) {
            callbacks_copy.push_back(cb.second);
        }
    }

    // Call all registered cancel callbacks without holding the lock
    for (auto& cb : callbacks_copy) {
        try {
            cb();
        } catch (...) {
            // Ignore exceptions from callbacks
        }
    }
}

void QoreSandboxManager::clearInterrupt() {
    interrupt_flags.fetch_and(~QSB_INTR_REQUESTED, std::memory_order_seq_cst);
}

bool QoreSandboxManager::isInterruptRequested() const {
    if (const auto* managers = QoreSandboxComposition::members(this)) {
        for (auto* manager : *managers) {
            if (manager->isInterruptRequested()) {
                return true;
            }
        }
    }
    return interrupt_flags.load(std::memory_order_seq_cst) & QSB_INTR_REQUESTED;
}

void QoreSandboxManager::registerCancelCallback(void* context, cancel_callback_t callback) {
    if (const auto* managers = QoreSandboxComposition::members(this)) {
        auto invoked = std::make_shared<std::atomic<bool>>(false);
        cancel_callback_t once = [invoked, callback = std::move(callback)]() {
            return !invoked->exchange(true) && callback();
        };
        size_t registered = 0;
        try {
            for (auto* manager : *managers) {
                manager->registerCancelCallback(context, once);
                ++registered;
            }
        } catch (...) {
            while (registered) {
                (*managers)[--registered]->unregisterCancelCallback(context);
            }
            throw;
        }
        return;
    }
    std::lock_guard<std::mutex> lock(cancel_mutex);
    cancel_callbacks.emplace_back(context, std::move(callback));
}

void QoreSandboxManager::unregisterCancelCallback(void* context) {
    if (const auto* managers = QoreSandboxComposition::members(this)) {
        for (auto* manager : *managers) {
            manager->unregisterCancelCallback(context);
        }
        return;
    }
    std::lock_guard<std::mutex> lock(cancel_mutex);
    cancel_callbacks.erase(
        std::remove_if(cancel_callbacks.begin(), cancel_callbacks.end(),
            [context](const auto& pair) { return pair.first == context; }),
        cancel_callbacks.end());
}

bool QoreSandboxManager::checkIOInterrupt(ExceptionSink* xsink, const char* operation) const {
    if (isInterruptRequested()) {
        xsink->raiseException("PROGRAM-INTERRUPTED",
            "program execution was interrupted during %s", operation);
        return true;
    }
    return false;
}

QoreHashNode* QoreSandboxManager::getConfiguration(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> config(new QoreHashNode(hashdeclSandboxConfigInfo, xsink), xsink);

    config->setKeyValue("filesystem", fs_mgr.getConfiguration(xsink), xsink);
    config->setKeyValue("network", net_mgr.getConfiguration(xsink), xsink);

    config->setKeyValue("max_threads", max_threads.load(), xsink);
    config->setKeyValue("wall_time_limit", wall_time_limit.load(), xsink);
    config->setKeyValue("cpu_time_limit", cpu_time_limit.load(), xsink);
    config->setKeyValue("max_stack_size", max_stack_size.load(), xsink);
    config->setKeyValue("memory_limit", static_cast<int64>(mem_account->limit.load()), xsink);
    config->setKeyValue("interrupt_requested", static_cast<bool>(interrupt_flags.load() & QSB_INTR_REQUESTED),
        xsink);

    return config.release();
}

//! Returns true for a read of a resource beneath the module's own directory while the module is being loaded
/** Module initialization commonly materializes immutable resources such as application logos into constants.
    The module loader sets a private thread-local module context only for the load/initialization phase.  Allowing
    read-only access beneath that module's directory preserves this supported packaging pattern without granting
    module methods any filesystem access when they are later called by sandboxed user code.

    Both paths are canonicalized before the containment check, so `..` traversal and symlinks escaping the module
    directory remain denied.  Directory-backed source modules use the module path itself; file-backed modules use
    its lexical parent.  The parent is canonicalized rather than the module file because in-tree `.qmod` files can
    be symlinks while their resources intentionally remain beside the symlink.
*/
static bool is_module_initialization_resource_read(const char* path, int mode) {
    if (mode != QSEC_READ) {
        return false;
    }

    const char* module_path = get_module_context_path();
    if (!module_path || !*module_path) {
        return false;
    }

    std::string module_root(module_path);
    struct stat sb;
    if (stat(module_path, &sb) || !S_ISDIR(sb.st_mode)) {
        size_t sep = module_root.find_last_of("/\\");
        if (sep == std::string::npos) {
            // A module name alone does not establish a safe resource directory.
            return false;
        }
        module_root.resize(sep ? sep : 1);
    }

    std::unique_ptr<char, decltype(&free)> canonical_root(realpath(module_root.c_str(), nullptr), &free);
    std::unique_ptr<char, decltype(&free)> canonical_path(realpath(path, nullptr), &free);
    if (!canonical_root || !canonical_path) {
        return false;
    }

    std::string root(canonical_root.get());
    std::string target(canonical_path.get());
    if (target.compare(0, root.size(), root)) {
        return false;
    }
    return target.size() == root.size() || root.back() == '/' || target[root.size()] == '/';
}

bool QoreSandboxManager::checkFilesystemAccess(const char* path, int mode, ExceptionSink* xsink) {
    if (is_module_initialization_resource_read(path, mode)) {
        return true;
    }
    return fs_mgr.checkAccess(path, mode, xsink);
}

bool QoreSandboxManager::checkNetworkAccess(const struct sockaddr* addr, socklen_t len,
                                             int proto, ExceptionSink* xsink) {
    return net_mgr.checkConnect(addr, len, proto, xsink);
}

bool QoreSandboxManager::checkNetworkAccess(const char* hostname, const struct sockaddr* addr, socklen_t len,
                                             int proto, ExceptionSink* xsink) {
    return net_mgr.checkConnect(hostname, addr, len, proto, xsink);
}

bool QoreSandboxManager::checkNetworkBind(const struct sockaddr* addr, socklen_t len,
                                           int proto, ExceptionSink* xsink) {
    return net_mgr.checkBind(addr, len, proto, xsink);
}

QoreSandboxManager* QoreSandboxManager::copy() const {
    QoreSandboxManager* result = new QoreSandboxManager();

    // Copy filesystem and network managers
    result->fs_mgr.copyFrom(fs_mgr);
    result->net_mgr.copyFrom(net_mgr);

    // Copy the thread limit; the copy governs no threads yet
    result->max_threads = max_threads.load();
    result->wall_time_limit = wall_time_limit.load();
    result->cpu_time_limit = cpu_time_limit.load();
    result->max_stack_size = max_stack_size.load();
    result->mem_account->limit.store(mem_account->limit.load());
    // Don't copy interrupt state

    return result;
}

QoreSandboxManager* QoreSandboxManager::createLockdown() {
    QoreSandboxManager* sm = new QoreSandboxManager();

    // No filesystem access - leave empty allow list
    sm->fs_mgr.setDefaultPolicy(false);

    // No network access - leave empty allow list
    sm->net_mgr.setDefaultPolicy(false);

    // one thread started by the sandboxed code at a time
    sm->setMaxThreads(1);

    // 5 seconds per call, of which 1 second of CPU time
    sm->setWallTimeLimit(5000);
    sm->setCPUTimeLimit(1000);

    // 1MB of stack per call
    sm->setMaxStackSize(1024 * 1024);

    // 10MB of data
    sm->setMemoryLimit(10 * 1024 * 1024);

    return sm;
}

QoreSandboxManager* QoreSandboxManager::createWebSafe() {
    QoreSandboxManager* sm = new QoreSandboxManager();

    // No filesystem access
    sm->fs_mgr.setDefaultPolicy(false);

    // Network allowed but private networks blocked
    sm->net_mgr.setDefaultPolicy(true);
    sm->net_mgr.blockPrivateNetworks();

    // up to 4 threads
    sm->setMaxThreads(4);

    // 30 seconds per call, of which 10 seconds of CPU time
    sm->setWallTimeLimit(30000);
    sm->setCPUTimeLimit(10000);

    // 2MB of stack per call
    sm->setMaxStackSize(2 * 1024 * 1024);

    // 100MB of data
    sm->setMemoryLimit(100 * 1024 * 1024);

    return sm;
}

//------------------------------------------------------------------------------
// qore_program_private::getSandboxManagerRef() - defined here to avoid
// incomplete type issues (QoreSandboxManager is forward-declared in the header)
//------------------------------------------------------------------------------

QoreSandboxManager* qore_program_private::getSandboxManagerRef() {
    // Almost every program has no manager, and this runs at every cancellation point of every thread running in
    // the program, so that is decided without sm_lock.  A manager set concurrently is found by the next lookup,
    // exactly as if this one had taken the lock just before it was set.
    if (!sandbox_manager.load(std::memory_order_acquire)) {
        return nullptr;
    }
    // a manager has to be referenced under the lock, which keeps it from being released in between
    AutoLocker al(sm_lock);
    QoreSandboxManager* sm = sandbox_manager.load(std::memory_order_relaxed);
    if (sm && sm->optRef()) {
        return sm;
    }
    return nullptr;
}

bool qore_program_is_sandboxed(const QoreProgram* pgm) {
    if (!pgm) {
        return false;
    }
    const qore_program_private* pp = qore_program_private::get(*const_cast<QoreProgram*>(pgm));
    // the flag is set whenever a manager is set; the manager is read as well, so that the answer never depends on
    // the order of the two stores
    return pp->sandboxed.load(std::memory_order_acquire) || pp->sandbox_manager.load(std::memory_order_acquire);
}

void qore_program_mark_sandboxed(QoreProgram& pgm) {
    qore_program_private::get(pgm)->sandboxed.store(true, std::memory_order_release);
}

namespace {
//! the Programs with module lifetime, each with a dependency reference; see qore_program_set_module_lifetime()
struct ModuleProgramPins {
    std::mutex m;
    //! leaf lock: only the reference count is changed while it is held
    QoreForkStdMutex fork_handler{m, QFLR_LEAF, "module_program_pins"};
    std::vector<QoreProgram*> pgms;
};

ModuleProgramPins& module_program_pins() {
    // never destroyed: the pins are released by qore_cleanup(), which can run after static destructors started
    static ModuleProgramPins* p = new ModuleProgramPins;
    return *p;
}
}

// written under ModuleProgramPins::m; constant-initialized, so it is valid before and after static initialization
std::atomic<bool> qore_module_lifetime_flag{true};

void qore_program_set_module_lifetime(QoreProgram& pgm) {
    qore_program_private* pp = qore_program_private::get(pgm);
    if (pp->module_lifetime.load(std::memory_order_acquire)) {
        return;
    }
    ModuleProgramPins& pins = module_program_pins();
    std::lock_guard<std::mutex> l(pins.m);
    if (!qore_module_lifetime_flag.load(std::memory_order_relaxed)
            || pp->module_lifetime.load(std::memory_order_relaxed)) {
        return;
    }
    pins.pgms.push_back(&pgm);
    pgm.depRef();
    // published after the reference has been taken
    pp->module_lifetime.store(true, std::memory_order_release);
}

bool qore_program_has_module_lifetime(const QoreProgram* pgm) {
    return pgm && qore_module_lifetime_flag.load(std::memory_order_acquire)
        && qore_program_private::get(*const_cast<QoreProgram*>(pgm))->module_lifetime.load(
            std::memory_order_acquire);
}

void qore_get_module_lifetime_programs(std::vector<QoreProgram*>& pgms) {
    ModuleProgramPins& pins = module_program_pins();
    std::lock_guard<std::mutex> l(pins.m);
    pgms = pins.pgms;
}

void qore_release_module_program_pins() {
    ModuleProgramPins& pins = module_program_pins();
    std::vector<QoreProgram*> pgms;
    {
        std::lock_guard<std::mutex> l(pins.m);
        qore_module_lifetime_flag.store(false, std::memory_order_release);
        pgms.swap(pins.pgms);
    }
    for (QoreProgram* pgm : pgms) {
        pgm->depDeref();
    }
}

//------------------------------------------------------------------------------
// QoreSandboxManagerHelper implementation
//------------------------------------------------------------------------------

const std::vector<QoreSandboxManager*>* QoreSandboxComposition::members(const QoreSandboxManager* manager) {
    return manager && manager->fs_mgr.priv->composition
        ? &manager->fs_mgr.priv->composition->managers : nullptr;
}

bool QoreSandboxComposition::same(const QoreSandboxManager* a, const QoreSandboxManager* b) {
    if (a == b) {
        return true;
    }
    const auto* am = members(a);
    const auto* bm = members(b);
    return am && bm && *am == *bm;
}

QoreSandboxManager* QoreSandboxComposition::combine(const std::vector<QoreSandboxManager*>& managers) {
    QoreSandboxManager* first = nullptr;
    bool identical = true;
    for (auto* manager : managers) {
        if (!manager) {
            continue;
        }
        if (!first) {
            first = manager;
        } else if (first != manager) {
            identical = false;
        }
    }
    if (identical) {
        if (first) {
            first->ref();
        }
        return first;
    }
    std::vector<QoreSandboxManager*> flat;
    for (auto* manager : managers) {
        if (const auto* entries = members(manager)) {
            flat.insert(flat.end(), entries->begin(), entries->end());
        } else if (manager) {
            flat.push_back(manager);
        }
    }
    std::sort(flat.begin(), flat.end(), std::less<QoreSandboxManager*>());
    flat.erase(std::unique(flat.begin(), flat.end()), flat.end());
    for (auto* manager : managers) {
        const auto* entries = members(manager);
        if ((entries && *entries == flat) || (flat.size() == 1 && manager == flat.front())) {
            manager->ref();
            return manager;
        }
    }
    auto composition = std::make_shared<const QoreSandboxMembers>(std::move(flat));
    ReferenceHolder<QoreSandboxManager> result(new QoreSandboxManager, nullptr);
    result->fs_mgr.priv->composition = composition;
    result->net_mgr.priv->composition = std::move(composition);
    return result.release();
}

QoreSandboxManagerHelper::QoreSandboxManagerHelper() {
    // Resolve the active manager across the thread's program-context (call) stack so that
    // module/library code invoked from a sandboxed program inherits the caller's sandbox.
    // A Qore-language module executes in the module's own QoreProgram (which has no manager
    // of its own), so resolving from getProgram() alone would silently bypass the sandbox.
    // See design/module-sandboxing-audit-guide.md.
    ptr = qore_find_thread_sandbox_manager_ref();
}

QoreSandboxManagerHelper::QoreSandboxManagerHelper(Purpose purpose) {
    // a POLICY check honors an active policy barrier, so trusted infrastructure does not
    // inherit a sandboxed caller's filesystem/network policy; every other purpose (notably
    // interrupt/force-terminate) resolves unchanged
    ptr = purpose == Policy
        ? qore_find_thread_sandbox_policy_manager_ref()
        : qore_find_thread_sandbox_manager_ref();
}

QoreSandboxManagerHelper::QoreSandboxManagerHelper(QoreProgram* pgm) {
    if (pgm) {
        ptr = qore_program_private::get(*pgm)->getSandboxManagerRef();
    } else {
        ptr = nullptr;
    }
}

QoreSandboxManagerHelper::~QoreSandboxManagerHelper() {
    if (ptr) {
        ptr->deref(nullptr);
    }
}

QoreSandboxPolicyBarrierHelper::QoreSandboxPolicyBarrierHelper() {
    active = !qore_push_sandbox_policy_barrier();
}

QoreSandboxPolicyBarrierHelper::~QoreSandboxPolicyBarrierHelper() {
    if (active) {
        qore_pop_sandbox_policy_barrier();
    }
}
