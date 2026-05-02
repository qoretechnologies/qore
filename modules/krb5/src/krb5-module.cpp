/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    krb5-module.cpp

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

#include "krb5-module.h"
#include "QC_GssAcceptorContext.h"
#include "QC_GssClientContext.h"
#include "QC_GssCredential.h"
#include "QC_Krb5Context.h"
#include "QC_Krb5Credentials.h"
#include "QC_Krb5CredentialCache.h"
#include "QC_Krb5Keytab.h"
#include "QC_Krb5Principal.h"

#include <qore/QoreSandboxManager.h>

#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <strings.h>

static QoreNamespace krb5ns("Qore::Krb5");

TypedHashDecl* hashdeclKrb5KeytabEntryInfo = nullptr;
TypedHashDecl* hashdeclKrb5CredentialsInfo = nullptr;
TypedHashDecl* hashdeclKrb5InitialCredentialsOptions = nullptr;
TypedHashDecl* hashdeclGssAcceptorContextOptions = nullptr;
TypedHashDecl* hashdeclGssClientContextOptions = nullptr;
TypedHashDecl* hashdeclGssStepInfo = nullptr;
TypedHashDecl* hashdeclGssWrapInfo = nullptr;
TypedHashDecl* hashdeclGssUnwrapInfo = nullptr;
TypedHashDecl* hashdeclGssMicInfo = nullptr;
TypedHashDecl* hashdeclGssMicVerifyInfo = nullptr;
TypedHashDecl* hashdeclKrb5CredentialCacheInfo = nullptr;

static void krb5_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void krb5_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void krb5_module_delete();

extern "C" DLLEXPORT void krb5_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "krb5";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Kerberos 5 and GSSAPI module";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = krb5_module_init;
    mod_info.ns_init = krb5_module_ns_init;
    mod_info.del = krb5_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

DLLLOCAL int krb5_raise_exception(ExceptionSink* xsink, krb5_context ctx, krb5_error_code rc,
        const char* err, const char* context) {
    const char* msg = ctx ? krb5_get_error_message(ctx, rc) : nullptr;
    if (msg) {
        xsink->raiseException(err, "%s: %s (%d)", context, msg, static_cast<int>(rc));
        krb5_free_error_message(ctx, msg);
    } else {
        xsink->raiseException(err, "%s: error code %d", context, static_cast<int>(rc));
    }
    return -1;
}

DLLLOCAL int gss_raise_exception(ExceptionSink* xsink, const char* err, OM_uint32 major, OM_uint32 minor,
        const char* context) {
    OM_uint32 msg_ctx = 0;
    OM_uint32 min_stat = 0;
    gss_buffer_desc status = GSS_C_EMPTY_BUFFER;
    std::string message;

    do {
        gss_display_status(&min_stat, major, GSS_C_GSS_CODE, GSS_C_NO_OID, &msg_ctx, &status);
        if (status.length) {
            if (!message.empty()) {
                message += "; ";
            }
            message.append(static_cast<const char*>(status.value), status.length);
            gss_release_buffer(&min_stat, &status);
        }
    } while (msg_ctx);

    msg_ctx = 0;
    do {
        gss_display_status(&min_stat, minor, GSS_C_MECH_CODE, GSS_C_NO_OID, &msg_ctx, &status);
        if (status.length) {
            if (!message.empty()) {
                message += "; ";
            }
            message.append(static_cast<const char*>(status.value), status.length);
            gss_release_buffer(&min_stat, &status);
        }
    } while (msg_ctx);

    if (message.empty()) {
        message = "unknown GSSAPI error";
    }

    xsink->raiseException(err, "%s: %s (major=%u minor=%u)", context, message.c_str(),
        static_cast<unsigned int>(major), static_cast<unsigned int>(minor));
    return -1;
}

static int from_hex(unsigned char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

DLLLOCAL bool decode_hex(const char* str, std::vector<unsigned char>& out, ExceptionSink* xsink,
        const char* err, const char* context) {
    size_t len = strlen(str);
    if (len % 2) {
        xsink->raiseException(err, "%s: hex string must have even length", context);
        return false;
    }

    out.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        if (i && !(i % 200) && qore_check_cancel(xsink, context)) {
            return false;
        }

        int hi = from_hex(str[i]);
        int lo = from_hex(str[i + 1]);
        if (hi < 0 || lo < 0) {
            xsink->raiseException(err, "%s: invalid hex character at position %d", context, static_cast<int>(i));
            return false;
        }
        out.push_back((hi << 4) | lo);
    }
    return true;
}

DLLLOCAL bool decode_hex(const char* str, std::vector<unsigned char>& out, ExceptionSink* xsink,
        const char* context) {
    return decode_hex(str, out, xsink, "KRB5-TOKEN-ERROR", context);
}

DLLLOCAL QoreStringNode* encode_hex(const unsigned char* ptr, size_t len, ExceptionSink* xsink,
        const char* context) {
    static const char* digits = "0123456789abcdef";
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
    str->allocate(len * 2);
    for (size_t i = 0; i < len; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, context)) {
            return nullptr;
        }

        str->concat(digits[(ptr[i] >> 4) & 0x0f]);
        str->concat(digits[ptr[i] & 0x0f]);
    }
    return str.release();
}

// RAII wrapper that always releases a gss_buffer_desc on scope exit, including on error paths.
class GssBufferHolder {
public:
    gss_buffer_desc buf = GSS_C_EMPTY_BUFFER;

    DLLLOCAL GssBufferHolder() = default;
    DLLLOCAL ~GssBufferHolder() {
        if (buf.length || buf.value) {
            OM_uint32 min_stat = 0;
            gss_release_buffer(&min_stat, &buf);
        }
    }

    DLLLOCAL gss_buffer_desc* operator&() { return &buf; }
    DLLLOCAL gss_buffer_desc* get() { return &buf; }

    GssBufferHolder(const GssBufferHolder&) = delete;
    GssBufferHolder& operator=(const GssBufferHolder&) = delete;
};

class SecureBytes : public std::vector<unsigned char> {
public:
    DLLLOCAL ~SecureBytes() {
        if (!empty()) {
            // Avoid leaving decoded key material in heap memory after transfer to krb5 structures.
            volatile unsigned char* p = data();
            for (size_t i = 0; i < size(); ++i) {
                p[i] = 0;
            }
        }
    }

    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;
    SecureBytes() = default;
};

class Krb5CredentialCacheCursorHolder {
public:
    krb5_context ctx = nullptr;
    krb5_ccache cache = nullptr;
    krb5_cc_cursor cursor = nullptr;
    bool active = false;

    DLLLOCAL Krb5CredentialCacheCursorHolder(krb5_context ctx, krb5_ccache cache) : ctx(ctx), cache(cache) {
    }

    DLLLOCAL ~Krb5CredentialCacheCursorHolder() {
        close();
    }

    DLLLOCAL krb5_error_code start() {
        krb5_error_code rc = krb5_cc_start_seq_get(ctx, cache, &cursor);
        active = !rc;
        return rc;
    }

    DLLLOCAL void close() {
        if (active) {
            krb5_cc_end_seq_get(ctx, cache, &cursor);
            active = false;
        }
    }

    Krb5CredentialCacheCursorHolder(const Krb5CredentialCacheCursorHolder&) = delete;
    Krb5CredentialCacheCursorHolder& operator=(const Krb5CredentialCacheCursorHolder&) = delete;
};

class Krb5KeytabCursorHolder {
public:
    krb5_context ctx = nullptr;
    krb5_keytab keytab = nullptr;
    krb5_kt_cursor cursor = nullptr;
    bool active = false;

    DLLLOCAL Krb5KeytabCursorHolder(krb5_context ctx, krb5_keytab keytab) : ctx(ctx), keytab(keytab) {
    }

    DLLLOCAL ~Krb5KeytabCursorHolder() {
        close();
    }

    DLLLOCAL krb5_error_code start() {
        krb5_error_code rc = krb5_kt_start_seq_get(ctx, keytab, &cursor);
        active = !rc;
        return rc;
    }

    DLLLOCAL void close() {
        if (active) {
            krb5_kt_end_seq_get(ctx, keytab, &cursor);
            active = false;
        }
    }

    Krb5KeytabCursorHolder(const Krb5KeytabCursorHolder&) = delete;
    Krb5KeytabCursorHolder& operator=(const Krb5KeytabCursorHolder&) = delete;
};

class Krb5PrincipalHolder {
public:
    krb5_context ctx = nullptr;
    krb5_principal principal = nullptr;

    DLLLOCAL explicit Krb5PrincipalHolder(krb5_context ctx) : ctx(ctx) {
    }

    DLLLOCAL ~Krb5PrincipalHolder() {
        if (principal) {
            krb5_free_principal(ctx, principal);
        }
    }

    DLLLOCAL krb5_principal* out() {
        return &principal;
    }

    DLLLOCAL krb5_principal release() {
        krb5_principal rv = principal;
        principal = nullptr;
        return rv;
    }

    Krb5PrincipalHolder(const Krb5PrincipalHolder&) = delete;
    Krb5PrincipalHolder& operator=(const Krb5PrincipalHolder&) = delete;
};

class Krb5CredsContentsHolder {
public:
    krb5_context ctx = nullptr;
    krb5_creds* creds = nullptr;

    DLLLOCAL Krb5CredsContentsHolder(krb5_context ctx, krb5_creds* creds) : ctx(ctx), creds(creds) {
    }

    DLLLOCAL ~Krb5CredsContentsHolder() {
        if (creds) {
            krb5_free_cred_contents(ctx, creds);
        }
    }

    Krb5CredsContentsHolder(const Krb5CredsContentsHolder&) = delete;
    Krb5CredsContentsHolder& operator=(const Krb5CredsContentsHolder&) = delete;
};

class Krb5CccolCursorHolder {
public:
    krb5_context ctx = nullptr;
    krb5_cccol_cursor cursor = nullptr;
    bool active = false;

    DLLLOCAL explicit Krb5CccolCursorHolder(krb5_context ctx) : ctx(ctx) {
    }

    DLLLOCAL ~Krb5CccolCursorHolder() {
        close();
    }

    DLLLOCAL krb5_error_code start() {
        krb5_error_code rc = krb5_cccol_cursor_new(ctx, &cursor);
        active = !rc;
        return rc;
    }

    DLLLOCAL void close() {
        if (active) {
            krb5_cccol_cursor_free(ctx, &cursor);
            active = false;
        }
    }

    Krb5CccolCursorHolder(const Krb5CccolCursorHolder&) = delete;
    Krb5CccolCursorHolder& operator=(const Krb5CccolCursorHolder&) = delete;
};

DLLLOCAL bool krb5_is_empty_cache_error(krb5_error_code rc) {
    return rc == KRB5_FCC_NOFILE || rc == KRB5_CC_NOTFOUND || rc == KRB5_CC_END;
}

static bool krb5_cache_backend_uses_filesystem(const char* type) {
    return !strcmp(type, "FILE") || !strcmp(type, "DIR");
}

static bool krb5_cache_backend_is_non_filesystem(const char* type) {
    return !strcmp(type, "MEMORY") || !strcmp(type, "API") || !strcmp(type, "KCM")
        || !strcmp(type, "KEYRING") || !strcmp(type, "MSLSA");
}

static bool krb5_check_cache_access(krb5_context ctx, krb5_ccache cache, int mode, ExceptionSink* xsink,
        const char* context) {
    QoreSandboxManagerHelper smh;
    if (!smh) {
        return true;
    }

    const char* type = krb5_cc_get_type(ctx, cache);
    const char* name = krb5_cc_get_name(ctx, cache);
    if (!type || !*type || !name || !*name) {
        xsink->raiseException("KRB5-CACHE-ERROR", "%s: credential cache metadata is unavailable", context);
        return false;
    }

    if (krb5_cache_backend_uses_filesystem(type)) {
        return smh->checkFilesystemAccess(name, mode, xsink);
    }

    if (krb5_cache_backend_is_non_filesystem(type)) {
        return true;
    }

    xsink->raiseException("KRB5-SANDBOX-ERROR",
        "%s: credential cache backend '%s' is not supported in sandboxed mode", context, type);
    return false;
}

