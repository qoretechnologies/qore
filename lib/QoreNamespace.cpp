/* -*- indent-tabs-mode: nil -*- */
/*
    QoreNamespace.cpp

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

#include <algorithm>

#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreRegexBase.h"
#include "qore/intern/ssl_constants.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreSignal.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/QoreRegex.h"

// include files for default object classes
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_EventLoop.h"
#include "qore/intern/QC_EventNotifier.h"
#include "qore/intern/QC_LoggerInterfaceBase.h"
#include "qore/intern/QC_AsyncIoController.h"
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"
#include "qore/intern/QC_ProgramControl.h"
#include "qore/intern/QC_Program.h"
#include "qore/intern/QC_DebugProgram.h"
#include "qore/intern/QC_Breakpoint.h"
#include "qore/intern/QC_Expression.h"
#include "qore/intern/QC_Regex.h"
#include "qore/intern/QC_RegexSubst.h"
#include "qore/intern/QC_RegexExtract.h"
#include "qore/intern/QC_File.h"
#include "qore/intern/QC_Dir.h"
#include "qore/intern/QC_GetOpt.h"
#include "qore/intern/QC_FtpClient.h"
#include "qore/intern/QC_HTTPClient.h"
#include "qore/intern/QC_TermIOS.h"
#include "qore/intern/QC_TimeZone.h"
#include "qore/intern/QC_TreeMap.h"
#include "qore/intern/QC_Serializable.h"
#include "qore/intern/QC_AbstractPollableIoObjectBase.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_FilePollOperation.h"
#include "qore/intern/QC_SandboxManager.h"
#include "qore/intern/QC_QoreParseOptions.h"

#include "qore/intern/QC_Datasource.h"
#include "qore/intern/QC_DatasourcePool.h"
#include "qore/intern/QC_SQLStatement.h"

// functions
#include "qore/intern/ql_time.h"
#include "qore/intern/ql_lib.h"
#include "qore/intern/ql_math.h"
#include "qore/intern/ql_type.h"
#include "qore/intern/ql_env.h"
#include "qore/intern/ql_string.h"
#include "qore/intern/ql_pwd.h"
#include "qore/intern/ql_misc.h"
#include "qore/intern/ql_list.h"
#include "qore/intern/ql_thread.h"
#include "qore/intern/ql_crypto.h"
#include "qore/intern/ql_object.h"
#include "qore/intern/ql_file.h"
#include "qore/intern/ql_compression.h"

#ifdef DEBUG
#include "qore/intern/ql_debug.h"
#endif // DEBUG

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>

DLLLOCAL QoreClass* initReadOnlyFileClass(QoreNamespace& ns);

DLLLOCAL QoreClass* initAbstractDatasourceClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractSQLStatementClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractQuantifiedIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractBidirectionalIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractQuantifiedBidirectionalIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initListIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initListReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashKeyIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashKeyReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashPairIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashPairReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initObjectIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initObjectReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initObjectKeyIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initObjectKeyReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initObjectPairIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initObjectPairReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashListIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initHashListReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initListHashIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initListHashReverseIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractLineIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initFileLineIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initDataLineIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initInputStreamLineIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initSingleValueIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initRangeIteratorClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStreamBaseClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initBinaryInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStringInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initFileInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initEncodingConversionInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initEncodingConversionOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initBinaryOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStringOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initFileOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStreamPipeClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initPipeInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initPipeOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStreamWriterClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStreamReaderClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initBufferedStreamReaderClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initTransformClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initTransformInputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initTransformOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStdoutOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initStderrOutputStreamClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractPollOperationClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initAbstractPollableIoObjectClass(QoreNamespace& ns);

DLLLOCAL void init_type_constants(QoreNamespace& ns);
DLLLOCAL void init_compression_constants(QoreNamespace& ns);
DLLLOCAL void init_crypto_constants(QoreNamespace& ns);
DLLLOCAL void init_misc_constants(QoreNamespace& ns);
DLLLOCAL void init_string_constants(QoreNamespace& ns);
DLLLOCAL void init_option_constants(QoreNamespace& ns);
DLLLOCAL void init_math_constants(QoreNamespace& ns);
DLLLOCAL void init_qore_constants(QoreNamespace& ns);
DLLLOCAL void init_errno_constants(QoreNamespace& ns);
DLLLOCAL void init_lib_constants(QoreNamespace& ns);

DLLLOCAL void init_dbi_functions(QoreNamespace& ns);
DLLLOCAL void init_dbi_constants(QoreNamespace& ns);

// constants defined in pseudo-class implementations
DLLLOCAL void init_QC_Number_constants(QoreNamespace& ns);

DLLLOCAL void preinitTimeZoneClass();
DLLLOCAL void preinitInputStreamClass();
DLLLOCAL void preinitOutputStreamClass();
DLLLOCAL void preinitAbstractPollableIoObjectClass();
DLLLOCAL void preinitSocketClass();

StaticSystemNamespace* staticSystemNamespace;

const TypedHashDecl* hashdeclStatInfo,
    * hashdeclDirStatInfo,
    * hashdeclFilesystemInfo,
    * hashdeclDateTimeInfo,
    * hashdeclIsoWeekInfo,
    * hashdeclCallStackInfo,
    * hashdeclExceptionInfo,
    * hashdeclStatementInfo,
    * hashdeclNetIfInfo,
    * hashdeclSourceLocationInfo,
    * hashdeclSerializationInfo,
    * hashdeclObjectSerializationInfo,
    * hashdeclIndexedObjectSerializationInfo,
    * hashdeclHashSerializationInfo,
    * hashdeclListSerializationInfo,
    * hashdeclUrlInfo,
    * hashdeclFtpResponseInfo,
    * hashdeclSocketPollInfo,
    * hashdeclDatagramInfo,
    * hashdeclQuicGoawayStateInfo,
    * hashdeclPipeInfo,
    * hashdeclSseMessageInfo,
    * hashdeclPortRangeInfo,
    * hashdeclFilesystemPathInfo,
    * hashdeclFilesystemSecurityConfigInfo,
    * hashdeclNetworkSecurityConfigInfo,
    * hashdeclSandboxConfigInfo,
    * hashdeclSocketPollOperationInfo,
    * hashdeclSocketPollResultInfo,
    * hashdeclEventPollInfo,
    * hashdeclTimerEventInfo;

const QoreEnumDecl* enumHTTP2Mode;
const QoreEnumDecl* enumHTTP3Mode;

DLLLOCAL void init_context_functions(QoreNamespace& ns);
DLLLOCAL void init_RangeIterator_functions(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_HTTP2Mode(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_HTTP3Mode(QoreNamespace& ns);

GVEntryBase::GVEntryBase(const QoreProgramLocation* loc, char* n, const QoreTypeInfo* typeInfo,
        QoreParseTypeInfo* parseTypeInfo, qore_var_t type) :
    name(new NamedScope(n)),
    var(typeInfo
        ? new Var(loc, name->getIdentifier(), typeInfo, false, type == VT_THREAD_LOCAL)
        : new Var(loc, name->getIdentifier(), parseTypeInfo, type == VT_THREAD_LOCAL)
    ) {
}

void GVEntryBase::clear() {
    //printd(5, "GVEntryBase::clear() name: '%s' var: %p '%s'\n", name->ostr, var, var ? var->getName() : "n/a");
    delete name;
    if (var) {
        var->deref(nullptr);
        var = nullptr;
    }
}

QoreNamespace::QoreNamespace(const char* n) : priv(new qore_ns_private(this, n)) {
}

QoreNamespace::QoreNamespace(const QoreNamespace& old, const QoreParseOptions& po) : priv(new qore_ns_private(*old.priv, po, this)) {
}

QoreNamespace::QoreNamespace(qore_ns_private* p) : priv(p) {
    if (p) {
        p->ns = this;
    }
}

QoreNamespace::~QoreNamespace() {
    delete priv;
}

QoreProgram* QoreNamespace::getProgram() const {
    return priv->getProgram();
}

const char* QoreNamespace::getModuleName() const {
    return priv->getModuleName();
}

const char* QoreNamespace::getName() const {
   return priv->name.c_str();
}

void QoreNamespace::setClassHandler(q_ns_class_handler_t class_handler) {
   priv->setClassHandler(class_handler);
}

void QoreNamespace::addSystemHashDecl(TypedHashDecl* hd) {
    // set sys and pub flags
    typed_hash_decl_private::get(*hd)->setSystemPublic();
    typed_hash_decl_private::get(*hd)->setNamespace(priv);
#ifdef DEBUG
    if (priv->hashDeclList.add(hd))
        assert(false);
    else {
        assert(!priv->classList.find(hd->getName()));
    }
#else
    priv->hashDeclList.add(hd);
#endif

    // see if namespace is attached to the root
    qore_root_ns_private* rns = priv->getRoot();
    if (!rns)
        return;

    //printd(5, "QoreNamespace::addSystemHashDecl() adding '%s' %p to hashdecl map %p in ns '%s'\n", hd->getName(), hd, &rns->thdmap, priv->name.c_str());
    rns->thdmap.update(hd->getName(), priv, hd);
}

void QoreNamespace::addSystemEnum(QoreEnumDecl* enumdecl) {
    qore_enum_decl_private* enum_priv = qore_enum_decl_private::get(*enumdecl);

    // set sys and pub flags
    if (!enum_priv->isSystem()) {
        enum_priv->setSystemPublic();
    }
    enum_priv->setNamespace(priv);

#ifdef DEBUG
    if (priv->enumList.add(enumdecl)) {
        assert(false);
    } else {
        assert(!priv->classList.find(enumdecl->getName()));
    }
#else
    priv->enumList.add(enumdecl);
#endif

    bool new_ns = false;
    qore_ns_private* enum_ns_priv = nullptr;
    QoreNamespace* existing_ns = priv->nsl.find(enumdecl->getName());
    if (existing_ns) {
        enum_ns_priv = qore_ns_private::get(*existing_ns);
    } else {
        QoreNamespace* enum_ns = new QoreNamespace(enumdecl->getName());
        enum_ns_priv = qore_ns_private::get(*enum_ns);
        new_ns = true;
        priv->nsl.runtimeAdd(enum_ns, priv);
    }

    if (enum_priv->isPublic()) {
        enum_ns_priv->setPublic();
    }

    const std::vector<QoreEnumMember*>& members = enum_priv->getMembers();
    for (auto* member : members) {
        if (enum_ns_priv->constant.inList(member->getName())) {
            continue;
        }
        QoreValue val = member->getValue();
        val.ref();
        enum_ns_priv->constant.add(member->getName(), val, enum_priv->getTypeInfo());
    }

    qore_root_ns_private* rns = priv->getRoot();
    if (rns && new_ns) {
        rns->rebuildIndexes(enum_ns_priv);
    }
    if (!rns) {
        return;
    }
    rns->edmap.update(enumdecl->getName(), priv, enumdecl);
}

// public, only called in single-threaded initialization
void QoreNamespace::addSystemClass(QoreClass* oc) {
    QORE_TRACE("QoreNamespace::addSystemClass()");

    oc->setSystem();
    if (qore_class_private::get(*oc)->setNamespaceConditional(priv)) {
        // generate builtin class signature
        std::string path;
        priv->getPath(path);
        qore_class_private::get(*oc)->finalizeBuiltin(path.c_str());
    } else {
        assert(qore_class_private::get(*oc)->initialized);
    }

#ifdef DEBUG
    if (priv->classList.add(oc)) {
        assert(false);
    } else {
        assert(!priv->hashDeclList.find(oc->getName()));
    }
#else
    priv->classList.add(oc);
#endif

    // see if namespace is attached to the root
    qore_root_ns_private* rns = priv->getRoot();
    if (!rns) {
        return;
    }

    //printd(5, "QoreNamespace::addSystemClass() adding '%s' %p to classmap %p in ns '%s'\n", oc->getName(), oc,
    //    &rns->clmap, priv->name.c_str());
    rns->clmap.update(oc->getName(), priv, oc);
}

void QoreNamespace::addNamespace(QoreNamespace* ns) {
    priv->addNamespace(ns->priv);
}

void QoreNamespace::addInitialNamespace(QoreNamespace* ns) {
    priv->addNamespace(ns->priv);
}

qore_ns_private::qore_ns_private(const QoreProgramLocation* loc) : loc(loc), constant(this), pub(false),
        builtin(false) {
    new QoreNamespace(this);
    name = parse_pop_ns_name(path);
    setModuleName();

    assert(name.rfind("::") == std::string::npos);
    assert(path.rfind("::", 0) == 0);
    assert(path.rfind("::::", 0) == std::string::npos);
}

qore_ns_private::~qore_ns_private() {
    purge();

    // Clean up typedef entries
    for (auto& i : typedefMap) {
        delete i.second;
    }
}

QoreProgram* qore_ns_private::getProgram() const {
    const qore_root_ns_private* rns = getRoot();
    return rns ? rns->pgm : nullptr;
}

void qore_ns_private::setPublic() {
    pub = true;
    //printd(5, "qore_ns_private::setPublic() this: %p '%s::' pub:%d\n", this, name.c_str(), pub);
}

void qore_ns_private::setClassHandler(q_ns_class_handler_t n_class_handler) {
    class_handler = n_class_handler;

    // register namespace with class handler in the root namespace
    qore_root_ns_private* rootns = getRoot();
    if (rootns)
        rootns->nshlist.add(this);
}

void qore_ns_private::runtimeImportSystemClasses(const qore_ns_private& source, qore_root_ns_private& rns,
        ExceptionSink* xsink) {
    assert(xsink);
    if (classList.importSystemClasses(source.classList, this, xsink)) {
        rns.runtimeRebuildClassIndexes(this);
    }

    if (*xsink) {
        return;
    }

    // add sub namespaces
    for (nsmap_t::const_iterator i = source.nsl.nsmap.begin(), e = source.nsl.nsmap.end(); i != e; ++i) {
        QoreNamespace* nns = nsl.find(i->first);
        if (!nns) {
            qore_ns_private* npns = new qore_ns_private(i->first.c_str(), *i->second->priv);
            nns = npns->ns;
            nns->priv->imported = true;
            nns = nsl.runtimeAdd(nns, this)->ns;
        }

        nns->priv->runtimeImportSystemClasses(*i->second->priv, rns, xsink);
        //printd(5, "qore_ns_private::runtimeImportSystemClasses() this: %p '%s::' imported %p '%s::'\n", this,
        //    name.c_str(), ns, ns->getName());
        if (*xsink)
            break;
    }
}

void qore_ns_private::runtimeImportSystemHashDecls(const qore_ns_private& source, qore_root_ns_private& rns,
        ExceptionSink* xsink) {
    assert(xsink);
    if (hashDeclList.importSystemHashDecls(source.hashDeclList, this, xsink))
        rns.runtimeRebuildHashDeclIndexes(this);

    if (*xsink)
        return;

    // add sub namespaces
    for (nsmap_t::const_iterator i = source.nsl.nsmap.begin(), e = source.nsl.nsmap.end(); i != e; ++i) {
        QoreNamespace* nns = nsl.find(i->first);
        if (!nns) {
            qore_ns_private* npns = new qore_ns_private(i->first.c_str(), *i->second->priv);
            nns = npns->ns;
            nns->priv->imported = true;
            nns = nsl.runtimeAdd(nns, this)->ns;
        }

        nns->priv->runtimeImportSystemHashDecls(*i->second->priv, rns, xsink);
        //printd(5, "qore_ns_private::runtimeImportSystemHashDecls() this: %p '%s::' imported %p '%s::'\n", this,
        //    name.c_str(), ns, ns->getName());
        if (*xsink)
            break;
    }
}

void qore_ns_private::runtimeImportSystemConstants(const qore_ns_private& source, qore_root_ns_private& rns,
        ExceptionSink* xsink) {
    assert(xsink);
    if (constant.importSystemConstants(source.constant, xsink))
        rns.runtimeRebuildConstantIndexes(this);

    if (*xsink)
        return;

    // add sub namespaces
    for (nsmap_t::const_iterator i = source.nsl.nsmap.begin(), e = source.nsl.nsmap.end(); i != e; ++i) {
        QoreNamespace* nns = nsl.find(i->first);
        if (!nns) {
            qore_ns_private* npns = new qore_ns_private(i->first.c_str(), *i->second->priv);
            nns = npns->ns;
            nns->priv->imported = true;
            nns = nsl.runtimeAdd(nns, this)->ns;
        }

        nns->priv->runtimeImportSystemConstants(*i->second->priv, rns, xsink);
        //printd(5, "qore_ns_private::runtimeImportSystemConstants() this: %p '%s::' imported %p '%s::'\n", this,
        //   name.c_str(), ns, ns->getName());
        if (*xsink)
            break;
    }
}

void qore_ns_private::runtimeImportSystemFunctions(const qore_ns_private& source, qore_root_ns_private& rns,
        ExceptionSink* xsink) {
    assert(xsink);
    if (func_list.importSystemFunctions(source.func_list, this, xsink))
        rns.runtimeRebuildFunctionIndexes(this);

    if (*xsink)
        return;

    // add sub namespaces
    for (nsmap_t::const_iterator i = source.nsl.nsmap.begin(), e = source.nsl.nsmap.end(); i != e; ++i) {
        QoreNamespace* nns = nsl.find(i->first);
        if (!nns) {
            qore_ns_private* npns = new qore_ns_private(i->first.c_str(), *i->second->priv);
            nns = npns->ns;
            nns->priv->imported = true;
            nns = nsl.runtimeAdd(nns, this)->ns;
        }

        nns->priv->runtimeImportSystemFunctions(*i->second->priv, rns, xsink);
        //printd(5, "qore_ns_private::runtimeImportSystemFunctions() this: %p '%s::' imported %p '%s::'\n", this,
        //    name.c_str(), ns, ns->getName());
        if (*xsink)
            break;
    }
}

FunctionEntry* qore_ns_private::addPendingVariantIntern(const char* fname, AbstractQoreFunctionVariant* v,
        bool& new_func) {
    SimpleRefHolder<AbstractQoreFunctionVariant> vh(v);

    if (!imported && !pub && v->isModulePublic() && parse_check_parse_option(PO_IN_MODULE))
        qore_program_private::makeParseWarning(getProgram(),
            *v->getUserVariantBase()->getUserSignature()->getParseLocation(), QP_WARN_INVALID_OPERATION,
            "INVALID-OPERATION", "function variant '%s::%s(%s)' is declared public but the enclosing namespace "
            "'%s::' is not public", name.c_str(), fname, v->getSignature()->getSignatureText(), name.c_str());

    FunctionEntry* fe = func_list.findNode(fname);

    if (!fe) {
        QoreFunction* u = new QoreFunction(fname);
        u->addPendingVariant(vh.release());
        fe = func_list.add(u, this);
        new_func = true;
        return fe;
    }

    return fe->getFunction()->addPendingVariant(vh.release()) ? nullptr : fe;
}

void qore_ns_private::addModuleNamespace(qore_ns_private* nns, QoreModuleContext& qmc) {
    if (nsl.find(nns->name)) {
        std::string path;
        getPath(path, true);
        qmc.error("Namespace '%s' already exists in '%s'", nns->name.c_str(), path.c_str());
        return;
    }

    if (classList.find(nns->name.c_str())) {
        std::string path;
        getPath(path, true);
        qmc.error("A class with the same name as the namespace ('%s') already exists in '%s'", nns->name.c_str(),
            path.c_str());
        return;
    }

    qmc.mcnl.push_back(ModuleContextNamespaceCommit(this, nns));
}

void qore_ns_private::addCommitNamespaceIntern(qore_ns_private* nns) {
    assert(!classList.find(nns->name.c_str()));

    nns = nsl.runtimeAdd(nns->ns, this);

    assert(nns->parent == this);

    // see if namespace is attached to the root
    qore_root_ns_private* rns = getRoot();
    if (!rns)
        return;

    // rebuild indexes for objects in new namespace tree
    QorePrivateNamespaceIterator qpni(nns);
    while (qpni.next())
        rns->rebuildIndexes(qpni.get());
}

void qore_ns_private::addNamespace(qore_ns_private* nns) {
    assert(nns != this);
    // set parent namespace unconditionally
    nns->parent = this;

    QoreModuleContext* qmc = get_module_context();
    if (qmc) {
        addModuleNamespace(nns, *qmc);
    } else {
        addCommitNamespaceIntern(nns);
    }
}

void qore_ns_private::updateDepthRecursive(unsigned ndepth) {
    //printd(5, "qore_ns_private::updateDepthRecursive(ndepth: %d) this: %p '%s' curr depth: %d\n", ndepth, this,
    //    name.c_str(), depth);
    assert(depth <= ndepth);
    assert(!ndepth || !name.empty());

    if (depth < ndepth) {
        depth = ndepth;

        for (nsmap_t::iterator i = nsl.nsmap.begin(), e = nsl.nsmap.end(); i != e; ++i)
            i->second->priv->updateDepthRecursive(ndepth + 1);
    }
}

void qore_ns_private::addBuiltinModuleVariant(const char* fname, AbstractQoreFunctionVariant* v,
        QoreModuleContext& qmc) {
    SimpleRefHolder<AbstractQoreFunctionVariant> vh(v);

    FunctionEntry* fe = func_list.findNode(fname);

    if (fe)
        qmc.error("function '%s()' has already been declared in namespace '%s'", fname, name.c_str());
    else
        qmc.mcfl.push_back(ModuleContextFunctionCommit(this, fname, vh.release()));
}

void qore_ns_private::addBuiltinVariant(const char* fname, AbstractQoreFunctionVariant* v) {
    QoreModuleContext* qmc = get_module_context();
    if (qmc)
        addBuiltinModuleVariant(fname, v, *qmc);
    else
        addBuiltinVariantIntern(fname, v);
}

void qore_ns_private::addBuiltinVariantIntern(const char* fname, AbstractQoreFunctionVariant* v) {
    SimpleRefHolder<AbstractQoreFunctionVariant> vh(v);

    FunctionEntry* fe = func_list.findNode(fname);

    if (fe) {
        fe->getFunction()->addBuiltinVariant(vh.release());
        return;
    }

    QoreFunction* u = new QoreFunction(fname);
    u->addBuiltinVariant(vh.release());
    fe = func_list.add(u, this);

    // add to root function map if attached
    qore_root_ns_private* rns = getRoot();
    if (!rns) {
        return;
    }

    assert(fe->getNamespace() == this);
    rns->fmap.update(fe->getName(), fe);
}

void QoreNamespaceList::deleteAll() {
    for (auto& i : nsmap) {
        delete i.second;
    }
    nsmap.clear();
}

bool QoreNamespaceList::addGlobalVars(qore_root_ns_private& rns) {
    bool ok = true;
    for (auto& i : nsmap) {
        if (!i.second->priv->addGlobalVars(rns) && ok) {
            ok = false;
        }
    }
    return ok;
}

void QoreNamespaceList::parseAssimilate(QoreNamespaceList& n, qore_ns_private* parent,
        std::vector<std::string>* pending_names) {
    for (auto& i : n.nsmap) {
        nsmap_t::iterator ni = nsmap.find(i.first);
        if (ni != nsmap.end()) {
            ni->second->priv->parseAssimilate(i.second);
        } else {
            nsmap[i.first] = i.second;
            i.second->priv->parent = parent;
            assert(parent || i.second->priv->root);
            i.second->priv->updateDepthRecursive((parent ? parent->depth : 0) + 1);
            // track the new namespace name for rollback support
            if (pending_names) {
                pending_names->push_back(i.first);
            }
        }
    }
    n.nsmap.clear();
}

void QoreNamespaceList::runtimeAssimilate(QoreNamespaceList& n, qore_ns_private* parent) {
    for (auto& i : n.nsmap) {
        nsmap_t::iterator ni = nsmap.find(i.first);
        if (ni != nsmap.end()) {
            ni->second->priv->runtimeAssimilate(i.second);
        } else {
            nsmap[i.first] = i.second;
            i.second->priv->parent = parent;
            assert(parent || i.second->priv->root);
            i.second->priv->updateDepthRecursive((parent ? parent->depth : 0) + 1);
        }
    }
    n.nsmap.clear();
}

void QoreNamespaceList::reset() {
    deleteAll();
}

void QoreNamespaceList::parseRemove(const char* name, ExceptionSink* xsink) {
    nsmap_t::iterator i = nsmap.find(name);
    if (i != nsmap.end()) {
        // Roll back the namespace contents first
        i->second->priv->parseRollback(xsink);
        // Then delete the namespace
        delete i->second;
        nsmap.erase(i);
    }
}

qore_ns_private* QoreNamespaceList::parseAdd(QoreNamespace* ns, qore_ns_private* parent) {
    // if namespace is already registered, then assimilate
    QoreNamespace* ons;
    if ((ons = find(ns->priv->name.c_str()))) {
        //printd(5, "QoreNamespaceList::add() this: %p ns: %p (%s) assimilating with ons: %p (%s)\n", this, ns,
        //    ns->getName(), ons, ons->getName());
        ons->priv->parseAssimilate(ns);
        return ons->priv;
    }
    nsmap[ns->priv->name] = ns;
    ns->priv->parent = parent;
    assert(!ns->priv->root || !parent);
    ns->priv->updateDepthRecursive(parent->depth + 1);
    return ns->priv;
}

qore_ns_private* QoreNamespaceList::runtimeAdd(QoreNamespace* ns, qore_ns_private* parent) {
    // if namespace is already registered, then assimilate
    QoreNamespace* ons;
    if ((ons = find(ns->priv->name.c_str()))) {
        //printd(5, "QoreNamespaceList::add() this: %p ns: %p (%s) assimilating with ons: %p (%s)\n", this, ns,
        //    ns->getName(), ons, ons->getName());
        ons->priv->runtimeAssimilate(ns);
        return ons->priv;
    }
    nsmap[ns->priv->name] = ns;
    ns->priv->parent = parent;
    assert(!ns->priv->root || !parent);
    ns->priv->updateDepthRecursive(parent->depth + 1);
    return ns->priv;
}

void QoreNamespace::clear(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);
    priv->clearConstants(**l);
    priv->clearData(xsink);
    priv->deleteData(true, xsink);
}

QoreNamespace* QoreNamespace::copy(const QoreParseOptions& po) const {
    //printd(5, "QoreNamespace::copy() this: %p po: %lld %s\n", this, po, priv->name.c_str());
    return new QoreNamespace(*this, po);
}

QoreNamespaceList::QoreNamespaceList(const QoreNamespaceList& old, const QoreParseOptions& po, const qore_ns_private& parent) {
    if ((po & PO_NO_API) == PO_NO_API) {
        return;
    }

    bool skip_builtin = (po & PO_NO_SYSTEM_API) == PO_NO_SYSTEM_API;
    bool skip_user = (po & PO_NO_USER_API) == PO_NO_USER_API;

    //printd(5, "QoreNamespaceList::QoreNamespaceList(old: %p) this: %p po: %lld size: %ld\n", &old, this, po,
    //    nsmap.size());
    nsmap_t::iterator last = nsmap.begin();
    for (nsmap_t::const_iterator i = old.nsmap.begin(), e = old.nsmap.end(); i != e; ++i) {
        // issue #3504: do not copy namespaces if API should not be imported
        if (!qore_ns_private::isPublic(*i->second)
            || (skip_builtin && i->second->priv->builtin)
            || (skip_user && !i->second->priv->builtin)) {
            continue;
        }
        QoreNamespace* ns = i->second->copy(po);
        ns->priv->parent = &parent;
        assert(!ns->priv->root);
        // do not assert() ns->priv->depth > 0 here; we may be in an unattached namespace tree
        last = nsmap.insert(last, nsmap_t::value_type(i->first, ns));

        //printd(5, "QoreNamespaceList::QoreNamespaceList(old: %p) this: %p po: %lld copied '%s'\n", &old, this, po,
        //    ns->getName());
    }
}

void QoreNamespaceList::resolveExternalParentHashDeclsRecursive(qore_root_ns_private* root) {
    for (auto& i : nsmap) {
        i.second->priv->resolveExternalParentHashDeclsRecursive(root);
    }
}

int QoreNamespaceList::parseInitGlobalVars() {
    int err = 0;
    for (nsmap_t::iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i) {
        if (i->second->priv->parseInitGlobalVars() && !err) {
            err = -1;
        }
    }
    return err;
}

void QoreNamespaceList::clearConstants(QoreListNode& l) {
    for (nsmap_t::iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i)
        i->second->priv->clearConstants(l);
}

void QoreNamespaceList::clearData(ExceptionSink* xsink) {
    for (nsmap_t::iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i)
        i->second->priv->clearData(xsink);
}

int QoreNamespaceList::parseInitConstants() {
    int err = 0;
    for (nsmap_t::iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i) {
        if (i->second->priv->parseInitConstants() && !err) {
            err = -1;
        }
    }
    return err;
}

void QoreNamespaceList::deleteAllConstants(ExceptionSink* xsink) {
    for (nsmap_t::iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i)
        i->second->priv->constant.deleteAll(xsink);
}

int QoreNamespaceList::parseInit() {
    int err = 0;
    for (auto& i : nsmap) {
        if (i.second->priv->parseInit() && !err) {
            err = -1;
        }
    }
    return err;
}

void QoreNamespaceList::parseResolveHierarchy() {
    for (auto& i : nsmap) {
        i.second->priv->parseResolveHierarchy();
    }
}

void QoreNamespaceList::parseResolveClassMembers() {
    for (auto& i : nsmap) {
        i.second->priv->parseResolveClassMembers ();
    }
}

void QoreNamespaceList::parseResolveAbstract() {
    for (auto& i : nsmap) {
        i.second->priv->parseResolveAbstract();
    }
}

void QoreNamespaceList::parseCommit() {
    for (auto& i : nsmap) {
        i.second->priv->parseCommit();
    }
}

void QoreNamespaceList::parseCommitRuntimeInit(ExceptionSink* xsink) {
    for (auto& i : nsmap) {
        i.second->priv->parseCommitRuntimeInit(xsink);
    }
}

void QoreNamespaceList::parseRollback(ExceptionSink* xsink, bool atomic_rollback) {
    for (auto& i : nsmap) {
        i.second->priv->parseRollback(xsink, atomic_rollback);
    }
}

// public: only called during Qore initialization to setup
// system constant types directly in Qore system namespaces
// FIXME: change to addSystemConstant() to avoid confusion
void QoreNamespace::addConstant(const char* cname, QoreValue val) {
    return addConstant(cname, val, 0);
}

void QoreNamespace::addConstant(const char* cname, QoreValue val, const QoreTypeInfo* typeInfo) {
    // see if namespace is attached to the root
    qore_root_ns_private* rns = priv->getRoot();
    if (!rns) {
        priv->constant.add(cname, val, typeInfo);
        return;
    }

    rns->addConstant(*priv, cname, val, typeInfo);
}

QoreNamespace* QoreNamespace::findCreateNamespacePath(const char* nspath) {
    NamedScope nscope(nspath);
    bool is_new = false;
    return priv->findCreateNamespacePath(nscope, false, false, is_new, 1);
}

QoreNamespace* QoreNamespace::findCreateNamespacePathAll(const char* nspath) {
    NamedScope nscope(nspath);
    bool is_new = false;
    return priv->findCreateNamespacePath(nscope, false, false, is_new, 0);
}

QoreClass* qore_ns_private::runtimeImportClass(ExceptionSink* xsink, const QoreClass* c, QoreProgram* spgm,
        q_setpub_t set_pub, const char* new_name, bool inject, const qore_class_private* injectedClass) {
    if (checkImportClass(new_name ? new_name : c->getName(), xsink)) {
        return nullptr;
    }

    QoreClass* nc = qore_class_private::makeImportClass(*c, spgm, new_name, inject, injectedClass, this, set_pub);
    classList.add(nc);

    return nc;
}

TypedHashDecl* qore_ns_private::runtimeImportHashDecl(ExceptionSink* xsink, const TypedHashDecl* hd,
    QoreProgram* spgm, q_setpub_t set_pub, const char* new_name) {
    if (checkImportHashDecl(new_name ? new_name : hd->getName(), xsink)) {
        return nullptr;
    }

    TypedHashDecl* nhd = new TypedHashDecl(*hd);
    // issue #3518: must set namespace before adding hashdecl
    typed_hash_decl_private::get(*nhd)->setNamespace(this);
    if (new_name) {
        typed_hash_decl_private::get(*nhd)->setName(new_name);
    }
    if (set_pub == CSP_SETPUB) {
        if (!typed_hash_decl_private::get(*nhd)->isPublic()) {
            typed_hash_decl_private::get(*nhd)->setPublic();
        }
    } else if (set_pub == CSP_SETPRIV) {
        if (typed_hash_decl_private::get(*nhd)->isPublic()) {
            typed_hash_decl_private::get(*nhd)->setPrivate();
        }
    }
    hashDeclList.add(nhd);

    return nhd;
}

QoreNamespace* qore_ns_private::findCreateNamespacePath(const NamedScope& nscope, bool pub, bool user, bool& is_new,
        int ignore_end) {
    assert(!is_new);

    // get root ns to add to namespace map if attached
    qore_root_ns_private* rns = getRoot();

    // iterate through each level of the namespace path and find/create namespaces as needed
    QoreNamespace* nns = ns;
    for (unsigned i = 0; i < nscope.size() - ignore_end; ++i) {
        nns = nns->priv->findCreateNamespace(nscope[i], user, is_new, rns);
        if (pub)
            nns->priv->pub = true;
    }

    return nns;
}

QoreNamespace* qore_ns_private::findCreateNamespace(const char* nsn, bool user, bool& is_new,
        qore_root_ns_private* rns) {
    QoreNamespace* ns = nsl.find(nsn);
    if (!ns) {
        std::string new_path;
        getPath(new_path, false, true);
        new_path.append(nsn);
        ns = new QoreNamespace(new_path.c_str());
        if (user) {
            ns->priv->builtin = false;
        }
        ns = nsl.runtimeAdd(ns, this)->ns;
        is_new = true;
        // add to namespace map if attached
        if (rns)
            rns->rebuildIndexes(ns->priv);
    }
    return ns;
}

QoreNamespace* qore_ns_private::findCreateNamespacePath(const nslist_t& nsl, bool user, bool& is_new) {
    assert(!nsl.empty());
    assert(!is_new);

    // get root ns to add to namespace map if attached
    qore_root_ns_private* rns = getRoot();

    //printd(5, "qore_ns_private::findCreateNamespacePath() this: %p nsv: %ld\n", this, nsv.size());

    // iterate through each level of the namespace path and find/create namespaces as needed
    QoreNamespace* nns = ns;
    for (nslist_t::const_iterator i = nsl.begin(), e = nsl.end(); i != e; ++i)
        nns = nns->priv->findCreateNamespace((*i)->name.c_str(), user, is_new, rns);

    return nns;
}

QoreClass* QoreNamespace::findLocalClass(const char* cname) const {
    return priv->classList.find(cname);
}

QoreClass* QoreNamespace::findLoadLocalClass(const char* cname) {
    return priv->findLoadClass(cname);
}

QoreNamespace* QoreNamespace::findLocalNamespace(const char* cname) const {
    return priv->nsl.find(cname);
}

const QoreNamespace* QoreNamespace::getParent() const {
    return priv->parent ? priv->parent->ns : nullptr;
}

void QoreNamespace::deleteData(ExceptionSink* xsink) {
    priv->deleteData(true, xsink);
}

void QoreNamespaceList::deleteData(bool deref_vars, ExceptionSink* xsink) {
    for (nsmap_t::iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i)
        i->second->priv->deleteData(deref_vars, xsink);
}

void QoreNamespaceList::getGlobalVars(QoreHashNode& h) const {
    for (nsmap_t::const_iterator i = nsmap.begin(), e = nsmap.end(); i != e; ++i)
        i->second->priv->getGlobalVars(h);
}

/*
static void showNSL(QoreNamespaceList* nsl) {
    printd(5, "showNSL() dumping %p\n", nsl);
    for (int i = 0; i < nsl.num_namespaces; i++)
        printd(5, "showNSL()  %d: %p %s (list: %p)\n", i, nsl.nslist[i], nsl.nslist[i]->name, nsl.nslist[i]->nsl);
}
*/

