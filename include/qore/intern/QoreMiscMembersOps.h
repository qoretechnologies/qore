/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMiscMembersOps.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREMISCMEMBERSOPS_H
#define _QORE_INTERN_QOREMISCMEMBERSOPS_H

#include <qore/AbstractQoreNode.h>
#include <qore/AutoVLock.h>
#include <qore/BinaryNode.h>
#include <qore/CallReferenceNode.h>
#include <qore/DBI.h>
#include <qore/Datasource.h>
#include <qore/DateTime.h>
#include <qore/HttpClientConnection.h>
#include <qore/ParseOptionMap.h>
#include <qore/QoreClass.h>
#include <qore/QoreEncoding.h>
#include <qore/QoreHttpClientObject.h>
#include <qore/QoreSSLCertificate.h>
#include <qore/QoreURL.h>
#include <qore/SocketPollOperation.h>
#include <qore/UnicodeCharacterIterator.h>
#include <qore/qore_thread.h>


#include <utility>

/** The non-virtual DLLLOCAL member functions of several classes were declared in the public headers although no module can call them (they are
    not exported); they are members of the classes below, in the implementation of the library.  Each class is a friend
    of the class it serves; call an operation through a temporary object:

    @code
    qore_url_ops(url).reset();
    @endcode

    A function that stays out of line is a static fooImpl(Cls* b, ...) with an inline wrapper, so that the temporary is
    optimized away and the call costs what the member function cost (a temporary whose address a call takes makes GCC
    add a stack-protector canary to the caller).
*/

//! The library-internal operations of AbstractQoreNode (qore#4380)
class qore_abstract_node_ops {
public:
    //! the object
    AbstractQoreNode* b;

    DLLLOCAL explicit qore_abstract_node_ops(AbstractQoreNode* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_abstract_node_ops(H& holder) : qore_abstract_node_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_abstract_node_ops(const AbstractQoreNode* n_b) : b(const_cast<AbstractQoreNode*>(n_b)) {
    }

    DLLLOCAL explicit qore_abstract_node_ops(AbstractQoreNode& r) : b(&r) {
    }

    DLLLOCAL explicit qore_abstract_node_ops(const AbstractQoreNode& r) : b(const_cast<AbstractQoreNode*>(&r)) {
    }

    //! optionally evaluates the argument with RuntimeConfig
    /** return value requires a dereference if needs_deref is true
        @param rc the runtime configuration
        @param needs_deref this is an output parameter, if needs_deref is true then the value returned must be dereferenced
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @see ValueEvalRefHolder
    */
    DLLLOCAL static QoreValue evalImpl(AbstractQoreNode* b, RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink);
    DLLLOCAL QoreValue eval(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) {
        return evalImpl(b, rc, needs_deref, xsink);
    }

};

//! The library-internal operations of AutoVLock (qore#4380)
class qore_auto_vlock_ops {
public:
    //! the object
    AutoVLock* b;

    DLLLOCAL explicit qore_auto_vlock_ops(AutoVLock* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_auto_vlock_ops(H& holder) : qore_auto_vlock_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_auto_vlock_ops(const AutoVLock* n_b) : b(const_cast<AutoVLock*>(n_b)) {
    }

    DLLLOCAL explicit qore_auto_vlock_ops(AutoVLock& r) : b(&r) {
    }

    DLLLOCAL explicit qore_auto_vlock_ops(const AutoVLock& r) : b(const_cast<AutoVLock*>(&r)) {
    }

    //! gets the current object
    DLLLOCAL static QoreObject* getObjectImpl(AutoVLock* b);
    DLLLOCAL QoreObject* getObject() {
        return getObjectImpl(b);
    }

    //! gets the current read-write lock
    DLLLOCAL static QoreVarRWLock* getRWLImpl(AutoVLock* b);
    DLLLOCAL QoreVarRWLock* getRWL() {
        return getRWLImpl(b);
    }

    //! adds an object member notification entry, internal-only
    /** @param o the object to add
         @param member the member that was changed (must be in QCS_DEFAULT encoding)
    */
    DLLLOCAL static void addMemberNotificationImpl(AutoVLock* b, QoreObject* o, const char* member);
    DLLLOCAL void addMemberNotification(QoreObject* o, const char* member) {
        addMemberNotificationImpl(b, o, member);
    }

    //! leaves the lock locked and the object referenced and clears the object and lock pointers
    DLLLOCAL static void clearImpl(AutoVLock* b);
    DLLLOCAL void clear() {
        clearImpl(b);
    }

    //! sets the current object (for dereference) and lock
    DLLLOCAL static void setImpl(AutoVLock* b, QoreObject* n_o, QoreVarRWLock* n_rwl);
    DLLLOCAL void set(QoreObject* n_o, QoreVarRWLock* n_rwl) {
        setImpl(b, n_o, n_rwl);
    }

    //! sets the current lock
    DLLLOCAL static void setImpl(AutoVLock* b, QoreVarRWLock* n_rwl);
    DLLLOCAL void set(QoreVarRWLock* n_rwl) {
        setImpl(b, n_rwl);
    }

};

//! The library-internal operations of BinaryNode (qore#4380)
class qore_binary_node_ops {
public:
    //! the object
    BinaryNode* b;

