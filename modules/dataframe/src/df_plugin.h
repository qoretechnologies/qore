/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_plugin.h

    DataFrame plugin-type registration and dispatch hooks

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_DATAFRAME_DF_PLUGIN_H
#define _QORE_DATAFRAME_DF_PLUGIN_H

#include <qore/Qore.h>

DLLLOCAL void registerDataFramePluginTypes(QoreModuleInitContext& ctx, ExceptionSink& xsink);

#endif
