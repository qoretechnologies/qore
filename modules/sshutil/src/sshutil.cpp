/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    sshutil.cpp

    Qore sshutil module implementation helpers

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

#include "sshutil-module.h"

#include <qore/qore_thread.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>

using SshUtilObjectMap = std::map<std::string, QoreObject*>;

static QoreThreadLock sshutil_registry_lock;
static SshUtilObjectMap client_identity_providers;
static SshUtilObjectMap server_host_key_providers;
static SshUtilObjectMap host_key_stores;

static bool sshutil_check_name(const char* name, ExceptionSink* xsink) {
    if (!name || !*name) {
        xsink->raiseException("SSHUTIL-REGISTRY-ERROR", "provider/store registry names cannot be empty");
        return false;
    }
    size_t count = 0;
    for (const char* p = name; *p; ++p, ++count) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "validating SSH utility registry name")) {
            return false;
        }
        if (isspace(static_cast<unsigned char>(*p))) {
            xsink->raiseException("SSHUTIL-REGISTRY-ERROR", "invalid provider/store registry name: '%s'", name);
            return false;
        }
    }
    return true;
}

static void sshutil_deref_map(SshUtilObjectMap& m, ExceptionSink* xsink) {
    for (SshUtilObjectMap::value_type& i : m) {
        i.second->deref(xsink);
    }
}

static void sshutil_register(SshUtilObjectMap& m, const char* kind, const char* name, const QoreObject* provider,
        bool replace, ExceptionSink* xsink) {
    if (!sshutil_check_name(name, xsink)) {
        return;
    }

    QoreObject* ref = const_cast<QoreObject*>(provider)->objectRefSelf();
    QoreObject* old = nullptr;
    {
        AutoLocker al(sshutil_registry_lock);
        SshUtilObjectMap::iterator i = m.find(name);
        if (i != m.end() && !replace) {
            ref->deref(xsink);
            xsink->raiseException("SSHUTIL-REGISTRY-ERROR", "%s '%s' is already registered", kind, name);
            return;
        }

        if (i != m.end()) {
            old = i->second;
            i->second = ref;
        } else {
            m[name] = ref;
        }
    }
    if (old) {
        old->deref(xsink);
    }
}

static QoreObject* sshutil_deregister(SshUtilObjectMap& m, const char* name) {
    AutoLocker al(sshutil_registry_lock);
    SshUtilObjectMap::iterator i = m.find(name);
    if (i == m.end()) {
        return nullptr;
    }

    QoreObject* rv = i->second;
    m.erase(i);
    return rv;
}

static QoreObject* sshutil_get(SshUtilObjectMap& m, const char* name) {
    AutoLocker al(sshutil_registry_lock);
    SshUtilObjectMap::iterator i = m.find(name);
    return i == m.end() ? nullptr : i->second->objectRefSelf();
}

static QoreListNode* sshutil_list(SshUtilObjectMap& m, ExceptionSink* xsink) {
    ValueHolder rv_holder(new QoreListNode(stringTypeInfo), xsink);
    QoreListNode* rv = rv_holder->get<QoreListNode>();
    AutoLocker al(sshutil_registry_lock);
    size_t count = 0;
    for (const SshUtilObjectMap::value_type& i : m) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "listing SSH utility registry names")) {
            return nullptr;
        }
        rv->push(new QoreStringNode(i.first), xsink);
        if (*xsink) {
            return nullptr;
        }
        ++count;
    }
    return rv_holder.releaseAs<QoreListNode>();
}

