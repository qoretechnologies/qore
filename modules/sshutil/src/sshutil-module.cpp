/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    sshutil-module.cpp

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

#include "sshutil-module.h"

static void sshutil_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void sshutil_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void sshutil_module_delete();

QoreNamespace SshUtilNS("Qore::SshUtil");

QoreEnumDecl* enumSshHostKeyDecision = nullptr;
QoreEnumDecl* enumSshClientAuthOrder = nullptr;
QoreEnumDecl* enumSshClientIdentityFallbackPolicy = nullptr;
QoreEnumDecl* enumSshProviderPurpose = nullptr;
QoreEnumDecl* enumSshClientIdentityCandidateKind = nullptr;
QoreEnumDecl* enumSshPrivateKeyMaterialKind = nullptr;
TypedHashDecl* hashdeclSshPublicKeyInfo = nullptr;
TypedHashDecl* hashdeclSshClientIdentityCandidate = nullptr;
TypedHashDecl* hashdeclSshPrivateKeyMaterial = nullptr;
TypedHashDecl* hashdeclSshHostKeyRecord = nullptr;
TypedHashDecl* hashdeclSshHostKeyDecisionInfo = nullptr;
TypedHashDecl* hashdeclSshProviderContext = nullptr;
TypedHashDecl* hashdeclSshProviderSet = nullptr;

extern "C" DLLEXPORT void sshutil_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "sshutil";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Qore SSH credential and host-key provider utility module";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = sshutil_module_init;
    mod_info.ns_init = sshutil_module_ns_init;
    mod_info.del = sshutil_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

static void sshutil_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    enumSshHostKeyDecision = init_enum_SshHostKeyDecision(SshUtilNS);
    enumSshClientAuthOrder = init_enum_SshClientAuthOrder(SshUtilNS);
    enumSshClientIdentityFallbackPolicy = init_enum_SshClientIdentityFallbackPolicy(SshUtilNS);
    enumSshProviderPurpose = init_enum_SshProviderPurpose(SshUtilNS);
    enumSshClientIdentityCandidateKind = init_enum_SshClientIdentityCandidateKind(SshUtilNS);
    enumSshPrivateKeyMaterialKind = init_enum_SshPrivateKeyMaterialKind(SshUtilNS);

    hashdeclSshPublicKeyInfo = init_hashdecl_SshPublicKeyInfo(SshUtilNS);
    hashdeclSshClientIdentityCandidate = init_hashdecl_SshClientIdentityCandidate(SshUtilNS);
    hashdeclSshPrivateKeyMaterial = init_hashdecl_SshPrivateKeyMaterial(SshUtilNS);
    hashdeclSshHostKeyRecord = init_hashdecl_SshHostKeyRecord(SshUtilNS);
    hashdeclSshHostKeyDecisionInfo = init_hashdecl_SshHostKeyDecisionInfo(SshUtilNS);
    hashdeclSshProviderContext = init_hashdecl_SshProviderContext(SshUtilNS);

    preinitAbstractSshClientIdentityProviderClass();
    preinitAbstractSshServerHostKeyProviderClass();
    preinitAbstractSshHostKeyStoreClass();
    preinitFilesystemSshClientIdentityProviderClass();
    preinitFilesystemSshServerHostKeyProviderClass();
    preinitSshUtilRegistryClass();

    hashdeclSshProviderSet = init_hashdecl_SshProviderSet(SshUtilNS);

    init_sshutil_constants(SshUtilNS);
    init_sshutil_functions(SshUtilNS);

    SshUtilNS.addSystemClass(initAbstractSshClientIdentityProviderClass(SshUtilNS));
    SshUtilNS.addSystemClass(initAbstractSshServerHostKeyProviderClass(SshUtilNS));
    SshUtilNS.addSystemClass(initAbstractSshHostKeyStoreClass(SshUtilNS));
    SshUtilNS.addSystemClass(initFilesystemSshClientIdentityProviderClass(SshUtilNS));
    SshUtilNS.addSystemClass(initFilesystemSshServerHostKeyProviderClass(SshUtilNS));
    SshUtilNS.addSystemClass(initSshUtilRegistryClass(SshUtilNS));
}

static void sshutil_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(SshUtilNS.copy());
}

static void sshutil_module_delete() {
    ExceptionSink xsink;
    SshUtilRegistry::clear(&xsink);
    SshUtilNS.clear(&xsink);
}

// public accessors for the abstract provider classes; dependent modules (ssh2,
// ssh) call these from their own module init (which runs after the sshutil
// dependency has been loaded and initialized) instead of referencing the
// class-pointer data symbols directly. A function symbol is bound lazily, so
// the dependent module's dlopen() succeeds even though Qore dlopen()s the
// requesting module before loading its dependencies; the call itself happens
// later, once sshutil is initialized. Resolving via a uniquely-named exported
// function (rather than the QC_* data symbol) also avoids the symbol collision
// that occurs when the dependent module declares its own QC_* global.
extern "C" DLLEXPORT QoreClass* sshutil_get_abstract_ssh_client_identity_provider_class() {
    return QC_ABSTRACTSSHCLIENTIDENTITYPROVIDER;
}

extern "C" DLLEXPORT QoreClass* sshutil_get_abstract_ssh_server_host_key_provider_class() {
    return QC_ABSTRACTSSHSERVERHOSTKEYPROVIDER;
}

extern "C" DLLEXPORT QoreClass* sshutil_get_abstract_ssh_host_key_store_class() {
    return QC_ABSTRACTSSHHOSTKEYSTORE;
}
