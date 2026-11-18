// Copyright (C) 2026 Qore Technologies, s.r.o.
#include <qore/Qore.h>
#include <atomic>

namespace {
QoreNamespace* probe_ns = nullptr;
qore_classid_t builtin_id;
std::atomic<int> bodies{0}, destructors{0}, released_data{0};

class Data : public AbstractPrivateData {
    ~Data() override { ++released_data; }
};

void construct(QoreObject* self, qore_classid_t id, const QoreListNode* args, ExceptionSink* xsink) {
    self->setPrivate(id, new Data);
    ++bodies;
    if (args->retrieveEntry(1).getAsBool()) {
        xsink->raiseException("NATIVE-BODY", "constructor body failed");
    }
}
void builtin_constructor(QoreObject* self, const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    construct(self, builtin_id, args, xsink);
}
void external_constructor(const QoreMethod& method, const void*, QoreObject* self, const QoreListNode* args,
        RuntimeConfig&, ExceptionSink* xsink) {
    construct(self, method.getClass()->getID(), args, xsink);
}
void destruct(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    ++destructors;
    data->deref(xsink);
}
QoreValue counts(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), xsink);
    result->push(static_cast<int64>(bodies.load()), xsink);
    result->push(static_cast<int64>(destructors.load()), xsink);
    result->push(static_cast<int64>(released_data.load()), xsink);
    return result.release();
}
void init(QoreModuleInitContext&, ExceptionSink&) {
    probe_ns = new QoreNamespace("CtorProbe");
    QoreClass* builtin = new QoreClass("Builtin", "CtorProbe");
    builtin_id = builtin->getID();
    builtin->addConstructor(builtin_constructor, Public, QCF_NO_FLAGS, QDOM_DEFAULT, 2,
        autoTypeInfo, QORE_PARAM_NO_ARG, "value", boolTypeInfo, QORE_PARAM_NO_ARG, "fail");
    builtin->setDestructor(destruct);
    probe_ns->addSystemClass(builtin);
    QoreClass* external = new QoreClass("External", "CtorProbe");
    external->addConstructor(nullptr, external_constructor, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        {autoTypeInfo, boolTypeInfo}, {}, {"value", "fail"});
    external->setDestructor(destruct);
    probe_ns->addSystemClass(external);
    probe_ns->addBuiltinVariant("counts", counts, QCF_NO_FLAGS, QDOM_DEFAULT, autoListTypeInfo, 0);
}
void ns_init(QoreNamespace*, QoreNamespace* qns, ExceptionSink&) { qns->addNamespace(probe_ns->copy()); }
void cleanup() { delete probe_ns; probe_ns = nullptr; }
}
extern "C" DLLEXPORT void ctorprobe_qore_module_desc(QoreModuleInfo& info) {
    info.name = "ctorprobe";
    info.version = "1.0";
    info.desc = "Constructor argument cleanup regression fixture";
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