QoreHashNode* QoreNamespace::getConstantInfo() const {
    return priv->constant.getInfo();
}

QoreHashNode* QoreNamespace::getClassInfo() const {
    return priv->classList.getInfo();
}

// returns a hash of namespace information
QoreHashNode* QoreNamespace::getInfo() const {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);

    h->setKeyValue("constants", getConstantInfo(), 0);
    h->setKeyValue("classes", getClassInfo(), 0);

    if (!priv->nsl.nsmap.empty()) {
        QoreHashNode* nsh = new QoreHashNode(autoTypeInfo);

        for (nsmap_t::iterator i = priv->nsl.nsmap.begin(), e = priv->nsl.nsmap.end(); i != e; ++i)
            nsh->setKeyValue(i->second->priv->name.c_str(), i->second->getInfo(), 0);

        h->setKeyValue("subnamespaces", nsh, 0);
    }

    return h;
}

void QoreNamespace::addBuiltinVariant(const char* name, q_func_t f, int64 code_flags, int64 functional_domain, const QoreTypeInfo* returnTypeInfo, unsigned num_params, ...) {
    va_list args;
    va_start(args, num_params);
    priv->addBuiltinVariant<q_func_t, BuiltinFunctionValueVariant>(name, f, code_flags, functional_domain, returnTypeInfo, num_params, args);
    va_end(args);
}

void QoreNamespace::addBuiltinVariant(void* ptr, const char* name, q_external_func_t f, int64 code_flags,
    int64 functional_domain, const QoreTypeInfo* returnTypeInfo, unsigned num_params, ...) {
    va_list args;
    va_start(args, num_params);
    priv->addBuiltinVariant<q_external_func_t, BuiltinFunctionExternalVariant>(ptr, name, f, code_flags,
        functional_domain, returnTypeInfo, num_params, args);
    va_end(args);
}

const QoreExternalFunction* QoreNamespace::findLocalFunction(const char* name) const {
    return reinterpret_cast<const QoreExternalFunction*>(priv->func_list.find(name, true));
}

const QoreExternalConstant* QoreNamespace::findLocalConstant(const char* name) const {
    return reinterpret_cast<const QoreExternalConstant*>(priv->constant.findEntry(name));
}

const QoreExternalGlobalVar* QoreNamespace::findLocalGlobalVar(const char* name) const {
    return reinterpret_cast<const QoreExternalGlobalVar*>(priv->var_list.runtimeFindVar(name));
}

const TypedHashDecl* QoreNamespace::findLocalTypedHash(const char* name) const {
    return priv->hashDeclList.find(name);
}

const QoreEnumDecl* QoreNamespace::findLocalEnum(const char* name) const {
    return priv->enumList.find(name);
}

std::string QoreNamespace::getPath(bool anchored) const {
    std::string rv;
    priv->getPath(rv, anchored);
    return rv;
}

bool QoreNamespace::isModulePublic() const {
    return priv->pub;
}

bool QoreNamespace::isBuiltin() const {
    return priv->builtin;
}

bool QoreNamespace::isImported() const {
    return priv->imported;
}

bool QoreNamespace::isRoot() const {
    return priv->root;
}

QoreValue QoreNamespace::setKeyValue(const std::string& key, QoreValue val) {
    return priv->setKeyValue(key, val);
}

QoreValue QoreNamespace::setKeyValueIfNotSet(const std::string& key, QoreValue val) {
    return priv->setKeyValueIfNotSet(key, val);
}

bool QoreNamespace::setKeyValueIfNotSet(const std::string& key, const char* val) {
    return priv->setKeyValueIfNotSet(key, val);
}

QoreValue QoreNamespace::getReferencedKeyValue(const std::string& key) const {
    return priv->getReferencedKeyValue(key);
}

QoreValue QoreNamespace::getReferencedKeyValue(const char* key) const {
    return priv->getReferencedKeyValue(key);
}

RootQoreNamespace::RootQoreNamespace(qore_root_ns_private* p) : QoreNamespace(p), rpriv(p) {
    if (p) {
        p->rns = this;
    }
}

RootQoreNamespace::~RootQoreNamespace() {
    delete rpriv;
    // make sure priv is not deleted (again)
    priv = nullptr;
}

QoreProgram* RootQoreNamespace::getProgram() const {
    return rpriv->pgm;
}

QoreNamespace* RootQoreNamespace::rootGetQoreNamespace() const {
    return rpriv->qoreNS;
}

extern void preinitBreakpointClass();