    DLLLOCAL explicit qore_binary_node_ops(BinaryNode* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_binary_node_ops(H& holder) : qore_binary_node_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_binary_node_ops(const BinaryNode* n_b) : b(const_cast<BinaryNode*>(n_b)) {
    }

    DLLLOCAL explicit qore_binary_node_ops(BinaryNode& r) : b(&r) {
    }

    DLLLOCAL explicit qore_binary_node_ops(const BinaryNode& r) : b(const_cast<BinaryNode*>(&r)) {
    }

    DLLLOCAL static void checkOffsetImpl(BinaryNode* b, qore_offset_t& offset);
    DLLLOCAL void checkOffset(qore_offset_t& offset) {
        checkOffsetImpl(b, offset);
    }

    DLLLOCAL static void checkOffsetImpl(BinaryNode* b, qore_offset_t& offset, qore_offset_t& num);
    DLLLOCAL void checkOffset(qore_offset_t& offset, qore_offset_t& num) {
        checkOffsetImpl(b, offset, num);
    }

};

//! The library-internal operations of ResolvedCallReferenceNode (qore#4380)
class qore_resolved_call_reference_node_ops {
public:
    //! the object
    ResolvedCallReferenceNode* b;

    DLLLOCAL explicit qore_resolved_call_reference_node_ops(ResolvedCallReferenceNode* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_resolved_call_reference_node_ops(H& holder) : qore_resolved_call_reference_node_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_resolved_call_reference_node_ops(const ResolvedCallReferenceNode* n_b) : b(const_cast<ResolvedCallReferenceNode*>(n_b)) {
    }

    DLLLOCAL explicit qore_resolved_call_reference_node_ops(ResolvedCallReferenceNode& r) : b(&r) {
    }

    DLLLOCAL explicit qore_resolved_call_reference_node_ops(const ResolvedCallReferenceNode& r) : b(const_cast<ResolvedCallReferenceNode*>(&r)) {
    }

    //! Internal invocation by a parsed operation with an immutable compatibility policy.
    DLLLOCAL static QoreValue invokeLexicalImpl(ResolvedCallReferenceNode* b, const QoreListNode* args, ExceptionSink* xsink, const LexicalSemanticPolicy& policy);
    DLLLOCAL QoreValue invokeLexical(const QoreListNode* args, ExceptionSink* xsink, const LexicalSemanticPolicy& policy) {
        return invokeLexicalImpl(b, args, xsink, policy);
    }

};

//! The library-internal operations of DBIDriverList (qore#4380)
class qore_dbi_driver_list_ops {
public:
    //! the object
    DBIDriverList* b;

