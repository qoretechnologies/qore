/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    krb5-module.h

    Qore Kerberos Module

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#ifndef _QORE_KRB5_MODULE_H
#define _QORE_KRB5_MODULE_H

#include <krb5-config.h>
#include <qore/Qore.h>
#include <qore/qore_thread.h>

#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>
#include <gssapi/gssapi_krb5.h>
#include <krb5.h>

#include <string>
#include <cstring>
#include <vector>

DLLLOCAL int krb5_raise_exception(ExceptionSink* xsink, krb5_context ctx, krb5_error_code rc,
    const char* err, const char* context);
DLLLOCAL int gss_raise_exception(ExceptionSink* xsink, const char* err, OM_uint32 major, OM_uint32 minor,
    const char* context);

DLLLOCAL QoreClass* initKrb5ContextClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initKrb5CredentialsClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initKrb5CredentialCacheClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initKrb5KeytabClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initKrb5PrincipalClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initGssCredentialClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initGssAcceptorContextClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initGssClientContextClass(QoreNamespace& ns);

DLLLOCAL TypedHashDecl* init_hashdecl_Krb5KeytabEntryInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Krb5CredentialsInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Krb5InitialCredentialsOptions(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssAcceptorContextOptions(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssClientContextOptions(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssStepInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssWrapInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssUnwrapInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssMicInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GssMicVerifyInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Krb5CredentialCacheInfo(QoreNamespace& ns);

extern QoreClass* QC_KRB5CONTEXT;
extern qore_classid_t CID_KRB5CONTEXT;
extern QoreClass* QC_KRB5CREDENTIALS;
extern qore_classid_t CID_KRB5CREDENTIALS;
extern QoreClass* QC_KRB5CREDENTIALCACHE;
extern qore_classid_t CID_KRB5CREDENTIALCACHE;
extern QoreClass* QC_KRB5KEYTAB;
extern qore_classid_t CID_KRB5KEYTAB;
extern QoreClass* QC_KRB5PRINCIPAL;
extern qore_classid_t CID_KRB5PRINCIPAL;
extern QoreClass* QC_GSSACCEPTORCONTEXT;
extern qore_classid_t CID_GSSACCEPTORCONTEXT;
extern QoreClass* QC_GSSCLIENTCONTEXT;
extern qore_classid_t CID_GSSCLIENTCONTEXT;
extern QoreClass* QC_GSSCREDENTIAL;
extern qore_classid_t CID_GSSCREDENTIAL;

DLLLOCAL extern TypedHashDecl* hashdeclKrb5KeytabEntryInfo;
DLLLOCAL extern TypedHashDecl* hashdeclKrb5CredentialsInfo;
DLLLOCAL extern TypedHashDecl* hashdeclKrb5InitialCredentialsOptions;
DLLLOCAL extern TypedHashDecl* hashdeclGssAcceptorContextOptions;
DLLLOCAL extern TypedHashDecl* hashdeclGssClientContextOptions;
DLLLOCAL extern TypedHashDecl* hashdeclGssStepInfo;
DLLLOCAL extern TypedHashDecl* hashdeclGssWrapInfo;
DLLLOCAL extern TypedHashDecl* hashdeclGssUnwrapInfo;
DLLLOCAL extern TypedHashDecl* hashdeclGssMicInfo;
DLLLOCAL extern TypedHashDecl* hashdeclGssMicVerifyInfo;
DLLLOCAL extern TypedHashDecl* hashdeclKrb5CredentialCacheInfo;

DLLLOCAL bool decode_hex(const char* str, std::vector<unsigned char>& out, ExceptionSink* xsink,
    const char* context);
DLLLOCAL bool decode_hex(const char* str, std::vector<unsigned char>& out, ExceptionSink* xsink,
    const char* err, const char* context);
DLLLOCAL QoreStringNode* encode_hex(const unsigned char* ptr, size_t len);
DLLLOCAL bool krb5_is_empty_cache_error(krb5_error_code rc);
DLLLOCAL QoreStringNode* krb5_unparse_principal(krb5_context ctx, krb5_const_principal principal, ExceptionSink* xsink,
    const char* err, const char* context);

class QoreKrb5CredentialCache;
class QoreKrb5Keytab;

class QoreKrb5Principal : public AbstractPrivateData {
public:
    krb5_context ctx = nullptr;
    krb5_principal principal = nullptr;

    DLLLOCAL explicit QoreKrb5Principal(const char* p, ExceptionSink* xsink);
    DLLLOCAL QoreKrb5Principal(const char* p, krb5_flags parse_flags, ExceptionSink* xsink);
    DLLLOCAL QoreKrb5Principal(krb5_context source_ctx, krb5_principal source_principal, ExceptionSink* xsink);
    DLLLOCAL QoreKrb5Principal(const QoreKrb5Principal& other, ExceptionSink* xsink);
    DLLLOCAL ~QoreKrb5Principal() override;

    DLLLOCAL QoreStringNode* toString(ExceptionSink* xsink) const;
    DLLLOCAL QoreStringNode* getRealm(ExceptionSink* xsink) const;
    DLLLOCAL int getComponentCount() const;
    DLLLOCAL QoreStringNode* getComponent(int idx, ExceptionSink* xsink) const;
    DLLLOCAL bool equals(const QoreKrb5Principal& other) const;
};

class QoreGssClientContext : public AbstractPrivateData {
public:
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t target_name = GSS_C_NO_NAME;
    class QoreGssCredential* cred_ref = nullptr;
    gss_OID mech = GSS_C_NO_OID;
    gss_OID name_type = GSS_KRB5_NT_PRINCIPAL_NAME;
    OM_uint32 req_flags = GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_INTEG_FLAG;
    OM_uint32 lifetime_req = 0;
    gss_channel_bindings_struct channel_bindings;
    std::vector<unsigned char> channel_binding_initiator_address;
    std::vector<unsigned char> channel_binding_acceptor_address;
    std::vector<unsigned char> channel_binding_application_data;
    bool has_channel_bindings = false;
    bool complete = false;
    std::string target_display;

    DLLLOCAL QoreGssClientContext(const char* service_principal, const QoreHashNode* opts,
        ExceptionSink* xsink);
    DLLLOCAL QoreGssClientContext(const char* service_principal, QoreGssCredential* cred,
        const QoreHashNode* opts, ExceptionSink* xsink);
    DLLLOCAL ~QoreGssClientContext() override;

    DLLLOCAL QoreStringNode* getTargetName() const;
    DLLLOCAL bool isComplete() const;
    DLLLOCAL void reset();
    DLLLOCAL QoreHashNode* step(const char* token_hex, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* wrap(const char* message_hex, bool confidential, int qop, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* unwrap(const char* token_hex, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getMic(const char* message_hex, int qop, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* verifyMic(const char* message_hex, const char* mic_hex, ExceptionSink* xsink);
    DLLLOCAL int64 getWrapSizeLimit(int64 output_size, bool confidential, int qop, ExceptionSink* xsink);
};

class QoreGssAcceptorContext : public AbstractPrivateData {
public:
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t initiator_name = GSS_C_NO_NAME;
    class QoreGssCredential* cred_ref = nullptr;
    gss_channel_bindings_struct channel_bindings;
    std::vector<unsigned char> channel_binding_initiator_address;
    std::vector<unsigned char> channel_binding_acceptor_address;
    std::vector<unsigned char> channel_binding_application_data;
    bool has_channel_bindings = false;
    bool complete = false;
    std::string initiator_display;
    gss_cred_id_t delegated_cred = GSS_C_NO_CREDENTIAL;

    DLLLOCAL QoreGssAcceptorContext(const QoreHashNode* opts, ExceptionSink* xsink);
    DLLLOCAL QoreGssAcceptorContext(QoreGssCredential* cred, const QoreHashNode* opts, ExceptionSink* xsink);
    DLLLOCAL ~QoreGssAcceptorContext() override;

    DLLLOCAL QoreStringNode* getInitiatorName() const;
    DLLLOCAL bool isComplete() const;
    DLLLOCAL void reset();
    DLLLOCAL QoreHashNode* step(const char* token_hex, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* wrap(const char* message_hex, bool confidential, int qop, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* unwrap(const char* token_hex, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getMic(const char* message_hex, int qop, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* verifyMic(const char* message_hex, const char* mic_hex, ExceptionSink* xsink);
    DLLLOCAL int64 getWrapSizeLimit(int64 output_size, bool confidential, int qop, ExceptionSink* xsink);
    DLLLOCAL QoreGssCredential* getDelegatedCredential(ExceptionSink* xsink);
};

class QoreGssCredential : public AbstractPrivateData {
public:
    gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;

    DLLLOCAL QoreGssCredential(const QoreKrb5CredentialCache& cache, ExceptionSink* xsink);
    DLLLOCAL QoreGssCredential(const QoreKrb5Keytab& keytab, const QoreKrb5Principal* principal,
        ExceptionSink* xsink);
    DLLLOCAL explicit QoreGssCredential(gss_cred_id_t existing_cred);
    DLLLOCAL QoreGssCredential(const QoreGssCredential& impersonator, const QoreKrb5Principal& user,
        ExceptionSink* xsink);
    DLLLOCAL ~QoreGssCredential() override;
};

class QoreKrb5CredentialCache : public AbstractPrivateData {
public:
    krb5_context ctx = nullptr;
    krb5_ccache cache = nullptr;

    DLLLOCAL QoreKrb5CredentialCache(const char* cache_name, bool use_default, ExceptionSink* xsink);
    DLLLOCAL ~QoreKrb5CredentialCache() override;

    DLLLOCAL QoreStringNode* getName() const;
    DLLLOCAL QoreStringNode* getType() const;
    DLLLOCAL QoreStringNode* getFullName(ExceptionSink* xsink) const;
    DLLLOCAL int initialize(const QoreKrb5Principal& principal, ExceptionSink* xsink);
    DLLLOCAL int storeCredentials(const class QoreKrb5Credentials& creds, ExceptionSink* xsink);
    DLLLOCAL QoreListNode* listCredentials(ExceptionSink* xsink) const;
    DLLLOCAL bool hasPrimaryPrincipal(ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Principal* getPrimaryPrincipal(ExceptionSink* xsink) const;
};

class QoreKrb5Credentials : public AbstractPrivateData {
public:
    krb5_context ctx = nullptr;
    krb5_creds* creds = nullptr;

    DLLLOCAL QoreKrb5Credentials(const QoreKrb5Principal& client, const QoreKrb5Principal& server,
        const char* session_key_hex, krb5_enctype enctype, krb5_timestamp start_time, krb5_timestamp end_time,
        krb5_timestamp renew_until, krb5_flags flags, const char* ticket_hex, ExceptionSink* xsink);
    DLLLOCAL QoreKrb5Credentials(krb5_context source_ctx, const krb5_creds& source_creds, ExceptionSink* xsink);
    DLLLOCAL ~QoreKrb5Credentials() override;

    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Principal* getClientPrincipal(ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Principal* getServerPrincipal(ExceptionSink* xsink) const;
};

class QoreKrb5Context : public AbstractPrivateData {
public:
    krb5_context ctx = nullptr;

    DLLLOCAL explicit QoreKrb5Context(ExceptionSink* xsink);
    DLLLOCAL ~QoreKrb5Context() override;

    DLLLOCAL QoreStringNode* getDefaultRealm(ExceptionSink* xsink) const;
    DLLLOCAL QoreStringNode* getDefaultCredentialCacheName(ExceptionSink* xsink) const;
    DLLLOCAL QoreStringNode* getDefaultKeytabName(ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5CredentialCache* openCredentialCache(const char* cache_name, ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5CredentialCache* openDefaultCredentialCache(ExceptionSink* xsink) const;
    DLLLOCAL class QoreKrb5Keytab* openKeytab(const char* keytab_name, ExceptionSink* xsink) const;
    DLLLOCAL class QoreKrb5Keytab* openDefaultKeytab(ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5CredentialCache* createMemoryCredentialCache(const QoreKrb5Principal& principal,
        const char* cache_name, ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Credentials* acquireCredentialsWithPassword(const QoreKrb5Principal& principal,
        const char* password, const QoreHashNode* opts, ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Credentials* acquireCredentialsWithKeytab(const QoreKrb5Principal& principal,
        const QoreKrb5Keytab& keytab, const QoreHashNode* opts, ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Credentials* renewCredentials(const QoreKrb5CredentialCache& cache,
        const QoreKrb5Principal& client, ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Credentials* acquireServiceCredentials(const QoreKrb5CredentialCache& cache,
        const QoreKrb5Principal& service, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* listCredentialCaches(ExceptionSink* xsink) const;
    DLLLOCAL QoreKrb5Credentials* acquireS4U2ProxyCredentials(const QoreKrb5CredentialCache& cache,
        const QoreKrb5Credentials& evidence, const QoreKrb5Principal& target, ExceptionSink* xsink) const;
};

class QoreKrb5Keytab : public AbstractPrivateData {
public:
    krb5_context ctx = nullptr;
    krb5_keytab keytab = nullptr;

    DLLLOCAL QoreKrb5Keytab(const char* keytab_name, bool use_default, ExceptionSink* xsink);
    DLLLOCAL ~QoreKrb5Keytab() override;

    DLLLOCAL QoreStringNode* getName(ExceptionSink* xsink) const;
    DLLLOCAL QoreStringNode* getType() const;
    DLLLOCAL bool hasContent(ExceptionSink* xsink) const;
    DLLLOCAL int addEntry(const QoreKrb5Principal& principal, const char* key_hex, krb5_enctype enctype,
        krb5_kvno kvno, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getEntry(const QoreKrb5Principal& principal, krb5_kvno kvno, krb5_enctype enctype,
        ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* listEntries(ExceptionSink* xsink) const;
    DLLLOCAL int removeEntry(const QoreKrb5Principal& principal, krb5_kvno kvno, krb5_enctype enctype,
        ExceptionSink* xsink);
};

#endif