static bool krb5_keytab_backend_uses_filesystem(const char* type) {
    return !strcmp(type, "FILE") || !strcmp(type, "WRFILE") || !strcmp(type, "DIR");
}

static bool krb5_keytab_backend_is_non_filesystem(const char* type) {
    return !strcmp(type, "MEMORY");
}

static const char* krb5_keytab_filesystem_path(const char* full_name) {
    const char* delim = strchr(full_name, ':');
    return delim ? delim + 1 : full_name;
}

static bool krb5_check_keytab_access(krb5_context ctx, krb5_keytab keytab, int mode, ExceptionSink* xsink,
        const char* context) {
    QoreSandboxManagerHelper smh;
    if (!smh) {
        return true;
    }

    const char* type = krb5_kt_get_type(ctx, keytab);
    if (!type || !*type) {
        xsink->raiseException("KRB5-KEYTAB-ERROR", "%s: keytab metadata is unavailable", context);
        return false;
    }

    char name[MAX_KEYTAB_NAME_LEN] = {0};
    krb5_error_code rc = krb5_kt_get_name(ctx, keytab, name, sizeof(name));
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "getting keytab name for sandbox check");
        return false;
    }

    if (krb5_keytab_backend_uses_filesystem(type)) {
        return smh->checkFilesystemAccess(krb5_keytab_filesystem_path(name), mode, xsink);
    }

    if (krb5_keytab_backend_is_non_filesystem(type)) {
        return true;
    }

    xsink->raiseException("KRB5-SANDBOX-ERROR",
        "%s: keytab backend '%s' is not supported in sandboxed mode", context, type);
    return false;
}

static bool krb5_network_sandbox_allows_unmanaged_library_io(ExceptionSink* xsink, const char* context) {
    QoreSandboxManagerHelper smh;
    if (!smh) {
        return true;
    }

    ReferenceHolder<QoreHashNode> config(smh->network().getConfiguration(xsink), xsink);
    if (*xsink) {
        return false;
    }

    QoreValue policy = config->getKeyValue("default_policy");
    bool default_allow = false;
    if (policy.getType() == NT_STRING) {
        QoreStringValueHelper policy_str(policy);
        default_allow = !strcmp(policy_str->c_str(), "allow");
    }
    const QoreListNode* allowed_hosts = config->getKeyValue("allowed_hosts").get<const QoreListNode>();
    const QoreListNode* allowed_ports = config->getKeyValue("allowed_ports").get<const QoreListNode>();
    bool unrestricted = default_allow
        && (!allowed_hosts || allowed_hosts->empty())
        && (!allowed_ports || allowed_ports->empty())
        && !config->getKeyValue("allowed_ranges_count").getAsBigInt()
        && !config->getKeyValue("denied_ranges_count").getAsBigInt();

    if (unrestricted) {
        return true;
    }

    xsink->raiseException("KRB5-SANDBOX-ERROR",
        "%s: MIT Kerberos/GSSAPI performs KDC network I/O internally, so this module cannot enforce "
        "the active fine-grained network sandbox policy with post-DNS checks", context);
    return false;
}

DLLLOCAL QoreStringNode* krb5_unparse_principal(krb5_context ctx, krb5_const_principal principal, ExceptionSink* xsink,
        const char* err, const char* context) {
    char* name = nullptr;
    krb5_error_code rc = krb5_unparse_name(ctx, principal, &name);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, err, context);
        return nullptr;
    }

    QoreStringNode* rv = new QoreStringNode(name);
    krb5_free_unparsed_name(ctx, name);
    return rv;
}

QoreKrb5Principal::QoreKrb5Principal(const char* p, ExceptionSink* xsink) : QoreKrb5Principal(p, 0, xsink) {
}

QoreKrb5Principal::QoreKrb5Principal(const char* p, krb5_flags parse_flags, ExceptionSink* xsink) {
    if (!p || !*p) {
        xsink->raiseException("KRB5-PRINCIPAL-ERROR", "principal string cannot be empty");
        return;
    }

    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }

    rc = parse_flags ? krb5_parse_name_flags(ctx, p, parse_flags, &principal) : krb5_parse_name(ctx, p, &principal);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-PRINCIPAL-ERROR", "parsing kerberos principal");
        return;
    }
}

QoreKrb5Principal::QoreKrb5Principal(krb5_context source_ctx, krb5_principal source_principal, ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }

    rc = krb5_copy_principal(source_ctx, source_principal, &principal);
    if (rc) {
        krb5_raise_exception(xsink, source_ctx, rc, "KRB5-PRINCIPAL-ERROR", "copying kerberos principal");
    }
}

QoreKrb5Principal::QoreKrb5Principal(const QoreKrb5Principal& other, ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }
    rc = krb5_copy_principal(other.ctx, other.principal, &principal);
    if (rc) {
        krb5_raise_exception(xsink, other.ctx, rc, "KRB5-PRINCIPAL-ERROR", "copying kerberos principal");
    }
}

QoreKrb5Principal::~QoreKrb5Principal() {
    if (principal) {
        krb5_free_principal(ctx, principal);
    }
    if (ctx) {
        krb5_free_context(ctx);
    }
}

QoreStringNode* QoreKrb5Principal::toString(ExceptionSink* xsink) const {
    return krb5_unparse_principal(ctx, principal, xsink, "KRB5-PRINCIPAL-ERROR",
        "unparsing kerberos principal");
}

QoreStringNode* QoreKrb5Principal::getRealm(ExceptionSink* xsink) const {
    const krb5_data* realm = krb5_princ_realm(ctx, principal);
    if (!realm || !realm->data) {
        xsink->raiseException("KRB5-REALM-ERROR", "kerberos principal does not have a realm");
        return nullptr;
    }
    return new QoreStringNode(realm->data, realm->length);
}

int QoreKrb5Principal::getComponentCount() const {
    return krb5_princ_size(ctx, principal);
}

QoreStringNode* QoreKrb5Principal::getComponent(int idx, ExceptionSink* xsink) const {
    if (idx < 0 || idx >= krb5_princ_size(ctx, principal)) {
        xsink->raiseException("KRB5-PRINCIPAL-ERROR", "principal component index %d out of range", idx);
        return nullptr;
    }
    const krb5_data* data = krb5_princ_component(ctx, principal, idx);
    return new QoreStringNode(data->data, data->length);
}

bool QoreKrb5Principal::equals(const QoreKrb5Principal& other) const {
    return krb5_principal_compare(ctx, principal, other.principal);
}

class QoreGssChannelBindingOptions {
public:
    OM_uint32 initiator_addrtype = GSS_C_AF_NULLADDR;
    OM_uint32 acceptor_addrtype = GSS_C_AF_NULLADDR;
    std::vector<unsigned char> initiator_address;
    std::vector<unsigned char> acceptor_address;
    std::vector<unsigned char> application_data;
    bool has_channel_bindings = false;

protected:
    DLLLOCAL void parseChannelBindingOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
        parseAddrType(opts, "channel_binding_initiator_addrtype", initiator_addrtype, xsink);
        if (*xsink) {
            return;
        }
        parseAddrType(opts, "channel_binding_acceptor_addrtype", acceptor_addrtype, xsink);
        if (*xsink) {
            return;
        }
        parseHexOption(opts, "channel_binding_initiator_address_hex", initiator_address, xsink);
        if (*xsink) {
            return;
        }
        parseHexOption(opts, "channel_binding_acceptor_address_hex", acceptor_address, xsink);
        if (*xsink) {
            return;
        }
        parseHexOption(opts, "channel_binding_application_data_hex", application_data, xsink);
        if (*xsink) {
            return;
        }
        has_channel_bindings = !initiator_address.empty() || !acceptor_address.empty() || !application_data.empty();
        if (!has_channel_bindings) {
            if (initiator_addrtype != GSS_C_AF_NULLADDR || acceptor_addrtype != GSS_C_AF_NULLADDR) {
                xsink->raiseException("KRB5-GSS-ARG-ERROR", "channel-binding address types require address data");
            }
            return;
        }
        if ((initiator_addrtype == GSS_C_AF_NULLADDR) != initiator_address.empty()) {
            xsink->raiseException("KRB5-GSS-ARG-ERROR",
                "channel-binding initiator address requires a non-null address type and address data");
            return;
        }
        if ((acceptor_addrtype == GSS_C_AF_NULLADDR) != acceptor_address.empty()) {
            xsink->raiseException("KRB5-GSS-ARG-ERROR",
                "channel-binding acceptor address requires a non-null address type and address data");
        }
    }

private:
    DLLLOCAL static void parseAddrType(const QoreHashNode* opts, const char* key, OM_uint32& value,
            ExceptionSink* xsink) {
        QoreValue v = opts->getKeyValue(key);
        if (v.isNullOrNothing()) {
            return;
        }
        int64 i = v.getAsBigInt();
        if (i < 0 || i > UINT32_MAX) {
            xsink->raiseException("KRB5-GSS-ARG-ERROR", "option '%s' must be between 0 and %u", key, UINT32_MAX);
            return;
        }
        value = static_cast<OM_uint32>(i);
    }

    DLLLOCAL static void parseHexOption(const QoreHashNode* opts, const char* key, std::vector<unsigned char>& value,
            ExceptionSink* xsink) {
        QoreValue v = opts->getKeyValue(key);
        if (v.isNullOrNothing()) {
            return;
        }

        QoreStringValueHelper str(v, QCS_UTF8, xsink);
        if (*xsink) {
            return;
        }
        if (!str->c_str() || !*str->c_str()) {
            xsink->raiseException("KRB5-GSS-ARG-ERROR", "option '%s' cannot be empty", key);
            return;
        }
        decode_hex(str->c_str(), value, xsink, "KRB5-GSS-ARG-ERROR", key);
    }
};

class QoreGssClientContextOptions : public QoreGssChannelBindingOptions {
public:
    OM_uint32 req_flags = GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_INTEG_FLAG;
    OM_uint32 lifetime_req = 0;
    gss_OID mech = gss_mech_krb5;
    gss_OID name_type = GSS_KRB5_NT_PRINCIPAL_NAME;

    DLLLOCAL QoreGssClientContextOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
        if (!opts) {
            return;
        }

        QoreValue v = opts->getKeyValue("flags");
        if (!v.isNullOrNothing()) {
            int64 flags = v.getAsBigInt();
            if (flags < 0 || flags > UINT32_MAX) {
                xsink->raiseException("KRB5-GSS-ARG-ERROR", "option 'flags' must be between 0 and %u", UINT32_MAX);
                return;
            }
            req_flags = static_cast<OM_uint32>(flags);
        }

        v = opts->getKeyValue("lifetime");
        if (!v.isNullOrNothing()) {
            int64 lifetime = v.getAsBigInt();
            if (lifetime < 0 || lifetime > UINT32_MAX) {
                xsink->raiseException("KRB5-GSS-ARG-ERROR", "option 'lifetime' must be between 0 and %u",
                    UINT32_MAX);
                return;
            }
            lifetime_req = static_cast<OM_uint32>(lifetime);
        }

        v = opts->getKeyValue("mechanism");
        if (!v.isNullOrNothing()) {
            QoreStringValueHelper mechanism(v, QCS_UTF8, xsink);
            if (*xsink) {
                return;
            }
            if (!mechanism->c_str() || !*mechanism->c_str()) {
                xsink->raiseException("KRB5-GSS-ARG-ERROR", "option 'mechanism' cannot be empty");
                return;
            }
            if (!strcasecmp(mechanism->c_str(), "kerberos")) {
                mech = gss_mech_krb5;
            } else if (!strcasecmp(mechanism->c_str(), "default")) {
                mech = GSS_C_NO_OID;
            } else {
                xsink->raiseException("KRB5-GSS-ARG-ERROR",
                    "unsupported GSSAPI mechanism '%s'; supported values are 'kerberos' and 'default'",
                    mechanism->c_str());
                return;
            }
        }