// sets up the root namespace
StaticSystemNamespace::StaticSystemNamespace() : RootQoreNamespace(new qore_root_ns_private(this)) {
    rpriv->qoreNS = new QoreNamespace("Qore");
    QoreNamespace& qns = *rpriv->qoreNS;

    // now add hashdecls
    hashdeclStatInfo = init_hashdecl_StatInfo(qns);
    hashdeclDirStatInfo = init_hashdecl_DirStatInfo(qns);
    hashdeclFilesystemInfo = init_hashdecl_FilesystemInfo(qns);
    preinitTimeZoneClass();
    hashdeclDateTimeInfo = init_hashdecl_DateTimeInfo(qns);
    hashdeclIsoWeekInfo = init_hashdecl_IsoWeekInfo(qns);
    hashdeclCallStackInfo = init_hashdecl_CallStackInfo(qns);
    hashdeclExceptionInfo = init_hashdecl_ExceptionInfo(qns);
    hashdeclStatementInfo = init_hashdecl_StatementInfo(qns);
    hashdeclNetIfInfo = init_hashdecl_NetIfInfo(qns);
    hashdeclSourceLocationInfo = init_hashdecl_SourceLocationInfo(qns);
    hashdeclObjectSerializationInfo = init_hashdecl_ObjectSerializationInfo(qns);
    hashdeclSerializationInfo = init_hashdecl_SerializationInfo(qns);
    hashdeclIndexedObjectSerializationInfo = init_hashdecl_IndexedObjectSerializationInfo(qns);
    hashdeclHashSerializationInfo = init_hashdecl_HashSerializationInfo(qns);
    hashdeclListSerializationInfo = init_hashdecl_ListSerializationInfo(qns);
    hashdeclUrlInfo = init_hashdecl_UrlInfo(qns);
    hashdeclFtpResponseInfo = init_hashdecl_FtpResponseInfo(qns);
    preinitAbstractPollableIoObjectClass();
    hashdeclSocketPollInfo = init_hashdecl_SocketPollInfo(qns);
    hashdeclDatagramInfo = init_hashdecl_DatagramInfo(qns);
    hashdeclQuicGoawayStateInfo = init_hashdecl_QuicGoawayStateInfo(qns);
    preinitReadOnlyFileClass();
    preinitFileClass();
    hashdeclPipeInfo = init_hashdecl_PipeInfo(qns);
    hashdeclSseMessageInfo = init_hashdecl_SseMessageInfo(qns);
    hashdeclPortRangeInfo = init_hashdecl_PortRangeInfo(qns);
    hashdeclFilesystemPathInfo = init_hashdecl_FilesystemPathInfo(qns);
    hashdeclFilesystemSecurityConfigInfo = init_hashdecl_FilesystemSecurityConfigInfo(qns);
    hashdeclNetworkSecurityConfigInfo = init_hashdecl_NetworkSecurityConfigInfo(qns);
    hashdeclSandboxConfigInfo = init_hashdecl_SandboxConfigInfo(qns);

    enumHTTP2Mode = init_enum_HTTP2Mode(qns);
    enumHTTP3Mode = init_enum_HTTP3Mode(qns);

    qore_ns_private::addNamespace(qns, get_thread_ns(qns));

    // pre-init classes
    // serializable class
    preinitSerializableClass();
    preinitInputStreamClass();
    preinitOutputStreamClass();

    qns.addSystemClass(initAbstractPollOperationClass(qns));
    qns.addSystemClass(initSocketPollOperationBaseClass(qns));
    preinitSocketClass();
    qns.addSystemClass(initSocketPollOperationClass(qns));
    qns.addSystemClass(initFilePollOperationClass(qns));
    qns.addSystemClass(initAbstractPollableIoObjectClass(qns));
    qns.addSystemClass(initAbstractPollableIoObjectBaseClass(qns));

    // add stream classes
    qns.addSystemClass(initStreamBaseClass(qns));
    qns.addSystemClass(initInputStreamClass(qns));
    qns.addSystemClass(initOutputStreamClass(qns));
    qns.addSystemClass(initTransformClass(qns));
    qns.addSystemClass(initTransformInputStreamClass(qns));
    qns.addSystemClass(initTransformOutputStreamClass(qns));
    qns.addSystemClass(initBinaryInputStreamClass(qns));
    qns.addSystemClass(initStringInputStreamClass(qns));
    qns.addSystemClass(initFileInputStreamClass(qns));
    qns.addSystemClass(initEncodingConversionInputStreamClass(qns));
    qns.addSystemClass(initEncodingConversionOutputStreamClass(qns));
    qns.addSystemClass(initBinaryOutputStreamClass(qns));
    qns.addSystemClass(initStringOutputStreamClass(qns));
    qns.addSystemClass(initFileOutputStreamClass(qns));
    qns.addSystemClass(initPipeInputStreamClass(qns));
    qns.addSystemClass(initPipeOutputStreamClass(qns));
    qns.addSystemClass(initStdoutOutputStreamClass(qns));
    qns.addSystemClass(initStderrOutputStreamClass(qns));
    qns.addSystemClass(initStreamPipeClass(qns));
    qns.addSystemClass(initStreamWriterClass(qns));
    qns.addSystemClass(initStreamReaderClass(qns));
    qns.addSystemClass(initBufferedStreamReaderClass(qns));

    // add system object types
    qns.addSystemClass(initTimeZoneClass(qns));
    qns.addSystemClass(initSSLCertificateClass(qns));
    qns.addSystemClass(initSSLPrivateKeyClass(qns));
    qns.addSystemClass(initSocketClass(qns));
    qns.addSystemClass(initEventNotifierClass(qns));
    // EventPollInfo/TimerEventInfo must be initialized before initEventLoopClass which uses them
    // EventPollInfo references AbstractPollableIoObject; must be after initSocketClass
    hashdeclEventPollInfo = init_hashdecl_EventPollInfo(qns);
    // TimerEventInfo has no special dependencies
    hashdeclTimerEventInfo = init_hashdecl_TimerEventInfo(qns);
    qns.addSystemClass(initEventLoopClass(qns));
    // Init hashdecls that reference Socket, AbstractPollOperation, and Queue classes
    // Must be after initSocketClass, initAbstractPollOperationClass, and get_thread_ns (Queue)
    // Must be before initAsyncIoControllerClass which references these hashdecls
    hashdeclSocketPollOperationInfo = init_hashdecl_SocketPollOperationInfo(qns);
    hashdeclSocketPollResultInfo = init_hashdecl_SocketPollResultInfo(qns);

    qns.addSystemClass(initLoggerInterfaceBaseClass(qns));  // must be before AsyncIoController and logger_bin module
    qns.addSystemClass(initAsyncIoControllerClass(qns));
    qns.addSystemClass(initSandboxManagerClass(qns));  // must be before Program class
    qns.addSystemClass(initParseOptionsClass(qns));  // must be before Program class
    preinitProgramClass();  // to resolve circular dependency Program/Expression class
    qns.addSystemClass(initExpressionClass(qns));
    preinitBreakpointClass();  // to resolve circular dependency Program/Breakpoint class
    qns.addSystemClass(initProgramControlClass(qns));
    qns.addSystemClass(initProgramClass(qns));
    qns.addSystemClass(initDebugProgramClass(qns));
    qns.addSystemClass(initBreakpointClass(qns));

    qns.addSystemClass(initRegexClass(qns));
    qns.addSystemClass(initRegexSubstClass(qns));
    qns.addSystemClass(initRegexExtractClass(qns));

    qns.addSystemClass(initTermIOSClass(qns));
    qns.addSystemClass(initReadOnlyFileClass(qns));
    qns.addSystemClass(initFileClass(qns));
    qns.addSystemClass(initDirClass(qns));
    qns.addSystemClass(initGetOptClass(qns));
    qns.addSystemClass(initFtpClientClass(qns));

    // add HTTPClient namespace
    qns.addSystemClass(initHTTPClientClass(qns));

    qns.addSystemClass(initAbstractIteratorClass(qns));
    qns.addSystemClass(initAbstractQuantifiedIteratorClass(qns));
    qns.addSystemClass(initAbstractBidirectionalIteratorClass(qns));
    qns.addSystemClass(initAbstractQuantifiedBidirectionalIteratorClass(qns));
    qns.addSystemClass(initListIteratorClass(qns));
    qns.addSystemClass(initListReverseIteratorClass(qns));
    qns.addSystemClass(initHashIteratorClass(qns));
    qns.addSystemClass(initHashReverseIteratorClass(qns));
    qns.addSystemClass(initHashKeyIteratorClass(qns));
    qns.addSystemClass(initHashKeyReverseIteratorClass(qns));
    qns.addSystemClass(initHashPairIteratorClass(qns));
    qns.addSystemClass(initHashPairReverseIteratorClass(qns));
    qns.addSystemClass(initObjectIteratorClass(qns));
    qns.addSystemClass(initObjectReverseIteratorClass(qns));
    qns.addSystemClass(initObjectKeyIteratorClass(qns));
    qns.addSystemClass(initObjectKeyReverseIteratorClass(qns));
    qns.addSystemClass(initObjectPairIteratorClass(qns));
    qns.addSystemClass(initObjectPairReverseIteratorClass(qns));
    qns.addSystemClass(initHashListIteratorClass(qns));
    qns.addSystemClass(initHashListReverseIteratorClass(qns));
    qns.addSystemClass(initListHashIteratorClass(qns));
    qns.addSystemClass(initListHashReverseIteratorClass(qns));
    qns.addSystemClass(initAbstractLineIteratorClass(qns));
    qns.addSystemClass(initFileLineIteratorClass(qns));
    qns.addSystemClass(initDataLineIteratorClass(qns));
    qns.addSystemClass(initInputStreamLineIteratorClass(qns));
    qns.addSystemClass(initSingleValueIteratorClass(qns));
    qns.addSystemClass(initRangeIteratorClass(qns));
    qns.addSystemClass(initTreeMapClass(qns));
    qns.addSystemClass(initSerializableClass(qns));

    init_qore_constants(qns);

    // set up Option namespace for Qore options
    QoreNamespace* option = new QoreNamespace("Qore::Option");
    init_option_constants(*option);
    qore_ns_private::addNamespace(qns, option);

    // create Qore::SQL namespace
    QoreNamespace* sqlns = new QoreNamespace("Qore::SQL");

    sqlns->addSystemClass(initAbstractSQLStatementClass(*sqlns));
    sqlns->addSystemClass(initAbstractDatasourceClass(*sqlns));
    sqlns->addSystemClass(initDatasourceClass(*sqlns));
    sqlns->addSystemClass(initDatasourcePoolClass(*sqlns));
    sqlns->addSystemClass(initSQLStatementClass(*sqlns));

    init_dbi_functions(*sqlns);
    init_dbi_constants(*sqlns);
    qore_ns_private::addNamespace(qns, sqlns);

    // create get Qore::Err namespace with ERRNO constants
    QoreNamespace* Err = new QoreNamespace("Qore::Err");
    init_errno_constants(*Err);
    qore_ns_private::addNamespace(qns, Err);

    QoreNamespace* tns = new QoreNamespace("Qore::Type");
    init_type_constants(*tns);
    qore_ns_private::addNamespace(qns, tns);

    init_QC_Number_constants(qns);

    init_type_constants(qns);
    init_compression_constants(qns);
    init_crypto_constants(qns);
    init_misc_constants(qns);
    init_string_constants(qns);
    init_math_constants(qns);
    init_lib_constants(qns);

    init_string_functions(qns);
    init_time_functions(qns);
    init_lib_functions(qns);
    init_misc_functions(qns);
    init_list_functions(qns);
    init_type_functions(qns);
    init_pwd_functions(qns);
    init_math_functions(qns);
    init_env_functions(qns);
    init_thread_functions(qns);
    init_crypto_functions(qns);
    init_object_functions(qns);
    init_file_functions(qns);
    init_compression_functions(qns);
    init_context_functions(qns);
    init_RangeIterator_functions(qns);

#ifdef DEBUG
    init_debug_functions(qns);
#endif

    qore_ns_private::addNamespace(*this, rpriv->qoreNS);
}

StaticSystemNamespace::~StaticSystemNamespace() {
    ExceptionSink xsink;
    priv->deleteData(true, &xsink);
    priv->purge();
}

// returns 0 for success, non-zero return value means error
int qore_root_ns_private::parseAddMethodToClassIntern(const QoreProgramLocation* loc, const NamedScope& scname, MethodVariantBase* qcmethod, bool static_flag) {
    std::unique_ptr<MethodVariantBase> v(qcmethod);

    // find class
    QoreClass* oc = parseFindScopedClassWithMethodInternError(loc, scname, true);
    if (!oc)
        return -1;

    return qore_class_private::addUserMethod(*oc, scname.getIdentifier(), v.release(), static_flag);
}

// returns 0 for success, non-zero for error
QoreValue qore_root_ns_private::parseResolveBarewordIntern(const QoreProgramLocation* loc, const char* bword,
        const QoreTypeInfo*& typeInfo, bool& found) {
    assert(!found);
    QoreClass* pc = parse_get_class();

    printd(5, "qore_root_ns_private::parseResolveBarewordIntern() bword: %s pc: %p (%s)\n", bword, pc,
        pc ? pc->getName() : "<none>");

    bool abr = (bool)(parse_get_parse_options() & PO_ALLOW_BARE_REFS);

    // if bare refs are enabled, first look for a local variable
    if (abr) {
        bool in_closure;
        LocalVar* id = find_local_var(bword, in_closure);
        if (id) {
            //printd(5, "qore_root_ns_private::parseResolveBarewordIntern() %s is an lvar: %p\n", bword, id);
            typeInfo = id->parseGetTypeInfo();
            found = true;
            return new VarRefNode(loc, strdup(bword), id, in_closure);
        }
    }

    // if there is a current parse class context, then check for class objects
    if (pc) {
        // if bare refs are enabled, check for member reference first
        if (abr && !qore_class_private::parseResolveInternalMemberAccess(pc, bword, typeInfo)) {
            found = true;
            return new SelfVarrefNode(loc, strdup(bword));
        }

        // now try to find a class constant with this name
        QoreValue rv = qore_class_private::parseFindConstantValue(pc, bword, typeInfo, found,
            qore_class_private::get(*pc));
        if (found) {
            return rv.refSelf();
        }

        // now check for class static var reference
        const QoreClass* qc = nullptr;
        ClassAccess access;
        QoreVarInfo* vi = qore_class_private::parseFindStaticVar(pc, bword, qc, access);
        if (vi) {
            assert(qc);
            typeInfo = vi->getTypeInfo();
            found = true;
            //printd(5, "qore_root_ns_private::parseResolveBarewordIntern() resolved '%s' -> static var of class "
            //    "'%s'\n", bword, pc->getName());
            return new StaticClassVarRefNode(loc, bword, *qc, *vi);
        }
    }

    // try to resolve a global variable
    if (abr) {
        Var* v = parseFindGlobalVar(bword);
        if (v) {
            found = true;
            return new GlobalVarRefNode(loc, strdup(bword), v);
        }
    }

    // try to resolve constant
    // first try to find a namespace context
    QoreValue rv{};

    qore_ns_private* nscx = parse_get_ns();
    //printd(5, "qore_root_ns_private::parseResolveBarewordIntern() bword: %s nscx: %p ('%s' root: %d)\n", bword,
    //  nscx, nscx ? nscx->name.c_str() : "n/a", nscx ? nscx->root : false);
    if (nscx) {
        rv = nscx->getConstantValue(bword, typeInfo, found);
        if (found) {
            //printd(5, "qore_root_ns_private::parseResolveBarewordIntern() bword: %s nscx: %p (%s) got rv: %s\n",
            //  bword, nscx, nscx ? nscx->name.c_str() : "n/a", rv.getTypeName());
            return rv.refSelf();
        }
    }

    rv = parseFindOnlyConstantValueIntern(loc, bword, typeInfo, found);
    if (found) {
        return rv.refSelf();
    }

    parse_error(*loc, "cannot resolve bareword '%s' to any reachable object", bword);

    //printd(5, "qore_root_ns_private::parseResolveBarewordIntern() this: %p '%s' abr: %d\n", this, bword, abr);
    return QoreValue();
}

QoreValue qore_root_ns_private::parseResolveReferencedScopedReferenceIntern(const QoreProgramLocation* loc,
        const NamedScope& nscope, const QoreTypeInfo*& typeInfo, bool& found) {
    assert(nscope.size() > 1);
    assert(!found);

    unsigned m = 0;
    QoreValue rv{};

    bool abr = (bool)(parse_get_parse_options() & PO_ALLOW_BARE_REFS);

    {
        // try to check in current namespace first
        qore_ns_private* nscx = parse_get_ns();
        printd(5, "qore_root_ns_private::parseResolveReferencedScopedReferenceIntern(%s) ns: %p (%s)\n", nscope.ostr,
            nscx, nscx ? nscx->name.c_str() : "n/a");
        if (nscx) {
            QoreNamespace* ns = nscx->parseFindLocalNamespace(nscope[0]);
            if (ns) {
                rv = ns->priv->parseCheckScopedReference(loc, nscope, m, typeInfo, found, abr);
                if (found) {
                    printd(5, "qore_root_ns_private::parseResolveReferencedScopedReferenceIntern(%s) found current " \
                        "ns: %s\n", nscope.ostr, rv.getFullTypeName());
                    return rv;
                }
            }
        }
    }

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            printd(5, "qore_root_ns_private::parseResolveReferencedScopedReferenceIntern(%s) ns: %p (%s)\n",
                nscope.ostr, nmi.get(), nmi.get()->name.c_str());
            rv = nmi.get()->parseCheckScopedReference(loc, nscope, m, typeInfo, found, abr);
            if (found) {
                printd(5, "qore_root_ns_private::parseResolveReferencedScopedReferenceIntern(%s) found ns map: %s\n",
                    nscope.ostr, rv.getFullTypeName());
                return rv;
            }
        }
    }

    // now look for class constants if there is only a single namespace or class name in the beginning
    if (nscope.size() == 2) {
        QoreClass* qc = parseFindClassIntern(nscope[0]);
        if (qc) {
            rv = parseResolveReferencedClassConstant(loc, qc, nscope.getIdentifier(), typeInfo, found);
            if (found) {
                printd(5, "qore_root_ns_private::parseResolveReferencedScopedReferenceIntern(%s) found class " \
                    "constant: %s\n", nscope.ostr, rv.getFullTypeName());
                return rv;
            }
        }
    }

    // raise parse exception
    if (m != (nscope.size() - 1)) {
        parse_error(*loc, "cannot find any namespace or class '%s' in '%s' providing a constant or static class " \
            "variable '%s'", nscope[m], nscope.ostr, nscope.getIdentifier());
    } else {
        QoreString err;
        err.sprintf("cannot resolve bareword '%s' to any reachable object in any namespace or class '",
            nscope.getIdentifier());
        for (unsigned i = 0; i < (nscope.size() - 1); i++) {
            err.concat(nscope[i]);
            if (i != (nscope.size() - 2))
                err.concat("::");
        }
        err.concat("'");
        parse_error(*loc, err.c_str());
    }

    printd(5, "RootQoreNamespace::parseResolveReferencedScopedReferenceIntern(%s) not found\n", nscope.ostr);
    return QoreValue();
}

// private
QoreClass* qore_root_ns_private::parseFindScopedClassWithMethodIntern(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 2);

    QoreClass* oc;

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            if ((oc = nmi.get()->parseMatchScopedClassWithMethod(nscope, matched)))
                return oc;
        }
    }

    return nullptr;
}

TypedHashDecl* qore_root_ns_private::parseFindScopedHashDeclIntern(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 1);

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            TypedHashDecl* hd;
            //printd(5, "qore_root_ns_private::parseFindScopedHashDeclIntern(%s) ns: %p (%s)\n", nscope.ostr, nmi.get(), nmi.get()->name.c_str());
            if ((hd = nmi.get()->parseMatchScopedHashDecl(nscope, matched)))
                return hd;
        }
    }

    return nullptr;
}

