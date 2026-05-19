/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    sshutil-module.h

    Qore sshutil module

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

#ifndef _QORE_SSHUTIL_MODULE_H
#define _QORE_SSHUTIL_MODULE_H

#include "qore/Qore.h"
#include "qore/QoreThreadLock.h"

#include <map>
#include <string>
#include <vector>

DLLLOCAL void preinitAbstractSshClientIdentityProviderClass();
DLLLOCAL QoreClass* initAbstractSshClientIdentityProviderClass(QoreNamespace& ns);
DLLLOCAL void preinitAbstractSshServerHostKeyProviderClass();
DLLLOCAL QoreClass* initAbstractSshServerHostKeyProviderClass(QoreNamespace& ns);
DLLLOCAL void preinitAbstractSshHostKeyStoreClass();
DLLLOCAL QoreClass* initAbstractSshHostKeyStoreClass(QoreNamespace& ns);
DLLLOCAL void preinitFilesystemSshClientIdentityProviderClass();
DLLLOCAL QoreClass* initFilesystemSshClientIdentityProviderClass(QoreNamespace& ns);
DLLLOCAL void preinitFilesystemSshServerHostKeyProviderClass();
DLLLOCAL QoreClass* initFilesystemSshServerHostKeyProviderClass(QoreNamespace& ns);
DLLLOCAL void preinitSshUtilRegistryClass();
DLLLOCAL QoreClass* initSshUtilRegistryClass(QoreNamespace& ns);

DLLLOCAL QoreEnumDecl* init_enum_SshHostKeyDecision(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_SshClientAuthOrder(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_SshClientIdentityFallbackPolicy(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_SshProviderPurpose(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_SshClientIdentityCandidateKind(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_SshPrivateKeyMaterialKind(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshPublicKeyInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshClientIdentityCandidate(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshPrivateKeyMaterial(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshHostKeyRecord(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshHostKeyDecisionInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshProviderContext(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SshProviderSet(QoreNamespace& ns);
DLLLOCAL void init_sshutil_functions(QoreNamespace& ns);
DLLLOCAL void init_sshutil_constants(QoreNamespace& ns);

DLLLOCAL extern QoreEnumDecl* enumSshHostKeyDecision;
DLLLOCAL extern QoreEnumDecl* enumSshClientAuthOrder;
DLLLOCAL extern QoreEnumDecl* enumSshClientIdentityFallbackPolicy;
DLLLOCAL extern QoreEnumDecl* enumSshProviderPurpose;
DLLLOCAL extern QoreEnumDecl* enumSshClientIdentityCandidateKind;
DLLLOCAL extern QoreEnumDecl* enumSshPrivateKeyMaterialKind;
DLLLOCAL extern TypedHashDecl* hashdeclSshPublicKeyInfo;
DLLLOCAL extern TypedHashDecl* hashdeclSshClientIdentityCandidate;
DLLLOCAL extern TypedHashDecl* hashdeclSshPrivateKeyMaterial;
DLLLOCAL extern TypedHashDecl* hashdeclSshHostKeyRecord;
DLLLOCAL extern TypedHashDecl* hashdeclSshHostKeyDecisionInfo;
DLLLOCAL extern TypedHashDecl* hashdeclSshProviderContext;
DLLLOCAL extern TypedHashDecl* hashdeclSshProviderSet;

// sshutil-internal class-pointer globals (defined by the qpp-generated code).
// Dependent modules (ssh2, ssh) must NOT reference these directly; they obtain
// the classes through the public accessor functions declared below.
DLLLOCAL extern QoreClass* QC_ABSTRACTSSHCLIENTIDENTITYPROVIDER;
DLLLOCAL extern QoreClass* QC_ABSTRACTSSHSERVERHOSTKEYPROVIDER;
DLLLOCAL extern QoreClass* QC_ABSTRACTSSHHOSTKEYSTORE;

// public accessor function declarations (also installed as <qore/sshutil.h>)
#include <qore/sshutil.h>

class AbstractSshClientIdentityProviderData : public AbstractPrivateData {
};

class AbstractSshServerHostKeyProviderData : public AbstractPrivateData {
};

class AbstractSshHostKeyStoreData : public AbstractPrivateData {
};

class FilesystemSshClientIdentityProviderData : public AbstractPrivateData {
public:
    DLLLOCAL FilesystemSshClientIdentityProviderData(const QoreListNode* paths, bool agent, bool optional,
            ExceptionSink* xsink);

    DLLLOCAL QoreListNode* getClientIdentityCandidates(ExceptionSink* xsink) const;
    DLLLOCAL FilesystemSshClientIdentityProviderData* copy() const;

private:
    std::vector<std::string> key_paths;
    bool agent;
    bool optional;
};

class FilesystemSshServerHostKeyProviderData : public AbstractPrivateData {
public:
    DLLLOCAL FilesystemSshServerHostKeyProviderData(const QoreListNode* paths, const QoreStringNode* passphrase_ref,
            ExceptionSink* xsink);

    DLLLOCAL QoreListNode* getServerHostKeyMaterial(ExceptionSink* xsink) const;
    DLLLOCAL FilesystemSshServerHostKeyProviderData* copy() const;

private:
    std::vector<std::string> key_paths;
    std::string passphrase_ref;
    bool has_passphrase_ref;
};

class SshUtilRegistryData : public AbstractPrivateData {
};

class SshUtilRegistry {
public:
    DLLLOCAL static void clear(ExceptionSink* xsink);
    DLLLOCAL static void registerClientIdentityProvider(const char* name, const QoreObject* provider, bool replace,
            ExceptionSink* xsink);
    DLLLOCAL static QoreObject* deregisterClientIdentityProvider(const char* name);
    DLLLOCAL static QoreObject* getClientIdentityProvider(const char* name);
    DLLLOCAL static QoreListNode* listClientIdentityProviders(ExceptionSink* xsink);
    DLLLOCAL static void registerServerHostKeyProvider(const char* name, const QoreObject* provider, bool replace,
            ExceptionSink* xsink);
    DLLLOCAL static QoreObject* deregisterServerHostKeyProvider(const char* name);
    DLLLOCAL static QoreObject* getServerHostKeyProvider(const char* name);
    DLLLOCAL static QoreListNode* listServerHostKeyProviders(ExceptionSink* xsink);
    DLLLOCAL static void registerHostKeyStore(const char* name, const QoreObject* store, bool replace,
            ExceptionSink* xsink);
    DLLLOCAL static QoreObject* deregisterHostKeyStore(const char* name);
    DLLLOCAL static QoreObject* getHostKeyStore(const char* name);
    DLLLOCAL static QoreListNode* listHostKeyStores(ExceptionSink* xsink);
};

DLLLOCAL QoreValue sshutil_enum_value(const QoreEnumDecl* ed, const char* value, ExceptionSink* xsink);
DLLLOCAL QoreStringNode* sshutil_normalize_ssh_host(const QoreStringNode* host);
DLLLOCAL QoreStringNode* sshutil_format_known_hosts_host(const QoreStringNode* host, int64 port);
DLLLOCAL QoreStringNode* sshutil_get_host_key_decision_exception_name(const QoreStringNode* decision_value,
    const QoreStringNode* prefix);

#endif