        v = opts->getKeyValue("name_type");
        if (!v.isNullOrNothing()) {
            QoreStringValueHelper nt(v, QCS_UTF8, xsink);
            if (*xsink) {
                return;
            }
            if (!nt->c_str() || !*nt->c_str()) {
                xsink->raiseException("KRB5-GSS-ARG-ERROR", "option 'name_type' cannot be empty");
                return;
            }
            if (!strcasecmp(nt->c_str(), "krb5-principal")) {
                name_type = GSS_KRB5_NT_PRINCIPAL_NAME;
            } else if (!strcasecmp(nt->c_str(), "hostbased-service")) {
                name_type = GSS_C_NT_HOSTBASED_SERVICE;
            } else if (!strcasecmp(nt->c_str(), "default")) {
                name_type = GSS_C_NO_OID;
            } else {
                xsink->raiseException("KRB5-GSS-ARG-ERROR",
                    "unsupported GSSAPI name type '%s'; supported values are 'krb5-principal', "
                    "'hostbased-service', and 'default'", nt->c_str());
                return;
            }
        }

        parseChannelBindingOptions(opts, xsink);
    }
};

QoreGssClientContext::QoreGssClientContext(const char* service_principal, const QoreHashNode* opts,
        ExceptionSink* xsink) : QoreGssClientContext(service_principal, nullptr, opts, xsink) {
}

QoreGssClientContext::QoreGssClientContext(const char* service_principal, QoreGssCredential* cred,
        const QoreHashNode* opts, ExceptionSink* xsink) {
    if (!service_principal || !*service_principal) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "service principal cannot be empty");
        return;
    }

    QoreGssClientContextOptions parsed_opts(opts, xsink);
    if (*xsink) {
        return;
    }
    req_flags = parsed_opts.req_flags;
    lifetime_req = parsed_opts.lifetime_req;
    mech = parsed_opts.mech;
    name_type = parsed_opts.name_type;
    memset(&channel_bindings, 0, sizeof(channel_bindings));
    if (parsed_opts.has_channel_bindings) {
        has_channel_bindings = true;
        channel_binding_initiator_address = parsed_opts.initiator_address;
        channel_binding_acceptor_address = parsed_opts.acceptor_address;
        channel_binding_application_data = parsed_opts.application_data;
        channel_bindings.initiator_addrtype = parsed_opts.initiator_addrtype;
        channel_bindings.acceptor_addrtype = parsed_opts.acceptor_addrtype;
        channel_bindings.initiator_address.length = channel_binding_initiator_address.size();
        channel_bindings.initiator_address.value = channel_binding_initiator_address.empty()
            ? nullptr : channel_binding_initiator_address.data();
        channel_bindings.acceptor_address.length = channel_binding_acceptor_address.size();
        channel_bindings.acceptor_address.value = channel_binding_acceptor_address.empty()
            ? nullptr : channel_binding_acceptor_address.data();
        channel_bindings.application_data.length = channel_binding_application_data.size();
        channel_bindings.application_data.value = channel_binding_application_data.empty()
            ? nullptr : channel_binding_application_data.data();
    }

    OM_uint32 min_stat = 0;
    gss_buffer_desc namebuf;
    namebuf.length = strlen(service_principal);
    namebuf.value = const_cast<char*>(service_principal);
    OM_uint32 maj = gss_import_name(&min_stat, &namebuf, name_type, &target_name);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "importing GSSAPI target name");
        return;
    }

    target_display = service_principal;

    if (cred) {
        cred->ref();
        cred_ref = cred;
    }
}

QoreGssClientContext::~QoreGssClientContext() {
    reset();
    if (target_name != GSS_C_NO_NAME) {
        OM_uint32 min_stat = 0;
        gss_release_name(&min_stat, &target_name);
    }
    if (cred_ref) {
        cred_ref->deref();
        cred_ref = nullptr;
    }
}

QoreStringNode* QoreGssClientContext::getTargetName() const {
    return new QoreStringNode(target_display.c_str());
}

bool QoreGssClientContext::isComplete() const {
    return complete;
}

void QoreGssClientContext::reset() {
    if (ctx != GSS_C_NO_CONTEXT) {
        OM_uint32 min_stat = 0;
        gss_delete_sec_context(&min_stat, &ctx, GSS_C_NO_BUFFER);
        ctx = GSS_C_NO_CONTEXT;
    }
    complete = false;
}

QoreHashNode* QoreGssClientContext::step(const char* token_hex, ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "gssapi context initialization")) {
        return nullptr;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink, "initializing GSSAPI security context")) {
        return nullptr;
    }

    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    std::vector<unsigned char> input_bytes;
    if (token_hex && *token_hex) {
        if (!decode_hex(token_hex, input_bytes, xsink, "decoding GSSAPI input token")) {
            return nullptr;
        }
        input_token.value = input_bytes.data();
        input_token.length = input_bytes.size();
    }

    OM_uint32 actual_flags = 0;
    OM_uint32 lifetime = 0;
    GssBufferHolder output_token;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_init_sec_context(&min_stat, cred_ref ? cred_ref->cred : GSS_C_NO_CREDENTIAL, &ctx,
        target_name, mech, req_flags, lifetime_req, has_channel_bindings ? &channel_bindings : GSS_C_NO_CHANNEL_BINDINGS,
        input_token.length ? &input_token : GSS_C_NO_BUFFER, nullptr, &output_token.buf, &actual_flags, &lifetime);

    if (maj != GSS_S_COMPLETE && maj != GSS_S_CONTINUE_NEEDED) {
        reset();
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "initializing GSSAPI security context");
        return nullptr;
    }

    complete = maj == GSS_S_COMPLETE;

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssStepInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("complete", complete, xsink);
    rv->setKeyValue("flags", (int64)actual_flags, xsink);
    rv->setKeyValue("lifetime", (int64)lifetime, xsink);
    if (output_token.buf.length) {
        ReferenceHolder<QoreStringNode> token(
            encode_hex(static_cast<const unsigned char*>(output_token.buf.value), output_token.buf.length, xsink,
                "encoding GSSAPI output token"), xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->setKeyValue("token", token.release(), xsink);
    } else {
        rv->setKeyValue("token", QoreValue(), xsink);
    }
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssClientContext::wrap(const char* message_hex, bool confidential, int qop, ExceptionSink* xsink) {
    std::vector<unsigned char> message_bytes;
    if (!decode_hex(message_hex, message_bytes, xsink, "decoding GSSAPI wrap message")) {
        return nullptr;
    }
    if (qop < 0) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "qop value cannot be negative");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi message wrapping")) {
        return nullptr;
    }

    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    input.value = message_bytes.empty() ? nullptr : message_bytes.data();
    input.length = message_bytes.size();

    int conf_state = 0;
    GssBufferHolder output;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_wrap(&min_stat, ctx, confidential ? 1 : 0, static_cast<gss_qop_t>(qop), &input, &conf_state,
        &output.buf);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "wrapping GSSAPI message");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssWrapInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreStringNode> token(
        encode_hex(static_cast<const unsigned char*>(output.buf.value), output.buf.length, xsink,
            "encoding GSSAPI wrapped token"), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("token", token.release(), xsink);
    rv->setKeyValue("confidential", conf_state ? true : false, xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssClientContext::unwrap(const char* token_hex, ExceptionSink* xsink) {
    std::vector<unsigned char> token_bytes;
    if (!decode_hex(token_hex, token_bytes, xsink, "decoding GSSAPI wrapped token")) {
        return nullptr;
    }
    if (token_bytes.empty()) {
        xsink->raiseException("KRB5-TOKEN-ERROR", "decoding GSSAPI wrapped token: token cannot be empty");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi message unwrapping")) {
        return nullptr;
    }

    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    input.value = token_bytes.data();
    input.length = token_bytes.size();

    int conf_state = 0;
    gss_qop_t qop_state = 0;
    GssBufferHolder output;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_unwrap(&min_stat, ctx, &input, &output.buf, &conf_state, &qop_state);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "unwrapping GSSAPI message");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssUnwrapInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreStringNode> message(
        encode_hex(static_cast<const unsigned char*>(output.buf.value), output.buf.length, xsink,
            "encoding GSSAPI unwrapped message"), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("message", message.release(), xsink);
    rv->setKeyValue("confidential", conf_state ? true : false, xsink);
    rv->setKeyValue("qop", (int64)qop_state, xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssClientContext::getMic(const char* message_hex, int qop, ExceptionSink* xsink) {
    std::vector<unsigned char> message_bytes;
    if (!decode_hex(message_hex, message_bytes, xsink, "decoding GSSAPI getMic message")) {
        return nullptr;
    }
    if (qop < 0) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "qop value cannot be negative");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi getMic")) {
        return nullptr;
    }

    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    input.value = message_bytes.empty() ? nullptr : message_bytes.data();
    input.length = message_bytes.size();

    GssBufferHolder output;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_get_mic(&min_stat, ctx, static_cast<gss_qop_t>(qop), &input, &output.buf);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "computing GSSAPI MIC");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssMicInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreStringNode> token(
        encode_hex(static_cast<const unsigned char*>(output.buf.value), output.buf.length, xsink,
            "encoding GSSAPI MIC token"), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("token", token.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssClientContext::verifyMic(const char* message_hex, const char* mic_hex, ExceptionSink* xsink) {
    std::vector<unsigned char> message_bytes;
    if (!decode_hex(message_hex, message_bytes, xsink, "decoding GSSAPI verifyMic message")) {
        return nullptr;
    }
    std::vector<unsigned char> mic_bytes;
    if (!decode_hex(mic_hex, mic_bytes, xsink, "decoding GSSAPI verifyMic MIC token")) {
        return nullptr;
    }
    if (mic_bytes.empty()) {
        xsink->raiseException("KRB5-TOKEN-ERROR", "decoding GSSAPI verifyMic MIC token: token cannot be empty");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi verifyMic")) {
        return nullptr;
    }

    gss_buffer_desc message = GSS_C_EMPTY_BUFFER;
    message.value = message_bytes.empty() ? nullptr : message_bytes.data();
    message.length = message_bytes.size();

    gss_buffer_desc mic = GSS_C_EMPTY_BUFFER;
    mic.value = mic_bytes.data();
    mic.length = mic_bytes.size();

    gss_qop_t qop_state = 0;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_verify_mic(&min_stat, ctx, &message, &mic, &qop_state);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "verifying GSSAPI MIC");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssMicVerifyInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("qop", (int64)qop_state, xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

int64 QoreGssClientContext::getWrapSizeLimit(int64 output_size, bool confidential, int qop, ExceptionSink* xsink) {
    if (output_size < 0 || output_size > UINT32_MAX) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "output size must be between 0 and %u bytes", UINT32_MAX);
        return 0;
    }
    if (qop < 0) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "qop value cannot be negative");
        return 0;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return 0;
    }
    if (qore_check_cancel(xsink, "checking GSSAPI wrap size limit")) {
        return 0;
    }

    OM_uint32 max_input_size = 0;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_wrap_size_limit(&min_stat, ctx, confidential ? 1 : 0, static_cast<gss_qop_t>(qop),
        static_cast<OM_uint32>(output_size), &max_input_size);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "checking GSSAPI wrap size limit");
        return 0;
    }
    return max_input_size;
}

class QoreGssAcceptorContextOptions : public QoreGssChannelBindingOptions {
public:
    DLLLOCAL QoreGssAcceptorContextOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
        if (!opts) {
            return;
        }
        parseChannelBindingOptions(opts, xsink);
    }
};

QoreGssAcceptorContext::QoreGssAcceptorContext(const QoreHashNode* opts, ExceptionSink* xsink)
        : QoreGssAcceptorContext(nullptr, opts, xsink) {
}