static QoreHashNode* sshutil_make_candidate(const char* kind, const char* path, bool optional, ExceptionSink* xsink) {
    ValueHolder rv_holder(new QoreHashNode(hashdeclSshClientIdentityCandidate, xsink), xsink);
    QoreHashNode* rv = rv_holder->get<QoreHashNode>();
    if (*xsink) {
        return nullptr;
    }

    rv->setKeyValue("kind", sshutil_enum_value(enumSshClientIdentityCandidateKind, kind, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("optional", optional, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (path) {
        rv->setKeyValue("path", new QoreStringNode(path), xsink);
    } else {
        rv->setKeyValue("comment", new QoreStringNode("ssh-agent"), xsink);
    }
    if (*xsink) {
        return nullptr;
    }
    return rv_holder.releaseAs<QoreHashNode>();
}

static QoreHashNode* sshutil_make_private_key_material(const char* path, const std::string& passphrase_ref,
        bool has_passphrase_ref, ExceptionSink* xsink) {
    ValueHolder rv_holder(new QoreHashNode(hashdeclSshPrivateKeyMaterial, xsink), xsink);
    QoreHashNode* rv = rv_holder->get<QoreHashNode>();
    if (*xsink) {
        return nullptr;
    }

    rv->setKeyValue("kind", sshutil_enum_value(enumSshPrivateKeyMaterialKind, "file", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("path", new QoreStringNode(path), xsink);
    rv->setKeyValue("source_kind", new QoreStringNode("file"), xsink);
    if (has_passphrase_ref) {
        rv->setKeyValue("passphrase_ref", new QoreStringNode(passphrase_ref), xsink);
    }
    if (*xsink) {
        return nullptr;
    }
    return rv_holder.releaseAs<QoreHashNode>();
}

FilesystemSshClientIdentityProviderData::FilesystemSshClientIdentityProviderData(const QoreListNode* paths,
        bool agent, bool optional, ExceptionSink* xsink) : agent(agent), optional(optional) {
    if (!paths) {
        return;
    }

    ConstListIterator i(paths);
    size_t count = 0;
    while (i.next()) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "loading filesystem SSH client identity paths")) {
            return;
        }
        QoreStringValueHelper str(i.getValue());
        key_paths.emplace_back(str->c_str());
        ++count;
    }
}

QoreListNode* FilesystemSshClientIdentityProviderData::getClientIdentityCandidates(ExceptionSink* xsink) const {
    ValueHolder rv_holder(new QoreListNode(hashdeclSshClientIdentityCandidate->getTypeInfo()), xsink);
    QoreListNode* rv = rv_holder->get<QoreListNode>();

    if (agent) {
        rv->push(sshutil_make_candidate("agent", nullptr, optional, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    size_t count = 0;
    for (const std::string& path : key_paths) {
        if (count && !(count % 100)
                && qore_check_cancel(xsink, "building SSH client identity candidate list")) {
            return nullptr;
        }
        rv->push(sshutil_make_candidate("file", path.c_str(), optional, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        ++count;
    }

    return rv_holder.releaseAs<QoreListNode>();
}

FilesystemSshClientIdentityProviderData* FilesystemSshClientIdentityProviderData::copy() const {
    return new FilesystemSshClientIdentityProviderData(*this);
}

FilesystemSshServerHostKeyProviderData::FilesystemSshServerHostKeyProviderData(const QoreListNode* paths,
        const QoreStringNode* passphrase_ref, ExceptionSink* xsink) : has_passphrase_ref(passphrase_ref) {
    ConstListIterator i(paths);
    size_t count = 0;
    while (i.next()) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "loading filesystem SSH server host-key paths")) {
            return;
        }
        QoreStringValueHelper str(i.getValue());
        key_paths.emplace_back(str->c_str());
        ++count;
    }
    if (has_passphrase_ref) {
        this->passphrase_ref = passphrase_ref->c_str();
    }
}

QoreListNode* FilesystemSshServerHostKeyProviderData::getServerHostKeyMaterial(ExceptionSink* xsink) const {
    ValueHolder rv_holder(new QoreListNode(hashdeclSshPrivateKeyMaterial->getTypeInfo()), xsink);
    QoreListNode* rv = rv_holder->get<QoreListNode>();
    size_t count = 0;
    for (const std::string& path : key_paths) {
        if (count && !(count % 100) && qore_check_cancel(xsink, "building SSH server host-key material list")) {
            return nullptr;
        }
        rv->push(sshutil_make_private_key_material(path.c_str(), passphrase_ref, has_passphrase_ref, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        ++count;
    }
    return rv_holder.releaseAs<QoreListNode>();
}

FilesystemSshServerHostKeyProviderData* FilesystemSshServerHostKeyProviderData::copy() const {
    return new FilesystemSshServerHostKeyProviderData(*this);
}

void SshUtilRegistry::clear(ExceptionSink* xsink) {
    SshUtilObjectMap old_client_identity_providers;
    SshUtilObjectMap old_server_host_key_providers;
    SshUtilObjectMap old_host_key_stores;
    {
        AutoLocker al(sshutil_registry_lock);
        old_client_identity_providers.swap(client_identity_providers);
        old_server_host_key_providers.swap(server_host_key_providers);
        old_host_key_stores.swap(host_key_stores);
    }
    sshutil_deref_map(old_client_identity_providers, xsink);
    sshutil_deref_map(old_server_host_key_providers, xsink);
    sshutil_deref_map(old_host_key_stores, xsink);
}

void SshUtilRegistry::registerClientIdentityProvider(const char* name, const QoreObject* provider, bool replace,
        ExceptionSink* xsink) {
    sshutil_register(client_identity_providers, "client identity provider", name, provider, replace, xsink);
}

QoreObject* SshUtilRegistry::deregisterClientIdentityProvider(const char* name) {
    return sshutil_deregister(client_identity_providers, name);
}

QoreObject* SshUtilRegistry::getClientIdentityProvider(const char* name) {
    return sshutil_get(client_identity_providers, name);
}

QoreListNode* SshUtilRegistry::listClientIdentityProviders(ExceptionSink* xsink) {
    return sshutil_list(client_identity_providers, xsink);
}

void SshUtilRegistry::registerServerHostKeyProvider(const char* name, const QoreObject* provider, bool replace,
        ExceptionSink* xsink) {
    sshutil_register(server_host_key_providers, "server host-key provider", name, provider, replace, xsink);
}

QoreObject* SshUtilRegistry::deregisterServerHostKeyProvider(const char* name) {
    return sshutil_deregister(server_host_key_providers, name);
}

QoreObject* SshUtilRegistry::getServerHostKeyProvider(const char* name) {
    return sshutil_get(server_host_key_providers, name);
}

QoreListNode* SshUtilRegistry::listServerHostKeyProviders(ExceptionSink* xsink) {
    return sshutil_list(server_host_key_providers, xsink);
}

void SshUtilRegistry::registerHostKeyStore(const char* name, const QoreObject* store, bool replace,
        ExceptionSink* xsink) {
    sshutil_register(host_key_stores, "host-key store", name, store, replace, xsink);
}

QoreObject* SshUtilRegistry::deregisterHostKeyStore(const char* name) {
    return sshutil_deregister(host_key_stores, name);
}

QoreObject* SshUtilRegistry::getHostKeyStore(const char* name) {
    return sshutil_get(host_key_stores, name);
}

QoreListNode* SshUtilRegistry::listHostKeyStores(ExceptionSink* xsink) {
    return sshutil_list(host_key_stores, xsink);
}

QoreValue sshutil_enum_value(const QoreEnumDecl* ed, const char* value, ExceptionSink* xsink) {
    ValueHolder str(new QoreStringNode(value), xsink);
    const QoreEnumMember* member = ed->findMemberByValue(*str);
    if (!member) {
        xsink->raiseException("SSHUTIL-ENUM-ERROR", "value '%s' is not valid for enum '%s'", value, ed->getName());
        return QoreValue();
    }
    return QoreValue::makeEnum(member);
}

QoreStringNode* sshutil_normalize_ssh_host(const QoreStringNode* host) {
    std::string rv(host->c_str());
    if (!rv.empty() && rv[rv.size() - 1] == '.') {
        rv.resize(rv.size() - 1);
    }

    for (char& c : rv) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
    }
    return new QoreStringNode(rv, host->getEncoding());
}

QoreStringNode* sshutil_format_known_hosts_host(const QoreStringNode* host, int64 port) {
    ValueHolder normalized_holder(sshutil_normalize_ssh_host(host), nullptr);
    // note: safe; sshutil_normalize_ssh_host() always returns a heap QoreStringNode, so the value
    // is never held in inline short string storage
    QoreStringNode* normalized = normalized_holder->get<QoreStringNode>();
    std::string rv(normalized->c_str());
    if (port == 22 && rv.find(':') == std::string::npos) {
        return normalized_holder.releaseAs<QoreStringNode>();
    }

    QoreStringNode* str = new QoreStringNode("[", host->getEncoding());
    str->concat(rv.c_str());
    str->sprintf("]:%lld", port);
    return str;
}

QoreStringNode* sshutil_get_host_key_decision_exception_name(const QoreStringNode* decision_value,
        const QoreStringNode* prefix) {
    const char* suffix = nullptr;
    const char* value = decision_value->c_str();

    if (!strcmp(value, "unknown")) {
        suffix = "HOSTKEY-UNKNOWN";
    } else if (!strcmp(value, "mismatch")) {
        suffix = "HOSTKEY-MISMATCH";
    } else if (!strcmp(value, "revoked")) {
        suffix = "HOSTKEY-REVOKED";
    } else if (!strcmp(value, "store-error")) {
        suffix = "HOSTKEY-STORE-ERROR";
    } else if (!strcmp(value, "invalid-response")) {
        suffix = "HOSTKEY-INVALID-RESPONSE";
    } else if (!strcmp(value, "provider-error")) {
        suffix = "HOSTKEY-PROVIDER-ERROR";
    } else if (!strcmp(value, "timeout")) {
        suffix = "HOSTKEY-TIMEOUT";
    } else {
        return nullptr;
    }

    QoreStringNode* rv = new QoreStringNode(prefix->c_str(), prefix->getEncoding());
    rv->concat("-");
    rv->concat(suffix);
    return rv;
}

static void sshutil_set_host_key_decision(QoreHashNode* h, const char* key, const char* member_name,
        ExceptionSink* xsink) {
    const QoreEnumMember* member = enumSshHostKeyDecision->findMember(member_name);
    assert(member);
    h->setKeyValue(key, QoreValue::makeEnum(member), xsink);
}

void init_sshutil_constants(QoreNamespace& ns) {
    ExceptionSink xsink;
    ValueHolder decision_map_holder(new QoreHashNode(enumSshHostKeyDecision->getTypeInfo(false)), &xsink);
    QoreHashNode* decision_map = decision_map_holder->get<QoreHashNode>();

    sshutil_set_host_key_decision(decision_map, "matched", "Matched", &xsink);
    sshutil_set_host_key_decision(decision_map, "added", "Added", &xsink);
    sshutil_set_host_key_decision(decision_map, "accepted-session", "AcceptedSession", &xsink);
    sshutil_set_host_key_decision(decision_map, "unknown", "Unknown", &xsink);
    sshutil_set_host_key_decision(decision_map, "mismatch", "Mismatch", &xsink);
    sshutil_set_host_key_decision(decision_map, "revoked", "Revoked", &xsink);
    sshutil_set_host_key_decision(decision_map, "store-error", "StoreError", &xsink);
    sshutil_set_host_key_decision(decision_map, "invalid-response", "InvalidResponse", &xsink);
    sshutil_set_host_key_decision(decision_map, "provider-error", "ProviderError", &xsink);
    sshutil_set_host_key_decision(decision_map, "timeout", "Timeout", &xsink);
    assert(!xsink);

    ns.addConstant("SshHostKeyDecisionMap", decision_map_holder.release(),
        qore_get_complex_hash_type(enumSshHostKeyDecision->getTypeInfo(false)));
}