TypedHashDecl* qore_root_ns_private::parseFindHashDecl(const QoreProgramLocation* loc, const NamedScope& nscope) {
    TypedHashDecl* hd;
    // if there is no namespace specified, then just find class
    if (nscope.size() == 1) {
        hd = parseFindHashDeclIntern(nscope.ostr);
        if (!hd)
            parse_error(*loc, "reference to undefined hashdecl '%s'", nscope.ostr);
        return hd;
    }

    unsigned m = 0;
    hd = parseFindScopedHashDeclIntern(nscope, m);
    if (hd)
        return hd;

    if (m != (nscope.size() - 1))
        parse_error(*loc, "cannot resolve namespace '%s' in '%s'", nscope[m], nscope.ostr);
    else {
        QoreString err;
        err.sprintf("cannot find hashdecl '%s' in any namespace '", nscope.getIdentifier());
        for (unsigned i = 0; i < (nscope.size() - 1); i++) {
            err.concat(nscope[i]);
            if (i != (nscope.size() - 2))
                err.concat("::");
        }
        err.concat("'");
        parse_error(*loc, err.getBuffer());
    }

    printd(5, "qore_root_ns_private::parseFindHashDecl('%s') returning %p\n", nscope.ostr, hd);
    return hd;
}

const QoreEnumDecl* qore_root_ns_private::parseFindScopedEnumIntern(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 1);

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            const QoreEnumDecl* ed;
            if ((ed = nmi.get()->parseMatchScopedEnum(nscope, matched))) {
                return ed;
            }
        }
    }

    return nullptr;
}

TypedefEntry* qore_root_ns_private::parseFindScopedTypedefIntern(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 1);

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            TypedefEntry* td;
            if ((td = nmi.get()->parseMatchScopedTypedef(nscope, matched))) {
                return td;
            }
        }
    }

    return nullptr;
}

const QoreEnumDecl* qore_root_ns_private::parseFindEnum(const QoreProgramLocation* loc, const NamedScope& nscope) {
    const QoreEnumDecl* ed;
    // if there is no namespace specified, then just find enum
    if (nscope.size() == 1) {
        ed = parseFindEnumIntern(nscope.ostr);
        if (!ed) {
            parse_error(*loc, "reference to undefined enum '%s'", nscope.ostr);
        }
        return ed;
    }

    unsigned m = 0;
    ed = parseFindScopedEnumIntern(nscope, m);
    if (ed) {
        return ed;
    }

    if (m != (nscope.size() - 1)) {
        parse_error(*loc, "cannot resolve namespace '%s' in '%s'", nscope[m], nscope.ostr);
    } else {
        QoreString err;
        err.sprintf("cannot find enum '%s' in any namespace '", nscope.getIdentifier());
        for (unsigned i = 0; i < (nscope.size() - 1); i++) {
            err.concat(nscope[i]);
            if (i != (nscope.size() - 2)) {
                err.concat("::");
            }
        }
        err.concat("'");
        parse_error(*loc, err.getBuffer());
    }

    return ed;
}

const QoreClass* qore_root_ns_private::runtimeFindScopedClassWithMethod(const NamedScope& scname) const {
    // must have at least 2 elements
    assert(scname.size() > 1);

    if (scname.size() == 2) {
        return runtimeFindClass(scname[0]);
    }

    return runtimeFindScopedClassWithMethodIntern(scname);
}

const QoreClass* qore_root_ns_private::runtimeFindScopedClassWithMethodIntern(const NamedScope& nscope) const {
    assert(nscope.size() > 2);

    // iterate all namespaces with the initial name and look for the match
    {
        ConstNamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            const QoreClass* oc;
            if ((oc = nmi.get()->runtimeMatchScopedClassWithMethod(nscope)))
                return oc;
        }
    }

    return nullptr;
}

const QoreClass* qore_root_ns_private::runtimeFindScopedClass(const NamedScope& nscope) const {
    if (nscope.size() == 1)
        return runtimeFindClass(nscope.ostr);

    // iterate all namespaces with the initial name and look for the match
    {
        ConstNamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            const QoreClass* oc;
            //printd(5, "qore_root_ns_private::runtimeFindScopedClass(%s) ns: %p (%s)\n", nscope.ostr, nmi.get(), nmi.get()->name.c_str());
            const qore_ns_private* ns;
            if ((oc = nmi.get()->runtimeMatchClass(nscope, ns)))
                return oc;
        }
    }

    return nullptr;
}

QoreClass* qore_root_ns_private::parseFindScopedClassIntern(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 1);

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            QoreClass* oc;
            //printd(5, "qore_root_ns_private::parseFindScopedClassIntern(%s) ns: %p (%s) matched: %d\n", nscope.ostr, nmi.get(), nmi.get()->name.c_str(), matched);
            if ((oc = nmi.get()->parseMatchScopedClass(nscope, matched)))
                return oc;
        }
    }

    return nullptr;
}

QoreClass* qore_root_ns_private::parseFindScopedClassIntern(const QoreProgramLocation* loc, const NamedScope& nscope, bool raise_error) {
    QoreClass* oc;
    // if there is no namespace specified, then just find class
    if (nscope.size() == 1) {
        oc = parseFindClassIntern(nscope.ostr);
        if (!oc && raise_error) {
            parse_error(*loc, "reference to undefined class '%s'", nscope.ostr);
        }
        return oc;
    }

    unsigned m = 0;
    oc = parseFindScopedClassIntern(nscope, m);
    if (oc)
        return oc;

    if (raise_error) {
        if (m != (nscope.size() - 1)) {
            parse_error(*loc, "cannot resolve namespace '%s' in '%s'", nscope[m], nscope.ostr);
        } else {
            QoreString err;
            err.sprintf("cannot find class '%s' in any namespace '", nscope.getIdentifier());
            for (unsigned i = 0; i < (nscope.size() - 1); i++) {
                err.concat(nscope[i]);
                if (i != (nscope.size() - 2))
                    err.concat("::");
            }
            err.concat("'");
            parse_error(*loc, err.getBuffer());
        }
    }

    printd(5, "qore_root_ns_private::parseFindScopedClassIntern('%s') returning %p\n", nscope.ostr, oc);
    return oc;
}

QoreClass* qore_root_ns_private::parseFindScopedClassWithMethodInternError(const QoreProgramLocation* loc, const NamedScope& scname, bool error) {
    // must have at least 2 elements
    assert(scname.size() > 1);

    QoreClass* oc;

    if (scname.size() == 2) {
        oc = parseFindClassIntern(scname[0]);
        if (!oc && error)
            parse_error(*loc, "reference to undefined class '%s' in '%s()'", scname[0], scname.ostr);
        return oc;
    }

    unsigned m = 0;
    oc = parseFindScopedClassWithMethodIntern(scname, m);
    if (!oc && error) {
        if (m >= (scname.size() - 2))
            parse_error(*loc, "cannot resolve class '%s' in '%s()'", scname[m], scname.ostr);
        else  {
            QoreString err;
            err.sprintf("cannot find class '%s' in any namespace '", scname[scname.size() - 2]);
            for (unsigned i = 0; i < (scname.size() - 2); i++) {
                err.concat(scname.get(i));
                if (i != (scname.size() - 3))
                    err.concat("::");
            }
            err.concat("'");
            parse_error(*loc, err.getBuffer());
        }
    }

    printd(5, "qore_ns_private::parseFindScopedClassWithMethodIntern('%s') returning %p\n", scname.ostr, oc);
    return oc;
}

// called in 2nd stage of parsing to resolve constant references
QoreValue qore_root_ns_private::parseFindReferencedConstantValueIntern(const QoreProgramLocation* loc, const NamedScope& scname, const QoreTypeInfo*& typeInfo, bool& found, bool error) {
    assert(!found);
    if (scname.size() == 1) {
        QoreValue rv = parseFindConstantValueIntern(loc, scname.ostr, typeInfo, found, error);
        return found ? rv.refSelf() : QoreValue();
    }

    QoreValue rv{};
    unsigned m = 0;

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, scname[0]);
        while (nmi.next()) {
            //printd(5, "qore_root_ns_private::parseFindConstantValueIntern(%s) ns: %p (%s)\n", nscope.ostr, nmi.get(), nmi.get()->name.c_str());
            rv = nmi.get()->parseMatchScopedConstantValue(scname, m, typeInfo, found);
            if (found) {
                return rv.refSelf();
            }
        }
    }

    // look for a class constant if there are only 2 elements in the scope list
    if (scname.size() == 2) {
        QoreClass* qc = parseFindClassIntern(scname[0]);
        if (qc) {
            rv = parseResolveReferencedClassConstant(loc, qc, scname.getIdentifier(), typeInfo, found);
            if (found) {
                return rv;
            }
        }
    }

    if (!error)
        return QoreValue();

    if (m != (scname.size() - 1)) {
        parse_error(*loc, "cannot resolve namespace '%s' in constant reference '%s'", scname[m], scname.ostr);
    } else {
        QoreString err;
        err.sprintf("cannot find constant '%s' in any namespace '", scname.getIdentifier());
        for (unsigned i = 0; i < (scname.size() - 1); i++) {
            err.concat(scname[i]);
            if (i != (scname.size() - 2))
                err.concat("::");
        }
        err.concat("'");
        parse_error(*loc, err.getBuffer());
    }

    return QoreValue();
}

void qore_root_ns_private::parseAddHashDeclIntern(const QoreProgramLocation* loc, const NamedScope& name, TypedHashDecl* hd) {
    qore_ns_private* sns = parseResolveNamespace(loc, name);

    if (sns) {
        //printd(5, "qore_root_ns_private::parseAddHashDeclIntern() '%s' adding %s:%p to %s:%p\n", nscope.ostr, hd->getName(), parseAddHashDeclIntern, sns->name.c_str(), sns);
        // add to pending hashdecl map if add was successful
        if (!sns->parseAddPendingHashDecl(loc, hd)) {
            thdmap.update(hd->getName(), sns, hd);
        }
    } else {
        //printd(5, "qore_root_ns_private::parseAddHashDeclIntern() hashdecl '%s' not added: '%s' namespace not found\n", hd->getName(), nscope.ostr);
        typed_hash_decl_private::get(*hd)->deref();
    }
}

void qore_root_ns_private::parseAddEnumIntern(const QoreProgramLocation* loc, const NamedScope& name, QoreEnumDecl* ed) {
    qore_ns_private* sns = parseResolveNamespace(loc, name);

    if (sns) {
        // add to pending enum map if add was successful
        if (!sns->parseAddPendingEnum(loc, ed)) {
            edmap.update(ed->getName(), sns, ed);
        }
    } else {
        qore_enum_decl_private::get(*ed)->deref();
    }
}

// only called with RootNS
void qore_root_ns_private::parseAddClassIntern(const QoreProgramLocation* loc, const NamedScope& nscope, QoreClass* oc) {
    QORE_TRACE("qore_root_ns_private::parseAddClassIntern()");

    qore_ns_private* sns = parseResolveNamespace(loc, nscope);

    if (sns) {
        //printd(5, "qore_root_ns_private::parseAddClassIntern() '%s' adding %s:%p to %s:%p\n", nscope.ostr, oc->getName(), oc, sns->name.c_str(), sns);
        // add to pending class map if add was successful
        if (!sns->parseAddPendingClass(loc, oc))
            clmap.update(oc->getName(), sns, oc);
    } else {
        //printd(5, "qore_root_ns_private::parseAddClassIntern() class '%s' not added: '%s' namespace not found\n", oc->getName(), nscope.ostr);
        qore_class_private::get(*oc)->deref(true, true);
    }
}

void qore_root_ns_private::addConstant(qore_ns_private& ns, const char* cname, QoreValue val, const QoreTypeInfo* typeInfo) {
   cnemap_t::iterator i = ns.constant.add(cname, val, typeInfo);
   if (i == ns.constant.end())
      return;

   cnmap.update(i->first, &ns, i->second);
}

void qore_root_ns_private::parseAddConstantIntern(const QoreProgramLocation* loc, QoreNamespace& ns, const NamedScope& name, QoreValue value, bool cpub) {
    ValueHolder vh(value, 0);

    QoreNamespace* sns = ns.priv->resolveNameScope(loc, name);
    if (!sns)
        return;

    const char* cname = name.get(name.size() - 1);
    cnemap_t::iterator i = sns->priv->parseAddConstant(loc, cname, vh.release(), cpub);
    if (i == sns->priv->constant.end())
        return;

    cnmap.update(i->first, sns->priv, i->second);
}

qore_ns_private* qore_root_ns_private::parseResolveNamespaceIntern(const QoreProgramLocation* loc, const NamedScope& nscope, qore_ns_private* sns) {
    assert(nscope.size() > 1);

    unsigned match = 0;

    // try to check in current namespace first
    if (sns) {
        QoreNamespace* tns = sns->parseFindLocalNamespace(nscope[0]);
        if (tns && (tns = tns->priv->parseMatchNamespace(nscope, match)))
            return tns->priv;
    }

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            QoreNamespace* tns = nmi.get()->parseMatchNamespace(nscope, match);
            if (tns)
                return tns->priv;
        }
    }

    parse_error(*loc, "cannot resolve namespace '%s' in '%s'", nscope[match], nscope.ostr);
    return nullptr;
}

qore_ns_private* qore_root_ns_private::parseResolveNamespace(const QoreProgramLocation* loc, const NamedScope& n, qore_ns_private* sns) {
    if (n.size() == 1)
        return sns ? sns : this;

    return parseResolveNamespaceIntern(loc, n, sns);
}

qore_ns_private* qore_root_ns_private::parseResolveNamespace(const QoreProgramLocation* loc, const NamedScope& nscope) {
    if (nscope.size() == 1)
        return this;

    return parseResolveNamespaceIntern(loc, nscope, parse_get_ns());
}

Var* qore_root_ns_private::runtimeFindGlobalVar(const NamedScope& name, const qore_ns_private*& ns) const {
    assert(name.size() > 1);

    ConstNamespaceMapIterator nmi(nsmap, name.get(0));
    while (nmi.next()) {
        Var* v;
        if ((v = nmi.get()->runtimeMatchGlobalVar(name, ns)))
            return v;
    }

    return nullptr;
}

const ConstantEntry* qore_root_ns_private::runtimeFindNamespaceConstant(const NamedScope& name, const qore_ns_private*& cns) const {
    assert(name.size() > 1);

    ConstNamespaceMapIterator nmi(nsmap, name.get(0));
    while (nmi.next()) {
        const ConstantEntry* ce;
        if ((ce = nmi.get()->runtimeMatchNamespaceConstant(name, cns)))
            return ce;
    }

    return nullptr;
}

const QoreClass* qore_root_ns_private::runtimeFindClassIntern(const NamedScope& name, const qore_ns_private*& ns) const {
    assert(name.size() > 1);

    // iterate all namespaces with the initial name and look for the match
    const QoreClass* c = nullptr;
    ConstNamespaceMapIterator nmi(nsmap, name.get(0));
    while (nmi.next()) {
        if ((c = nmi.get()->runtimeMatchClass(name, ns)))
            return c;
    }

    return nullptr;
}

class_vec_t qore_root_ns_private::runtimeFindAllClassesRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    class_vec_t class_vec;

    QoreRegex re(pattern, re_opts, xsink);
    if (*xsink) {
        return class_vec;
    }

    for (auto& i : clmap) {
        const qore_class_private* qc = qore_class_private::get(*i.second.obj);
        if (re.exec(qc->name.c_str(), qc->name.size())) {
            class_vec.push_back(i.second.obj);
        }
    }

    return class_vec;
}

hashdecl_vec_t qore_root_ns_private::runtimeFindAllHashDeclsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    hashdecl_vec_t hashdecl_vec;

    QoreRegex re(pattern, re_opts, xsink);
    if (*xsink) {
        return hashdecl_vec;
    }

    for (auto& i : thdmap) {
        const typed_hash_decl_private* th = typed_hash_decl_private::get(*i.second.obj);
        const std::string& name = th->getNameStr();
        if (re.exec(name.c_str(), name.size())) {
            hashdecl_vec.push_back(hashdecl_vec_t::value_type(i.second.obj, i.second.ns->ns));
        }
    }

    return hashdecl_vec;
}

func_vec_t qore_root_ns_private::runtimeFindAllFunctionsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    func_vec_t func_vec;

    QoreRegex re(pattern, re_opts, xsink);
    if (*xsink) {
        return func_vec;
    }

    for (auto& i : fmap) {
        const char* name = i.second.obj->getName();
        size_t len = strlen(name);
        if (re.exec(name, len)) {
            func_vec.push_back(reinterpret_cast<const QoreExternalFunction*>(i.second.obj->getFunction()));
        }
    }

    return func_vec;
}

ns_vec_t qore_root_ns_private::runtimeFindAllNamespacesRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    ns_vec_t ns_vec;

    QoreRegex re(pattern, re_opts, xsink);
    if (*xsink) {
        return ns_vec;
    }

    ConstAllNamespacesIterator i(nsmap);
    while (i.next()) {
        const qore_ns_private* ns = i.get()->priv;
        if (re.exec(ns->name.c_str(), ns->name.size())) {
            ns_vec.push_back(i.get());
        }
    }

    return ns_vec;
}

gvar_vec_t qore_root_ns_private::runtimeFindAllGlobalVarsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    gvar_vec_t gvar_vec;

    QoreRegex re(pattern, re_opts, xsink);
    if (*xsink) {
        return gvar_vec;
    }

    for (auto& i : varmap) {
        Var* v = i.second.obj;
        const std::string& name = v->getNameStr();
        if (re.exec(name.c_str(), name.size())) {
            gvar_vec.push_back(gvar_vec_t::value_type(reinterpret_cast<const QoreExternalGlobalVar*>(v), i.second.ns->ns));
        }
    }

    return gvar_vec;
}

const_vec_t qore_root_ns_private::runtimeFindAllNamespaceConstantsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    const_vec_t const_vec;

    QoreRegex re(pattern, re_opts, xsink);
    if (*xsink) {
        return const_vec;
    }

    for (auto& i : cnmap) {
        const ConstantEntry* ce = i.second.obj;
        if (re.exec(ce->name.c_str(), ce->name.size())) {
            const_vec.push_back(const_vec_t::value_type(reinterpret_cast<const QoreExternalConstant*>(ce), i.second.ns->ns));
        }
    }

    return const_vec;
}

const TypedHashDecl* qore_root_ns_private::runtimeFindHashDeclIntern(const NamedScope& name, const qore_ns_private*& ns) {
    if (name.size() == 1)
        return runtimeFindHashDeclIntern(name.ostr, ns);

    // iterate all namespaces with the initial name and look for the match
    const TypedHashDecl* c = nullptr;
    NamespaceMapIterator nmi(nsmap, name.get(0));
    while (nmi.next()) {
        if ((c = nmi.get()->runtimeMatchHashDecl(name, ns)))
            return c;
    }

    return nullptr;
}

const QoreEnumDecl* qore_root_ns_private::runtimeFindEnumIntern(const NamedScope& name, const qore_ns_private*& ns) {
    if (name.size() == 1) {
        return runtimeFindEnumIntern(name.ostr, ns);
    }

    // iterate all namespaces with the initial name and look for the match
    const QoreEnumDecl* e = nullptr;
    NamespaceMapIterator nmi(nsmap, name.get(0));
    while (nmi.next()) {
        if ((e = nmi.get()->runtimeMatchEnum(name, ns))) {
            return e;
        }
    }

    return nullptr;
}

const FunctionEntry* qore_root_ns_private::runtimeFindFunctionEntryIntern(const NamedScope& name) {
    assert(name.size() > 1);

    // iterate all namespaces with the initial name and look for the match
    const FunctionEntry* f = nullptr;
    NamespaceMapIterator nmi(nsmap, name.get(0));
    while (nmi.next()) {
        if ((f = nmi.get()->runtimeMatchFunctionEntry(name)))
            return f;
    }

    return nullptr;
}

const FunctionEntry* qore_root_ns_private::parseResolveFunctionEntryIntern(const NamedScope& nscope) {
    assert(nscope.size() > 1);

    const FunctionEntry* f = nullptr;
    unsigned match = 0;

    {
        // try to check in current namespace first
        qore_ns_private* nscx = parse_get_ns();
        if (nscx) {
            QoreNamespace* ns = nscx->parseFindLocalNamespace(nscope[0]);
            if (ns && (f = ns->priv->parseMatchFunctionEntry(nscope, match)))
                return f;
        }
    }

    // iterate all namespaces with the initial name and look for the match
    {
        NamespaceMapIterator nmi(nsmap, nscope[0]);
        while (nmi.next()) {
            if ((f = nmi.get()->parseMatchFunctionEntry(nscope, match)))
                return f;
        }
    }

    return nullptr;
}