QoreGssAcceptorContext::QoreGssAcceptorContext(QoreGssCredential* cred, const QoreHashNode* opts,
        ExceptionSink* xsink) {
    QoreGssAcceptorContextOptions parsed_opts(opts, xsink);
    if (*xsink) {
        return;
    }

    memset(&channel_bindings, 0, sizeof(channel_bindings));
    if (parsed_opts.has_channel_bindings) {
        has_channel_bindings = true;
        channel_binding_initiator_address = parsed_opts.initiator_address;
        channel_binding_acceptor_address = parsed_opts.acceptor_address;
        channel_binding_application_data = parsed_opts.application_data;
        channel_bindings.initiator_addrtype = parsed_opts.initiator_addrtype;
        channel_bindings.acceptor_addrtype = parsed_opts.acceptor_addrtype;
        channel_bindings.initiator_address.length = channel_binding_initiator_address.size();
        channel_bindings.initiator_address.value = channel_binding_initiator_address.empty()
            ? nullptr : channel_binding_initiator_address.data();
        channel_bindings.acceptor_address.length = channel_binding_acceptor_address.size();
        channel_bindings.acceptor_address.value = channel_binding_acceptor_address.empty()
            ? nullptr : channel_binding_acceptor_address.data();
        channel_bindings.application_data.length = channel_binding_application_data.size();
        channel_bindings.application_data.value = channel_binding_application_data.empty()
            ? nullptr : channel_binding_application_data.data();
    }

    if (cred) {
        cred->ref();
        cred_ref = cred;
    }
}

QoreGssAcceptorContext::~QoreGssAcceptorContext() {
    reset();
    if (cred_ref) {
        cred_ref->deref();
        cred_ref = nullptr;
    }
}

QoreStringNode* QoreGssAcceptorContext::getInitiatorName() const {
    return initiator_display.empty() ? nullptr : new QoreStringNode(initiator_display.c_str());
}

bool QoreGssAcceptorContext::isComplete() const {
    return complete;
}

void QoreGssAcceptorContext::reset() {
    if (ctx != GSS_C_NO_CONTEXT) {
        OM_uint32 min_stat = 0;
        gss_delete_sec_context(&min_stat, &ctx, GSS_C_NO_BUFFER);
        ctx = GSS_C_NO_CONTEXT;
    }
    if (initiator_name != GSS_C_NO_NAME) {
        OM_uint32 min_stat = 0;
        gss_release_name(&min_stat, &initiator_name);
        initiator_name = GSS_C_NO_NAME;
    }
    if (delegated_cred != GSS_C_NO_CREDENTIAL) {
        OM_uint32 min_stat = 0;
        gss_release_cred(&min_stat, &delegated_cred);
        delegated_cred = GSS_C_NO_CREDENTIAL;
    }
    initiator_display.clear();
    complete = false;
}

QoreHashNode* QoreGssAcceptorContext::step(const char* token_hex, ExceptionSink* xsink) {
    if (!token_hex || !*token_hex) {
        xsink->raiseException("KRB5-TOKEN-ERROR", "GSSAPI acceptor input token cannot be empty");
        return nullptr;
    }

    std::vector<unsigned char> input_bytes;
    if (!decode_hex(token_hex, input_bytes, xsink, "decoding GSSAPI acceptor input token")) {
        return nullptr;
    }
    if (input_bytes.empty()) {
        xsink->raiseException("KRB5-TOKEN-ERROR", "decoding GSSAPI acceptor input token: token cannot be empty");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi context acceptance")) {
        return nullptr;
    }

    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    input_token.value = input_bytes.data();
    input_token.length = input_bytes.size();

    gss_name_t src_name = GSS_C_NO_NAME;
    gss_OID mech_type = GSS_C_NO_OID;
    OM_uint32 actual_flags = 0;
    OM_uint32 lifetime = 0;
    GssBufferHolder output_token;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_accept_sec_context(&min_stat, &ctx, cred_ref ? cred_ref->cred : GSS_C_NO_CREDENTIAL,
        &input_token, has_channel_bindings ? &channel_bindings : GSS_C_NO_CHANNEL_BINDINGS, &src_name,
        &mech_type, &output_token.buf, &actual_flags, &lifetime, &delegated_cred);

    if (maj != GSS_S_COMPLETE && maj != GSS_S_CONTINUE_NEEDED) {
        reset();
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "accepting GSSAPI security context");
        return nullptr;
    }

    complete = maj == GSS_S_COMPLETE;

    if (src_name != GSS_C_NO_NAME) {
        GssBufferHolder display_name;
        maj = gss_display_name(&min_stat, src_name, &display_name.buf, nullptr);
        if (maj != GSS_S_COMPLETE) {
            gss_release_name(&min_stat, &src_name);
            reset();
            gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "displaying GSSAPI initiator name");
            return nullptr;
        }

        if (initiator_name != GSS_C_NO_NAME) {
            gss_release_name(&min_stat, &initiator_name);
        }
        initiator_name = src_name;
        initiator_display.assign(static_cast<const char*>(display_name.buf.value), display_name.buf.length);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssStepInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("complete", complete, xsink);
    rv->setKeyValue("flags", (int64)actual_flags, xsink);
    rv->setKeyValue("lifetime", (int64)lifetime, xsink);
    if (output_token.buf.length) {
        ReferenceHolder<QoreStringNode> token(
            encode_hex(static_cast<const unsigned char*>(output_token.buf.value), output_token.buf.length, xsink,
                "encoding GSSAPI acceptor output token"), xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->setKeyValue("token", token.release(), xsink);
    } else {
        rv->setKeyValue("token", QoreValue(), xsink);
    }
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssAcceptorContext::wrap(const char* message_hex, bool confidential, int qop,
        ExceptionSink* xsink) {
    std::vector<unsigned char> message_bytes;
    if (!decode_hex(message_hex, message_bytes, xsink, "decoding GSSAPI acceptor wrap message")) {
        return nullptr;
    }
    if (qop < 0) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "qop value cannot be negative");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi acceptor message wrapping")) {
        return nullptr;
    }

    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    input.value = message_bytes.empty() ? nullptr : message_bytes.data();
    input.length = message_bytes.size();

    int conf_state = 0;
    GssBufferHolder output;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_wrap(&min_stat, ctx, confidential ? 1 : 0, static_cast<gss_qop_t>(qop), &input, &conf_state,
        &output.buf);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "wrapping GSSAPI acceptor message");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssWrapInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreStringNode> token(
        encode_hex(static_cast<const unsigned char*>(output.buf.value), output.buf.length, xsink,
            "encoding GSSAPI acceptor wrapped token"), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("token", token.release(), xsink);
    rv->setKeyValue("confidential", conf_state ? true : false, xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssAcceptorContext::unwrap(const char* token_hex, ExceptionSink* xsink) {
    std::vector<unsigned char> token_bytes;
    if (!decode_hex(token_hex, token_bytes, xsink, "decoding GSSAPI acceptor wrapped token")) {
        return nullptr;
    }
    if (token_bytes.empty()) {
        xsink->raiseException("KRB5-TOKEN-ERROR", "decoding GSSAPI acceptor wrapped token: token cannot be empty");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi acceptor message unwrapping")) {
        return nullptr;
    }

    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    input.value = token_bytes.data();
    input.length = token_bytes.size();

    int conf_state = 0;
    gss_qop_t qop_state = 0;
    GssBufferHolder output;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_unwrap(&min_stat, ctx, &input, &output.buf, &conf_state, &qop_state);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "unwrapping GSSAPI acceptor message");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssUnwrapInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreStringNode> message(
        encode_hex(static_cast<const unsigned char*>(output.buf.value), output.buf.length, xsink,
            "encoding GSSAPI acceptor unwrapped message"), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("message", message.release(), xsink);
    rv->setKeyValue("confidential", conf_state ? true : false, xsink);
    rv->setKeyValue("qop", (int64)qop_state, xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssAcceptorContext::getMic(const char* message_hex, int qop, ExceptionSink* xsink) {
    std::vector<unsigned char> message_bytes;
    if (!decode_hex(message_hex, message_bytes, xsink, "decoding GSSAPI acceptor getMic message")) {
        return nullptr;
    }
    if (qop < 0) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "qop value cannot be negative");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi acceptor getMic")) {
        return nullptr;
    }

    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    input.value = message_bytes.empty() ? nullptr : message_bytes.data();
    input.length = message_bytes.size();

    GssBufferHolder output;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_get_mic(&min_stat, ctx, static_cast<gss_qop_t>(qop), &input, &output.buf);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "computing GSSAPI acceptor MIC");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssMicInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreStringNode> token(
        encode_hex(static_cast<const unsigned char*>(output.buf.value), output.buf.length, xsink,
            "encoding GSSAPI acceptor MIC token"), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("token", token.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreHashNode* QoreGssAcceptorContext::verifyMic(const char* message_hex, const char* mic_hex,
        ExceptionSink* xsink) {
    std::vector<unsigned char> message_bytes;
    if (!decode_hex(message_hex, message_bytes, xsink, "decoding GSSAPI acceptor verifyMic message")) {
        return nullptr;
    }
    std::vector<unsigned char> mic_bytes;
    if (!decode_hex(mic_hex, mic_bytes, xsink, "decoding GSSAPI acceptor verifyMic MIC token")) {
        return nullptr;
    }
    if (mic_bytes.empty()) {
        xsink->raiseException("KRB5-TOKEN-ERROR",
            "decoding GSSAPI acceptor verifyMic MIC token: token cannot be empty");
        return nullptr;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "gssapi acceptor verifyMic")) {
        return nullptr;
    }

    gss_buffer_desc message = GSS_C_EMPTY_BUFFER;
    message.value = message_bytes.empty() ? nullptr : message_bytes.data();
    message.length = message_bytes.size();

    gss_buffer_desc mic = GSS_C_EMPTY_BUFFER;
    mic.value = mic_bytes.data();
    mic.length = mic_bytes.size();

    gss_qop_t qop_state = 0;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_verify_mic(&min_stat, ctx, &message, &mic, &qop_state);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "verifying GSSAPI acceptor MIC");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGssMicVerifyInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("qop", (int64)qop_state, xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

int64 QoreGssAcceptorContext::getWrapSizeLimit(int64 output_size, bool confidential, int qop, ExceptionSink* xsink) {
    if (output_size < 0 || output_size > UINT32_MAX) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "output size must be between 0 and %u bytes", UINT32_MAX);
        return 0;
    }
    if (qop < 0) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "qop value cannot be negative");
        return 0;
    }
    if (ctx == GSS_C_NO_CONTEXT || !complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return 0;
    }
    if (qore_check_cancel(xsink, "checking GSSAPI acceptor wrap size limit")) {
        return 0;
    }

    OM_uint32 max_input_size = 0;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_wrap_size_limit(&min_stat, ctx, confidential ? 1 : 0, static_cast<gss_qop_t>(qop),
        static_cast<OM_uint32>(output_size), &max_input_size);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "checking GSSAPI acceptor wrap size limit");
        return 0;
    }
    return max_input_size;
}

QoreGssCredential* QoreGssAcceptorContext::getDelegatedCredential(ExceptionSink* xsink) {
    if (!complete) {
        xsink->raiseException("KRB5-GSS-STATE-ERROR", "GSSAPI context is not complete");
        return nullptr;
    }
    if (delegated_cred == GSS_C_NO_CREDENTIAL) {
        xsink->raiseException("KRB5-GSS-DELEGATION-ERROR",
            "no delegated credential available; the initiator may not have set GSS_C_DELEG_FLAG");
        return nullptr;
    }

    // Transfer ownership to the returned QoreGssCredential
    QoreGssCredential* rv = new QoreGssCredential(delegated_cred);
    delegated_cred = GSS_C_NO_CREDENTIAL;
    return rv;
}

QoreGssCredential::QoreGssCredential(gss_cred_id_t existing_cred) : cred(existing_cred) {
}

QoreGssCredential::QoreGssCredential(const QoreKrb5CredentialCache& cache, ExceptionSink* xsink) {
    if (!krb5_check_cache_access(cache.ctx, cache.cache, QSEC_READ, xsink, "importing GSSAPI credential from cache")) {
        return;
    }

    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_krb5_import_cred(&min_stat, cache.cache, nullptr, nullptr, &cred);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat,
            "importing GSSAPI credential from credential cache");
        return;
    }
}

QoreGssCredential::QoreGssCredential(const QoreKrb5Keytab& keytab, const QoreKrb5Principal* principal,
        ExceptionSink* xsink) {
    if (!krb5_check_keytab_access(keytab.ctx, keytab.keytab, QSEC_READ, xsink,
            "importing GSSAPI credential from keytab")) {
        return;
    }

    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_krb5_import_cred(&min_stat, nullptr, principal ? principal->principal : nullptr,
        keytab.keytab, &cred);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "importing GSSAPI credential from keytab");
        return;
    }
}

