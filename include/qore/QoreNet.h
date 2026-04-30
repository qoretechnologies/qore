/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreNet.h

  Network functions and macros

  Qore Programming Language

  Copyright (C) 2003 - 2023 David Nichols

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

#ifndef _QORE_QORENET_H

#define _QORE_QORENET_H

#include <sys/types.h>

//! Get host's IP address (struct in_addr*) from name.
/**
    @param host host's name
    @param sin_addr host's IP address will be saved here
    @return 0 in case of success, non-zero in case of error

    @note delegates internally to the async I/O controller-backed address resolver instead of
    \c gethostbyname(3)
 */
DLLEXPORT int q_gethostbyname(const char* host, struct in_addr* sin_addr);

//! Get host's IP addresses and info from name.
/**
    @param host host's name
    @return hash containing info about host; nullptr in case of error

    @note delegates internally to the async I/O controller-backed address resolver instead of
    \c gethostbyname(3)
 */
DLLEXPORT QoreHashNode* q_gethostbyname_to_hash(const char* host);

//! Get host's IP address from name.
/**
    @param host host's name
    @return host's IP address; nullptr in case of error

    @note delegates internally to the async I/O controller-backed address resolver instead of
    \c gethostbyname(3)
 */
DLLEXPORT QoreStringNode* q_gethostbyname_to_string(const char* host);

//! Get host's name from IP address.
/**
    @param addr in_addr structure (in the form of char*)
    @param len length of addr
    @param type IP address type
    @return host's name (string returned must be freed); nullptr in case of error

    @note delegates internally to the async I/O controller-backed reverse address resolver instead of
    \c gethostbyaddr(3)
 */
DLLEXPORT char* q_gethostbyaddr(const char* addr, int len, int type);

//! thread-safe gethostbyaddr (0/NULL = error)
/**
    @note delegates internally to the async I/O controller-backed reverse address resolver instead of
    \c gethostbyaddr(3)
 */
DLLEXPORT QoreHashNode* q_gethostbyaddr_to_hash(ExceptionSink* xsink, const char* addr, int type = Q_AF_INET);

//! Get host's name from IP address.
/**
    @param xsink exception sink
    @param addr host's name
    @param type IP address type
    @return host's name; nullptr in case of error

    @note delegates internally to the async I/O controller-backed reverse address resolver instead of
    \c gethostbyaddr(3)
 */
DLLEXPORT QoreStringNode* q_gethostbyaddr_to_string(ExceptionSink* xsink, const char* addr, int type = Q_AF_INET);

//! converts a network address in network byte order to a string (address_family = AF_INET or AF_INET6), returns 0 on error
/** @see q_addr_to_string2()
 */
DLLEXPORT QoreStringNode* q_addr_to_string(int address_family, const char* addr);

//! converts a network address in network byte order to a string (address_family = AF_INET or AF_INET6), returns -1 on error
/** @param address_family the address family
    @param addr the address
    @param str the output string

    @return 0 for OK, -1 for error

    @see q_addr_to_string2()
 */
DLLEXPORT int q_addr_to_string(int address_family, const char* addr, QoreString& str);

//! converts a network address in network byte order to a string (address_family = AF_INET or AF_INET6), returns 0 on error
DLLEXPORT QoreStringNode* q_addr_to_string2(const struct sockaddr* ai_addr);

//! converts a network address in network byte order to a string (address_family = AF_INET or AF_INET6), returns -1 on error
/** @param ai_addr the address
    @param str the output string

    @return 0 for OK, -1 for error

    @see q_addr_to_string2()
 */
DLLEXPORT int q_addr_to_string2(const struct sockaddr* ai_addr, QoreString& str);

//! get port from struct sockaddr, returns -1 if port cannot be determined
DLLEXPORT int q_get_port_from_addr(const struct sockaddr* ai_addr);

//! returns address info as a hash
/**
    @note delegates internally to the async I/O controller-backed address resolver
 */
DLLEXPORT QoreListNode* q_getaddrinfo_to_list(ExceptionSink* xsink, const char* node, const char* service, int family = Q_AF_UNSPEC, int flags = 0, int socktype = Q_SOCK_STREAM);

//! adds the address family as "type" and a descriptive name as "typename" to the hash; writes "unknown" if the address family is unknown
DLLEXPORT void q_af_to_hash(int af, QoreHashNode& h, ExceptionSink* xsink);

//! provides an interface to getaddrinfo
class QoreAddrInfo {
protected:
   struct addrinfo* ai;
   bool has_svc;

public:
   //! create an empty structure
   DLLEXPORT QoreAddrInfo();

   //! destroy the object
   DLLEXPORT ~QoreAddrInfo();

   //! clears the current results, if any
   DLLEXPORT void clear();

   //! get address info with the given parameters, if any errors occur, a Qore-language exception is thrown
   /** @param xsink if any errors occur, Qore-language exception info is added to this object
       @param node the node name for the lookup
       @param service the service name (from /etc/services, for example) or port number
       @param family a hint for the address family, AF_UNSPEC means any family
       @param flags hint flags as per the getaddrinfo() call: AI_ADDRCONFIG, AI_ALL, AI_CANONNAME, AI_NUMERICHOST, AI_NUMERICSERV, AI_PASSIVE, AI_V4MAPPED
       @param socktype a hint for the type of socket; 0 = any socket type
       @param protocol a hint for the protocol number; 0 = the default protocol
    */
   DLLEXPORT int getInfo(ExceptionSink* xsink, const char* node, const char* service, int family = Q_AF_UNSPEC, int flags = 0, int socktype = Q_SOCK_STREAM, int protocol = 0);

   //! returns the struct addrinfo * being managed (may by 0)
   DLLLOCAL struct addrinfo* getAddrInfo() const {
      return ai;
   }
   
   //! returns a list of hashes of address info, if an addrinfo structure is being managed
   DLLEXPORT QoreListNode* getList() const;

   //! returns the name of the address family as a string (ie AF_INET = "ipv4", etc)
   DLLEXPORT static const char* getFamilyName(int address_family);

   //! returns a descriptive string for the address family and an address string (ie AF_INET6, "::1" = "ipv6[::1]")
   DLLEXPORT static QoreStringNode* getAddressDesc(int address_family, const char* addr);
};

#endif // _QORE_QORENET_H