void qore_root_ns_private::parseWarnAmbiguousFunctionCall(const QoreProgramLocation* loc, const char* fname,
        const FunctionEntry* resolved) const {
    assert(loc);
    assert(fname);
    assert(resolved);

    if (parse_check_parse_option(PO_IN_MODULE)) {
        return;
    }

    const qore_ns_private* resolved_ns = resolved->getNamespace();
    if (!parse_check_parse_option(PO_BROKEN_NAMESPACE_RESOLUTION)) {
        const qore_ns_private* qore_ns = getQore();
        if (resolved_ns && qore_ns) {
            for (const qore_ns_private* cur = resolved_ns; cur; cur = cur->parent) {
                if (cur == qore_ns) {
                    return;
                }
            }
        }
    }
    qore_ns_private* nscx = parse_get_ns();
    if (nscx && resolved_ns && nscx == resolved_ns) {
        return;
    }

    std::vector<std::string> match_paths;
    ConstAllNamespacesIterator nsi(nsmap);
    while (nsi.next()) {
        const QoreNamespace* ns = nsi.get();
        if (!ns) {
            continue;
        }
        const qore_ns_private* ns_priv = qore_ns_private::get(*ns);
        if (!ns_priv->func_list.findNode(fname)) {
            continue;
        }
        std::string path;
        ns_priv->getPath(path, false, true);
        match_paths.push_back(path);
    }

    if (match_paths.size() <= 1) {
        return;
    }

    std::string resolved_path;
    if (resolved_ns) {
        resolved_ns->getPath(resolved_path, false, true);
    }

    std::sort(match_paths.begin(), match_paths.end());
    QoreString matches;
    for (size_t i = 0; i < match_paths.size(); ++i) {
        if (i) {
            matches.concat(", ");
        }
        matches.sprintf("'%s%s()'", match_paths[i].c_str(), fname);
    }

    QoreStringNode* desc = new QoreStringNode;
    if (!resolved_path.empty()) {
        desc->sprintf("call to function '%s()' is ambiguous; matches: %s; resolved to '%s%s()'; use a "
            "fully-qualified namespace path in the call or rename the object to remove ambiguity", fname,
            matches.c_str(), resolved_path.c_str(), fname);
    } else {
        desc->sprintf("call to function '%s()' is ambiguous; matches: %s; use a fully-qualified namespace path in "
            "the call or rename the object to remove ambiguity", fname, matches.c_str());
    }

    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_AMBIGUOUS_CALL_RESOLUTION,
        "AMBIGUOUS-CALL-RESOLUTION", desc);
}

AbstractCallReferenceNode* qore_root_ns_private::parseResolveCallReferenceIntern(
        UnresolvedProgramCallReferenceNode* fr) {
    std::unique_ptr<UnresolvedProgramCallReferenceNode> fr_holder(fr);
    char* fname = fr->str;

    FunctionEntry* fe = parseFindFunctionEntryIntern(fname);
    if (fe) {
        if (parse_check_parse_option(PO_REQUIRE_TYPES)) {
            parseWarnAmbiguousFunctionCall(fr->loc, fname, fe);
        }
        // check parse options to see if access is allowed
        if (!qore_program_private::parseAddDomain(getProgram(), fe->getFunction()->parseGetUniqueFunctionality()))
            return fe->makeCallReference(fr->loc);
        parse_error(*fr->loc, "parse options do not allow access to function '%s'", fname);
    } else { // cannot find function, throw exception
        parse_error(*fr->loc, "reference to function '%s()' cannot be resolved", fname);
    }

    return fr_holder.release();
}

static void get_params(const QoreListNode* params, QoreString& desc) {
    ConstListIterator li(params);
    while (li.next()) {
        QoreValue v = li.getValue();
        if (v.getType() != NT_STRING) {
            desc.concat("<unknown>");
        }
        else {
            desc.concat(v.get<QoreStringNode>()->c_str());
        }
        if (!li.last())
            desc.concat(", ");
    }
}

const AbstractQoreFunctionVariant* qore_root_ns_private::runtimeFindCall(const char* name, const QoreListNode* params, ExceptionSink* xsink) {
    // function or method
    const QoreFunction* qf = nullptr;

    // class context
    const QoreClass* qc = nullptr;

    // resolve call to function or method
    if (!strstr(name, "::")) {
        {
            const FunctionEntry* fe = runtimeFindFunctionEntryIntern(name);
            if (fe) {
                qf = fe->getFunction();
            }
        }

        if (!qf) {
            QoreStringNode* desc = new QoreStringNodeMaker("function call \"%s(", name);
            get_params(params, *desc);
            desc->concat(")\" cannot be resolved to any accessible function");
            xsink->raiseException("FIND-CALL-ERROR", desc);
            return nullptr;
        }
    } else {
        const QoreMethod* method = nullptr;

        NamedScope scope(name);
        qc = runtimeFindScopedClassWithMethod(scope);
        //printd(5, "qore_root_ns_private::runtimeFindCall() '%s' qc: %p\n", scope.ostr, qc);
        if (qc) {
            method = qore_class_private::get(*qc)->runtimeFindAnyCommittedMethod(scope.getIdentifier());
            //printd(5, "qore_root_ns_private::runtimeFindCall() '%s' qc: %p '%s' -> %p\n", scope.ostr, qc, scope.getIdentifier(), method);
            if (method)
                qf = qore_method_private::get(*method)->getFunction();
            else
                qc = nullptr;
        }

        if (!method) {
            // see if this is a function call to a function defined in a namespace
            {
                const FunctionEntry* fe = runtimeFindFunctionEntryIntern(scope);
                if (fe) {
                    qf = fe->getFunction();
                }
            }

            if (!qf) {
                QoreStringNode* desc = new QoreStringNodeMaker("scoped call \"%s(", name);
                get_params(params, *desc);
                desc->concat(")\" cannot be resolved to any accessible function or class method");
                xsink->raiseException("FIND-CALL-ERROR", desc);
                return nullptr;
            }
        }
    }

    assert(qf);

    // convert params to real param list
    type_vec_t tvec;
    if (params && !params->empty()) {
        // resolve types
        tvec.reserve(params->size());
        ConstListIterator li(params);
        while (li.next()) {
            QoreValue v = li.getValue();
            if (v.getType() != NT_STRING) {
                xsink->raiseException("FIND-CALL-ERROR", "call \"%s()\" parameter %lu given as type \"%s\"; " \
                    "expecting \"string\"", name, li.index() + 1, v.getTypeName());
                return nullptr;
            }
            // get string value for type
            const QoreString& tname = *v.get<const QoreStringNode>();
            // issue #2601: ensure that the string is not empty (client error)
            if (tname.empty()) {
                xsink->raiseException("FIND-CALL-ERROR", "call \"%s()\" parameter %lu is an empty string", name,
                    li.index() + 1);
                return nullptr;
            }
            // look up type from string
            const QoreTypeInfo* ti = qore_get_type_from_string_intern(tname.c_str());
            if (!ti) {
                xsink->raiseException("FIND-CALL-ERROR", "call \"%s()\" parameter %lu \"%s\" cannot be " \
                    "resolved to a known type", name, li.index() + 1, tname.c_str());
                return nullptr;
            }
            tvec.push_back(ti);
        }
    }

    return qf->runtimeFindExactVariant(xsink, tvec, qc ? qore_class_private::get(*qc) : nullptr);
}

QoreListNode* qore_root_ns_private::runtimeFindCallVariants(const char* name, ExceptionSink* xsink) {
    // function or method
    const QoreFunction* qf = nullptr;

    // class context
    const QoreClass* qc = nullptr;

    // resolve call to function or method
    if (!strstr(name, "::")) {
        const FunctionEntry* fe = runtimeFindFunctionEntryIntern(name);
        if (!fe)
            return nullptr;
        qf = fe->getFunction();
    } else {
        const QoreMethod* method = nullptr;

        NamedScope scope(name);
        qc = runtimeFindScopedClassWithMethod(scope);
        if (qc) {
            // issue #1865: find any method including special methods
            method = qore_class_private::get(*qc)->runtimeFindAnyCommittedMethod(scope.getIdentifier());
            //printd(5, "qore_root_ns_private::runtimeFindCallVariants() %s: qc: %p method: %p\n", scope.ostr, qc, method);
            if (method)
                qf = qore_method_private::get(*method)->getFunction();
            else
                qc = nullptr;
        }

        if (!method) {
            // see if this is a function call to a function defined in a namespace
            const FunctionEntry* fe = runtimeFindFunctionEntryIntern(scope);
            if (!fe)
                return nullptr;
            qf = fe->getFunction();
        }
    }

    assert(qf);
    return qf->runtimeGetCallVariants();
}

int qore_ns_private::parseInitGlobalVars() {
    int err = var_list.parseInit();
    if (nsl.parseInitGlobalVars() && !err) {
        err = -1;
    }
    return err;
}

void qore_ns_private::clearConstants(QoreListNode& l) {
    // clear constants
    constant.clear(l);
    // clear/finalize class constants
    classList.clearConstants(l);

    nsl.clearConstants(l);

    // clear properties
    AutoLocker al(kvlck);
    for (auto& i : kvmap) {
        if (i.second.hasNode()) {
            l.push(i.second, nullptr);
        }
    }
    kvmap.clear();
}

void qore_ns_private::clearData(ExceptionSink* xsink) {
    // clear/finalize global variables
    var_list.clearAll(xsink);
    // clear/finalize static class vars
    classList.clear(xsink);

    nsl.clearData(xsink);
}

/*
void qore_ns_private::deleteClearData(ExceptionSink* xsink) {
    // clear all constants
    constant.deleteAll(xsink);
    // clear all constants and static class vars
    classList.deleteClearData(xsink);
    // clear all user functions
    func_list.del();
    // delete all global variables
    var_list.deleteAll(xsink);

    // repeat for all subnamespaces
    nsl.deleteClearData(xsink);
}
*/

void qore_ns_private::deleteData(bool deref_vars, ExceptionSink* xsink) {
    // clear all constants
    constant.deleteAll(xsink);
    // clear all constants and static class vars
    classList.deleteClassData(deref_vars, xsink);
    // clear all user functions
    func_list.del();
    // delete all global variables
    var_list.deleteAll(xsink);

    // repeat for all subnamespaces
    nsl.deleteData(deref_vars, xsink);
}

int qore_ns_private::checkGlobalVarDecl(Var* v, const NamedScope& vname) {
    int err = 0;
    QoreParseOptions po = parse_get_parse_options();
    if ((po & PO_NO_GLOBAL_VARS) && v->isGlobal()) {
        parse_error(*v->getParseLocation(), "illegal reference to new global variable '%s' (conflicts with parse " \
            "option NO_GLOBAL_VARS)", vname.ostr);
        err = -1;
    }
    if ((po & PO_NO_THREAD_CONTROL) && v->isThreadLocal()) {
        parse_error(*v->getParseLocation(), "illegal reference to new thread_local variable '%s' (conflicts with parse " \
            "option NO_THREAD_CONTROL)", vname.ostr);
        if (!err) {
            err = -1;
        }
    }

    if (!v->hasTypeInfo() && (po & PO_REQUIRE_TYPES)) {
        parse_error(*v->getParseLocation(), "%s variable '%s' declared without type information, but parse " \
            "options require all declarations to have type information",
            v->isThreadLocal() ? "thread_local" : "global", vname.ostr);
        if (!err) {
            err = -1;
        }
    }

    if (!imported && !pub && v->isPublic() && (po & PO_IN_MODULE)) {
        qore_program_private::makeParseWarning(getProgram(), *v->getParseLocation(), QP_WARN_INVALID_OPERATION,
            "INVALID-OPERATION", "%s variable '%s::%s' is declared public but the enclosing namespace '%s::' " \
            "is not public", v->isThreadLocal() ? "thread_local" : "global", name.c_str(), v->getName(),
            name.c_str());
    }
    return err;
}

void qore_ns_private::parseAddGlobalVarDecl(const QoreProgramLocation* loc, char* name, const QoreTypeInfo* typeInfo,
        QoreParseTypeInfo* parseTypeInfo, bool pub, qore_var_t type) {
    GVEntryBase e(loc, name, typeInfo, parseTypeInfo, type);
    if (pub)
        e.var->setPublic();
    pend_gvblist.push_back(e);
    //printd(5, "qore_ns_private::parseAddGlobalVarDecl() this: %p var: %p '%s' %d-%d\n", this, e.var, e.var->getName(), loc.start_line, loc.end_line);

    checkGlobalVarDecl(e.var, *e.name);
}

int qore_root_ns_private::parseInit() {
    int err = qore_ns_private::parseInitGlobalVars();
    if (qore_ns_private::parseInitConstants() && !err) {
        err = -1;
    }
    if (qore_ns_private::parseInit() && !err) {
        err = -1;
    }
    qore_ns_private::parseResolveAbstract();

    if (!deferred_new_check_vec.empty()) {
        for (auto& i : deferred_new_check_vec) {
            i.qc->parseDoCheckAbstractNew(i.loc);
        }
        deferred_new_check_vec.clear();
    }

    parseResolveClassMembers();
    return err;
}

bool qore_root_ns_private::parseResolveGlobalVarsAndClassHierarchiesIntern() {
    bool ok = addGlobalVars(*this);

    for (gvlist_t::iterator i = pend_gvlist.begin(), e = pend_gvlist.end(); i != e; ++i) {
        // resolve namespace
        const NamedScope& n = *((*i).name);

        const QoreProgramLocation* loc = (*i).var->getParseLocation();

        // find the namespace
        qore_ns_private* tns = parseResolveNamespace(loc, n, (*i).ns);
        if (!tns)
            continue;

        Var* v = tns->var_list.parseFindVar(n.getIdentifier());
        if (v) {
            QoreString locstr;
            v->getParseLocation()->toString(locstr);
            parse_error(*loc, "global variable '%s::%s' has been %s this Program object multiple times (last "
                "detinition at %s)",
                tns->name.c_str(), n.getIdentifier(), v->isRef() ? "imported into" : "declared in", locstr.c_str());
            if (ok) {
                ok = false;
            }
            continue;
        }

        v = (*i).takeVar();
        //printd(5, "qore_root_ns_private::parseResolveGlobalVarsAndClassHierarchiesIntern() resolved '%s::%s' "
        //    "('%s') %p ns\n", tns->name.c_str(), n.getIdentifier(), n.ostr, v);
        tns->var_list.parseAdd(v);
        varmap.update(v->getName(), tns, v);
    }
    pend_gvlist.clear();

    if (ok) {
        parseResolveHierarchy();
    }

    return ok;
}

Var* qore_root_ns_private::parseAddResolvedGlobalVarDefIntern(const QoreProgramLocation* loc, const NamedScope& vname,
        const QoreTypeInfo* typeInfo, qore_var_t type) {
    Var* v = new Var(loc, vname.getIdentifier(), typeInfo, false, type == VT_THREAD_LOCAL);
    pend_gvlist.push_back(GVEntry(this, vname, v));

    checkGlobalVarDecl(v, vname);
    return v;
}

Var* qore_root_ns_private::parseAddGlobalVarDefIntern(const QoreProgramLocation* loc, const NamedScope& vname,
        QoreParseTypeInfo* typeInfo, qore_var_t type) {
    Var* v = new Var(loc, vname.getIdentifier(), typeInfo, type == VT_THREAD_LOCAL);
    pend_gvlist.push_back(GVEntry(this, vname, v));

    checkGlobalVarDecl(v, vname);
    return v;
}

Var* qore_root_ns_private::parseCheckImplicitGlobalVarIntern(const QoreProgramLocation* loc, const NamedScope& vname,
        const QoreTypeInfo* typeInfo) {
    Var* rv;

    qore_ns_private* tns;
    if (vname.size() == 1) {
        rv = parseFindGlobalVarIntern(vname.ostr);
        // for backwards-compatibility, assume the root namespace for all unscoped global variables
        tns = this;
    } else {
        tns = parseResolveNamespace(loc, vname);
        if (!tns)
            tns = this;
        rv = tns->var_list.parseFindVar(vname.getIdentifier());
    }

    //printd(5, "qore_root_ns_private::parseCheckImplicitGlobalVar() this: %p '%s' rv: %p (omq: %p)\n", this,
    //  vname.ostr, rv, parseFindGlobalVarIntern("omq"));
    if (!rv) {
        // check for errors & warnings for implicit global variables
        QoreParseOptions po = parse_get_parse_options();

        // check if unflagged global vars are allowed
        if (po & PO_REQUIRE_OUR) {
            parseException(*loc, "UNDECLARED-GLOBAL-VARIABLE", "global variable '%s' must first be declared with " \
                "'our' (conflicts with parse option REQUIRE_OUR)", vname.ostr);
        } else if (po & PO_NO_GLOBAL_VARS) { // check if new global variables are allowed to be created at all
            parseException(*loc, "ILLEGAL-GLOBAL-VARIABLE", "illegal reference to new global variable '%s' " \
                "(conflicts with parse option NO_GLOBAL_VARS)", vname.ostr);
        } else {
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_UNDECLARED_VAR,
                "UNDECLARED-GLOBAL-VARIABLE", "global variable '%s' should be explicitly declared with 'our'",
                vname.ostr);
        }

        assert(!tns->var_list.parseFindVar(vname.getIdentifier()));
        rv = tns->var_list.parseCreatePendingVar(loc, vname.getIdentifier(), typeInfo);
        varmap.update(rv->getName(), this, rv);
    } else
        rv->checkAssignType(loc, typeInfo);

    return rv;
}

void qore_root_ns_private::parseAddNamespaceIntern(QoreNamespace* nns) {
    qore_ns_private* ns = qore_ns_private::parseAddNamespace(nns);
    // issue #2735: do not reindex namespaces where parse errors have occurred
    if (!ns || qore_program_private::get(*getProgram())->parseExceptionRaised())
        return;

    // add all objects to the new (or assimilated) namespace

    //printd(5, "qore_root_ns_private::parseAddNamespaceIntern() this: %p ns: %p\n", this, ns);

    /*
    // take global variable decls
    for (unsigned i = 0; i < ns->pend_gvblist.size(); ++i) {
        //printd(5, "qore_root_ns_private::parseAddNamespaceIntern() merging global var decl '%s::%s' into the root list\n", ns->name.c_str(), ns->pend_gvblist[i].name->ostr);
        pend_gvlist.push_back(GVEntry(ns->pend_gvblist[i], ns));
    }
    ns->pend_gvblist.zero();
    */

    {
        QorePrivateNamespaceIterator qpni(ns);
        while (qpni.next())
            parseRebuildIndexes(qpni.get());
    }
}

int qore_ns_private::parseInitConstants() {
   printd(5, "qore_ns_private::parseInitConstants() %s\n", name.c_str());

   NamespaceParseContextHelper nspch(this);

   // do 2nd stage parse initialization on new constants
   int err = constant.parseInit();
   if (nsl.parseInitConstants() && !err) {
       err = -1;
   }
   return err;
}

int qore_ns_private::parseInit() {
    printd(5, "qore_ns_private::parseInit() this: %p ns: %p\n", this, ns);

    // do 2nd stage parse initialization on classes
    int err = classList.parseInit();

    // do 2nd stage parse initialization on pending hashdecls
    if (hashDeclList.parseInit() && !err) {
        err = -1;
    }

    {
        NamespaceParseContextHelper nspch(this);

        // do 2nd stage parse initialization on user functions
        if (func_list.parseInit() && !err) {
            err = -1;
        }
    }

    // do 2nd stage parse initialization in subnamespaces
    if (nsl.parseInit() && !err) {
        err = -1;
    }
    return err;
}

void qore_ns_private::parseResolveHierarchy() {
    classList.parseResolveHierarchy();
    nsl.parseResolveHierarchy();
}

void qore_ns_private::parseResolveClassMembers() {
    classList.parseResolveClassMembers();
    nsl.parseResolveClassMembers();
}

void qore_ns_private::parseResolveAbstract() {
    classList.parseResolveAbstract();
    nsl.parseResolveAbstract();
}

void qore_ns_private::parseCommit() {
    // merge pending user functions
    func_list.parseCommit();

    // commit pending changes to committed classes
    classList.parseCommit();

    // parse commit namespaces and repeat for all subnamespaces
    nsl.parseCommit();

    // clear pending name lists - these items are now committed
    pending_var_names.clear();
    pending_func_names.clear();
    pending_class_names.clear();
    pending_const_names.clear();
    pending_hashdecl_names.clear();
    pending_typedef_names.clear();
    pending_ns_names.clear();
}

void qore_ns_private::parseCommitRuntimeInit(ExceptionSink* xsink) {
    classList.parseCommitRuntimeInit(xsink);
    constant.parseCommitRuntimeInit();
    nsl.parseCommitRuntimeInit(xsink);
}