QoreGssCredential::QoreGssCredential(const QoreGssCredential& impersonator, const QoreKrb5Principal& user,
        ExceptionSink* xsink) {
#ifdef HAVE_GSS_ACQUIRE_CRED_IMPERSONATE_NAME
    if (impersonator.cred == GSS_C_NO_CREDENTIAL) {
        xsink->raiseException("KRB5-GSS-ARG-ERROR", "impersonator credential is not valid");
        return;
    }
    if (qore_check_cancel(xsink, "acquiring S4U2Self impersonated credential")) {
        return;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink, "acquiring S4U2Self impersonated credential")) {
        return;
    }

    // Import the user principal as a gss_name_t
    QoreStringNodeHolder princ_str(user.toString(xsink));
    if (*xsink) {
        return;
    }

    gss_buffer_desc name_buf = GSS_C_EMPTY_BUFFER;
    name_buf.value = const_cast<void*>(static_cast<const void*>(princ_str->c_str()));
    name_buf.length = princ_str->size();

    gss_name_t user_name = GSS_C_NO_NAME;
    OM_uint32 min_stat = 0;
    OM_uint32 maj = gss_import_name(&min_stat, &name_buf, GSS_KRB5_NT_PRINCIPAL_NAME, &user_name);
    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat, "importing user principal for S4U2Self");
        return;
    }

    gss_cred_id_t impersonated_cred = GSS_C_NO_CREDENTIAL;
    maj = gss_acquire_cred_impersonate_name(&min_stat, impersonator.cred, user_name, 0,
        GSS_C_NO_OID_SET, GSS_C_INITIATE, &impersonated_cred, nullptr, nullptr);

    OM_uint32 release_min = 0;
    gss_release_name(&release_min, &user_name);

    if (maj != GSS_S_COMPLETE) {
        gss_raise_exception(xsink, "KRB5-GSS-ERROR", maj, min_stat,
            "acquiring S4U2Self impersonated credential");
        return;
    }

    cred = impersonated_cred;
#else
    xsink->raiseException("KRB5-NOT-SUPPORTED",
        "S4U2Self requires gss_acquire_cred_impersonate_name (MIT krb5 1.8 or later)");
#endif
}

QoreGssCredential::~QoreGssCredential() {
    if (cred != GSS_C_NO_CREDENTIAL) {
        OM_uint32 min_stat = 0;
        gss_release_cred(&min_stat, &cred);
    }
}

QoreKrb5CredentialCache::QoreKrb5CredentialCache(const char* cache_name, bool use_default, ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }

    if (use_default) {
        rc = krb5_cc_default(ctx, &cache);
        if (rc) {
            krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "opening default credential cache");
        }
        return;
    }

    if (!cache_name || !*cache_name) {
        xsink->raiseException("KRB5-CACHE-ERROR", "credential cache name cannot be empty");
        return;
    }

    rc = krb5_cc_resolve(ctx, cache_name, &cache);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "resolving credential cache");
    }
}

QoreKrb5CredentialCache::~QoreKrb5CredentialCache() {
    if (cache) {
        krb5_cc_close(ctx, cache);
    }
    if (ctx) {
        krb5_free_context(ctx);
    }
}

QoreStringNode* QoreKrb5CredentialCache::getName() const {
    return new QoreStringNode(krb5_cc_get_name(ctx, cache));
}

QoreStringNode* QoreKrb5CredentialCache::getType() const {
    return new QoreStringNode(krb5_cc_get_type(ctx, cache));
}

QoreStringNode* QoreKrb5CredentialCache::getFullName(ExceptionSink* xsink) const {
    char* full_name = nullptr;
    krb5_error_code rc = krb5_cc_get_full_name(ctx, cache, &full_name);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "getting credential cache full name");
        return nullptr;
    }

    QoreStringNode* rv = new QoreStringNode(full_name);
    krb5_free_string(ctx, full_name);
    return rv;
}

int QoreKrb5CredentialCache::initialize(const QoreKrb5Principal& principal, ExceptionSink* xsink) {
    if (!krb5_check_cache_access(ctx, cache, QSEC_WRITE | QSEC_CREATE, xsink, "initializing credential cache")) {
        return -1;
    }

    krb5_error_code rc = krb5_cc_initialize(ctx, cache, principal.principal);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "initializing credential cache");
    }
    return 0;
}

bool QoreKrb5CredentialCache::hasPrimaryPrincipal(ExceptionSink* xsink) const {
    if (!krb5_check_cache_access(ctx, cache, QSEC_READ, xsink, "reading credential cache primary principal")) {
        return false;
    }

    krb5_principal principal = nullptr;
    krb5_error_code rc = krb5_cc_get_principal(ctx, cache, &principal);
    if (!rc) {
        krb5_free_principal(ctx, principal);
        return true;
    }
    if (krb5_is_empty_cache_error(rc)) {
        return false;
    }
    krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "reading credential cache primary principal");
    return false;
}

QoreKrb5Principal* QoreKrb5CredentialCache::getPrimaryPrincipal(ExceptionSink* xsink) const {
    if (!krb5_check_cache_access(ctx, cache, QSEC_READ, xsink, "reading credential cache primary principal")) {
        return nullptr;
    }

    krb5_principal principal = nullptr;
    krb5_error_code rc = krb5_cc_get_principal(ctx, cache, &principal);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "reading credential cache primary principal");
        return nullptr;
    }

    SimpleRefHolder<QoreKrb5Principal> rv(new QoreKrb5Principal(ctx, principal, xsink));
    krb5_free_principal(ctx, principal);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

int QoreKrb5CredentialCache::storeCredentials(const QoreKrb5Credentials& c, ExceptionSink* xsink) {
    if (!krb5_check_cache_access(ctx, cache, QSEC_WRITE | QSEC_CREATE, xsink, "storing credentials")) {
        return -1;
    }

    // Copy credentials into this cache's own krb5 context to avoid cross-context use;
    // MIT krb5 APIs that accept krb5_creds expect the caller to own them in the same context.
    krb5_creds* local_creds = nullptr;
    krb5_error_code rc = krb5_copy_creds(ctx, c.creds, &local_creds);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "copying credentials");
    }

    rc = krb5_cc_store_cred(ctx, cache, local_creds);
    krb5_free_creds(ctx, local_creds);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "storing credentials");
    }
    return 0;
}

QoreListNode* QoreKrb5CredentialCache::listCredentials(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "listing credential cache")) {
        return nullptr;
    }
    if (!krb5_check_cache_access(ctx, cache, QSEC_READ, xsink, "listing credentials")) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclKrb5CredentialsInfo->getTypeInfo()), xsink);
    Krb5CredentialCacheCursorHolder cursor(ctx, cache);

    krb5_error_code rc = cursor.start();
    if (rc) {
        if (krb5_is_empty_cache_error(rc)) {
            return rv.release();
        }
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "starting credential cache enumeration");
        return nullptr;
    }

    while (true) {
        if (qore_check_cancel(xsink, "listing credential cache")) {
            return nullptr;
        }

        krb5_creds creds;
        memset(&creds, 0, sizeof(creds));
        rc = krb5_cc_next_cred(ctx, cache, &cursor.cursor, &creds);
        if (krb5_is_empty_cache_error(rc)) {
            break;
        }
        if (rc) {
            krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR", "enumerating credential cache");
            return nullptr;
        }

        SimpleRefHolder<QoreKrb5Credentials> c(new QoreKrb5Credentials(ctx, creds, xsink));
        krb5_free_cred_contents(ctx, &creds);
        if (*xsink) {
            return nullptr;
        }

        ReferenceHolder<QoreHashNode> info(c->getInfo(xsink), xsink);
        if (*xsink) {
            return nullptr;
        }

        rv->push(info.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    cursor.close();
    return rv.release();
}

static int krb5_set_data_from_hex(krb5_data& data, const char* data_hex, ExceptionSink* xsink, const char* context) {
    std::vector<unsigned char> bytes;
    if (!decode_hex(data_hex, bytes, xsink, "KRB5-CREDENTIALS-ERROR", context)) {
        return -1;
    }
    if (bytes.empty()) {
        xsink->raiseException("KRB5-CREDENTIALS-ERROR", "%s: data cannot be empty", context);
        return -1;
    }

    data.data = static_cast<char*>(malloc(bytes.size()));
    if (!data.data) {
        xsink->raiseException("KRB5-CREDENTIALS-ERROR", "%s: memory allocation failed", context);
        return -1;
    }

    memcpy(data.data, bytes.data(), bytes.size());
    data.length = bytes.size();
    return 0;
}

QoreKrb5Credentials::QoreKrb5Credentials(const QoreKrb5Principal& client, const QoreKrb5Principal& server,
        const char* session_key_hex, krb5_enctype enctype, krb5_timestamp start_time, krb5_timestamp end_time,
        krb5_timestamp renew_until, krb5_flags flags, const char* ticket_hex, ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }

    creds = static_cast<krb5_creds*>(calloc(1, sizeof(*creds)));
    if (!creds) {
        xsink->raiseException("KRB5-CREDENTIALS-ERROR", "memory allocation failed");
        return;
    }

    rc = krb5_copy_principal(ctx, client.principal, &creds->client);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CREDENTIALS-ERROR", "copying client principal");
        return;
    }

    rc = krb5_copy_principal(ctx, server.principal, &creds->server);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CREDENTIALS-ERROR", "copying server principal");
        return;
    }

    SecureBytes key_data;
    if (!decode_hex(session_key_hex, key_data, xsink, "KRB5-CREDENTIALS-ERROR", "decoding session key")) {
        return;
    }
    if (key_data.empty()) {
        xsink->raiseException("KRB5-CREDENTIALS-ERROR", "session key cannot be empty");
        return;
    }

    krb5_keyblock* keyblock = nullptr;
    rc = krb5_init_keyblock(ctx, enctype, key_data.size(), &keyblock);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CREDENTIALS-ERROR", "allocating session keyblock");
        return;
    }

    memcpy(keyblock->contents, key_data.data(), key_data.size());
    creds->keyblock = *keyblock;
    keyblock->contents = nullptr;
    keyblock->length = 0;
    krb5_free_keyblock(ctx, keyblock);

    creds->times.starttime = start_time;
    creds->times.authtime = start_time;
    creds->times.endtime = end_time;
    creds->times.renew_till = renew_until;
    creds->ticket_flags = flags;

    if (krb5_set_data_from_hex(creds->ticket, ticket_hex, xsink, "decoding ticket data")) {
        return;
    }
}

QoreKrb5Credentials::QoreKrb5Credentials(krb5_context source_ctx, const krb5_creds& source_creds,
        ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }

    rc = krb5_copy_creds(source_ctx, &source_creds, &creds);
    if (rc) {
        krb5_raise_exception(xsink, source_ctx, rc, "KRB5-CREDENTIALS-ERROR", "copying credentials");
    }
}

QoreKrb5Credentials::~QoreKrb5Credentials() {
    if (creds) {
        krb5_free_creds(ctx, creds);
    }
    if (ctx) {
        krb5_free_context(ctx);
    }
}

QoreHashNode* QoreKrb5Credentials::getInfo(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclKrb5CredentialsInfo, xsink), xsink);

    ReferenceHolder<QoreStringNode> client(
        krb5_unparse_principal(ctx, creds->client, xsink, "KRB5-CREDENTIALS-ERROR", "rendering client principal"), xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreStringNode> server(
        krb5_unparse_principal(ctx, creds->server, xsink, "KRB5-CREDENTIALS-ERROR", "rendering server principal"), xsink);
    if (*xsink) {
        return nullptr;
    }

    char enctype_name[128] = {0};
    if (krb5_enctype_to_name(creds->keyblock.enctype, false, enctype_name, sizeof(enctype_name))) {
        snprintf(enctype_name, sizeof(enctype_name), "etype-%d", static_cast<int>(creds->keyblock.enctype));
    }

    rv->setKeyValue("client", client.release(), xsink);
    rv->setKeyValue("server", server.release(), xsink);
    rv->setKeyValue("enctype", (int64)creds->keyblock.enctype, xsink);
    rv->setKeyValue("enctype_name", new QoreStringNode(enctype_name), xsink);
    rv->setKeyValue("start_time", (int64)creds->times.starttime, xsink);
    rv->setKeyValue("end_time", (int64)creds->times.endtime, xsink);
    rv->setKeyValue("renew_until", (int64)creds->times.renew_till, xsink);
    rv->setKeyValue("ticket_flags", (int64)creds->ticket_flags, xsink);
    rv->setKeyValue("is_skey", (bool)creds->is_skey, xsink);
    rv->setKeyValue("ticket_size", (int64)creds->ticket.length, xsink);
    if (*xsink) {
        return nullptr;
    }

    return rv.release();
}

