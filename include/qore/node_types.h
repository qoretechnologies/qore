/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    node_types.h

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

#ifndef _QORE_NODE_TYPES_H

#define _QORE_NODE_TYPES_H

/** @file node_types.h
    defines qore node type constants for all types implemented by the library
 */

// qore global system type constants
// value types must come first to support the operator matrix optimization
const qore_type_t NT_NOTHING                = 0;  //!< type value for QoreNothingNode
const qore_type_t NT_INT                    = 1;  //!< type value for integers (QoreValue only)
const qore_type_t NT_FLOAT                  = 2;  //!< type value for floating-point values (QoreValue only)
const qore_type_t NT_STRING                 = 3;  //!< type value for QoreStringNode
const qore_type_t NT_DATE                   = 4;  //!< type value for DateTimeNode
const qore_type_t NT_BOOLEAN                = 5;  //!< type value for bools (QoreValue only)
const qore_type_t NT_NULL                   = 6;  //!< type value for QoreNullNode
const qore_type_t NT_BINARY                 = 7;  //!< type value for BinaryNode
const qore_type_t NT_LIST                   = 8;  //!< type value for QoreListNode
const qore_type_t NT_HASH                   = 9;  //!< type value for QoreHashNode
const qore_type_t NT_OBJECT                 = 10; //!< type value for QoreObject
const qore_type_t NT_NUMBER                 = 11; //!< type value for QoreNumberNode
const qore_type_t NT_CONTEXTREF             = 12; //!< type value for ContextrefNode
const qore_type_t NT_COMPLEXCONTEXTREF      = 13; //!< type value for ComplexContextrefNode
const qore_type_t NT_VARREF                 = 14; //!< type value for VarRefNode
const qore_type_t NT_TREE                   = 15; //!< type value for QoreTreeNode
const qore_type_t NT_FIND                   = 16; //!< type value for FindNode
const qore_type_t NT_FUNCTION_CALL          = 17; //!< type value for FunctionCallNode
const qore_type_t NT_SELF_VARREF            = 18; //!< type value for SelfVarrefNode
const qore_type_t NT_SCOPE_REF              = 19; //!< type value for ScopedObjectCallNode
const qore_type_t NT_CONSTANT               = 20; //!< type value for ScopedRefNode (private class)
const qore_type_t NT_BAREWORD               = 21; //!< type value for BarewordNode
const qore_type_t NT_REFERENCE              = 22; //!< type value for ReferenceNode
const qore_type_t NT_CONTEXT_ROW            = 23; //!< type value for ContextRowNode
const qore_type_t NT_CLASSREF               = 24; //!< type value for ClassRefNode
const qore_type_t NT_OBJMETHREF             = 25; //!< type value for AbstractParseObjectMethodReferenceNode
const qore_type_t NT_FUNCREF                = 26; //!< type value for ResolvedCallReferenceNode
const qore_type_t NT_FUNCREFCALL            = 27; //!< type value for CallReferenceCallNode
const qore_type_t NT_CLOSURE                = 28; //!< type value for QoreClosureParseNode (private class)
const qore_type_t NT_RUNTIME_CLOSURE        = 29; //!< type value for ResolvedCallReferenceNode (QoreClosureNode, QoreObjectClosureNode)
const qore_type_t NT_IMPLICIT_ARG           = 30; //!< type value for QoreImplicitArgumentNode (private class)
const qore_type_t NT_METHOD_CALL            = 31; //!< type value for MethodCallNode (private class)
const qore_type_t NT_STATIC_METHOD_CALL     = 32; //!< type value for StaticMethodCallNode (private class)
const qore_type_t NT_SELF_CALL              = 33; //!< type value for SelfFunctionCallNode (private class)
const qore_type_t NT_OPERATOR               = 34; //!< type value for QoreOperatorNode (private class)
const qore_type_t NT_IMPLICIT_ELEMENT       = 35; //!< type value for QoreImplicitElementNode (private clas)
const qore_type_t NT_CLASS_VARREF           = 36; //!< type value for StaticClassVarRefNode (private class)
const qore_type_t NT_PROGRAM_FUNC_CALL      = 37; //!< type value for ProgramFunctionCallNode (private class)
const qore_type_t NT_PARSEREFERENCE         = 38; //!< type value for ParseReferenceNode (private class)
const qore_type_t NT_BACKQUOTE              = 39; //!< type value for BackquoteNode
const qore_type_t NT_RTCONSTREF             = 40; //!< type value for RuntimeConstantRefNode
const qore_type_t NT_PARSE_HASH             = 41; //!< type value for QoreParseHashNode
const qore_type_t NT_PARSE_LIST             = 42; //!< type value for QoreParseListNode
const qore_type_t NT_PARSE_NEW_COMPLEX_TYPE = 43; //!< type value for ParseNewComplexTypeNode
const qore_type_t NT_NEW_HASHDECL           = 44; //!< type value for NewHashDeclNode
const qore_type_t NT_WEAKREF                = 45; //!< type value for WeakReferenceNode
const qore_type_t NT_ELLIPSES               = 46; //!< type value for QoreEllipsesNode
const qore_type_t NT_NEW_OBJECT             = 47; //!< type value for NewObjectCallNode
const qore_type_t NT_WEAKREF_HASH           = 48; //!< type value for WeakHashReferenceNode
const qore_type_t NT_WEAKREF_LIST           = 49; //!< type value for WeakListReferenceNode
const qore_type_t NT_ENUM                   = 50; //!< type value for enum values (TAG_ENUM in QoreValue)

//! number of types implemented in the Qore library
#define QORE_NUM_TYPES 50

//! number of simple value types (not containers)
#define NUM_SIMPLE_TYPES 8

//! number of potential value types (including container types)
#define NUM_VALUE_TYPES 12

#endif
