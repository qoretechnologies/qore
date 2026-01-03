/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxManager.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>

//------------------------------------------------------------------------------
// Filesystem Security Manager - Private Implementation
//------------------------------------------------------------------------------

struct PathRule {
    std::string path;
    int mode;

    PathRule(const std::string& p, int m) : path(p), mode(m) {}
};

struct qore_fs_security_private {
    std::vector<PathRule> allowed_paths;
    std::vector<std::string> denied_paths;
    std::string sandbox_root;
    bool default_allow = false;

    // Canonicalize a path using realpath, handling non-existent paths
    std::string canonicalize(const char* path, ExceptionSink* xsink) {
        // First try realpath directly
        char* real = realpath(path, nullptr);
        if (real) {
            std::string result(real);
            free(real);
            return result;
        }

        // If path doesn't exist, canonicalize the parent directory
        // and append the filename
        std::string spath(path);

        // Find last separator
        size_t pos = spath.rfind('/');
        if (pos == std::string::npos) {
            // No separator, use current directory
            char* cwd = realpath(".", nullptr);
            if (cwd) {
                std::string result = std::string(cwd) + "/" + spath;
                free(cwd);
                return result;
            }
            xsink->raiseException("SANDBOX-PATH-ERROR",
                "Cannot canonicalize path '%s': %s", path, strerror(errno));
            return "";
        }

        // Get parent directory
        std::string parent = spath.substr(0, pos);
        std::string filename = spath.substr(pos + 1);

        if (parent.empty()) {
            parent = "/";
        }

        real = realpath(parent.c_str(), nullptr);
        if (!real) {
            xsink->raiseException("SANDBOX-PATH-ERROR",
                "Cannot canonicalize path '%s': parent directory does not exist", path);
            return "";
        }

        std::string result = std::string(real) + "/" + filename;
        free(real);
        return result;
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
    // Canonicalize the path
    std::string canon = priv->canonicalize(path, xsink);
    if (*xsink) {
        // Clear the exception and raise access denied instead
        xsink->clear();
        xsink->raiseException("FILESYSTEM-ACCESS-DENIED",
            "Access to '%s' denied: path cannot be resolved", path);
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

QoreHashNode* QoreFilesystemSecurityManager::getConfiguration() const {
    ReferenceHolder<QoreHashNode> config(new QoreHashNode(autoTypeInfo), nullptr);

    // Sandbox root
    if (!priv->sandbox_root.empty()) {
        config->setKeyValue("sandbox_root", new QoreStringNode(priv->sandbox_root.c_str()), nullptr);
    }

    // Default policy
    config->setKeyValue("default_policy",
        new QoreStringNode(priv->default_allow ? "allow" : "deny"), nullptr);

    // Allowed paths
    ReferenceHolder<QoreListNode> allowed(new QoreListNode(autoTypeInfo), nullptr);
    for (const auto& rule : priv->allowed_paths) {
        ReferenceHolder<QoreHashNode> entry(new QoreHashNode(autoTypeInfo), nullptr);
        entry->setKeyValue("path", new QoreStringNode(rule.path.c_str()), nullptr);
        entry->setKeyValue("mode", rule.mode, nullptr);
        allowed->push(entry.release(), nullptr);
    }
    config->setKeyValue("allowed_paths", allowed.release(), nullptr);

    // Denied paths
    ReferenceHolder<QoreListNode> denied(new QoreListNode(stringTypeInfo), nullptr);
    for (const auto& path : priv->denied_paths) {
        denied->push(new QoreStringNode(path.c_str()), nullptr);
    }
    config->setKeyValue("denied_paths", denied.release(), nullptr);

    return config.release();
}

QoreFilesystemSecurityManager* QoreFilesystemSecurityManager::copy() const {
    QoreFilesystemSecurityManager* result = new QoreFilesystemSecurityManager();
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

    // Check if host pattern matches hostname
    bool hostMatches(const std::string& pattern, const char* hostname) const {
        // Simple wildcard matching
        if (pattern == "*") return true;

        if (pattern[0] == '*' && pattern[1] == '.') {
            // *.example.com pattern
            std::string suffix = pattern.substr(1);  // .example.com
            std::string host(hostname);

            // Check if hostname ends with suffix
            if (host.size() >= suffix.size() &&
                host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0) {
                return true;
            }
            // Also match exact domain (*.example.com should match example.com)
            if (host == pattern.substr(2)) {
                return true;
            }
            return false;
        }

        // Exact match
        return pattern == hostname;
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

    // Check if address is in allowed ranges
    bool isAllowed(const struct sockaddr* addr) const {
        if (allowed_ranges.empty()) return default_allow;

        for (const auto& range : allowed_ranges) {
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

void QoreNetworkSecurityManager::setDefaultPolicy(bool allow) {
    priv->default_allow = allow;
}

bool QoreNetworkSecurityManager::getDefaultPolicy() const {
    return priv->default_allow;
}

bool QoreNetworkSecurityManager::checkConnect(const struct sockaddr* addr, socklen_t len,
                                               int proto, ExceptionSink* xsink) {
    // Check denied ranges first (always take precedence)
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

    // Check allowed ranges
    if (!priv->allowed_ranges.empty() && !priv->isAllowed(addr)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Connection to %s denied by security policy (not in allowed list)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    // Fall back to default policy
    if (!priv->default_allow && priv->allowed_ranges.empty()) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Connection to %s denied by security policy (default deny)",
            priv->getAddrString(addr).c_str());
        return false;
    }

    return true;
}

bool QoreNetworkSecurityManager::checkBind(const struct sockaddr* addr, socklen_t len,
                                            int proto, ExceptionSink* xsink) {
    // For bind, we typically want to be more permissive
    // But still check denied ranges and port restrictions
    if (priv->isDenied(addr)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Binding to %s denied by security policy",
            priv->getAddrString(addr).c_str());
        return false;
    }

    int port = priv->getPort(addr);
    if (port > 0 && !priv->isPortAllowed(port, proto)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED",
            "Binding to port %d denied by security policy", port);
        return false;
    }

    return true;
}

bool QoreNetworkSecurityManager::checkHostname(const char* hostname, int port, int proto) const {
    // Check if any host pattern matches
    for (const auto& pattern : priv->allowed_hosts) {
        if (priv->hostMatches(pattern, hostname)) {
            // Also check port if restrictions are in place
            if (!priv->allowed_ports.empty() && !priv->isPortAllowed(port, proto)) {
                return false;
            }
            return true;
        }
    }

    // If no host patterns defined, fall back to default policy
    // (but final check must still be done on resolved IP)
    return priv->allowed_hosts.empty() ? priv->default_allow : false;
}

QoreHashNode* QoreNetworkSecurityManager::getConfiguration() const {
    ReferenceHolder<QoreHashNode> config(new QoreHashNode(autoTypeInfo), nullptr);

    // Default policy
    config->setKeyValue("default_policy",
        new QoreStringNode(priv->default_allow ? "allow" : "deny"), nullptr);

    // Allowed hosts
    ReferenceHolder<QoreListNode> hosts(new QoreListNode(stringTypeInfo), nullptr);
    for (const auto& host : priv->allowed_hosts) {
        hosts->push(new QoreStringNode(host.c_str()), nullptr);
    }
    config->setKeyValue("allowed_hosts", hosts.release(), nullptr);

    // Note: For security, we don't expose the full CIDR range details
    config->setKeyValue("allowed_ranges_count", (int64)priv->allowed_ranges.size(), nullptr);
    config->setKeyValue("denied_ranges_count", (int64)priv->denied_ranges.size(), nullptr);

    // Allowed ports
    ReferenceHolder<QoreListNode> ports(new QoreListNode(autoTypeInfo), nullptr);
    for (const auto& pr : priv->allowed_ports) {
        ReferenceHolder<QoreHashNode> entry(new QoreHashNode(autoTypeInfo), nullptr);
        entry->setKeyValue("low", pr.low, nullptr);
        entry->setKeyValue("high", pr.high, nullptr);
        entry->setKeyValue("proto", pr.proto, nullptr);
        ports->push(entry.release(), nullptr);
    }
    config->setKeyValue("allowed_ports", ports.release(), nullptr);

    return config.release();
}

QoreNetworkSecurityManager* QoreNetworkSecurityManager::copy() const {
    QoreNetworkSecurityManager* result = new QoreNetworkSecurityManager();
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
    : memory_limit(0), cpu_time_limit(0), wall_time_limit(0),
      max_threads(0), max_recursion(0), interrupt_requested(false) {
}

QoreSandboxManager::~QoreSandboxManager() {
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

void QoreSandboxManager::setMemoryLimit(size_t bytes) {
    memory_limit = bytes;
}

size_t QoreSandboxManager::getMemoryLimit() const {
    return memory_limit;
}

void QoreSandboxManager::setCPUTimeLimit(int64 ms) {
    cpu_time_limit = ms;
}

int64 QoreSandboxManager::getCPUTimeLimit() const {
    return cpu_time_limit;
}

void QoreSandboxManager::setWallTimeLimit(int64 ms) {
    wall_time_limit = ms;
}

int64 QoreSandboxManager::getWallTimeLimit() const {
    return wall_time_limit;
}

void QoreSandboxManager::setMaxThreads(int count) {
    max_threads = count;
}

int QoreSandboxManager::getMaxThreads() const {
    return max_threads;
}

void QoreSandboxManager::setMaxRecursionDepth(int depth) {
    max_recursion = depth;
}

int QoreSandboxManager::getMaxRecursionDepth() const {
    return max_recursion;
}

void QoreSandboxManager::requestInterrupt() {
    interrupt_requested.store(true, std::memory_order_release);
}

void QoreSandboxManager::clearInterrupt() {
    interrupt_requested.store(false, std::memory_order_release);
}

bool QoreSandboxManager::isInterruptRequested() const {
    return interrupt_requested.load(std::memory_order_acquire);
}

bool QoreSandboxManager::checkIOInterrupt(ExceptionSink* xsink, const char* operation) const {
    if (interrupt_requested.load(std::memory_order_acquire)) {
        xsink->raiseException("PROGRAM-INTERRUPTED",
            "program execution was interrupted during %s", operation);
        return true;
    }
    return false;
}

QoreHashNode* QoreSandboxManager::getConfiguration() const {
    ReferenceHolder<QoreHashNode> config(new QoreHashNode(autoTypeInfo), nullptr);

    config->setKeyValue("filesystem", fs_mgr.getConfiguration(), nullptr);
    config->setKeyValue("network", net_mgr.getConfiguration(), nullptr);

    config->setKeyValue("memory_limit", (int64)memory_limit, nullptr);
    config->setKeyValue("cpu_time_limit", cpu_time_limit, nullptr);
    config->setKeyValue("wall_time_limit", wall_time_limit, nullptr);
    config->setKeyValue("max_threads", max_threads, nullptr);
    config->setKeyValue("max_recursion", max_recursion, nullptr);
    config->setKeyValue("interrupt_requested", interrupt_requested.load(), nullptr);

    return config.release();
}

bool QoreSandboxManager::checkFilesystemAccess(const char* path, int mode, ExceptionSink* xsink) {
    return fs_mgr.checkAccess(path, mode, xsink);
}

bool QoreSandboxManager::checkNetworkAccess(const struct sockaddr* addr, socklen_t len,
                                             int proto, ExceptionSink* xsink) {
    return net_mgr.checkConnect(addr, len, proto, xsink);
}

QoreSandboxManager* QoreSandboxManager::copy() const {
    QoreSandboxManager* result = new QoreSandboxManager();

    // Copy filesystem and network managers
    result->fs_mgr.copyFrom(fs_mgr);
    result->net_mgr.copyFrom(net_mgr);

    // Copy the limits
    result->memory_limit = memory_limit;
    result->cpu_time_limit = cpu_time_limit;
    result->wall_time_limit = wall_time_limit;
    result->max_threads = max_threads;
    result->max_recursion = max_recursion;
    // Don't copy interrupt state

    return result;
}

QoreSandboxManager* QoreSandboxManager::createLockdown() {
    QoreSandboxManager* sm = new QoreSandboxManager();

    // No filesystem access - leave empty allow list
    sm->fs_mgr.setDefaultPolicy(false);

    // No network access - leave empty allow list
    sm->net_mgr.setDefaultPolicy(false);

    // Strict resource limits
    sm->setMemoryLimit(10 * 1024 * 1024);   // 10 MB
    sm->setCPUTimeLimit(1000);               // 1 second
    sm->setWallTimeLimit(5000);              // 5 seconds
    sm->setMaxThreads(1);                    // Single thread only
    sm->setMaxRecursionDepth(100);           // Limited recursion

    return sm;
}

QoreSandboxManager* QoreSandboxManager::createWebSafe() {
    QoreSandboxManager* sm = new QoreSandboxManager();

    // No filesystem access
    sm->fs_mgr.setDefaultPolicy(false);

    // Network allowed but private networks blocked
    sm->net_mgr.setDefaultPolicy(true);
    sm->net_mgr.blockPrivateNetworks();

    // Reasonable resource limits
    sm->setMemoryLimit(100 * 1024 * 1024);   // 100 MB
    sm->setCPUTimeLimit(10000);               // 10 seconds
    sm->setWallTimeLimit(30000);              // 30 seconds
    sm->setMaxThreads(4);                     // Up to 4 threads
    sm->setMaxRecursionDepth(200);            // Reasonable recursion

    return sm;
}

//------------------------------------------------------------------------------
// Runtime sandbox manager access
//------------------------------------------------------------------------------

QoreSandboxManager* runtime_get_sandbox_manager() {
    QoreProgram* pgm = getProgram();
    return pgm ? pgm->getSandboxManager() : nullptr;
}

bool qore_check_io_interrupt(ExceptionSink* xsink, const char* operation) {
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (sm) {
        return sm->checkIOInterrupt(xsink, operation);
    }
    return false;
}
