/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Krb5Credentials.h

    Qore Kerberos Module

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#ifndef _QC_KRB5CREDENTIALS_H
#define _QC_KRB5CREDENTIALS_H

#include <qore/Qore.h>

DLLLOCAL TypedHashDecl* init_hashdecl_Krb5CredentialsInfo(QoreNamespace& ns);
DLLLOCAL extern TypedHashDecl* hashdeclKrb5CredentialsInfo;

DLLLOCAL QoreClass* initKrb5CredentialsClass(QoreNamespace& ns);

#endif
