// Copyright (C) 2026 Qore Technologies, s.r.o.
// A binary module that adds a namespace with a class and a function to the calling Program at run time (qore#7540):
// the class's constructor, destructor, method and private data are this module's code and data, so a release of the
// class or of its objects after the module is unloaded would run unloaded code
#include <qore/Qore.h>
#include <atomic>
#include <memory>

namespace {
QoreNamespace* rtsurv_ns = nullptr;
std::atomic<int> live_data{0};

class SurvData : public AbstractPrivateData {
public:
    SurvData() { ++live_data; }
protected:
    ~SurvData() override { --live_data; }
};

struct ClassId {
    qore_classid_t id = 0;
};
// the ID of the class added last; the test adds one class per process
ClassId surv_class;

void surv_constructor(QoreObject* self, const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    self->setPrivate(surv_class.id, new SurvData);
}
void surv_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    data->deref(xsink);
}
QoreValue surv_get(QoreObject*, AbstractPrivateData*, const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    return static_cast<int64>(live_data.load());
}
QoreValue surv_f(const QoreListNode* args, RuntimeConfig&, ExceptionSink*) {
    return static_cast<int64>(num_args(args));
}

// adds the namespace to the calling Program
QoreValue rtsurv_add(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreStringValueHelper name(get_hard_value_param(args, 0));
    std::unique_ptr<QoreNamespace> ns(new QoreNamespace(name->c_str()));
    std::string path = std::string("::") + name->c_str() + "::RtSurvClass";
    QoreClass* qc = new QoreClass("RtSurvClass", path.c_str());
    surv_class.id = qc->getID();
    qc->addConstructor(surv_constructor, Public, QCF_NO_FLAGS, QDOM_DEFAULT);
    qc->setDestructor(surv_destructor);
    qc->addMethod("get", surv_get, Public, QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo);
    qc->addBuiltinStaticVar("sv", new QoreStringNode("module"), Public, stringTypeInfo);
    ns->addSystemClass(qc);
    ns->addBuiltinVariant("rtsurv_f", surv_f, QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 1,
        qc->getOrNothingTypeInfo(), QORE_PARAM_NO_ARG, "obj");
    getProgram()->getRootNS()->addNamespace(ns.release());
    return QoreValue();
}

QoreValue rtsurv_live(const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    return static_cast<int64>(live_data.load());
}

void init(QoreModuleInitContext&, ExceptionSink&) {
    rtsurv_ns = new QoreNamespace("RtSurv");
    rtsurv_ns->addBuiltinVariant("rtsurv_add", rtsurv_add, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1,
        stringTypeInfo, QORE_PARAM_NO_ARG, "name");
    rtsurv_ns->addBuiltinVariant("rtsurv_live", rtsurv_live, QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 0);
}
void ns_init(QoreNamespace*, QoreNamespace* qns, ExceptionSink&) { qns->addNamespace(rtsurv_ns->copy()); }
void cleanup() { delete rtsurv_ns; rtsurv_ns = nullptr; }
}

extern "C" DLLEXPORT void rtsurv_qore_module_desc(QoreModuleInfo& info) {
    info.name = "rtsurv";
    info.version = "1.0";
    info.desc = "Run-time builtin declaration lifetime fixture (qore#7540)";
    info.author = "Qore Technologies, s.r.o.";
    info.url = "https://qore.org";
    info.api_major = QORE_MODULE_API_MAJOR;
    info.api_minor = QORE_MODULE_API_MINOR;
    info.init = init;
    info.ns_init = ns_init;
    info.del = cleanup;
    info.license = QL_MIT;
    info.license_str = "MIT";
}