void qore_ns_private::parseRollback(ExceptionSink* xsink, bool atomic_rollback) {
    //printd(5, "qore_ns_private::parseRollback() '::%s' this: %p ns: %p atomic: %d\n", name.c_str(), this, ns,
    //    atomic_rollback);

    // delete pending global variable declarations
    pend_gvblist.clear();

    if (atomic_rollback) {
        // Atomic rollback: only remove items tracked in pending_*_names (items committed in prior transactions are
        // preserved)

        // remove only global variables added in this pending transaction
        for (const auto& name : pending_var_names) {
            var_list.parseRemove(name.c_str(), xsink);
        }
        pending_var_names.clear();

        // roll back pending function variants (also removes entirely new functions)
        func_list.parseRollback();
        pending_func_names.clear();

        // roll back pending class members/methods
        classList.parseRollback();

        // remove only classes added in this pending transaction
        for (const auto& name : pending_class_names) {
            classList.parseRemove(name.c_str(), xsink);
        }
        pending_class_names.clear();

        // remove only constants added in this pending transaction
        for (const auto& name : pending_const_names) {
            constant.parseRemove(name.c_str(), xsink);
        }
        pending_const_names.clear();

        // remove only hashdecls added in this pending transaction
        for (const auto& name : pending_hashdecl_names) {
            hashDeclList.parseRemove(name.c_str());
        }
        pending_hashdecl_names.clear();

        // remove only typedefs added in this pending transaction
        for (const auto& name : pending_typedef_names) {
            typedef_map_t::iterator i = typedefMap.find(name);
            if (i != typedefMap.end()) {
                delete i->second;
                typedefMap.erase(i);
            }
        }
        pending_typedef_names.clear();

        // remove only namespaces added in this pending transaction
        for (const auto& name : pending_ns_names) {
            nsl.parseRemove(name.c_str(), xsink);
        }
        pending_ns_names.clear();

        // recursively rollback child namespaces that existed before this transaction
        nsl.parseRollback(xsink, atomic_rollback);
    } else {
        // Full destructive reset: clear ALL state (original behavior for programs without PO_ALLOW_REPARSE)
        // After this, the Program is unusable but memory is still valid until Program is destroyed

        // delete global variables
        var_list.reset();

        // delete pending user functions
        func_list.parseRollback();

        // clear all constants
        constant.deleteAll(xsink);
        classList.clearConstants(xsink);
        // clear all static class vars
        classList.deleteClassData(true, xsink);

        // delete pending constant list
        constant.reset();

        // delete classes
        classList.reset();

        // delete hashdecls
        hashDeclList.reset();

        // delete enums
        enumList.reset();

        // rollback namespaces
        nsl.parseRollback(xsink, atomic_rollback);
    }
}

bool qore_ns_private::addGlobalVars(qore_root_ns_private& rns) {
    bool ok = true;
    for (gvblist_t::iterator i = pend_gvblist.begin(), e = pend_gvblist.end(); i != e; ++i) {
        // resolve namespace
        const NamedScope& n = *((*i).name);

        const QoreProgramLocation* loc = (*i).var->getParseLocation();

        Var* v = var_list.parseFindVar(n.getIdentifier());
        if (v) {
            parse_error(*loc, "global variable '%s::%s' has been %s this Program object multiple times", name.c_str(),
                n.getIdentifier(), v->isRef() ? "imported into" : "declared in");
            if (ok) {
                ok = false;
            }
            continue;
        }

        v = (*i).takeVar();
        //printd(5, "qore_root_ns_private::addGlobalVars() resolved '%s::%s' ('%s') %p ns\n", name.c_str(),
        //  n.getIdentifier(), n.ostr, v);
        var_list.parseAdd(v);
        // track the new variable name for rollback support
        pending_var_names.push_back(v->getName());
        rns.varmap.update(v->getName(), this, v);
    }
    pend_gvblist.clear();

    if (!nsl.addGlobalVars(rns) && ok) {
        ok = false;
    }

    return ok;
}
/*
void qore_ns_private::addGlobalVars(gvlist_t& pend_gvlist) {
    // take global variable decls
    for (unsigned i = 0; i < pend_gvblist.size(); ++i) {
        //printd(5, "qore_root_ns_private::parseAddNamespaceIntern() merging global var decl '%s::%s' into the " \
            "root list\n", ns->name.c_str(), ns->pend_gvblist[i].name->ostr);
        pend_gvlist.push_back(GVEntry(pend_gvblist[i], this));
    }
    pend_gvblist.zero();

    nsl.addGlobalVars(pend_gvlist);
}
*/

qore_ns_private* qore_ns_private::parseAddNamespace(QoreNamespace* nns) {
    std::unique_ptr<QoreNamespace> nnsh(nns);

    //printd(5, "qore_ns_private::parseAddNamespace() this: %p '%s::' adding '%s' pub: %d nns->pub: %d gvars: %d\n",
    //  this, name.c_str(), nns->getName(), pub, nns->priv->pub, nns->priv->pend_gvblist.size());

    if (!imported && !pub && nns->priv->pub && parse_check_parse_option(PO_IN_MODULE))
        qore_program_private::makeParseWarning(getProgram(), *nns->priv->loc, QP_WARN_INVALID_OPERATION,
        "INVALID-OPERATION", "namespace '%s::%s' is declared public but the enclosing namespace '%s::' is not public",
        name.c_str(), nns->getName(), name.c_str());

    //printd(5, "qore_ns_private::parseAddNamespace() this: %p '%s' adding %p '%s' (exists %p)\n", this, getName(),
    //  ns, ns->getName(), priv->nsl.find(ns->getName()));

    // raise an exception if namespace collides with an object name
    if (classList.find(nns->getName())) {
        parse_error(*nns->priv->loc, "namespace name '%s' collides with class '%s'", ns->getName(), ns->getName());
        return nullptr;
    }

    nnsh.release();

    // see if a committed namespace with the same name already exists
    QoreNamespace* orig = nsl.find(nns->getName());
    if (orig) {
        orig->priv->parseAssimilate(nns);
        return orig->priv;
    }

    return nsl.parseAdd(nns, this);
}

// only called while parsing before addition to namespace tree, no locking needed
cnemap_t::iterator qore_ns_private::parseAddConstant(const QoreProgramLocation* loc, const char* cname,
        QoreValue value, bool cpub) {
    ValueHolder vh(value, 0);

    if (constant.inList(cname)) {
        std::string path;
        getPath(path, true);
        parse_error(*loc, "constant '%s' has already been defined in '%s'", cname, path.c_str());
        return constant.end();
    }

    if (!imported && cpub && !pub && parse_check_parse_option(PO_IN_MODULE))
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
        "constant '%s::%s' is declared public but the enclosing namespace '%s::' is not public", name.c_str(), cname,
        name.c_str());

    return constant.parseAdd(loc, cname, vh.release(), 0, cpub);
}

// only called while parsing before addition to namespace tree, no locking needed
void qore_ns_private::parseAddConstant(const QoreProgramLocation* loc, const NamedScope& nscope, QoreValue value,
        bool cpub) {
   ValueHolder vh(value, 0);

   QoreNamespace* sns = resolveNameScope(loc, nscope);
   if (!sns)
      return;

   sns->priv->parseAddConstant(loc, nscope[nscope.size() - 1], vh.release(), cpub);
}

// only called while parsing before addition to namespace tree, no locking needed
// returns TypedefEntry* on success, nullptr on error
TypedefEntry* qore_ns_private::parseAddTypedef(const QoreProgramLocation* loc, const char* tdname,
        const QoreTypeInfo* typeInfo, QoreParseTypeInfo* parseTypeInfo, bool pub) {
    std::unique_ptr<QoreParseTypeInfo> pti_holder(parseTypeInfo);

    // Check for duplicate typedef name
    if (typedefMap.find(tdname) != typedefMap.end()) {
        std::string nspath;
        getPath(nspath, true);
        parse_error(*loc, "typedef '%s' has already been defined in '%s'", tdname, nspath.c_str());
        return nullptr;
    }

    // Check for conflict with class name
    if (classList.find(tdname)) {
        parse_error(*loc, "typedef '%s' conflicts with existing class in namespace '%s::'", tdname, name.c_str());
        return nullptr;
    }

    // Check for conflict with hashdecl name
    if (hashDeclList.find(tdname)) {
        parse_error(*loc, "typedef '%s' conflicts with existing hashdecl in namespace '%s::'", tdname, name.c_str());
        return nullptr;
    }

    // Check public visibility warning
    if (!imported && pub && !this->pub && parse_check_parse_option(PO_IN_MODULE))
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
        "typedef '%s::%s' is declared public but the enclosing namespace '%s::' is not public", name.c_str(), tdname,
        name.c_str());

    // Add the typedef
    TypedefEntry* entry = new TypedefEntry(loc, typeInfo, pti_holder.release(), pub);
    typedefMap[tdname] = entry;
    pending_typedef_names.push_back(tdname);
    return entry;
}

// only called while parsing inside unattached namespaces
// global tdmap is updated when namespace is attached via parseRebuildIndexes
void qore_ns_private::parseAddTypedef(const QoreProgramLocation* loc, const NamedScope& nscope,
        const QoreTypeInfo* typeInfo, QoreParseTypeInfo* parseTypeInfo, bool pub) {
    std::unique_ptr<QoreParseTypeInfo> pti_holder(parseTypeInfo);

    QoreNamespace* sns = resolveNameScope(loc, nscope);
    if (!sns)
        return;

    sns->priv->parseAddTypedef(loc, nscope[nscope.size() - 1], typeInfo, pti_holder.release(), pub);
}

// only called with RootNS
void qore_root_ns_private::parseAddTypedefIntern(const QoreProgramLocation* loc, const NamedScope& nscope,
        const QoreTypeInfo* typeInfo, QoreParseTypeInfo* parseTypeInfo, bool pub) {
    std::unique_ptr<QoreParseTypeInfo> pti_holder(parseTypeInfo);

    qore_ns_private* sns = parseResolveNamespace(loc, nscope);
    if (!sns) {
        return;
    }

    const char* tdname = nscope.getIdentifier();
    TypedefEntry* entry = sns->parseAddTypedef(loc, tdname, typeInfo, pti_holder.release(), pub);
    if (entry) {
        // add to global typedef map
        tdmap.update(tdname, sns, entry);
    }
}

const TypedefEntry* qore_ns_private::findLocalTypedef(const char* tdname) const {
    typedef_map_t::const_iterator i = typedefMap.find(tdname);
    if (i != typedefMap.end())
        return i->second;
    return nullptr;
}

// public, only called either in single-threaded initialization or
// while the program-level parse lock is held
int qore_ns_private::parseAddPendingClass(const QoreProgramLocation* loc, QoreClass* oc) {
    qore_class_private_holder och(oc);

    if (!imported && !pub && qore_class_private::isPublic(*oc) && parse_check_parse_option(PO_IN_MODULE))
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
        "class '%s::%s' is declared public but the enclosing namespace '%s::' is not public", name.c_str(),
        oc->getName(), name.c_str());

    //printd(5, "qore_ns_private::parseAddPendingClass() adding str: %s (%p)\n", oc->name, oc);
    // raise an exception if object name collides with a namespace
    if (nsl.find(oc->getName())) {
        parse_error(*loc, "class name '%s' collides with previously-defined namespace '%s'", oc->getName(),
            oc->getName());
        return -1;
    }

    // look for conflicting hashdecl
    if (hashDeclList.find(oc->getName())) {
        parse_error(*loc, "hashdecl '%s' already exists in namespace '%s::'", oc->getName(), name.c_str());
        return -1;
    }

    {
        QoreClass* c = classList.find(oc->getName());
        if (c) {
            // see if the conflicting class in place has the injected flag set; if so, ignore the new class by
            // returning -1 without raising an exception
            if (!qore_class_private::injected(*c))
                parse_error(*loc, "class '%s' already exists in namespace '%s::'", oc->getName(), name.c_str());
            return -1;
        }
    }

    if (classList.add(oc)) {
        parse_error(*loc, "class '%s' is already defined in namespace '%s::'", oc->getName(), name.c_str());
        return -1;
    }
    och.release();

    qore_class_private::get(*oc)->setNamespace(this);

    //printd(5, "qore_ns_private::parseAddPendingClass() added class %p '%s' ns: %p '%s' parent: %p\n", oc,
    //    oc->getName(), this, name.c_str(), parent);

    return 0;
}

// public, only called when parsing unattached namespaces
int qore_ns_private::parseAddPendingClass(const QoreProgramLocation* loc, const NamedScope& n, QoreClass* oc) {
   qore_class_private_holder och(oc);

   //printd(5, "qore_ns_private::parseAddPendingClass() adding ns: %s (%s, %p)\n", n.ostr, oc->getName(), oc);
   QoreNamespace* sns = resolveNameScope(loc, n);
   if (!sns)
      return -1;

   return sns->priv->parseAddPendingClass(loc, och.release());
}

// public, only called either in single-threaded initialization or
// while the program-level parse lock is held
int qore_ns_private::parseAddPendingHashDecl(const QoreProgramLocation* loc, TypedHashDecl* hashdecl) {
    TypedHashDeclHolder thd(hashdecl);

    if (!imported && !pub && typed_hash_decl_private::get(*hashdecl)->isPublic()
        && parse_check_parse_option(PO_IN_MODULE))
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            "hashdecl '%s::%s' is declared public but the enclosing namespace '%s::' is not public", name.c_str(),
            hashdecl->getName(), name.c_str());

    typed_hash_decl_private::get(*hashdecl)->setNamespace(this);
    if (hashDeclList.add(hashdecl)) {
        parse_error(*loc, "hashdecl '%s' is already defined in namespace '%s::'", hashdecl->getName(), name.c_str());
        return -1;
    }

    thd.release();

    return 0;
}

// public, only called when parsing unattached namespaces
int qore_ns_private::parseAddPendingHashDecl(const QoreProgramLocation* loc, const NamedScope& n,
        TypedHashDecl* hashdecl) {
   TypedHashDeclHolder thd(hashdecl);

   //printd(5, "qore_ns_private::parseAddPendingClass() adding ns: %s (%s, %p)\n", n.ostr, oc->getName(), oc);
   QoreNamespace* sns = resolveNameScope(loc, n);
   if (!sns)
      return -1;

   return sns->priv->parseAddPendingHashDecl(loc, thd.release());
}

// public, only called either in single-threaded initialization or
// while the program-level parse lock is held
int qore_ns_private::parseAddPendingEnum(const QoreProgramLocation* loc, QoreEnumDecl* enumdecl) {
    QoreEnumDeclHolder ed(enumdecl);

    const char* enum_name = enumdecl->getName();
    qore_enum_decl_private* priv = qore_enum_decl_private::get(*enumdecl);

    if (!imported && !pub && priv->isPublic()
        && parse_check_parse_option(PO_IN_MODULE)) {
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            "enum '%s::%s' is declared public but the enclosing namespace '%s::' is not public", name.c_str(),
            enum_name, name.c_str());
    }

    // Check for name conflicts with existing namespaces, classes, etc.
    if (nsl.find(enum_name)) {
        parse_error(*loc, "enum '%s' conflicts with existing namespace '%s' in namespace '%s::'",
            enum_name, enum_name, name.c_str());
        return -1;
    }
    if (classList.find(enum_name)) {
        parse_error(*loc, "enum '%s' conflicts with existing class '%s' in namespace '%s::'",
            enum_name, enum_name, name.c_str());
        return -1;
    }

    priv->setNamespace(this);
    if (enumList.add(enumdecl)) {
        parse_error(*loc, "enum '%s' is already defined in namespace '%s::'", enum_name, name.c_str());
        return -1;
    }

    // Create a namespace for enum member access (e.g., EnumName::MemberName)
    QoreNamespace* enum_ns = new QoreNamespace(enum_name);
    qore_ns_private* ens_priv = qore_ns_private::get(*enum_ns);
    if (priv->isPublic()) {
        ens_priv->setPublic();
    }

    // Add enum members as constants in the namespace with enum type info
    // This ensures enum constants have the enum type, not the base type
    const std::vector<QoreEnumMember*>& members = priv->getMembers();
    for (auto* member : members) {
        QoreValue val = member->getValue();
        val.ref();  // Add a reference for the constant
        ens_priv->constant.add(member->getName(), val, priv->getTypeInfo());
    }

    // Add the enum namespace to this namespace
    qore_ns_private* new_ns = parseAddNamespace(enum_ns);
    if (!new_ns) {
        // This shouldn't happen since we check for conflicts above, but handle it gracefully
        parse_error(*loc, "failed to create namespace for enum '%s' in namespace '%s::'",
            enum_name, name.c_str());
        return -1;
    }

    // Index the new namespace and its constants in the root namespace
    qore_root_ns_private* rns = getRoot();
    if (rns) {
        rns->rebuildIndexes(new_ns);
    }

    ed.release();

    return 0;
}

// public, only called when parsing unattached namespaces
int qore_ns_private::parseAddPendingEnum(const QoreProgramLocation* loc, const NamedScope& n,
        QoreEnumDecl* enumdecl) {
    QoreEnumDeclHolder ed(enumdecl);

    QoreNamespace* sns = resolveNameScope(loc, n);
    if (!sns) {
        return -1;
    }

    return sns->priv->parseAddPendingEnum(loc, ed.release());
}

int qore_ns_private::parseAddMethodToClass(const QoreProgramLocation* loc, const NamedScope& mname,
        MethodVariantBase* qcmethod, bool static_flag) {
   std::unique_ptr<MethodVariantBase> v(qcmethod);

   unsigned m = 0;
   QoreClass* oc = mname.size() > 2 ? parseMatchScopedClassWithMethod(mname, m) : parseFindLocalClass(mname[0]);
   if (!oc) {
      parse_error(*loc, "cannot find class for to add method '%s' in namespace '%s'", mname.ostr, name.c_str());
      return -1;
   }

   qore_class_private::addUserMethod(*oc, mname.getIdentifier(), v.release(), static_flag);
   return 0;
}

