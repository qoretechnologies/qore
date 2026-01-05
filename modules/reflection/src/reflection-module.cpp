/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    reflection-module.cpp

    Qore reflection module

    Copyright (C) 2017 - 2024 Qore Technologies s.r.o.

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

#include "qore/Qore.h"

#include "QC_AbstractVariant.h"
#include "QC_FunctionVariant.h"
#include "QC_AbstractMethodVariant.h"
#include "QC_PseudoMethodVariant.h"
#include "QC_StaticMethodVariant.h"
#include "QC_NormalMethodVariant.h"
#include "QC_ConstructorMethodVariant.h"
#include "QC_DestructorMethodVariant.h"
#include "QC_CopyMethodVariant.h"
#include "QC_AbstractReflectionFunction.h"
#include "QC_Function.h"
#include "QC_AbstractMethod.h"
#include "QC_PseudoMethod.h"
#include "QC_NormalMethod.h"
#include "QC_StaticMethod.h"
#include "QC_ConstructorMethod.h"
#include "QC_DestructorMethod.h"
#include "QC_CopyMethod.h"
#include "QC_AbstractClass.h"
#include "QC_Class.h"
#include "QC_PseudoClass.h"
#include "QC_AbstractMember.h"
#include "QC_AbstractClassMember.h"
#include "QC_NormalMember.h"
#include "QC_StaticMember.h"
#include "QC_AbstractConstant.h"
#include "QC_ClassConstant.h"
#include "QC_Type.h"
#include "QC_Namespace.h"
#include "QC_Constant.h"
#include "QC_GlobalVar.h"
#include "QC_TypedHash.h"
#include "QC_TypedHashMember.h"
#include "QC_Enum.h"
#include "QC_EnumMember.h"

QoreStringNode* reflection_module_init();
void reflection_module_ns_init(QoreNamespace *rns, QoreNamespace *qns);
void reflection_module_delete();

// qore module symbols
DLLEXPORT char qore_module_name[] = "reflection";
DLLEXPORT char qore_module_version[] = PACKAGE_VERSION;
DLLEXPORT char qore_module_description[] = "Qore reflection module";
DLLEXPORT char qore_module_author[] = "David Nichols <david.nichols@qoretechnologies.com>";
DLLEXPORT char qore_module_url[] = "http://qore.org";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = reflection_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = reflection_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = reflection_module_delete;
DLLEXPORT qore_license_t qore_module_license = QL_MIT;
DLLEXPORT char qore_module_license_str[] = "MIT";

QoreNamespace ReflectionNS("Qore::Reflection");

const TypedHashDecl* hashdeclClassAccessInfo,
    * hashdeclMethodAccessInfo;

QoreStringNode* reflection_module_init() {
    // pre-initialize reflection classes
    preinitAbstractVariantClass();
    preinitFunctionVariantClass();
    preinitAbstractMethodVariantClass();
    preinitPseudoMethodVariantClass();
    preinitNormalMethodVariantClass();
    preinitStaticMethodVariantClass();
    preinitConstructorMethodVariantClass();
    preinitDestructorMethodVariantClass();
    preinitCopyMethodVariantClass();
    preinitAbstractReflectionFunctionClass();
    preinitFunctionClass();
    preinitAbstractMethodClass();
    preinitPseudoMethodClass();
    preinitNormalMethodClass();
    preinitStaticMethodClass();
    preinitConstructorMethodClass();
    preinitDestructorMethodClass();
    preinitCopyMethodClass();
    preinitAbstractClassClass();
    preinitClassClass();
    preinitPseudoClassClass();
    preinitAbstractMemberClass();
    preinitAbstractClassMemberClass();
    preinitNormalMemberClass();
    preinitStaticMemberClass();
    preinitAbstractConstantClass();
    preinitClassConstantClass();
    preinitTypeClass();
    preinitNamespaceClass();
    preinitConstantClass();
    preinitGlobalVarClass();
    preinitTypedHashClass();
    preinitTypedHashMemberClass();
    preinitEnumClass();
    preinitEnumMemberClass();

    // now add hashdecls
    hashdeclClassAccessInfo = init_hashdecl_ClassAccessInfo(ReflectionNS);
    hashdeclMethodAccessInfo = init_hashdecl_MethodAccessInfo(ReflectionNS);

    // set up Reflection namespace
    ReflectionNS.addSystemClass(initAbstractVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initFunctionVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initPseudoMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initNormalMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initStaticMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initConstructorMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initDestructorMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initCopyMethodVariantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractReflectionFunctionClass(ReflectionNS));
    ReflectionNS.addSystemClass(initFunctionClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initPseudoMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initNormalMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initStaticMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initConstructorMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initDestructorMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initCopyMethodClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractClassClass(ReflectionNS));
    ReflectionNS.addSystemClass(initClassClass(ReflectionNS));
    ReflectionNS.addSystemClass(initPseudoClassClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractMemberClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractClassMemberClass(ReflectionNS));
    ReflectionNS.addSystemClass(initNormalMemberClass(ReflectionNS));
    ReflectionNS.addSystemClass(initStaticMemberClass(ReflectionNS));
    ReflectionNS.addSystemClass(initAbstractConstantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initClassConstantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initTypeClass(ReflectionNS));
    ReflectionNS.addSystemClass(initNamespaceClass(ReflectionNS));
    ReflectionNS.addSystemClass(initConstantClass(ReflectionNS));
    ReflectionNS.addSystemClass(initGlobalVarClass(ReflectionNS));
    ReflectionNS.addSystemClass(initTypedHashClass(ReflectionNS));
    ReflectionNS.addSystemClass(initTypedHashMemberClass(ReflectionNS));
    ReflectionNS.addSystemClass(initEnumClass(ReflectionNS));
    ReflectionNS.addSystemClass(initEnumMemberClass(ReflectionNS));

    return nullptr;
}

void reflection_module_ns_init(QoreNamespace* rns, QoreNamespace* qns) {
    qns->addNamespace(ReflectionNS.copy());
}

void reflection_module_delete() {
}

