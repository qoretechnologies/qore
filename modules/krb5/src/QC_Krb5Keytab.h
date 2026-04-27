/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Krb5Keytab.h

    Qore Kerberos Module

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#ifndef _QC_KRB5KEYTAB_H
#define _QC_KRB5KEYTAB_H

#include <qore/Qore.h>

DLLLOCAL TypedHashDecl* init_hashdecl_Krb5KeytabEntryInfo(QoreNamespace& ns);
DLLLOCAL extern TypedHashDecl* hashdeclKrb5KeytabEntryInfo;

DLLLOCAL QoreClass* initKrb5KeytabClass(QoreNamespace& ns);

#endif