    DLLLOCAL explicit qore_dbi_driver_list_ops(DBIDriverList* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_dbi_driver_list_ops(H& holder) : qore_dbi_driver_list_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_dbi_driver_list_ops(const DBIDriverList* n_b) : b(const_cast<DBIDriverList*>(n_b)) {
    }

    DLLLOCAL explicit qore_dbi_driver_list_ops(DBIDriverList& r) : b(&r) {
    }

    DLLLOCAL explicit qore_dbi_driver_list_ops(const DBIDriverList& r) : b(const_cast<DBIDriverList*>(&r)) {
    }

    DLLLOCAL static DBIDriver* find_internImpl(DBIDriverList* b, const char* name);
    DLLLOCAL DBIDriver* find_intern(const char* name) {
        return find_internImpl(b, name);
    }

    DLLLOCAL static QoreListNode* getDriverListImpl(DBIDriverList* b);
    DLLLOCAL QoreListNode* getDriverList() {
        return getDriverListImpl(b);
    }

};

//! The library-internal operations of Datasource (qore#4380)
class qore_datasource_ops {
public:
    //! the object
    Datasource* b;

    DLLLOCAL explicit qore_datasource_ops(Datasource* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_datasource_ops(H& holder) : qore_datasource_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_datasource_ops(const Datasource* n_b) : b(const_cast<Datasource*>(n_b)) {
    }

    DLLLOCAL explicit qore_datasource_ops(Datasource& r) : b(&r) {
    }

    DLLLOCAL explicit qore_datasource_ops(const Datasource& r) : b(const_cast<Datasource*>(&r)) {
    }

    //! this helper method shares code for exec() and execRaw() methods
    DLLLOCAL static QoreValue exec_internalImpl(Datasource* b, bool doBind, const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue exec_internal(bool doBind, const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
        return exec_internalImpl(b, doBind, query_str, args, xsink);
    }

    //! returns the mutation observer context for this datasource or nullptr; internal only
    DLLLOCAL static SqlMutationContext* getMutationContextImpl(Datasource* b);
    DLLLOCAL SqlMutationContext* getMutationContext() {
        return getMutationContextImpl(b);
    }

    //! returns the mutation observer context, creating it if necessary; internal only
    DLLLOCAL static SqlMutationContext* getOrCreateMutationContextImpl(Datasource* b);
    DLLLOCAL SqlMutationContext* getOrCreateMutationContext() {
        return getOrCreateMutationContextImpl(b);
    }

    //! replaces the mutation observer context; takes ownership of the reference; internal only
    DLLLOCAL static void setMutationContextImpl(Datasource* b, SqlMutationContext* ctx, ExceptionSink* xsink);
    DLLLOCAL void setMutationContext(SqlMutationContext* ctx, ExceptionSink* xsink) {
        setMutationContextImpl(b, ctx, xsink);
    }

};

//! The library-internal operations of DateTime (qore#4380)
class qore_date_time_ops {
public:
    //! the object
    DateTime* b;

    DLLLOCAL explicit qore_date_time_ops(DateTime* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_date_time_ops(H& holder) : qore_date_time_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_date_time_ops(const DateTime* n_b) : b(const_cast<DateTime*>(n_b)) {
    }

    DLLLOCAL explicit qore_date_time_ops(DateTime& r) : b(&r) {
    }

    DLLLOCAL explicit qore_date_time_ops(const DateTime& r) : b(const_cast<DateTime*>(&r)) {
    }

    DLLLOCAL static void setDateLiteralImpl(DateTime* b, int64 date);
    DLLLOCAL void setDateLiteral(int64 date) {
        setDateLiteralImpl(b, date);
    }

    DLLLOCAL static void setRelativeDateLiteralImpl(DateTime* b, int64 date);
    DLLLOCAL void setRelativeDateLiteral(int64 date) {
        setRelativeDateLiteralImpl(b, date);
    }

};

//! The library-internal operations of HttpClientConnectionBase (qore#4380)
class qore_http_client_connection_base_ops {
public:
    //! the object
    HttpClientConnectionBase* b;

    DLLLOCAL explicit qore_http_client_connection_base_ops(HttpClientConnectionBase* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_http_client_connection_base_ops(H& holder) : qore_http_client_connection_base_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_http_client_connection_base_ops(const HttpClientConnectionBase* n_b) : b(const_cast<HttpClientConnectionBase*>(n_b)) {
    }

    DLLLOCAL explicit qore_http_client_connection_base_ops(HttpClientConnectionBase& r) : b(&r) {
    }

    DLLLOCAL explicit qore_http_client_connection_base_ops(const HttpClientConnectionBase& r) : b(const_cast<HttpClientConnectionBase*>(&r)) {
    }

    //! Checks that the network sandbox of the calling code allows it to use the connection
    /** A connection is checked against the network sandbox of the code that opens it; a connection pool hands it to
        later callers, whose code can run under another sandbox, so the pool calls this before it hands the
        connection to a caller.  The connection's destinations are checked as a new connection to them would be,
        with the sandbox as it is configured now.

        @param sm the network sandbox of the calling code (see SandboxManager::callWithSystemPolicy() for trusted
        code), or nullptr if it has none
        @param xsink raises \c NETWORK-ACCESS-DENIED if the sandbox denies a destination of the connection

        @return 0 if the calling code may use the connection: it has no sandbox, its sandbox allows every
        destination of the connection, or the connection is still being made for code under the same sandbox, which
        the connect checks itself; -1 if the sandbox denies a destination of the connection (an exception is raised);
        1 if the sandbox cannot decide on the connection (no exception is raised): it is still being made for code
        under another sandbox, or it leads to a destination that was not resolved, such as the destination of a
        proxy connection made without a sandbox

        @since %Qore 3.0
    */
    DLLLOCAL static int checkSandboxUseImpl(HttpClientConnectionBase* b, QoreSandboxManager* sm, ExceptionSink* xsink);
    DLLLOCAL int checkSandboxUse(QoreSandboxManager* sm, ExceptionSink* xsink) {
        return checkSandboxUseImpl(b, sm, xsink);
    }

    //! Makes this connection one opened for the code that @a from was opened for
    /** For a connection that takes over the socket of another one, such as the connection a negotiated protocol
        hands over to, which can be created in another thread than the connection it takes over

        @since %Qore 3.0
    */
    DLLLOCAL static void inheritOpenerImpl(HttpClientConnectionBase* b, const HttpClientConnectionBase& from);
    DLLLOCAL void inheritOpener(const HttpClientConnectionBase& from) {
        inheritOpenerImpl(b, from);
    }

};

//! The library-internal operations of ParseOptionMap (qore#4380)
class qore_parse_option_map_ops {
public:
    //! the object
    ParseOptionMap* b;

    DLLLOCAL explicit qore_parse_option_map_ops(ParseOptionMap* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_parse_option_map_ops(H& holder) : qore_parse_option_map_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_parse_option_map_ops(const ParseOptionMap* n_b) : b(const_cast<ParseOptionMap*>(n_b)) {
    }

    DLLLOCAL explicit qore_parse_option_map_ops(ParseOptionMap& r) : b(&r) {
    }

    DLLLOCAL explicit qore_parse_option_map_ops(const ParseOptionMap& r) : b(const_cast<ParseOptionMap*>(&r)) {
    }

    DLLLOCAL static QoreHashNode* getCodeToStringMap();

    DLLLOCAL static QoreHashNode* getStringToCodeMap();

    DLLLOCAL static void static_init();

};

//! The library-internal operations of QoreMethod (qore#4380)
class qore_method_ops {
public:
    //! the object
    QoreMethod* b;

    DLLLOCAL explicit qore_method_ops(QoreMethod* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_method_ops(H& holder) : qore_method_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_method_ops(const QoreMethod* n_b) : b(const_cast<QoreMethod*>(n_b)) {
    }

    DLLLOCAL explicit qore_method_ops(QoreMethod& r) : b(&r) {
    }

    DLLLOCAL explicit qore_method_ops(const QoreMethod& r) : b(const_cast<QoreMethod*>(&r)) {
    }

    DLLLOCAL static QoreMethod* copyImpl(QoreMethod* b, const QoreClass* p_class);
    DLLLOCAL QoreMethod* copy(const QoreClass* p_class) {
        return copyImpl(b, p_class);
    }

    DLLLOCAL static bool inMethodImpl(QoreMethod* b, const QoreObject* self);
    DLLLOCAL bool inMethod(const QoreObject* self) {
        return inMethodImpl(b, self);
    }

    DLLLOCAL static void assign_classImpl(QoreMethod* b, const QoreClass* p_class);
    DLLLOCAL void assign_class(const QoreClass* p_class) {
        assign_classImpl(b, p_class);
    }

};

//! The library-internal operations of QoreEncodingManager (qore#4380)
class qore_encoding_manager_ops {
public:
    //! the object
    QoreEncodingManager* b;

    DLLLOCAL explicit qore_encoding_manager_ops(QoreEncodingManager* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_encoding_manager_ops(H& holder) : qore_encoding_manager_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_encoding_manager_ops(const QoreEncodingManager* n_b) : b(const_cast<QoreEncodingManager*>(n_b)) {
    }

    DLLLOCAL explicit qore_encoding_manager_ops(QoreEncodingManager& r) : b(&r) {
    }

    DLLLOCAL explicit qore_encoding_manager_ops(const QoreEncodingManager& r) : b(const_cast<QoreEncodingManager*>(&r)) {
    }

    //! finds an encoding if it exists (also looks up against alias names); returns nullptr if it does not
    DLLLOCAL static const QoreEncoding* find(const char* name);

    DLLLOCAL static const QoreEncoding* findUnlocked(const char* name);

    DLLLOCAL static void init(const char* def);

};

//! The library-internal operations of QoreHttpClientObject (qore#4380)
class qore_http_client_object_ops {
public:
    //! the object
    QoreHttpClientObject* b;

    DLLLOCAL explicit qore_http_client_object_ops(QoreHttpClientObject* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_http_client_object_ops(H& holder) : qore_http_client_object_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_http_client_object_ops(const QoreHttpClientObject* n_b) : b(const_cast<QoreHttpClientObject*>(n_b)) {
    }

    DLLLOCAL explicit qore_http_client_object_ops(QoreHttpClientObject& r) : b(&r) {
    }

    DLLLOCAL explicit qore_http_client_object_ops(const QoreHttpClientObject& r) : b(const_cast<QoreHttpClientObject*>(&r)) {
    }

    //! Returns the standard HTTP reason phrase for a status code.
    /** @since %Qore 3.0
    */
    DLLLOCAL static const char* getHttpStatusMessage(int code);

    DLLLOCAL static void static_init();

    DLLLOCAL static void cleanupImpl(QoreHttpClientObject* b, ExceptionSink* xsink);
    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        cleanupImpl(b, xsink);
    }

};

//! The library-internal operations of QoreSSLCertificate (qore#4380)
class qore_ssl_certificate_ops {
public:
    //! the object
    QoreSSLCertificate* b;

    DLLLOCAL explicit qore_ssl_certificate_ops(QoreSSLCertificate* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_ssl_certificate_ops(H& holder) : qore_ssl_certificate_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_ssl_certificate_ops(const QoreSSLCertificate* n_b) : b(const_cast<QoreSSLCertificate*>(n_b)) {
    }

    DLLLOCAL explicit qore_ssl_certificate_ops(QoreSSLCertificate& r) : b(&r) {
    }

    DLLLOCAL explicit qore_ssl_certificate_ops(const QoreSSLCertificate& r) : b(const_cast<QoreSSLCertificate*>(&r)) {
    }

    DLLLOCAL static QoreValue doPurposeValueImpl(QoreSSLCertificate* b, int id, int ca);
    DLLLOCAL QoreValue doPurposeValue(int id, int ca) {
        return doPurposeValueImpl(b, id, ca);
    }

};

//! The library-internal operations of SocketPollSocketOperationBase (qore#4380)
class qore_socket_poll_socket_operation_base_ops {
public:
    //! the object
    SocketPollSocketOperationBase* b;

    DLLLOCAL explicit qore_socket_poll_socket_operation_base_ops(SocketPollSocketOperationBase* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_socket_poll_socket_operation_base_ops(H& holder) : qore_socket_poll_socket_operation_base_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_socket_poll_socket_operation_base_ops(const SocketPollSocketOperationBase* n_b) : b(const_cast<SocketPollSocketOperationBase*>(n_b)) {
    }

    DLLLOCAL explicit qore_socket_poll_socket_operation_base_ops(SocketPollSocketOperationBase& r) : b(&r) {
    }

    DLLLOCAL explicit qore_socket_poll_socket_operation_base_ops(const SocketPollSocketOperationBase& r) : b(const_cast<SocketPollSocketOperationBase*>(&r)) {
    }

    //! Releases the operation's claim on the socket without using the socket's descriptor
    /** The async I/O controller calls this before it delivers the result of an operation that it ends without
        aborting it, because the descriptor that the operation waited on was closed or replaced, or the socket was
        deleted; abort() is not called then, as it can close the descriptor that the socket has now.  The code that
        gets the result can then use the socket at once, although the operation object is released only later.
    */
    DLLLOCAL static void releaseSocketClaimImpl(SocketPollSocketOperationBase* b);
    DLLLOCAL void releaseSocketClaim() {
        releaseSocketClaimImpl(b);
    }

};

//! The library-internal operations of UnicodeCharacterIterator (qore#4380)
class qore_unicode_character_iterator_ops {
public:
    //! the object
    UnicodeCharacterIterator* b;

    DLLLOCAL explicit qore_unicode_character_iterator_ops(UnicodeCharacterIterator* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_unicode_character_iterator_ops(H& holder) : qore_unicode_character_iterator_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_unicode_character_iterator_ops(const UnicodeCharacterIterator* n_b) : b(const_cast<UnicodeCharacterIterator*>(n_b)) {
    }

    DLLLOCAL explicit qore_unicode_character_iterator_ops(UnicodeCharacterIterator& r) : b(&r) {
    }

    DLLLOCAL explicit qore_unicode_character_iterator_ops(const UnicodeCharacterIterator& r) : b(const_cast<UnicodeCharacterIterator*>(&r)) {
    }

    //! returns the next character of a string in a stateful encoding
    DLLLOCAL static bool nextStatefulImpl(UnicodeCharacterIterator* b, ExceptionSink* xsink);
    DLLLOCAL bool nextStateful(ExceptionSink* xsink) {
        return nextStatefulImpl(b, xsink);
    }

};

//! The library-internal operations of ThreadCleanupList (qore#4380)
class qore_thread_cleanup_list_ops {
public:
    //! the object
    ThreadCleanupList* b;

    DLLLOCAL explicit qore_thread_cleanup_list_ops(ThreadCleanupList* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_thread_cleanup_list_ops(H& holder) : qore_thread_cleanup_list_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_thread_cleanup_list_ops(const ThreadCleanupList* n_b) : b(const_cast<ThreadCleanupList*>(n_b)) {
    }

    DLLLOCAL explicit qore_thread_cleanup_list_ops(ThreadCleanupList& r) : b(&r) {
    }

    DLLLOCAL explicit qore_thread_cleanup_list_ops(const ThreadCleanupList& r) : b(const_cast<ThreadCleanupList*>(&r)) {
    }

    DLLLOCAL static void execImpl(ThreadCleanupList* b);
    DLLLOCAL void exec() {
        execImpl(b);
    }

};
//! The library-internal operations of the aligned byte buffer of QoreBufferNode (qore#4380)
class qore_aligned_byte_buffer_ops {
public:
    //! the buffer
    QoreBufferNode::AlignedByteBuffer* b;

    DLLLOCAL explicit qore_aligned_byte_buffer_ops(QoreBufferNode::AlignedByteBuffer* n_b) : b(n_b) {
    }

    DLLLOCAL explicit qore_aligned_byte_buffer_ops(QoreBufferNode::AlignedByteBuffer& r) : b(&r) {
    }

    //! resizes the buffer to the given size, keeping the data unless @a clear is true, in which case it is zeroed
    DLLLOCAL static void resizeImpl(QoreBufferNode::AlignedByteBuffer* b, size_t n_size, bool clear);
    DLLLOCAL void resize(size_t n_size, bool clear = false) {
        resizeImpl(b, n_size, clear);
    }

    //! frees the storage
    DLLLOCAL static void clearImpl(QoreBufferNode::AlignedByteBuffer* b);
    DLLLOCAL void clear() {
        clearImpl(b);
    }
};

#endif // _QORE_INTERN_QOREMISCMEMBERSOPS_H