void qore_ns_private::scanMergeCommittedNamespace(const qore_ns_private& mns, QoreModuleContext& qmc) const {
    //printd(5, "qore_ns_private::scanMergeCommittedNamespace() this: %p '%s' mns: %p '%s'\n", this, name.c_str(),
    //  &mns, mns.name.c_str());

    // check user constants
    {
        ConstConstantListIterator cli(mns.constant);
        while (cli.next()) {
            if (!cli.isUserPublic()) {
                continue;
            }
            if (constant.inList(cli.getName())) {
                qmc.error("duplicate constant %s::%s", name.c_str(), cli.getName().c_str());
            }
        }
    }

    // check user classes
    {
        ConstClassListIterator cli(mns.classList);
        while (cli.next()) {
            if (!cli.isUserPublic()) {
                continue;
            }

            const QoreClass* c = classList.find(cli.getName());
            if (c) {
                // ignore if the class is injected or already imported
                if (qore_class_private::get(*c) != qore_class_private::get(*cli.get()) &&
                    !qore_class_private::injected(*c))
                qmc.error("duplicate class %s::%s" , name.c_str(), cli.getName());
                /*
                qmc.error("duplicate class %s::%s (c: %p cli: %p c inj: %d cli inj: %d)", name.c_str(), cli.getName(),
                    qore_class_private::get(*c), qore_class_private::get(*cli.get()),
                    qore_class_private::injected(*c), qore_class_private::injected(*cli.get()));
                */
            } else if (hashDeclList.find(cli.getName()))
                qmc.error("duplicate hashdecl %s::%s", name.c_str(), cli.getName());
        }
    }

    // check user functions
    for (fl_map_t::const_iterator i = mns.func_list.begin(), e = mns.func_list.end(); i != e; ++i) {
        if (!i->second->isUserPublic()) {
            continue;
        }

        FunctionEntry* fe = func_list.findNode(i->first);
        if (fe && !fe->getFunction()->injected()) {
            qmc.error("duplicate function %s::%s()", name.c_str(), i->first);
        }
        //printd(5, "qore_ns_private::scanMergeCommittedNamespace() this: %p '%s::' looking for function '%s' (%d)\n",
        //    this, name.c_str(), i->first, func_list.findNode(i->first));
    }

    // check user variables
    // NOTE: importing builtin global variables is not supported
    for (map_var_t::const_iterator i = mns.var_list.vmap.begin(), e = mns.var_list.vmap.end(); i != e; ++i) {
        if (!i->second->isPublic()) {
            continue;
        }
        if (var_list.vmap.find(i->first) != var_list.vmap.end()) {
            qmc.error("duplicate global variable %s::%s", name.c_str(), i->first);
        }
    }

    // check hashdecls
    {
        ConstHashDeclListIterator i(mns.hashDeclList);
        while (i.next()) {
            const TypedHashDecl* th = i.get();
            if (!th->isPublic()) {
                continue;
            }

            const TypedHashDecl* curr = hashDeclList.find(th->getName());
            if (curr) {
                qmc.error("duplicate hashdecl %s::%s", name.c_str(), i.getName());
            }
        }
    }

    // check enums
    // NOTE: Unlike classes, enums don't support injection, so we simply skip
    // the duplicate check if an enum with the same name already exists.
    // mergeUserPublic() will skip adding it anyway, so this is safe.
    // This allows modules to be loaded in sub-Programs without errors.
    {
        ConstEnumListIterator i(mns.enumList);
        while (i.next()) {
            if (!i.isUserPublic()) {
                continue;
            }
            // Only report error if there's a conflict with a different type
            // (e.g., a class with the same name)
            if (classList.find(i.getName())) {
                qmc.error("enum '%s' conflicts with class '%s::%s'", i.getName(), name.c_str(), i.getName());
            } else if (hashDeclList.find(i.getName())) {
                qmc.error("enum '%s' conflicts with hashdecl '%s::%s'", i.getName(), name.c_str(), i.getName());
            }
            // If an enum with the same name exists, it's fine - skip silently
        }
    }

    bool in_mod = parse_check_parse_option(PO_IN_MODULE);

    // check subnamespaces
    for (nsmap_t::const_iterator i = mns.nsl.nsmap.begin(), e = mns.nsl.nsmap.end(); i != e; ++i) {
        if (!qore_ns_private::isUserPublic(*i->second))
            continue;
        // see if a subnamespace with the same name exists
        const QoreNamespace* cns = nsl.find(i->first);

        //printd(5, "qore_ns_private::scanMergeCommittedNamespace() this: %p '%s::' checking %p '%s::' (pub: %d) "
        //    "cns: %p (pub: %d)\n", this, name.c_str(), i->second, i->second->getName(), i->second->priv->pub, cns,
        //    cns ? cns->priv->pub : false);
        if (!i->second->priv->pub) {
            if (in_mod && cns && cns->priv->pub)
                qmc.error("cannot merge existing public namespace '%s' with new private namespace of the same name; "
                    "namespace '%s::%s' is declared both with and without the 'public' keyword", cns->getName(),
                    name.c_str(), i->first.c_str());

            continue;
        }

        // see if a class with the same name is present
        if (classList.find(i->first.c_str())) {
            qmc.error("namespace '%s::%s' clashes with an existing class of the same name", name.c_str(),
                i->first.c_str());
            continue;
        }
        if (cns) {
            cns->priv->scanMergeCommittedNamespace(*(i->second->priv), qmc);
            continue;
        }
    }
}

void qore_ns_private::copyMergeCommittedNamespace(const qore_ns_private& mns) {
    printd(5, "qore_ns_private::copyMergeCommittedNamespace() this: %p '%s'\n", this, name.c_str());

    // merge in source constants
    constant.mergeUserPublic(mns.constant);

    // merge in source classes
    classList.mergeUserPublic(mns.classList, this);

    // merge in source hashdecls
    hashDeclList.mergeUserPublic(mns.hashDeclList, this);

    // merge in source functions
    func_list.mergeUserPublic(mns.func_list, this);

    // merge in global variables
    var_list.mergePublic(mns.var_list);

    // merge in source enums
    enumList.mergeUserPublic(mns.enumList, this);

    // merge in source typedefs (with conflict checking)
    for (const auto& i : mns.typedefMap) {
        if (!i.second->pub) {
            continue;
        }
        // skip if typedef already exists
        if (typedefMap.find(i.first) != typedefMap.end()) {
            continue;
        }
        // skip if conflicts with existing class
        if (classList.find(i.first.c_str())) {
            continue;
        }
        // skip if conflicts with existing hashdecl
        if (hashDeclList.find(i.first.c_str())) {
            continue;
        }
        // skip if conflicts with existing enum
        if (enumList.find(i.first.c_str())) {
            continue;
        }
        typedefMap[i.first] = new TypedefEntry(*i.second);
    }

    // Create namespaces for enum member access (e.g., EnumName::MemberName)
    // This must be done after merging enums since mergeUserPublic only copies the enum declaration
    {
        ConstEnumListIterator i(mns.enumList);
        while (i.next()) {
            //printd(5, "qore_ns_private::copyMergeCommittedNamespace() checking enum '%s' isUserPublic: %d\n",
            //    i.getName(), i.isUserPublic());
            if (!i.isUserPublic()) {
                continue;
            }
            const char* enum_name = i.getName();
            // Skip if this enum was not actually merged (already existed)
            QoreEnumDecl* local_ed = enumList.find(enum_name);
            if (!local_ed) {
                continue;
            }
            // Check if namespace for this enum already exists
            QoreNamespace* enum_ns = nsl.find(enum_name);
            qore_ns_private* ens_priv;
            if (!enum_ns) {
                // Create new namespace for enum members
                enum_ns = new QoreNamespace(enum_name);
                ens_priv = qore_ns_private::get(*enum_ns);
                ens_priv->setPublic();
                ens_priv->imported = true;
                ens_priv = nsl.runtimeAdd(enum_ns, this);
            } else {
                ens_priv = qore_ns_private::get(*enum_ns);
            }
            // Add enum members as constants in the namespace
            qore_enum_decl_private* enum_priv = qore_enum_decl_private::get(*local_ed);
            const std::vector<QoreEnumMember*>& members = enum_priv->getMembers();
            for (auto* member : members) {
                if (ens_priv->constant.inList(member->getName())) {
                    continue;
                }
                QoreValue val = member->getValue();
                val.ref();
                ens_priv->constant.add(member->getName(), val, enum_priv->getTypeInfo());
            }
            // Rebuild indexes for the new namespace
            qore_root_ns_private* rns = getRoot();
            if (rns) {
                rns->rebuildIndexes(ens_priv);
            }
        }
    }

    // add sub namespaces
    for (nsmap_t::const_iterator i = mns.nsl.nsmap.begin(), e = mns.nsl.nsmap.end(); i != e; ++i) {
        if (!qore_ns_private::isUserPublic(*i->second)) {
            //printd(5, "qore_ns_private::copyMergeCommittedNamespace() this: %p (%p) '%s::' skipping %p (%p) '%s::' "
            //    "pub: %d builtin: %d\n", this, ns, name.c_str(), i->second->priv, i->second, i->second->getName(),
            //    i->second->priv->pub, i->second->priv->builtin);
            continue;
        }

        QoreNamespace* nns = nsl.find(i->first);
        if (!nns) {
            qore_ns_private* npns = new qore_ns_private(i->first.c_str(), *i->second->priv);
            nns = npns->ns;
            nns->priv->imported = true;

            // fix from_module when a module extends another module's namespace: if the source namespace's
            // from_module doesn't match the namespace name but a loaded module with the namespace's name exists,
            // use that module's name so the namespace is correctly attributed to its original module
            if (!npns->from_module.empty() && npns->from_module != i->first) {
                if (QMM.findModuleUnlocked(i->first.c_str())) {
                    printd(5, "qore_ns_private::copyMergeCommittedNamespace() fixing from_module for '%s' "
                        "from '%s' to '%s'\n", i->first.c_str(), npns->from_module.c_str(), i->first.c_str());
                    npns->from_module = i->first;
                }
            }

            //printd(5, "qore_ns_private::copyMergeCommittedNamespace() this: %p '%s::' merged %p '%s::' pub: %d\n",
            //    this, name.c_str(), nns, nns->getName(), nns->priv->pub);
            nns = nsl.runtimeAdd(nns, this)->ns;
        } else if (nns->priv->from_module != i->first && i->second->priv->from_module == i->first) {
            // fix from_module when a module's own namespace was previously created by another module:
            // the source namespace's from_module matches the namespace name, so it's the authoritative module
            printd(5, "qore_ns_private::copyMergeCommittedNamespace() fixing from_module for existing '%s' "
                "from '%s' to '%s'\n", i->first.c_str(), nns->priv->from_module.c_str(), i->first.c_str());
            nns->priv->from_module = i->first;
        }

        nns->priv->copyMergeCommittedNamespace(*i->second->priv);
        //printd(5, "qore_ns_private::copyMergeCommittedNamespace() this: %p '%s::' merged %p '%s::'\n", this,
        //    name.c_str(), nns, nns->getName());
    }
    //printd(5, "qore_ns_private::copyMergeCommittedNamespace() this: %p '%s' done\n", this, name.c_str());
}

void qore_ns_private::parseAssimilate(QoreNamespace* ans) {
    //printd(5, "qore_ns_private::parseAssimilate() this: %p (%p) '%s' pub: %d imported: %d ans->pub: %d ans->imported: %d (%p)\n", this, ns, name.c_str(), pub, imported, ans->priv->pub, ans->priv->imported, ans);
    // delete source namespace on exit
    std::unique_ptr<QoreNamespace> ns_ptr(ans);

    qore_ns_private* pns = ans->priv;

    // fix from_module when a module extends its own namespace that was previously created by a dependency:
    // if the current module context name matches the namespace name, this module owns the namespace
    const char* mod_name = get_module_context_name();
    if (mod_name && from_module != name && name == mod_name) {
        printd(5, "qore_ns_private::parseAssimilate() fixing from_module for '%s' from '%s' to '%s'\n",
            name.c_str(), from_module.c_str(), mod_name);
        from_module = mod_name;
    }

    // ensure that either both namespaces are public or both are not
    if (parse_check_parse_option(PO_IN_MODULE) && (pub != pns->pub)) {
        if (!imported) {
            std::string path;
            getPath(path, true);
            parse_error(*ans->priv->loc, "namespace '%s' is declared both with and without the 'public' keyword", path.c_str());
        } else if (!pub && !builtin) {
            // issue #3504: if an existing imported user namespace is not public, and a new namespace is public, then
            // change the current namespace to public
            pub = true;
        }
    }

    // assimilate pending constants
    // assimilate target list - if there were errors then the list will be deleted anyway
    constant.assimilate(pns->constant, "namespace", name.c_str(), &pending_const_names);

    // assimilate classes
    classList.assimilate(pns->classList, *this, &pending_class_names);

    // assimilate hashdecls
    hashDeclList.assimilate(pns->hashDeclList, *this, &pending_hashdecl_names);

    // assimilate pending functions
    func_list.assimilate(pns->func_list, this, &pending_func_names);

    // assimilate enums
    enumList.assimilate(pns->enumList, *this, &pending_enum_names);

    // assimilate typedefs with duplicate checking
    for (auto i = pns->typedefMap.begin(); i != pns->typedefMap.end();) {
        // check for duplicate typedef name
        if (typedefMap.find(i->first) != typedefMap.end()) {
            std::string nspath;
            getPath(nspath, true);
            parse_error(*i->second->loc, "typedef '%s' has already been defined in '%s'", i->first.c_str(),
                nspath.c_str());
            delete i->second;
            i = pns->typedefMap.erase(i);
            continue;
        }
        // check for conflict with class name
        if (classList.find(i->first.c_str())) {
            parse_error(*i->second->loc, "typedef '%s' conflicts with existing class in namespace '%s::'",
                i->first.c_str(), name.c_str());
            delete i->second;
            i = pns->typedefMap.erase(i);
            continue;
        }
        // check for conflict with hashdecl name
        if (hashDeclList.find(i->first.c_str())) {
            parse_error(*i->second->loc, "typedef '%s' conflicts with existing hashdecl in namespace '%s::'",
                i->first.c_str(), name.c_str());
            delete i->second;
            i = pns->typedefMap.erase(i);
            continue;
        }
        // move typedef to this namespace
        typedefMap[i->first] = i->second;
        pending_typedef_names.push_back(i->first);
        i = pns->typedefMap.erase(i);
    }

    // assimilate pending global variable declarations
    //printd(5, "qore_ns_private::parseAssimilate() this: %p assimilating %p (%d + %d gvars)\n", this, pns, pend_gvblist.size(), pns->pend_gvblist.size());
    for (auto& i : pns->pend_gvblist) {
        pend_gvblist.push_back(i);
    }
    pns->pend_gvblist.zero();

    // assimilate sub namespaces
    //printd(5, "qore_ns_private::parseAssimilate() %p '%s' ans: %p '%s' psn->nsl.nsmap.size: %d\n", this, name.c_str(), ans, ans->getName(), (int)pns->nsl.nsmap.size());

    for (nsmap_t::iterator i = pns->nsl.nsmap.begin(), e = pns->nsl.nsmap.end(); i != e;) {
        assert(i->first == i->second->getName());

        // throw parse exception if name is already defined as a namespace or class
        QoreNamespace* ns = nsl.find(i->second->priv->name.c_str());

        if (ns) {
            nsmap_t::iterator ni = i;
            ++i;
            QoreNamespace* nns = ni->second;
            pns->nsl.nsmap.erase(ni);
            ns->priv->parseAssimilate(nns);
            continue;
        }

        if (classList.find(i->second->priv->name.c_str())) {
            parse_error(*i->second->priv->loc, "cannot add namespace '%s' to existing namespace '%s' because a class has already been defined with this name",
                        i->second->priv->name.c_str(), name.c_str());
        }
        ++i;
    }

    // assimilate target namespace list
    nsl.parseAssimilate(pns->nsl, this, &pending_ns_names);

    // delete source namespace
    //printd(5, "qore_ns_private::parseAssimilate() %p '%s': ASSIMILATED %p '%s' deleting %p '%s'\n", this, name.c_str(), pns, pns->name.c_str(), ans, ans->getName());
}

void qore_ns_private::runtimeAssimilate(QoreNamespace* ans) {
    //printd(5, "qore_ns_private::runtimeAssimilate() this: %p '%s' pub: %d imported: %d ans->pub: %d ans->imported: %d\n", this, name.c_str(), pub, imported, ans->priv->pub, ans->priv->imported);
    // delete source namespace on exit
    std::unique_ptr<QoreNamespace> ns_ptr(ans);

    qore_ns_private* pns = ans->priv;
    // make sure there are no objects in the pending lists in the namespace to be merged
    assert(pns->pend_gvblist.empty());

    // assimilate constants
    constant.assimilate(pns->constant);

    // assimilate classes
    classList.assimilate(pns->classList, *this);

    // assimilate hashdecls
    hashDeclList.assimilate(pns->hashDeclList, *this);

    // assimilate pending functions
    func_list.assimilate(pns->func_list, this);

    // assimilate enums
    enumList.assimilate(pns->enumList, *this);

    // assimilate typedefs (runtime - with conflict checking)
    for (auto& i : pns->typedefMap) {
        bool skip = false;
        // skip if typedef already exists
        if (typedefMap.find(i.first) != typedefMap.end()) {
            skip = true;
        }
        // skip if conflicts with existing class
        else if (classList.find(i.first.c_str())) {
            skip = true;
        }
        // skip if conflicts with existing hashdecl
        else if (hashDeclList.find(i.first.c_str())) {
            skip = true;
        }
        // skip if conflicts with existing enum
        else if (enumList.find(i.first.c_str())) {
            skip = true;
        }

        if (skip) {
            delete i.second;
        } else {
            typedefMap[i.first] = i.second;
        }
    }
    pns->typedefMap.clear();

    if (pns->class_handler) {
        assert(!class_handler);
        class_handler = pns->class_handler;
    }

    // assimilate target namespace list
    nsl.runtimeAssimilate(pns->nsl, this);
}

QoreClass* qore_ns_private::parseFindLocalClass(const char* cname) {
   return classList.find(cname);
}

QoreValue qore_ns_private::getConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool& found) {
    assert(!found);
    return constant.find(cname, typeInfo, found);
}

QoreNamespace* qore_ns_private::resolveNameScope(const QoreProgramLocation* loc, const NamedScope& nscope) const {
   const QoreNamespace* sns = ns;

   // find namespace
   for (unsigned i = 0; i < (nscope.size() - 1); i++)
      if (!(sns = sns->priv->parseFindLocalNamespace(nscope[i]))) {
         parse_error(*loc, "namespace '%s' cannot be resolved while evaluating '%s' in constant declaration", nscope[i], nscope.ostr);
         return 0;
      }
   return (QoreNamespace* )sns;
}

const FunctionEntry* qore_ns_private::parseMatchFunctionEntry(const NamedScope& nscope, unsigned& match) const {
    assert(name == nscope.get(0));
    const QoreNamespace* fns = ns;

    assert(name == nscope[0]);

    // mark first namespace as matched
    if (!match)
        match = 1;

    // check for a match of the structure in this namespace
    for (unsigned i = 1; i < (nscope.size() - 1); i++) {
        //printd(5, "qore_ns_private::parseMatchFunctionEntry('%s') curr: %p '%s' element %d %s found: %p\n", nscope.ostr, fns, fns->getName(), i, nscope[i], fns->priv->parseFindLocalNamespace(nscope[i]));
        fns = fns->priv->parseFindLocalNamespace(nscope[i]);
        if (!fns)
            return nullptr;
        if (i >= match)
            match = i + 1;
    }

    return fns->priv->func_list.findNode(nscope.getIdentifier());
}

Var* qore_ns_private::runtimeMatchGlobalVar(const NamedScope& nscope, const qore_ns_private*& rns) const {
    assert(name == nscope[0]);

    rns = runtimeMatchNamespace(nscope, 1);
    return rns ? rns->var_list.runtimeFindVar(nscope.getIdentifier()) : nullptr;
}

const ConstantEntry* qore_ns_private::runtimeMatchNamespaceConstant(const NamedScope& nscope, const qore_ns_private*& rns) const {
    assert(name == nscope[0]);

    rns = runtimeMatchNamespace(nscope, 1);
    return rns ? rns->constant.findEntry(nscope.getIdentifier()) : nullptr;
}

const QoreClass* qore_ns_private::runtimeMatchClass(const NamedScope& nscope, const qore_ns_private*& rns) const {
    assert(name == nscope[0]);

    rns = runtimeMatchNamespace(nscope, 1);
    return rns ? rns->classList.find(nscope.getIdentifier()) : nullptr;
}

const qore_ns_private* qore_ns_private::runtimeMatchNamespace(const NamedScope& nscope, int offset) const {
    assert(name == nscope[0]);

    const QoreNamespace* fns = ns;
    // check for a match of the structure in this namespace
    for (unsigned i = 1; i < (nscope.size() - offset); ++i) {
        fns = fns->priv->nsl.find(nscope[i]);
        if (!fns) {
            return nullptr;
        }
    }
    return fns->priv;
}

const qore_ns_private* qore_ns_private::runtimeMatchAddClass(const NamedScope& nscope, bool& fnd) const {
    const qore_ns_private* fns = runtimeMatchNamespace(nscope, 1);
    if (!fns) {
        return nullptr;
    }
    if (!fnd) {
        fnd = true;
    }
    return fns->classList.find(nscope.getIdentifier()) ? nullptr : fns;
}

const TypedHashDecl* qore_ns_private::runtimeMatchHashDecl(const NamedScope& nscope, const qore_ns_private*& rns) const {
    rns = runtimeMatchNamespace(nscope, 1);
    return rns ? rns->hashDeclList.find(nscope.getIdentifier()) : nullptr;
}

const QoreEnumDecl* qore_ns_private::runtimeMatchEnum(const NamedScope& nscope, const qore_ns_private*& rns) const {
    rns = runtimeMatchNamespace(nscope, 1);
    return rns ? rns->enumList.find(nscope.getIdentifier()) : nullptr;
}

const FunctionEntry* qore_ns_private::runtimeMatchFunctionEntry(const NamedScope& nscope) const {
    const qore_ns_private* fns = runtimeMatchNamespace(nscope, 1);
    return fns ? fns->func_list.findNode(nscope.getIdentifier(), true) : nullptr;
}

const qore_ns_private* qore_ns_private::runtimeMatchAddFunction(const NamedScope& nscope, bool& fnd) const {
    const qore_ns_private* fns = runtimeMatchNamespace(nscope, 1);
    if (!fns) {
        return nullptr;
    }
    if (!fnd) {
        fnd = true;
    }
    return fns->func_list.find(nscope.getIdentifier(), false) ? nullptr : fns;
}

