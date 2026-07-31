/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_Datasource.h

  Qore Programming Language

  Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

/*
   FIXME: commit()s when autocommit=true should be made here, also after
          select()s (in case of a select for update, for example)

   FIXME: when raising an timeout exception there is a race condition
          getting the TID of the thread holding the lock, because the lock
	  could have been released after the ::enter() call fails... but it's
	  only cosmetic (for the exception text)
 */

#ifndef _QORE_QC_DATASOURCE_H

#define _QORE_QC_DATASOURCE_H

#ifdef _QORE_LIB_INTERN
#include "qore/intern/ManagedDatasource.h"

DLLEXPORT extern qore_classid_t CID_DATASOURCE;
DLLLOCAL extern QoreClass* QC_DATASOURCE;
DLLLOCAL extern QoreClass* QC_ABSTRACTDATASOURCE;
DLLLOCAL extern QoreClass* QC_ABSTRACTSQLSTATEMENT;

DLLLOCAL QoreClass* initDatasourceClass(QoreNamespace& ns);

// mutation observer hashdecls; see design/datasource-mutation-observer.md
DLLLOCAL TypedHashDecl* init_hashdecl_SqlMutationInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SqlMutationEventInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SqlMutationDecisionInfo(QoreNamespace& ns);

#endif // _QORE_LIB_INTERN
#endif // _QORE_SQL_OBJECTS_DATASOURCE_H