QoreKrb5Principal* QoreKrb5Credentials::getClientPrincipal(ExceptionSink* xsink) const {
    return new QoreKrb5Principal(ctx, creds->client, xsink);
}

QoreKrb5Principal* QoreKrb5Credentials::getServerPrincipal(ExceptionSink* xsink) const {
    return new QoreKrb5Principal(ctx, creds->server, xsink);
}

QoreKrb5Context::QoreKrb5Context(ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
    }
}

QoreKrb5Context::~QoreKrb5Context() {
    if (ctx) {
        krb5_free_context(ctx);
    }
}

QoreStringNode* QoreKrb5Context::getDefaultRealm(ExceptionSink* xsink) const {
    char* realm = nullptr;
    krb5_error_code rc = krb5_get_default_realm(ctx, &realm);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-REALM-ERROR", "getting default kerberos realm");
        return nullptr;
    }

    QoreStringNode* rv = new QoreStringNode(realm);
    krb5_free_default_realm(ctx, realm);
    return rv;
}

QoreStringNode* QoreKrb5Context::getDefaultCredentialCacheName(ExceptionSink* xsink) const {
    const char* name = krb5_cc_default_name(ctx);
    if (!name || !*name) {
        xsink->raiseException("KRB5-CACHE-ERROR", "the default credential cache name is not available");
        return nullptr;
    }

    return new QoreStringNode(name);
}

QoreStringNode* QoreKrb5Context::getDefaultKeytabName(ExceptionSink* xsink) const {
    char name[MAX_KEYTAB_NAME_LEN] = {0};
    krb5_error_code rc = krb5_kt_default_name(ctx, name, sizeof(name));
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "getting default keytab name");
        return nullptr;
    }

    return new QoreStringNode(name);
}

QoreKrb5CredentialCache* QoreKrb5Context::openCredentialCache(const char* cache_name, ExceptionSink* xsink) const {
    return new QoreKrb5CredentialCache(cache_name, false, xsink);
}

QoreKrb5CredentialCache* QoreKrb5Context::openDefaultCredentialCache(ExceptionSink* xsink) const {
    return new QoreKrb5CredentialCache(nullptr, true, xsink);
}

QoreKrb5Keytab* QoreKrb5Context::openKeytab(const char* keytab_name, ExceptionSink* xsink) const {
    return new QoreKrb5Keytab(keytab_name, false, xsink);
}

QoreKrb5Keytab* QoreKrb5Context::openDefaultKeytab(ExceptionSink* xsink) const {
    return new QoreKrb5Keytab(nullptr, true, xsink);
}

QoreKrb5CredentialCache* QoreKrb5Context::createMemoryCredentialCache(const QoreKrb5Principal& principal,
        const char* cache_name, ExceptionSink* xsink) const {
    if (!cache_name || !*cache_name) {
        xsink->raiseException("KRB5-CACHE-ERROR", "memory credential cache name cannot be empty");
        return nullptr;
    }

    std::string full_name = "MEMORY:";
    full_name += cache_name;
    SimpleRefHolder<QoreKrb5CredentialCache> cache(new QoreKrb5CredentialCache(full_name.c_str(), false, xsink));
    if (*xsink) {
        return nullptr;
    }
    if (cache->initialize(principal, xsink)) {
        return nullptr;
    }
    return cache.release();
}

class QoreKrb5InitCredsOptions {
public:
    krb5_context kctx = nullptr;
    krb5_get_init_creds_opt* opt = nullptr;
    krb5_deltat start_time = 0;
    std::string service;
    std::vector<krb5_enctype> enctypes;
    std::vector<krb5_ccache> ccaches;

    DLLLOCAL QoreKrb5InitCredsOptions(krb5_context ctx, const QoreHashNode* opts_hash, ExceptionSink* xsink)
            : kctx(ctx) {
        krb5_error_code rc = krb5_get_init_creds_opt_alloc(ctx, &opt);
        if (rc) {
            krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR",
                "allocating initial credential options");
            return;
        }

        // Enterprise service usage is noninteractive; never prompt unless explicitly requested.
        krb5_get_init_creds_opt_set_change_password_prompt(opt, 0);

        if (!opts_hash) {
            return;
        }

        parse(ctx, opts_hash, xsink);
    }

    DLLLOCAL ~QoreKrb5InitCredsOptions() {
        if (!kctx) {
            return;
        }
        if (opt) {
            krb5_get_init_creds_opt_free(kctx, opt);
        }
        for (krb5_ccache ccache : ccaches) {
            krb5_cc_close(kctx, ccache);
        }
    }

    DLLLOCAL const char* serviceName() const {
        return service.empty() ? nullptr : service.c_str();
    }

private:
    DLLLOCAL static bool hasOption(const QoreHashNode* h, const char* key, QoreValue& v) {
        v = h->getKeyValue(key);
        return !v.isNullOrNothing();
    }

    DLLLOCAL static bool getStringOption(const QoreHashNode* h, const char* key, std::string& out,
            ExceptionSink* xsink) {
        QoreValue v;
        if (!hasOption(h, key, v)) {
            return false;
        }

        QoreStringValueHelper str(v, QCS_UTF8, xsink);
        if (*xsink) {
            return false;
        }
        if (!str->c_str() || !*str->c_str()) {
            xsink->raiseException("KRB5-INIT-CREDS-ERROR", "option '%s' cannot be empty", key);
            return false;
        }
        out = str->c_str();
        return true;
    }

    DLLLOCAL static bool getDeltatOption(const QoreHashNode* h, const char* key, krb5_deltat& out,
            ExceptionSink* xsink) {
        QoreValue v;
        if (!hasOption(h, key, v)) {
            return false;
        }

        int64 value = v.getAsBigInt();
        if (value < 0 || value > INT32_MAX) {
            xsink->raiseException("KRB5-INIT-CREDS-ERROR",
                "option '%s' must be between 0 and %d seconds", key, INT32_MAX);
            return false;
        }
        out = static_cast<krb5_deltat>(value);
        return true;
    }

    DLLLOCAL static bool getBoolOption(const QoreHashNode* h, const char* key, bool& out) {
        QoreValue v;
        if (!hasOption(h, key, v)) {
            return false;
        }
        out = v.getAsBool();
        return true;
    }

    DLLLOCAL int addCcacheOption(krb5_context ctx, const char* key, const char* name, int access_mode,
            ExceptionSink* xsink) {
        krb5_ccache ccache = nullptr;
        krb5_error_code rc = krb5_cc_resolve(ctx, name, &ccache);
        if (rc) {
            return krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR",
                "resolving credential cache option");
        }

        if (!krb5_check_cache_access(ctx, ccache, access_mode, xsink, "using initial-credential cache option")) {
            krb5_cc_close(ctx, ccache);
            return -1;
        }

        if (!strcmp(key, "fast_ccache_name")) {
            rc = krb5_get_init_creds_opt_set_fast_ccache(ctx, opt, ccache);
        } else if (!strcmp(key, "in_ccache_name")) {
            rc = krb5_get_init_creds_opt_set_in_ccache(ctx, opt, ccache);
        } else {
            rc = krb5_get_init_creds_opt_set_out_ccache(ctx, opt, ccache);
        }
        if (rc) {
            krb5_cc_close(ctx, ccache);
            return krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR",
                "setting credential cache option");
        }

        ccaches.push_back(ccache);
        return 0;
    }

    DLLLOCAL void parse(krb5_context ctx, const QoreHashNode* h, ExceptionSink* xsink) {
        getStringOption(h, "service", service, xsink);
        if (*xsink) {
            return;
        }

        getDeltatOption(h, "start_time", start_time, xsink);
        if (*xsink) {
            return;
        }

        krb5_deltat delta = 0;
        if (getDeltatOption(h, "ticket_lifetime", delta, xsink)) {
            krb5_get_init_creds_opt_set_tkt_life(opt, delta);
        }
        if (*xsink) {
            return;
        }
        if (getDeltatOption(h, "renew_lifetime", delta, xsink)) {
            krb5_get_init_creds_opt_set_renew_life(opt, delta);
        }
        if (*xsink) {
            return;
        }

        bool b = false;
        if (getBoolOption(h, "forwardable", b)) {
            krb5_get_init_creds_opt_set_forwardable(opt, b ? 1 : 0);
        }
        if (getBoolOption(h, "proxiable", b)) {
            krb5_get_init_creds_opt_set_proxiable(opt, b ? 1 : 0);
        }
        if (getBoolOption(h, "canonicalize", b)) {
            krb5_get_init_creds_opt_set_canonicalize(opt, b ? 1 : 0);
        }
        if (getBoolOption(h, "anonymous", b)) {
            krb5_get_init_creds_opt_set_anonymous(opt, b ? 1 : 0);
        }
        if (getBoolOption(h, "change_password_prompt", b)) {
            krb5_get_init_creds_opt_set_change_password_prompt(opt, b ? 1 : 0);
        }

        QoreValue v;
        if (hasOption(h, "pac_request", v)) {
            krb5_error_code rc = krb5_get_init_creds_opt_set_pac_request(ctx, opt, v.getAsBool() ? 1 : 0);
            if (rc) {
                krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR", "setting PAC request option");
                return;
            }
        }
        if (hasOption(h, "fast_required", v) && v.getAsBool()) {
            krb5_error_code rc = krb5_get_init_creds_opt_set_fast_flags(ctx, opt, KRB5_FAST_REQUIRED);
            if (rc) {
                krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR", "setting FAST required option");
                return;
            }
        }

        std::string ccache_name;
        if (getStringOption(h, "fast_ccache_name", ccache_name, xsink)) {
            if (addCcacheOption(ctx, "fast_ccache_name", ccache_name.c_str(), QSEC_READ, xsink)) {
                return;
            }
        }
        if (*xsink) {
            return;
        }
        ccache_name.clear();
        if (getStringOption(h, "in_ccache_name", ccache_name, xsink)) {
            if (addCcacheOption(ctx, "in_ccache_name", ccache_name.c_str(), QSEC_READ, xsink)) {
                return;
            }
        }
        if (*xsink) {
            return;
        }
        ccache_name.clear();
        if (getStringOption(h, "out_ccache_name", ccache_name, xsink)) {
            if (addCcacheOption(ctx, "out_ccache_name", ccache_name.c_str(), QSEC_WRITE | QSEC_CREATE, xsink)) {
                return;
            }
        }
        if (*xsink) {
            return;
        }

        if (hasOption(h, "enctypes", v)) {
            const QoreListNode* l = v.get<const QoreListNode>();
            if (!l || l->empty()) {
                xsink->raiseException("KRB5-INIT-CREDS-ERROR", "option 'enctypes' cannot be empty");
                return;
            }
            enctypes.reserve(l->size());
            for (size_t i = 0; i < l->size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "parsing krb5 enctypes option")) {
                    return;
                }

                int64 etype = l->retrieveEntry(i).getAsBigInt();
                if (etype <= 0 || etype > INT32_MAX) {
                    xsink->raiseException("KRB5-INIT-CREDS-ERROR",
                        "option 'enctypes' has invalid enctype value at index %d", static_cast<int>(i));
                    return;
                }
                enctypes.push_back(static_cast<krb5_enctype>(etype));
            }

            krb5_get_init_creds_opt_set_etype_list(opt, enctypes.data(), enctypes.size());
        }
    }
};

