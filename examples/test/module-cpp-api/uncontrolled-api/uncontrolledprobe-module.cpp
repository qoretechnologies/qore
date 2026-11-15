/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file uncontrolledprobe-module.cpp a test binary module with builtins of the uncontrolled-API functional domain

    Test scaffolding for the run-time refusal of uncontrolled APIs to code that a sandbox governs (qore#5741): the core
    has no builtin class of the domain (QDOM_UNCONTROLLED_API) that %Qore code can inherit, so this module provides one,
    the way the jni and python modules provide Java and Python classes.  It is built into the build tree and never
    installed.

    The module has, in the namespace \c UncontrolledProbe:
    - the class \c SideEffect (no restricted domain): its constructor counts the objects created (\c count()), which
      makes a default argument expression that creates one (<tt>new SideEffect()</tt>) visible when it is evaluated;
      \c probeDestructors() counts the runs of the destructor of \c Probe
    - the class \c Probe, of the uncontrolled-API domain: a constructor, \c value() (returns 42),
      <tt>withDefault(SideEffect s = new SideEffect())</tt> (returns 1), the static
      <tt>staticWithDefault(SideEffect s = new SideEffect())</tt> (returns 2), and a destructor
    - the function <tt>call(SideEffect s = new SideEffect())</tt> of the uncontrolled-API domain (returns 3)

    Qore Programming Language

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
#include <qore/Qore.h>

#include <atomic>
#include <cassert>

namespace {
//! the number of SideEffect objects created
std::atomic<int64> side_effects{0};
//! the number of runs of the destructor of Probe
std::atomic<int64> probe_destructors{0};

qore_classid_t CID_SIDEEFFECT;
qore_classid_t CID_PROBE;

//! the namespace of the module, copied into each Program that loads it
QoreNamespace ProbeNs("UncontrolledProbe");

//! the private data of a Probe object
class ProbeData : public AbstractPrivateData {
public:
    ProbeData() = default;

    //! the value that Probe::value() returns
    const int64 value = 42;

protected:
    virtual ~ProbeData() = default;
};

void SideEffect_constructor(const QoreMethod& method, const void* ptr, QoreObject* self, const QoreListNode* args,
        RuntimeConfig& rc, ExceptionSink* xsink) {
    ++side_effects;
}

QoreValue SideEffect_count(const QoreMethod& method, const void* ptr, const QoreListNode* args, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return side_effects.load();
}

QoreValue SideEffect_probeDestructors(const QoreMethod& method, const void* ptr, const QoreListNode* args,
        RuntimeConfig& rc, ExceptionSink* xsink) {
    return probe_destructors.load();
}

void Probe_constructor(const QoreMethod& method, const void* ptr, QoreObject* self, const QoreListNode* args,
        RuntimeConfig& rc, ExceptionSink* xsink) {
    self->setPrivate(CID_PROBE, new ProbeData);
}

void Probe_destructor(const QoreClass& thisclass, const void* ptr, QoreObject* self, AbstractPrivateData* private_data,
        RuntimeConfig& rc, ExceptionSink* xsink) {
    ++probe_destructors;
    private_data->deref(xsink);
}

QoreValue Probe_value(const QoreMethod& method, const void* ptr, QoreObject* self, AbstractPrivateData* private_data,
        const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    assert(private_data);
    return static_cast<ProbeData*>(private_data)->value;
}

QoreValue Probe_withDefault(const QoreMethod& method, const void* ptr, QoreObject* self,
        AbstractPrivateData* private_data, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return 1;
}

QoreValue Probe_staticWithDefault(const QoreMethod& method, const void* ptr, const QoreListNode* args,
        RuntimeConfig& rc, ExceptionSink* xsink) {
    return 2;
}

QoreValue f_call(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return 3;
}

//! returns the default argument list with one argument: an expression that creates a SideEffect object
arg_vec_t side_effect_default(const QoreClass& side_effect) {
    arg_vec_t defaults;
    defaults.push_back(QoreValue(side_effect.getNewObjectExpression()));
    return defaults;
}

void uncontrolledprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    // the module's classes are created once; each Program gets a copy of the namespace
    QoreNamespace* ns = &ProbeNs;

    QoreClass* side_effect = new QoreClass("SideEffect", "Qore::UncontrolledProbe");
    CID_SIDEEFFECT = side_effect->getID();
    side_effect->addConstructor(nullptr, SideEffect_constructor);
    side_effect->addStaticMethod(nullptr, "count", SideEffect_count, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        bigIntTypeInfo);
    side_effect->addStaticMethod(nullptr, "probeDestructors", SideEffect_probeDestructors, Public, QCF_NO_FLAGS,
        QDOM_DEFAULT, bigIntTypeInfo);
    ns->addSystemClass(side_effect);

    const type_vec_t side_effect_param = {side_effect->getTypeInfo()};
    const name_vec_t side_effect_name = {"s"};

    QoreClass* probe = new QoreClass("Probe", "Qore::UncontrolledProbe", QDOM_UNCONTROLLED_API);
    CID_PROBE = probe->getID();
    probe->addConstructor(nullptr, Probe_constructor, Public, QCF_NO_FLAGS, QDOM_UNCONTROLLED_API);
    probe->setDestructor(nullptr, Probe_destructor);
    probe->addMethod(nullptr, "value", Probe_value, Public, QCF_NO_FLAGS, QDOM_UNCONTROLLED_API, bigIntTypeInfo);
    probe->addMethod(nullptr, "withDefault", Probe_withDefault, Public, QCF_NO_FLAGS, QDOM_UNCONTROLLED_API,
        bigIntTypeInfo, side_effect_param, side_effect_default(*side_effect), side_effect_name);
    probe->addStaticMethod(nullptr, "staticWithDefault", Probe_staticWithDefault, Public, QCF_NO_FLAGS,
        QDOM_UNCONTROLLED_API, bigIntTypeInfo, side_effect_param, side_effect_default(*side_effect),
        side_effect_name);
    ns->addSystemClass(probe);

    // the variadic parameters: the type, the default value (the variant takes over the reference) and the name
    QoreSimpleValue call_default(QoreValue(side_effect->getNewObjectExpression()));
    ns->addBuiltinVariant(nullptr, "call", f_call, QCF_NO_FLAGS, QDOM_UNCONTROLLED_API, bigIntTypeInfo, 1,
        side_effect->getTypeInfo(), call_default, "s");

}

void uncontrolledprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(ProbeNs.copy());
}

void uncontrolledprobe_module_delete() {
}
}

extern "C" DLLEXPORT void uncontrolledprobe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "uncontrolledprobe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module with builtins of the uncontrolled-API functional domain";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = uncontrolledprobe_module_init;
    mod_info.ns_init = uncontrolledprobe_module_ns_init;
    mod_info.del = uncontrolledprobe_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