// qore_ns_private::parseMatchNamespace()
// will only be called if there is a match with the name and nscope.size() > 1
QoreNamespace* qore_ns_private::parseMatchNamespace(const NamedScope& nscope, unsigned& matched) const {
    printd(5, "qore_ns_private::parseMatchNamespace() this: %p ns: %p '%s' ns: %s matched: %d\n", this, ns, name.c_str(), nscope.ostr, matched);

    assert(nscope[0] == name);
    const QoreNamespace* fns = ns;

    // mark first namespace as matched
    if (!matched)
        matched = 1;

    // check for a match of the structure in this namespace
    for (unsigned i = 1; i < (nscope.size() - 1); i++) {
        fns = fns->priv->parseFindLocalNamespace(nscope[i]);
        if (!fns)
            break;
        if (i >= matched)
            matched = i + 1;
    }
    return const_cast<QoreNamespace*>(fns);
}

TypedHashDecl* qore_ns_private::parseMatchScopedHashDecl(const NamedScope& nscope, unsigned& matched) {
    printd(5, "qore_ns_private::parseMatchScopedHashDecl() this: %p ns: %p '%s' nscope='%s' matched: %d\n", this, ns, name.c_str(), nscope.ostr, matched);
    assert(nscope.size() > 1);

    if (nscope[0] != name) {
        QoreNamespace* fns = nsl.find(nscope[0]);
        return fns ? fns->priv->parseMatchScopedHashDecl(nscope, matched) : 0;
    }

    // mark first namespace as matched
    if (!matched)
        matched = 1;

    printd(5, "qore_ns_private::parseMatchScopedHashDecl() matched %s in %s\n", name.c_str(), nscope.ostr);

    QoreNamespace* fns = ns;

    // if we need to follow the namespaces, then do so
    if (nscope.size() > 2) {
        for (unsigned i = 1; i < (nscope.size() - 1); ++i) {
            fns = fns->priv->parseFindLocalNamespace(nscope[i]);
            if (!fns)
                return 0;
            if (i >= matched)
                matched = i + 1;
        }
    }
    return fns->priv->hashDeclList.find(nscope[nscope.size() - 1]);
}

const QoreEnumDecl* qore_ns_private::parseMatchScopedEnum(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 1);

    if (nscope[0] != name) {
        QoreNamespace* fns = nsl.find(nscope[0]);
        return fns ? fns->priv->parseMatchScopedEnum(nscope, matched) : nullptr;
    }

    // mark first namespace as matched
    if (!matched) {
        matched = 1;
    }

    QoreNamespace* fns = ns;

    // if we need to follow the namespaces, then do so
    if (nscope.size() > 2) {
        for (unsigned i = 1; i < (nscope.size() - 1); ++i) {
            fns = fns->priv->parseFindLocalNamespace(nscope[i]);
            if (!fns) {
                return nullptr;
            }
            if (i >= matched) {
                matched = i + 1;
            }
        }
    }
    return fns->priv->enumList.find(nscope[nscope.size() - 1]);
}

TypedefEntry* qore_ns_private::parseMatchScopedTypedef(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 1);

    if (nscope[0] != name) {
        QoreNamespace* fns = nsl.find(nscope[0]);
        return fns ? fns->priv->parseMatchScopedTypedef(nscope, matched) : nullptr;
    }

    // mark first namespace as matched
    if (!matched) {
        matched = 1;
    }

    QoreNamespace* fns = ns;

    // if we need to follow the namespaces, then do so
    if (nscope.size() > 2) {
        for (unsigned i = 1; i < (nscope.size() - 1); ++i) {
            fns = fns->priv->parseFindLocalNamespace(nscope[i]);
            if (!fns) {
                return nullptr;
            }
            if (i >= matched) {
                matched = i + 1;
            }
        }
    }
    return const_cast<TypedefEntry*>(fns->priv->findLocalTypedef(nscope[nscope.size() - 1]));
}

QoreClass* qore_ns_private::parseMatchScopedClass(const NamedScope& nscope, unsigned& matched) {
    //printd(5, "qore_ns_private::parseMatchScopedClass() this: %p ns: %p '%s' nscope='%s' matched: %d\n", this, ns, name.c_str(), nscope.ostr, matched);

    if (nscope[0] != name) {
        QoreNamespace* fns = nsl.find(nscope[0]);
        return fns ? fns->priv->parseMatchScopedClass(nscope, matched) : nullptr;
    }

    // mark first namespace as matched
    if (!matched)
        matched = 1;

    //printd(5, "qore_ns_private::parseMatchScopedClass() matched %s in %s\n", name.c_str(), nscope.ostr);

    QoreNamespace* fns = ns;

    // if we need to follow the namespaces, then do so
    if (nscope.size() > 2) {
        for (unsigned i = 1; i < (nscope.size() - 1); i++) {
            fns = fns->priv->parseFindLocalNamespace(nscope[i]);
            if (!fns)
                return nullptr;
            if (i >= matched)
                matched = i + 1;
        }
    }
    return fns->priv->findLoadClass(nscope[nscope.size() - 1]);
}

QoreClass* qore_ns_private::parseMatchScopedClassWithMethod(const NamedScope& nscope, unsigned& matched) {
    assert(nscope.size() > 2);
    assert(name == nscope.get(0));

    printd(5, "qore_ns_private::parseMatchScopedClassWithMethod() this: %p ns: %p '%s' class: %s (%s)\n", this, ns, name.c_str(), nscope[nscope.size() - 2], nscope.ostr);

    QoreNamespace* fns = ns;

    // mark first namespace as matched
    if (!matched) {
        matched = 1;
    }

    // search the rest of the namespaces
    for (unsigned i = 1; i < (nscope.size() - 2); i++) {
        fns = fns->priv->parseFindLocalNamespace(nscope[i]);
        if (!fns) {
            return 0;
        }
        if (i >= matched) {
            matched = i + 1;
        }
    }

    // now get class from final namespace
    return fns->priv->findLoadClass(nscope[nscope.size() - 2]);
}

const QoreClass* qore_ns_private::runtimeMatchScopedClassWithMethod(const NamedScope& nscope) const {
    assert(nscope.size() > 2);
    assert(name == nscope.get(0));

    printd(5, "qore_ns_private::runtimeMatchScopedClassWithMethod() this: %p ns: %p '%s' class: %s (%s)\n", this,
        ns, name.c_str(), nscope[nscope.size() - 2], nscope.ostr);

    const qore_ns_private* fns = runtimeMatchNamespace(nscope, 2);
    // now get class from final namespace
    return fns ? fns->classList.find(nscope[nscope.size() - 2]) : nullptr;
}

QoreValue qore_ns_private::parseResolveReferencedClassConstant(const QoreProgramLocation* loc, QoreClass* qc,
        const char* name, const QoreTypeInfo*& typeInfo, bool& found) {
    assert(!found);
    QoreValue rv = qore_class_private::parseFindConstantValue(qc, name, typeInfo, found, parse_get_class_priv());
    if (found) {
        return rv.refSelf();
    }
    const QoreClass* aqc;
    ClassAccess access;
    QoreVarInfo* vi = qore_class_private::parseFindStaticVar(qc, name, aqc, access, true);
    //printd(5, "qore_ns_private::parseResolveReferencedClassConstant() '%s' %p '%s' static var: %p\n", qc->getName(),
    //  qc, name, vi);
    if (vi) {
        typeInfo = vi->getTypeInfo();
        found = true;
        return new StaticClassVarRefNode(loc, name, *qc, *vi);
    }
    return QoreValue();
}

QoreValue qore_ns_private::parseMatchScopedConstantValue(const NamedScope& nscope, unsigned& matched,
        const QoreTypeInfo*& typeInfo, bool& found) {
    assert(!found);
    printd(5, "qore_ns_private::parseMatchScopedConstantValue) trying to find %s in %s\n", nscope.getIdentifier(),
        name.c_str());

    assert(nscope[0] == name);

    // mark first namespace as matched
    if (!matched)
        matched = 1;

    const QoreNamespace* fns = ns;

    // if we need to follow the namespaces, then do so
    if (nscope.size() > 2) {
        unsigned last = nscope.size() - 1;
        for (unsigned i = 1; i < last; i++) {
            const char* oname = nscope[i];
            const QoreNamespace* nns = fns->priv->parseFindLocalNamespace(oname);
            if (!nns) {
                // if we are on the last element before the constant in the namespace path list,
                // then check for a class constant
                if (i == (last - 1)) {
                    QoreClass* qc = fns->priv->parseFindLocalClass(oname);
                    return qc ? qore_class_private::parseFindLocalConstantValue(qc, nscope.getIdentifier(), typeInfo, found) : QoreValue();
                }
                return QoreValue();
            }
            fns = nns;
            if (i >= matched)
                matched = i + 1;
        }
    }

    return fns->priv->getConstantValue(nscope.getIdentifier(), typeInfo, found);
}

QoreValue qore_ns_private::parseCheckScopedReference(const QoreProgramLocation* loc, const NamedScope& nsc,
        unsigned& matched, const QoreTypeInfo*& typeInfo, bool& found, bool abr) const {
    assert(!found);
    const QoreNamespace* pns = ns;

    matched = 1;

    // follow the namespaces
    unsigned last = nsc.size() - 1;
    for (unsigned i = 1; i < last; ++i) {
        QoreNamespace* nns = pns->priv->parseFindLocalNamespace(nsc.get(i));

        if (!nns) {
            // if we have matched all namespaces except the last one, check if it's a class
            // and try to resolve a class constant or static class variable
            if (i == (last - 1)) {
                const char* cname = nsc.get(last - 1);
                QoreClass* qc = pns->priv->parseFindLocalClass(cname);
                //printd(5, "qore_ns_private::parseCheckScopedReference() this: %p '%s' nsc: %s checking for class "
                //    "'%s' qc: %p\n", this, name.c_str(), nsc.ostr, cname, qc);
                if (qc) {
                    return parseResolveReferencedClassConstant(loc, qc, nsc.getIdentifier(), typeInfo, found);
                }
            }
            return QoreValue();
        }

        pns = nns;
        if (i >= matched)
            matched = i + 1;
    }

    // matched all namespaces, now try to find a constant
    QoreValue rv = pns->priv->parseFindLocalConstantValue(nsc.getIdentifier(), typeInfo, found);
    if (!found && abr) {
        Var* v = pns->priv->var_list.parseFindVar(nsc.getIdentifier());
        if (v) {
            typeInfo = v->getTypeInfo();
            found = true;
            return new GlobalVarRefNode(loc, strdup(nsc.ostr), v);
        }
    }

    return found ? rv.refSelf() : QoreValue();
}

QoreValue qore_ns_private::parseFindLocalConstantValue(const char* cname, const QoreTypeInfo*& typeInfo, bool& found) {
    assert(!found);
    return constant.find(cname, typeInfo, found);
}

QoreNamespace* qore_ns_private::parseFindLocalNamespace(const char* nname) {
    return nsl.find(nname);
}

QoreValue qore_ns_private::setKeyValue(const std::string& key, QoreValue val) {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(kvlck);

    kvmap_t::iterator i = kvmap.lower_bound(key);
    if (i != kvmap.end() && i->first == key) {
        QoreValue rv = i->second;
        i->second = val;
        return rv;
    }
    kvmap.insert(i, kvmap_t::value_type(key, val));
    return QoreValue();
}

QoreValue qore_ns_private::setKeyValueIfNotSet(const std::string& key, QoreValue val) {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(kvlck);

    kvmap_t::iterator i = kvmap.lower_bound(key);
    if (i != kvmap.end() && i->first == key) {
        if (i->second) {
            return val;
        }
        i->second = val;
        return QoreValue();
    }
    kvmap.insert(i, kvmap_t::value_type(key, val));
    return QoreValue();
}

bool qore_ns_private::setKeyValueIfNotSet(const std::string& key, const char* val) {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(kvlck);

    kvmap_t::iterator i = kvmap.lower_bound(key);
    if (i != kvmap.end() && i->first == key) {
        if (!i->second) {
            i->second = new QoreStringNode(val);
            return true;
        }
        return false;
    }
    kvmap.insert(i, kvmap_t::value_type(key, new QoreStringNode(val)));
    return true;
}

QoreValue qore_ns_private::getReferencedKeyValue(const std::string& key) const {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(kvlck);

    kvmap_t::const_iterator i = kvmap.find(key);
    if (i == kvmap.end()) {
        return QoreValue();
    }
    return i->second.refSelf();
}

QoreValue qore_ns_private::getReferencedKeyValue(const char* key) const {
    // ensure atomicity when reading from or writing to kvmap
    AutoLocker al(kvlck);

    kvmap_t::const_iterator i = kvmap.find(key);
    if (i == kvmap.end()) {
        return QoreValue();
    }
    return i->second.refSelf();
}

QoreNamespaceIterator::QoreNamespaceIterator(QoreNamespace& ns) : priv(new QorePrivateNamespaceIterator(qore_ns_private::get(ns))) {
}

QoreNamespaceIterator::~QoreNamespaceIterator() {
    delete priv;
}

bool QoreNamespaceIterator::next() {
    return priv->next();
}

QoreNamespace* QoreNamespaceIterator::operator->() {
    return priv->get()->ns;
}

QoreNamespace* QoreNamespaceIterator::operator*() {
    return priv->get()->ns;
}

const QoreNamespace* QoreNamespaceIterator::operator->() const {
    return priv->get()->ns;
}

const QoreNamespace* QoreNamespaceIterator::operator*() const {
    return priv->get()->ns;
}

QoreNamespace& QoreNamespaceIterator::get() {
    return *priv->get()->ns;
}

const QoreNamespace& QoreNamespaceIterator::get() const {
    return *priv->get()->ns;
}

QoreNamespaceConstIterator::QoreNamespaceConstIterator(const QoreNamespace& ns) : priv(new QorePrivateNamespaceIterator(const_cast<qore_ns_private*>(qore_ns_private::get(ns)))) {
}

QoreNamespaceConstIterator::~QoreNamespaceConstIterator() {
    delete priv;
}

bool QoreNamespaceConstIterator::next() {
    return priv->next();
}

const QoreNamespace* QoreNamespaceConstIterator::operator->() const {
    return priv->get()->ns;
}

const QoreNamespace* QoreNamespaceConstIterator::operator*() const {
    return priv->get()->ns;
}

const QoreNamespace& QoreNamespaceConstIterator::get() const {
    return *priv->get()->ns;
}

class qore_namespace_namespace_iterator {
public:
    DLLLOCAL qore_namespace_namespace_iterator(const qore_ns_private* ns) :
        ns(ns), i(ns->nsl.nsmap.end()) {
    }

    DLLLOCAL bool next() {
        if (i == ns->nsl.nsmap.end()) {
            i = ns->nsl.nsmap.begin();
        } else {
            ++i;
        }
        return (i != ns->nsl.nsmap.end());
    }

    const QoreNamespace& get() const {
        assert(i != ns->nsl.nsmap.end());
        assert(i->second);
        return *i->second;
    }

private:
    const qore_ns_private* ns;
    nsmap_t::const_iterator i;
};

QoreNamespaceNamespaceIterator::QoreNamespaceNamespaceIterator(const QoreNamespace& ns) : priv(new qore_namespace_namespace_iterator(qore_ns_private::get(ns))) {
}

QoreNamespaceNamespaceIterator::~QoreNamespaceNamespaceIterator() {
    delete priv;
}

bool QoreNamespaceNamespaceIterator::next() {
    return priv->next();
}

const QoreNamespace& QoreNamespaceNamespaceIterator::get() const {
    return priv->get();
}

class qore_namespace_function_iterator {
public:
    DLLLOCAL qore_namespace_function_iterator(const qore_ns_private* ns) :
        ns(ns), i(ns->func_list.end()) {
    }

    DLLLOCAL bool next() {
        if (i == ns->func_list.end()) {
            i = ns->func_list.begin();
        } else {
            ++i;
        }
        return (i != ns->func_list.end());
    }

    const QoreExternalFunction& get() const {
        assert(i != ns->func_list.end());
        assert(i->second->getFunction());
        return *reinterpret_cast<const QoreExternalFunction*>(i->second->getFunction());
    }

private:
    const qore_ns_private* ns;
    fl_map_t::const_iterator i;
};

QoreNamespaceFunctionIterator::QoreNamespaceFunctionIterator(const QoreNamespace& ns) : priv(new qore_namespace_function_iterator(qore_ns_private::get(ns))) {
}

QoreNamespaceFunctionIterator::~QoreNamespaceFunctionIterator() {
    delete priv;
}

bool QoreNamespaceFunctionIterator::next() {
    return priv->next();
}

const QoreExternalFunction& QoreNamespaceFunctionIterator::get() const {
    return priv->get();
}

class qore_namespace_constant_iterator {
public:
    DLLLOCAL qore_namespace_constant_iterator(const qore_ns_private* ns) :
        ns(ns), i(ns->constant.cnemap.end()) {
    }

    DLLLOCAL bool next() {
        if (i == ns->constant.cnemap.end()) {
            i = ns->constant.cnemap.begin();
        } else {
            ++i;
        }
        return (i != ns->constant.cnemap.end());
    }

    const QoreExternalConstant& get() const {
        assert(i != ns->constant.cnemap.end());
        assert(i->second);
        return *reinterpret_cast<const QoreExternalConstant*>(i->second);
    }

private:
    const qore_ns_private* ns;
    cnemap_t::const_iterator i;
};

QoreNamespaceConstantIterator::QoreNamespaceConstantIterator(const QoreNamespace& ns)
        : priv(new qore_namespace_constant_iterator(qore_ns_private::get(ns))) {
}

QoreNamespaceConstantIterator::~QoreNamespaceConstantIterator() {
    delete priv;
}

bool QoreNamespaceConstantIterator::next() {
    return priv->next();
}

const QoreExternalConstant& QoreNamespaceConstantIterator::get() const {
    return priv->get();
}

QoreNamespaceClassIterator::QoreNamespaceClassIterator(const QoreNamespace& ns)
        : priv(new ConstClassListIterator(qore_ns_private::get(ns)->classList)) {
}

QoreNamespaceClassIterator::~QoreNamespaceClassIterator() {
    delete priv;
}

bool QoreNamespaceClassIterator::next() {
    return priv->next();
}

const QoreClass& QoreNamespaceClassIterator::get() const {
    return *priv->get();
}

class qore_namespace_globalvar_iterator {
public:
    DLLLOCAL qore_namespace_globalvar_iterator(const qore_ns_private* ns) :
        ns(ns), i(ns->var_list.vmap.end()) {
    }

    DLLLOCAL bool next() {
        if (i == ns->var_list.vmap.end()) {
            i = ns->var_list.vmap.begin();
        } else {
            ++i;
        }
        return (i != ns->var_list.vmap.end());
    }

    const QoreExternalGlobalVar& get() const {
        assert(i != ns->var_list.vmap.end());
        assert(i->second);
        return *reinterpret_cast<const QoreExternalGlobalVar*>(i->second);
    }

private:
    const qore_ns_private* ns;
    map_var_t::const_iterator i;
};

QoreNamespaceGlobalVarIterator::QoreNamespaceGlobalVarIterator(const QoreNamespace& ns) : priv(new qore_namespace_globalvar_iterator(qore_ns_private::get(ns))) {
}

QoreNamespaceGlobalVarIterator::~QoreNamespaceGlobalVarIterator() {
    delete priv;
}

bool QoreNamespaceGlobalVarIterator::next() {
    return priv->next();
}

const QoreExternalGlobalVar& QoreNamespaceGlobalVarIterator::get() const {
    return priv->get();
}

QoreNamespaceTypedHashIterator::QoreNamespaceTypedHashIterator(const QoreNamespace& ns) : priv(new ConstHashDeclListIterator(qore_ns_private::get(ns)->hashDeclList)) {
}

QoreNamespaceTypedHashIterator::~QoreNamespaceTypedHashIterator() {
    delete priv;
}

bool QoreNamespaceTypedHashIterator::next() {
    return priv->next();
}

const TypedHashDecl& QoreNamespaceTypedHashIterator::get() const {
    return *priv->get();
}

QoreNamespaceEnumIterator::QoreNamespaceEnumIterator(const QoreNamespace& ns) : priv(new ConstEnumListIterator(qore_ns_private::get(ns)->enumList)) {
}

QoreNamespaceEnumIterator::~QoreNamespaceEnumIterator() {
    delete priv;
}

bool QoreNamespaceEnumIterator::next() {
    return priv->next();
}

const QoreEnumDecl& QoreNamespaceEnumIterator::get() const {
    return *priv->get();
}