QoreKrb5Credentials* QoreKrb5Context::acquireCredentialsWithPassword(const QoreKrb5Principal& principal,
        const char* password, const QoreHashNode* opts_hash, ExceptionSink* xsink) const {
    if (!password || !*password) {
        xsink->raiseException("KRB5-INIT-CREDS-ERROR", "password cannot be empty");
        return nullptr;
    }
    if (qore_check_cancel(xsink, "acquiring initial credentials with password")) {
        return nullptr;
    }

    QoreKrb5InitCredsOptions opts(ctx, opts_hash, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink,
            "acquiring initial credentials with password")) {
        return nullptr;
    }

    krb5_creds creds;
    memset(&creds, 0, sizeof(creds));
    krb5_error_code rc = krb5_get_init_creds_password(ctx, &creds, principal.principal,
        const_cast<char*>(password), nullptr, nullptr, opts.start_time, opts.serviceName(), opts.opt);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR",
            "acquiring initial credentials with password");
        return nullptr;
    }

    SimpleRefHolder<QoreKrb5Credentials> rv(new QoreKrb5Credentials(ctx, creds, xsink));
    krb5_free_cred_contents(ctx, &creds);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreKrb5Credentials* QoreKrb5Context::acquireCredentialsWithKeytab(const QoreKrb5Principal& principal,
        const QoreKrb5Keytab& keytab, const QoreHashNode* opts_hash, ExceptionSink* xsink) const {
    if (!krb5_check_keytab_access(keytab.ctx, keytab.keytab, QSEC_READ, xsink,
            "acquiring initial credentials with keytab")) {
        return nullptr;
    }
    if (qore_check_cancel(xsink, "acquiring initial credentials with keytab")) {
        return nullptr;
    }

    QoreKrb5InitCredsOptions opts(ctx, opts_hash, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink,
            "acquiring initial credentials with keytab")) {
        return nullptr;
    }

    krb5_creds creds;
    memset(&creds, 0, sizeof(creds));
    krb5_error_code rc = krb5_get_init_creds_keytab(ctx, &creds, principal.principal, keytab.keytab,
        opts.start_time, opts.serviceName(), opts.opt);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-INIT-CREDS-ERROR",
            "acquiring initial credentials with keytab");
        return nullptr;
    }

    SimpleRefHolder<QoreKrb5Credentials> rv(new QoreKrb5Credentials(ctx, creds, xsink));
    krb5_free_cred_contents(ctx, &creds);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreKrb5Credentials* QoreKrb5Context::renewCredentials(const QoreKrb5CredentialCache& cache,
        const QoreKrb5Principal& client, ExceptionSink* xsink) const {
    if (!krb5_check_cache_access(cache.ctx, cache.cache, QSEC_READ, xsink, "renewing credentials from cache")) {
        return nullptr;
    }
    if (qore_check_cancel(xsink, "renewing credentials")) {
        return nullptr;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink, "renewing credentials")) {
        return nullptr;
    }

    Krb5PrincipalHolder client_principal(cache.ctx);
    krb5_error_code rc = krb5_copy_principal(cache.ctx, client.principal, client_principal.out());
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-RENEW-ERROR",
            "copying client principal for credential renewal");
        return nullptr;
    }

    krb5_creds creds;
    memset(&creds, 0, sizeof(creds));
    Krb5CredsContentsHolder creds_holder(cache.ctx, &creds);
    rc = krb5_get_renewed_creds(cache.ctx, &creds, client_principal.principal, cache.cache, nullptr);
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-RENEW-ERROR", "renewing credentials");
        return nullptr;
    }

    SimpleRefHolder<QoreKrb5Credentials> rv(new QoreKrb5Credentials(cache.ctx, creds, xsink));
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreKrb5Credentials* QoreKrb5Context::acquireServiceCredentials(const QoreKrb5CredentialCache& cache,
        const QoreKrb5Principal& service, ExceptionSink* xsink) const {
    if (!krb5_check_cache_access(cache.ctx, cache.cache, QSEC_READ | QSEC_WRITE, xsink,
            "acquiring service credentials from cache")) {
        return nullptr;
    }
    if (qore_check_cancel(xsink, "acquiring service credentials")) {
        return nullptr;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink, "acquiring service credentials")) {
        return nullptr;
    }

    krb5_creds in_creds;
    memset(&in_creds, 0, sizeof(in_creds));
    Krb5CredsContentsHolder in_creds_holder(cache.ctx, &in_creds);

    // Get the cache's primary principal as the client
    Krb5PrincipalHolder cache_principal(cache.ctx);
    krb5_error_code rc = krb5_cc_get_principal(cache.ctx, cache.cache, cache_principal.out());
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-SERVICE-CREDS-ERROR",
            "reading cache principal for TGS request");
        return nullptr;
    }

    in_creds.client = cache_principal.release();

    rc = krb5_copy_principal(cache.ctx, service.principal, &in_creds.server);
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-SERVICE-CREDS-ERROR",
            "copying service principal for TGS request");
        return nullptr;
    }

    krb5_creds* out_creds = nullptr;
    rc = krb5_get_credentials(cache.ctx, 0, cache.cache, &in_creds, &out_creds);
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-SERVICE-CREDS-ERROR",
            "acquiring service credentials via TGS");
        return nullptr;
    }

    SimpleRefHolder<QoreKrb5Credentials> rv(new QoreKrb5Credentials(cache.ctx, *out_creds, xsink));
    krb5_free_creds(cache.ctx, out_creds);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

QoreKrb5Credentials* QoreKrb5Context::acquireS4U2ProxyCredentials(const QoreKrb5CredentialCache& cache,
        const QoreKrb5Credentials& evidence, const QoreKrb5Principal& target, ExceptionSink* xsink) const {
#ifdef HAVE_KRB5_GC_CONSTRAINED_DELEGATION
    if (!krb5_check_cache_access(cache.ctx, cache.cache, QSEC_READ | QSEC_WRITE, xsink,
            "acquiring S4U2Proxy credentials from cache")) {
        return nullptr;
    }
    if (qore_check_cancel(xsink, "acquiring S4U2Proxy credentials")) {
        return nullptr;
    }
    if (!krb5_network_sandbox_allows_unmanaged_library_io(xsink, "acquiring S4U2Proxy credentials")) {
        return nullptr;
    }

    krb5_creds in_creds;
    memset(&in_creds, 0, sizeof(in_creds));
    Krb5CredsContentsHolder in_creds_holder(cache.ctx, &in_creds);

    // Get the cache's primary principal as the client
    Krb5PrincipalHolder cache_principal(cache.ctx);
    krb5_error_code rc = krb5_cc_get_principal(cache.ctx, cache.cache, cache_principal.out());
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-S4U-PROXY-ERROR",
            "reading cache principal for S4U2Proxy request");
        return nullptr;
    }

    in_creds.client = cache_principal.release();

    rc = krb5_copy_principal(cache.ctx, target.principal, &in_creds.server);
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-S4U-PROXY-ERROR",
            "copying target principal for S4U2Proxy request");
        return nullptr;
    }

    // Copy the evidence ticket into second_ticket
    if (evidence.creds && evidence.creds->ticket.length > 0) {
        krb5_data* ticket_copy = nullptr;
        rc = krb5_copy_data(cache.ctx, &evidence.creds->ticket, &ticket_copy);
        if (rc) {
            krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-S4U-PROXY-ERROR",
                "copying evidence ticket for S4U2Proxy request");
            return nullptr;
        }
        in_creds.second_ticket = *ticket_copy;
        // Free the outer krb5_data struct but keep the copied data buffer
        // which is now owned by in_creds (freed by krb5_free_cred_contents)
        free(ticket_copy);
    } else {
        xsink->raiseException("KRB5-S4U-PROXY-ERROR", "evidence ticket is empty or invalid");
        return nullptr;
    }

    krb5_creds* out_creds = nullptr;
    rc = krb5_get_credentials(cache.ctx, KRB5_GC_CONSTRAINED_DELEGATION, cache.cache, &in_creds, &out_creds);
    if (rc) {
        krb5_raise_exception(xsink, cache.ctx, rc, "KRB5-S4U-PROXY-ERROR",
            "acquiring S4U2Proxy credentials");
        return nullptr;
    }

    SimpleRefHolder<QoreKrb5Credentials> rv(new QoreKrb5Credentials(cache.ctx, *out_creds, xsink));
    krb5_free_creds(cache.ctx, out_creds);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
#else
    xsink->raiseException("KRB5-NOT-SUPPORTED",
        "S4U2Proxy requires KRB5_GC_CONSTRAINED_DELEGATION (MIT krb5 1.8 or later)");
    return nullptr;
#endif
}

QoreListNode* QoreKrb5Context::listCredentialCaches(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "listing credential caches")) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclKrb5CredentialCacheInfo->getTypeInfo()), xsink);
    Krb5CccolCursorHolder cursor(ctx);

    krb5_error_code rc = cursor.start();
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR",
            "starting credential cache collection enumeration");
        return nullptr;
    }

    while (true) {
        if (qore_check_cancel(xsink, "listing credential caches")) {
            return nullptr;
        }

        krb5_ccache cc = nullptr;
        rc = krb5_cccol_cursor_next(ctx, cursor.cursor, &cc);
        if (rc) {
            krb5_raise_exception(xsink, ctx, rc, "KRB5-CACHE-ERROR",
                "enumerating credential cache collection");
            return nullptr;
        }
        if (!cc) {
            break;
        }

        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclKrb5CredentialCacheInfo, xsink), xsink);
        if (*xsink) {
            krb5_cc_close(ctx, cc);
            return nullptr;
        }

        const char* type = krb5_cc_get_type(ctx, cc);
        const char* name = krb5_cc_get_name(ctx, cc);
        bool accessible = krb5_check_cache_access(ctx, cc, QSEC_READ, xsink,
            "listing credential cache collection entry");
        if (!accessible) {
            xsink->clear();
        }

        info->setKeyValue("type", new QoreStringNode(type ? type : ""), xsink);
        info->setKeyValue("accessible", accessible, xsink);

        if (!accessible) {
            info->setKeyValue("name", new QoreStringNode(""), xsink);
            info->setKeyValue("full_name", new QoreStringNode(""), xsink);
            info->setKeyValue("has_principal", false, xsink);
            info->setKeyValue("principal", QoreValue(), xsink);
        } else {
            info->setKeyValue("name", new QoreStringNode(name ? name : ""), xsink);

            char* full_name = nullptr;
            rc = krb5_cc_get_full_name(ctx, cc, &full_name);
            if (!rc && full_name) {
                info->setKeyValue("full_name", new QoreStringNode(full_name), xsink);
                krb5_free_string(ctx, full_name);
            } else {
                info->setKeyValue("full_name", new QoreStringNode(""), xsink);
            }

            krb5_principal princ = nullptr;
            rc = krb5_cc_get_principal(ctx, cc, &princ);
            if (!rc && princ) {
                info->setKeyValue("has_principal", true, xsink);
                QoreStringNode* princ_str = krb5_unparse_principal(ctx, princ, xsink, "KRB5-CACHE-ERROR",
                    "rendering credential cache principal");
                krb5_free_principal(ctx, princ);
                if (*xsink) {
                    xsink->clear();
                    info->setKeyValue("has_principal", false, xsink);
                    info->setKeyValue("principal", QoreValue(), xsink);
                } else {
                    info->setKeyValue("principal", princ_str, xsink);
                }
            } else {
                info->setKeyValue("has_principal", false, xsink);
                info->setKeyValue("principal", QoreValue(), xsink);
            }
        }

        krb5_cc_close(ctx, cc);

        if (*xsink) {
            return nullptr;
        }
        rv->push(info.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return rv.release();
}

QoreKrb5Keytab::QoreKrb5Keytab(const char* keytab_name, bool use_default, ExceptionSink* xsink) {
    krb5_error_code rc = krb5_init_context(&ctx);
    if (rc) {
        krb5_raise_exception(xsink, nullptr, rc, "KRB5-INIT-ERROR", "initializing kerberos context");
        return;
    }

    if (use_default) {
        rc = krb5_kt_default(ctx, &keytab);
        if (rc) {
            krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "opening default keytab");
        }
        return;
    }

    if (!keytab_name || !*keytab_name) {
        xsink->raiseException("KRB5-KEYTAB-ERROR", "keytab name cannot be empty");
        return;
    }

    rc = krb5_kt_resolve(ctx, keytab_name, &keytab);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "resolving keytab");
    }
}

