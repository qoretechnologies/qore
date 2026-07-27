/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ql_json.h

    qore JSON support functions

    Qore Programming Language

    Copyright 2003 - 2022 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_QL_JSON_H

#define _QORE_QL_JSON_H

// JGF_* generation flags, parse_json() and the serialization entry points are declared in
// QoreJson.h, included from qore-json-module.h

DLLEXPORT QoreStringNode* make_jsonrpc_request_args(const std::string& version, const QoreListNode* params, QoreValue id, unsigned offset, ExceptionSink* xsink);
DLLEXPORT QoreStringNode* make_jsonrpc_request(const std::string& version, const QoreListNode* params, QoreValue id, unsigned offset, ExceptionSink* xsink);

DLLLOCAL void init_json_functions(QoreNamespace& ns);
DLLLOCAL void init_json_constants(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_JwtHeader(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_JwtClaims(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_JwtDecodeResult(QoreNamespace& ns);
DLLLOCAL extern const TypedHashDecl* hashdeclJwtHeader;
DLLLOCAL extern const TypedHashDecl* hashdeclJwtClaims;
DLLLOCAL extern const TypedHashDecl* hashdeclJwtDecodeResult;

#endif // _QORE_QL_JSON_H
