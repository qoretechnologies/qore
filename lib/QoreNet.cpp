/*
    QoreNet.cpp

    Network functions

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

#include "qore/Qore.h"
#include "qore/intern/QoreHashNodeIntern.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>

#define QORE_NET_ADDR_BUF_LEN 80

int q_get_af(int type) {
   if (type >= 0)
      return type;

   switch (type) {
      case Q_AF_UNSPEC:
         return AF_UNSPEC;
      case Q_AF_INET6:
         return AF_INET6;
   }

   return AF_INET;
}

int q_get_raf(int type) {
   if (type < 0)
      return type;

   switch (type) {
      case AF_UNSPEC:
         return Q_AF_UNSPEC;
      case AF_INET6:
         return Q_AF_INET6;
   }

   return Q_AF_INET;
}

int q_get_sock_type(int t) {
   if (t >= 0)
      return t;

   return SOCK_STREAM;
}

int q_addr_to_string(int family, const char* addr, QoreString& str) {
   family = q_get_af(family);

   char buf[QORE_NET_ADDR_BUF_LEN];
   if (!inet_ntop(family, addr, buf, QORE_NET_ADDR_BUF_LEN))
      return -1;
   str.concat(buf);
   return 0;
}

QoreStringNode* q_addr_to_string(int family, const char* addr) {
   family = q_get_af(family);

   char buf[QORE_NET_ADDR_BUF_LEN];
   return inet_ntop(family, addr, buf, QORE_NET_ADDR_BUF_LEN) ? new QoreStringNode(buf) : 0;
}

int q_addr_to_string2(const struct sockaddr* ai_addr, QoreString& str) {
   size_t slen = str.strlen();

   const void* addr;
   if (ai_addr->sa_family == AF_INET) {
      struct sockaddr_in* ipv4 = (struct sockaddr_in*)ai_addr;
      addr = &(ipv4->sin_addr);
      str.reserve(slen + INET_ADDRSTRLEN + 1);
   } else if (ai_addr->sa_family == AF_INET6) {
      struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ai_addr;
      addr = &(ipv6->sin6_addr);
      str.reserve(slen + INET6_ADDRSTRLEN + 1);
   }
#ifdef HAVE_SYS_UN_H
   // windows does not support UNIX sockets, for example
   else if (ai_addr->sa_family == AF_UNIX) {
      struct sockaddr_un* un = (struct sockaddr_un*)ai_addr;
      str.concat(un->sun_path);
      return 0;
   }
#endif
   else
      return -1;

   if (!inet_ntop(ai_addr->sa_family, addr, (char*)(str.getBuffer() + slen), str.capacity() - slen))
      return -1;

   str.terminate(slen + strlen(str.getBuffer() + slen));
   return 0;
}


QoreStringNode* q_addr_to_string2(const struct sockaddr* ai_addr) {
   SimpleRefHolder<QoreStringNode> str(new QoreStringNode);

   return q_addr_to_string2(ai_addr, **str) ? 0 : str.release();
}

int q_get_port_from_addr(const struct sockaddr* ai_addr) {
   if (ai_addr->sa_family == AF_INET) {
      const struct sockaddr_in* ipv4 = (struct sockaddr_in*)ai_addr;
      return ntohs(ipv4->sin_port);
   } else if (ai_addr->sa_family == AF_INET6) {
      const struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ai_addr;
      return ntohs(ipv6->sin6_port);
   }

   return -1;
}

//! Get host's IP address (struct in_addr*) from name.
int q_gethostbyname(const char* host, struct in_addr* sin_addr) {
   QORE_TRACE("q_gethostbyname()");

   ExceptionSink xsink;
   QoreAddrInfo qai;
   qai.getInfo(&xsink, host, NULL);
   if (xsink) {
      xsink.clear();
      return -1;
   }

   struct addrinfo* addrInfo = qai.getAddrInfo();
   while (addrInfo) {
      if (addrInfo->ai_addr->sa_family == AF_INET) {
         memcpy(sin_addr, &(reinterpret_cast<struct sockaddr_in*>(addrInfo->ai_addr)->sin_addr), sizeof(struct in_addr));
         return 0;
      }
      addrInfo = addrInfo->ai_next;
   }
   return -1;
}

static const char* q_af_to_str(int af) {
   switch (af) {
      case AF_INET:
         return "ipv4";
      case AF_INET6:
         return "ipv6";
      case AF_UNIX:
         return "unix";
#ifdef AF_PACKET
      case AF_PACKET:
         return "mac";
#endif
#ifdef AF_LINK
      case AF_LINK:
         return "mac";
#endif
   }
   return "unknown";
}

void q_af_to_hash(int af, QoreHashNode& h, ExceptionSink* xsink) {
    h.setKeyValue("type", af, xsink);
    h.setKeyValue("typename", new QoreStringNode(q_af_to_str(af)), xsink);
}

static QoreHashNode* he_to_hash(struct hostent& he) {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    qore_hash_private* hh = qore_hash_private::get(*h);

    if (he.h_name && he.h_name[0])
        hh->setKeyValueIntern("name", new QoreStringNode(he.h_name)); // official host name
    if (he.h_aliases) {
        QoreListNode* l = new QoreListNode(stringTypeInfo);
        char** a = he.h_aliases;
        while (*a)
            l->push(new QoreStringNode(*(a++)), nullptr);
        hh->setKeyValueIntern("aliases", l);
    }
    switch (he.h_addrtype) {
        case AF_INET:
            hh->setKeyValueIntern("typename", new QoreStringNode("ipv4"));
            hh->setKeyValueIntern("type", AF_INET);
            break;
        case AF_INET6:
            hh->setKeyValueIntern("typename", new QoreStringNode("ipv6"));
            hh->setKeyValueIntern("type", AF_INET6);
            break;
        default:
            hh->setKeyValueIntern("typename", new QoreStringNode("unknown"));
            break;
    }
    hh->setKeyValueIntern("len", he.h_length);

    if (he.h_addr_list) {
        char buf[QORE_NET_ADDR_BUF_LEN];

        QoreListNode* l = new QoreListNode(stringTypeInfo);
        char** a = he.h_addr_list;
        while (*a) {
            if (inet_ntop(he.h_addrtype, *(a++), buf, QORE_NET_ADDR_BUF_LEN))
                l->push(new QoreStringNode(buf), nullptr);
        }
        hh->setKeyValueIntern("addresses", l);
    }

    return h;
}

//! Get host's IP addresses and info from name.
QoreHashNode* q_gethostbyname_to_hash(const char* host) {
    ExceptionSink xsink;
    QoreAddrInfo qai;
    qai.getInfo(&xsink, host, NULL, Q_AF_UNSPEC, AI_CANONNAME);
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), &xsink);
    qore_hash_private* rh = qore_hash_private::get(**result);
    ReferenceHolder<QoreListNode> addresses(new QoreListNode(stringTypeInfo), &xsink);
    if (xsink || !result || !addresses) {
        xsink.clear();
        return nullptr;
    }

    struct addrinfo* addrInfo = qai.getAddrInfo();
    // NOTE: Darwin returns an empty name when the host argument is an address
    rh->setKeyValueIntern("name", new QoreStringNode(addrInfo->ai_canonname ? addrInfo->ai_canonname : host));
    rh->setKeyValueIntern("aliases", new QoreListNode(stringTypeInfo));
    int ai_family = addrInfo->ai_family;
    switch (ai_family) {
        case AF_INET:
            rh->setKeyValueIntern("typename", new QoreStringNode("ipv4"));
            rh->setKeyValueIntern("type", AF_INET);
            rh->setKeyValueIntern("len", 4);
            break;
        case AF_INET6:
            rh->setKeyValueIntern("typename", new QoreStringNode("ipv6"));
            rh->setKeyValueIntern("type", AF_INET6);
            rh->setKeyValueIntern("len", 16);
            break;
        default:
            rh->setKeyValueIntern("typename", new QoreStringNode("unknown"));
            break;
    }

    while (addrInfo && !xsink) {
        if (ai_family == addrInfo->ai_family) {
            SimpleRefHolder<QoreStringNode> addr(new QoreStringNode);
            if (q_addr_to_string2(addrInfo->ai_addr, **addr))
                return nullptr;
            addresses->push(addr.release(), nullptr);
        }
        addrInfo = addrInfo->ai_next;
    }
    rh->setKeyValueIntern("addresses", addresses.release());
    if (xsink) {
        xsink.clear();
        return nullptr;
    }

    return result.release();
}

//! Get host's IP address from name.
QoreStringNode* q_gethostbyname_to_string(const char* host) {
    ExceptionSink xsink;
    QoreAddrInfo qai;
    qai.getInfo(&xsink, host, NULL);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }

    SimpleRefHolder<QoreStringNode> addr(new QoreStringNode);
    if (q_addr_to_string2(qai.getAddrInfo()->ai_addr, **addr))
        return nullptr;

    return addr.release();
}

//! Get host's name from IP address.
char* q_gethostbyaddr(const char* addr, int len, int type) {
    sockaddr_in in;
    sockaddr_in6 in6;
    char buf[512];
    int rc;

    type = q_get_af(type);
    if (type == AF_INET) {
        memset(&in, 0, sizeof(sockaddr_in));
        in.sin_family = AF_INET;
        in.sin_addr = *((struct in_addr*)addr);
        rc = getnameinfo(reinterpret_cast<sockaddr*>(&in), sizeof(sockaddr_in), buf, sizeof buf, nullptr, 0, NI_NAMEREQD);
    } else if (type == AF_INET6) {
        memset(&in6, 0, sizeof(sockaddr_in6));
        in6.sin6_family = AF_INET6;
        in6.sin6_addr = *((struct in6_addr*)addr);
        rc = getnameinfo(reinterpret_cast<sockaddr*>(&in6), sizeof(sockaddr_in6), buf, sizeof buf, nullptr, 0, NI_NAMEREQD);
    } else {
        return nullptr;
    }

    return rc ? nullptr : strdup(buf);
}

// thread-safe gethostbyaddr
QoreHashNode* q_gethostbyaddr_to_hash(ExceptionSink* xsink, const char* addr, int type) {
    in_addr sin_addr;
    in6_addr sin6_addr;
    void* dst;
    int len;

    type = q_get_af(type);

    if (type == AF_INET) {
        dst = (void*)&sin_addr;
        len = sizeof(sin_addr);
    } else if (type == AF_INET6) {
        dst = (void*)&sin6_addr;
        len = sizeof(sin6_addr);
    } else {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "%d is an invalid address type (valid types are AF_INET=%d, AF_INET6=%d", type, AF_INET, AF_INET6);
        return 0;
    }

    int rc = inet_pton(type, addr, dst);
    if (rc == 0) {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "'%s' is not a valid address for %s addresses", addr, type == AF_INET ? "AF_INET (IPv4)" : "AF_INET6 (IPv6)");
        return 0;
    }
    if (rc < 0)
        return 0;

#ifdef HAVE_GETHOSTBYADDR_R
    struct hostent he;
    char buf[NET_BUFSIZE];
    int err;
# ifdef HAVE_SOLARIS_STYLE_GETHOST
    if (!gethostbyaddr_r((char*)dst, len, type, &he, buf, NET_BUFSIZE, &err)) {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "gethostbyaddr('%s') failed: %s", addr, hstrerror(err));
        return 0;
    }
# else // assume glibc2-style gethostbyaddr_r
    struct hostent* p;

    rc = gethostbyaddr_r(dst, len, type, &he, buf, NET_BUFSIZE, &p, &err);
    if (rc || !p) {
        if (rc) {
            xsink->raiseException("GETHOSTBYADDR-ERROR", "gethostbyaddr('%s') failed: %s", addr, strerror(rc));
        } else {
            xsink->raiseException("GETHOSTBYADDR-ERROR", "gethostbyaddr('%s') failed: %s", addr, hstrerror(err));
        }
        return 0;
    }
# endif // HAVE_SOLARIS_STYLE_GETHOST

    return he_to_hash(he);

#else  // else if !HAVE_GETHOSTBYADDR_R
    AutoLocker al(&lck_gethostbyaddr);
    struct hostent* he;
    if (!(he = gethostbyaddr((char*)dst, len, type))) {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "gethostbyaddr('%s') failed: %s", addr, hstrerror(h_errno));
        return 0;
    }

    return he_to_hash(*he);
#endif // HAVE_GETHOSTBYADDR_R
}

//! Get host's name from IP address.
QoreStringNode* q_gethostbyaddr_to_string(ExceptionSink* xsink, const char* addr, int type) {
    sockaddr_in in;
    sockaddr_in6 in6;
    char buf[512];
    void* dst;

    type = q_get_af(type);
    if (type == AF_INET) {
        memset(&in, 0, sizeof(sockaddr_in));
        in.sin_family = AF_INET;
        dst = (void*)&in.sin_addr;
    } else if (type == AF_INET6) {
        memset(&in6, 0, sizeof(sockaddr_in6));
        in6.sin6_family = AF_INET6;
        dst = (void*)&in6.sin6_addr;
    } else {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "%d is an invalid address type (valid types are AF_INET=%d, AF_INET6=%d", type, AF_INET, AF_INET6);
        return nullptr;
    }

    int rc = inet_pton(type, addr, dst);
    if (rc == 0) {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "'%s' is not a valid address for %s addresses", addr, type == AF_INET ? "AF_INET (IPv4)" : "AF_INET6 (IPv6)");
        return nullptr;
    }
    if (rc < 0)
        return nullptr;

    if (type == AF_INET) {
        rc = getnameinfo(reinterpret_cast<sockaddr*>(&in), sizeof(sockaddr_in), buf, sizeof buf, nullptr, 0, NI_NAMEREQD);
    } else {
        rc = getnameinfo(reinterpret_cast<sockaddr*>(&in6), sizeof(sockaddr_in6), buf, sizeof buf, nullptr, 0, NI_NAMEREQD);
    }

    if (rc)
        return nullptr;

    return new QoreStringNode(buf);
}

QoreListNode* q_getaddrinfo_to_list(ExceptionSink* xsink, const char* node, const char* service, int family, int flags, int socktype) {
    QoreAddrInfo ai;
    if (ai.getInfo(xsink, node, service, family, flags, socktype))
        return 0;

    return ai.getList();
}

QoreAddrInfo::QoreAddrInfo() : ai(0), has_svc(false) {
}

QoreAddrInfo::~QoreAddrInfo() {
    clear();
}

void QoreAddrInfo::clear() {
    if (ai) {
        freeaddrinfo(ai);
        ai = 0;
        has_svc = false;
    }
}

int QoreAddrInfo::getInfo(ExceptionSink* xsink, const char* node, const char* service, int family, int flags, int socktype, int protocol) {
    family = q_get_af(family);
    socktype = q_get_sock_type(socktype);

    if (ai)
        clear();

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); // make sure the struct is empty

    hints.ai_family = family;
    hints.ai_flags = flags;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;

    // retry on transient system-level errors (EINTR: interrupted by signal, EAGAIN: resolver temporarily unavailable)
    int status;
    int retries = 0;
    static const int GETADDRINFO_MAX_RETRIES = 10;
    do {
        status = getaddrinfo(node, service, &hints, &ai);
    } while (status == EAI_SYSTEM && (errno == EINTR || errno == EAGAIN) && ++retries < GETADDRINFO_MAX_RETRIES);

    if (status) {
        if (xsink) {
            if (status == EAI_SYSTEM)
                xsink->raiseException("QOREADDRINFO-GETINFO-ERROR", "getaddrinfo(node: '%s', service: '%s', address_family: %d='%s', flags: %d) error: %s (errno: %d: %s)", node ? node : "", service ? service : "", family, q_af_to_str(family), flags, gai_strerror(status), errno, strerror(errno));
            else
                xsink->raiseException("QOREADDRINFO-GETINFO-ERROR", "getaddrinfo(node: '%s', service: '%s', address_family: %d='%s', flags: %d) error: %s", node ? node : "", service ? service : "", family, q_af_to_str(family), flags, gai_strerror(status));
        }
        return -1;
    }

    if (service)
        has_svc = true;
    return 0;
}

QoreListNode* QoreAddrInfo::getList() const {
    if (!ai)
        return 0;

    QoreListNode* l = new QoreListNode(autoHashTypeInfo);

    for (struct addrinfo* p = ai; p; p = p->ai_next) {
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        qore_hash_private* hh = qore_hash_private::get(*h);

        const char* family = q_af_to_str(p->ai_family);

        if (p->ai_canonname && *p->ai_canonname)
            hh->setKeyValueIntern("canonname", new QoreStringNode(p->ai_canonname));

        QoreStringNode* addr = q_addr_to_string2(p->ai_addr);
        if (addr) {
            hh->setKeyValueIntern("address", addr);
            hh->setKeyValueIntern("address_desc", getAddressDesc(p->ai_family, addr->getBuffer()));
        }

        hh->setKeyValueIntern("family", p->ai_family);
        hh->setKeyValueIntern("familystr", new QoreStringNode(family));
        hh->setKeyValueIntern("addrlen", p->ai_addrlen);
        if (has_svc) {
            int port = q_get_port_from_addr(p->ai_addr);
            if (port != -1) {
                hh->setKeyValueIntern("port", port);
            }
        }

        l->push(h, nullptr);
    }

    return l;
}

const char* QoreAddrInfo::getFamilyName(int family) {
    return q_af_to_str(q_get_af(family));
}

QoreStringNode* QoreAddrInfo::getAddressDesc(int family, const char* addr) {
    family = q_get_af(family);

    QoreStringNode* str = new QoreStringNode;
    switch (family) {
        case AF_INET:
            str->sprintf("ipv4(%s)", addr);
            break;
        case AF_INET6:
            str->sprintf("ipv6[%s]", addr);
            break;
        // process mac addresses if possible
#ifdef AF_PACKET
        case AF_PACKET:
#endif
#ifdef AF_LINK
        case AF_LINK:
#endif
#if defined(AF_PACKET) || defined(AF_LINK)
            str->sprintf("mac<%s>", addr);
            break;
#endif
        default:
            str->sprintf("%s:%s", getFamilyName(family), addr);
            break;
    }
    return str;
}

void* qore_get_in_addr(struct sockaddr *sa) {
    switch (sa->sa_family) {
        case AF_INET:
            return &(((struct sockaddr_in*)sa)->sin_addr);
        case AF_INET6:
            return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
    assert(false);
    return nullptr;
}

size_t qore_get_in_len(struct sockaddr *sa) {
    switch (sa->sa_family) {
        case AF_INET:
            return sizeof(struct sockaddr_in);
        case AF_INET6:
            return sizeof(struct sockaddr_in6);
    }
    assert(false);
    return 0;
}