QoreKrb5Keytab::~QoreKrb5Keytab() {
    if (keytab) {
        krb5_kt_close(ctx, keytab);
    }
    if (ctx) {
        krb5_free_context(ctx);
    }
}

QoreStringNode* QoreKrb5Keytab::getName(ExceptionSink* xsink) const {
    char name[MAX_KEYTAB_NAME_LEN] = {0};
    krb5_error_code rc = krb5_kt_get_name(ctx, keytab, name, sizeof(name));
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "getting keytab name");
        return nullptr;
    }

    return new QoreStringNode(name);
}

QoreStringNode* QoreKrb5Keytab::getType() const {
    return new QoreStringNode(krb5_kt_get_type(ctx, keytab));
}

static QoreHashNode* krb5_keytab_entry_to_hash(krb5_context ctx, const krb5_keytab_entry& entry, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclKrb5KeytabEntryInfo, xsink), xsink);

    ReferenceHolder<QoreStringNode> principal(
        krb5_unparse_principal(ctx, entry.principal, xsink, "KRB5-KEYTAB-ERROR", "rendering keytab principal"), xsink);
    if (*xsink) {
        return nullptr;
    }

    char enctype_name[128] = {0};
    if (krb5_enctype_to_name(entry.key.enctype, false, enctype_name, sizeof(enctype_name))) {
        snprintf(enctype_name, sizeof(enctype_name), "etype-%d", static_cast<int>(entry.key.enctype));
    }

    rv->setKeyValue("principal", principal.release(), xsink);
    rv->setKeyValue("kvno", (int64)entry.vno, xsink);
    rv->setKeyValue("enctype", (int64)entry.key.enctype, xsink);
    rv->setKeyValue("enctype_name", new QoreStringNode(enctype_name), xsink);
    rv->setKeyValue("timestamp", (int64)entry.timestamp, xsink);
    rv->setKeyValue("key_size", (int64)entry.key.length, xsink);
    if (*xsink) {
        return nullptr;
    }

    return rv.release();
}

bool QoreKrb5Keytab::hasContent(ExceptionSink* xsink) const {
    if (!krb5_check_keytab_access(ctx, keytab, QSEC_READ, xsink, "checking keytab content")) {
        return false;
    }

    krb5_error_code rc = krb5_kt_have_content(ctx, keytab);
    if (!rc) {
        return true;
    }
    if (rc == KRB5_KT_NOTFOUND) {
        return false;
    }

    krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "checking keytab content");
    return false;
}

int QoreKrb5Keytab::addEntry(const QoreKrb5Principal& principal, const char* key_hex, krb5_enctype enctype,
        krb5_kvno kvno, ExceptionSink* xsink) {
    if (!krb5_check_keytab_access(ctx, keytab, QSEC_WRITE | QSEC_CREATE, xsink, "adding keytab entry")) {
        return -1;
    }

    SecureBytes key_data;
    if (!decode_hex(key_hex, key_data, xsink, "KRB5-KEYTAB-ERROR", "adding keytab entry")) {
        return -1;
    }
    if (key_data.empty()) {
        xsink->raiseException("KRB5-KEYTAB-ERROR", "adding keytab entry: key data cannot be empty");
        return -1;
    }

    krb5_keytab_entry entry;
    memset(&entry, 0, sizeof(entry));

    krb5_error_code rc = krb5_copy_principal(ctx, principal.principal, &entry.principal);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "copying keytab principal");
    }

    krb5_keyblock* keyblock = nullptr;
    rc = krb5_init_keyblock(ctx, enctype, key_data.size(), &keyblock);
    if (rc) {
        krb5_free_principal(ctx, entry.principal);
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "allocating keytab keyblock");
    }

    memcpy(keyblock->contents, key_data.data(), key_data.size());
    entry.key = *keyblock;
    keyblock->contents = nullptr;
    keyblock->length = 0;
    krb5_free_keyblock(ctx, keyblock);

    entry.vno = kvno;
    entry.timestamp = time(nullptr);

    rc = krb5_kt_add_entry(ctx, keytab, &entry);
    krb5_free_keytab_entry_contents(ctx, &entry);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "adding keytab entry");
    }

    return 0;
}

QoreHashNode* QoreKrb5Keytab::getEntry(const QoreKrb5Principal& principal, krb5_kvno kvno, krb5_enctype enctype,
        ExceptionSink* xsink) const {
    if (!krb5_check_keytab_access(ctx, keytab, QSEC_READ, xsink, "reading keytab entry")) {
        return nullptr;
    }

    krb5_keytab_entry entry;
    memset(&entry, 0, sizeof(entry));

    krb5_error_code rc = krb5_kt_get_entry(ctx, keytab, principal.principal, kvno, enctype, &entry);
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "reading keytab entry");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(krb5_keytab_entry_to_hash(ctx, entry, xsink), xsink);
    krb5_free_keytab_entry_contents(ctx, &entry);
    if (*xsink) {
        return nullptr;
    }

    return rv.release();
}

QoreListNode* QoreKrb5Keytab::listEntries(ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "listing keytab entries")) {
        return nullptr;
    }
    if (!krb5_check_keytab_access(ctx, keytab, QSEC_READ, xsink, "listing keytab entries")) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclKrb5KeytabEntryInfo->getTypeInfo()), xsink);
    Krb5KeytabCursorHolder cursor(ctx, keytab);

    krb5_error_code rc = cursor.start();
    if (rc) {
        krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "starting keytab enumeration");
        return nullptr;
    }

    while (true) {
        if (qore_check_cancel(xsink, "listing keytab entries")) {
            return nullptr;
        }

        krb5_keytab_entry entry;
        memset(&entry, 0, sizeof(entry));
        rc = krb5_kt_next_entry(ctx, keytab, &entry, &cursor.cursor);
        if (rc == KRB5_KT_END) {
            break;
        }
        if (rc) {
            krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "enumerating keytab entries");
            return nullptr;
        }

        ReferenceHolder<QoreHashNode> info(krb5_keytab_entry_to_hash(ctx, entry, xsink), xsink);
        krb5_free_keytab_entry_contents(ctx, &entry);
        if (*xsink) {
            return nullptr;
        }

        rv->push(info.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    cursor.close();
    return rv.release();
}

int QoreKrb5Keytab::removeEntry(const QoreKrb5Principal& principal, krb5_kvno kvno, krb5_enctype enctype,
        ExceptionSink* xsink) {
    if (!krb5_check_keytab_access(ctx, keytab, QSEC_WRITE, xsink, "removing keytab entry")) {
        return -1;
    }

    krb5_keytab_entry entry;
    memset(&entry, 0, sizeof(entry));

    krb5_error_code rc = krb5_kt_get_entry(ctx, keytab, principal.principal, kvno, enctype, &entry);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "looking up keytab entry for removal");
    }

    rc = krb5_kt_remove_entry(ctx, keytab, &entry);
    krb5_free_keytab_entry_contents(ctx, &entry);
    if (rc) {
        return krb5_raise_exception(xsink, ctx, rc, "KRB5-KEYTAB-ERROR", "removing keytab entry");
    }

    return 0;
}

static void krb5_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    krb5ns.addConstant("GSS_MUTUAL_FLAG", (int64)GSS_C_MUTUAL_FLAG);
    krb5ns.addConstant("GSS_SEQUENCE_FLAG", (int64)GSS_C_SEQUENCE_FLAG);
    krb5ns.addConstant("GSS_INTEG_FLAG", (int64)GSS_C_INTEG_FLAG);
    krb5ns.addConstant("GSS_CONF_FLAG", (int64)GSS_C_CONF_FLAG);
    krb5ns.addConstant("GSS_DELEG_FLAG", (int64)GSS_C_DELEG_FLAG);
    krb5ns.addConstant("GSS_REPLAY_FLAG", (int64)GSS_C_REPLAY_FLAG);
    krb5ns.addConstant("GSS_ANON_FLAG", (int64)GSS_C_ANON_FLAG);
    krb5ns.addConstant("GSS_CHANNEL_BOUND_FLAG", (int64)GSS_C_CHANNEL_BOUND_FLAG);
    krb5ns.addConstant("GSS_DEFAULT_FLAGS",
        (int64)(GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_INTEG_FLAG));
    krb5ns.addConstant("GSS_CRED_USAGE_INITIATE", (int64)GSS_C_INITIATE);
    krb5ns.addConstant("GSS_CRED_USAGE_ACCEPT", (int64)GSS_C_ACCEPT);
    krb5ns.addConstant("GSS_CRED_USAGE_BOTH", (int64)GSS_C_BOTH);
    krb5ns.addConstant("GSS_AF_UNSPEC", (int64)GSS_C_AF_UNSPEC);
    krb5ns.addConstant("GSS_AF_INET", (int64)GSS_C_AF_INET);
    krb5ns.addConstant("GSS_AF_NULLADDR", (int64)GSS_C_AF_NULLADDR);
    krb5ns.addConstant("ENCTYPE_AES128_CTS_HMAC_SHA1_96", (int64)ENCTYPE_AES128_CTS_HMAC_SHA1_96);
    krb5ns.addConstant("ENCTYPE_AES256_CTS_HMAC_SHA1_96", (int64)ENCTYPE_AES256_CTS_HMAC_SHA1_96);
#ifdef ENCTYPE_AES128_CTS_HMAC_SHA256_128
    krb5ns.addConstant("ENCTYPE_AES128_CTS_HMAC_SHA256_128", (int64)ENCTYPE_AES128_CTS_HMAC_SHA256_128);
#endif
#ifdef ENCTYPE_AES256_CTS_HMAC_SHA384_192
    krb5ns.addConstant("ENCTYPE_AES256_CTS_HMAC_SHA384_192", (int64)ENCTYPE_AES256_CTS_HMAC_SHA384_192);
#endif

    hashdeclKrb5KeytabEntryInfo = init_hashdecl_Krb5KeytabEntryInfo(krb5ns);
    hashdeclKrb5CredentialsInfo = init_hashdecl_Krb5CredentialsInfo(krb5ns);
    hashdeclKrb5InitialCredentialsOptions = init_hashdecl_Krb5InitialCredentialsOptions(krb5ns);
    hashdeclGssAcceptorContextOptions = init_hashdecl_GssAcceptorContextOptions(krb5ns);
    hashdeclGssClientContextOptions = init_hashdecl_GssClientContextOptions(krb5ns);
    hashdeclGssStepInfo = init_hashdecl_GssStepInfo(krb5ns);
    hashdeclGssWrapInfo = init_hashdecl_GssWrapInfo(krb5ns);
    hashdeclGssUnwrapInfo = init_hashdecl_GssUnwrapInfo(krb5ns);
    hashdeclGssMicInfo = init_hashdecl_GssMicInfo(krb5ns);
    hashdeclGssMicVerifyInfo = init_hashdecl_GssMicVerifyInfo(krb5ns);
    hashdeclKrb5CredentialCacheInfo = init_hashdecl_Krb5CredentialCacheInfo(krb5ns);

    krb5ns.addSystemClass(initKrb5PrincipalClass(krb5ns));
    krb5ns.addSystemClass(initKrb5CredentialsClass(krb5ns));
    krb5ns.addSystemClass(initKrb5CredentialCacheClass(krb5ns));
    krb5ns.addSystemClass(initKrb5KeytabClass(krb5ns));
    krb5ns.addSystemClass(initKrb5ContextClass(krb5ns));
    krb5ns.addSystemClass(initGssCredentialClass(krb5ns));
    krb5ns.addSystemClass(initGssClientContextClass(krb5ns));
    krb5ns.addSystemClass(initGssAcceptorContextClass(krb5ns));
}

static void krb5_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(krb5ns.copy());
}

static void krb5_module_delete() {
    krb5ns.clear(nullptr);
}
