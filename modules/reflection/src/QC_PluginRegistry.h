/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_PluginRegistry.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#ifndef _QORE_MODULE_REFLECTION_QC_PLUGINREGISTRY_H
#define _QORE_MODULE_REFLECTION_QC_PLUGINREGISTRY_H

#include "qore_reflection.h"

class QoreReflectionPluginRegistry : public AbstractPrivateData {
public:
    bool process_global;
    QoreProgram* pgm;

    DLLLOCAL QoreReflectionPluginRegistry(bool process_global, QoreProgram* pgm = nullptr)
            : process_global(process_global), pgm(pgm) {
    }
};

DLLLOCAL void preinitPluginRegistryClass();
DLLLOCAL QoreClass* initPluginRegistryClass(QoreNamespace& ns);

#endif
