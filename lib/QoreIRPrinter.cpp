/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRPrinter.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#include <qore/intern/QoreIRPrinter.h>

#include <ostream>

#include <qore/intern/QoreIR.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/NewComplexTypeNode.h>
#include <qore/intern/Variable.h>
#include <qore/QoreClass.h>
#include <qore/QoreEnumDecl.h>

static const char* opcodeName(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::ConstInt: return "const.int";
        case QoreIROpcode::ConstFloat: return "const.float";
        case QoreIROpcode::ConstBool: return "const.bool";
        case QoreIROpcode::ConstNothing: return "const.nothing";
        case QoreIROpcode::ConstNull: return "const.null";
        case QoreIROpcode::ConstString: return "const.string";
        case QoreIROpcode::ConstDate: return "const.date";
        case QoreIROpcode::ConstEnum: return "const.enum";
        case QoreIROpcode::MakeList: return "make.list";
        case QoreIROpcode::MakeHash: return "make.hash";
        case QoreIROpcode::CreateEmptyList: return "create.empty.list";
        case QoreIROpcode::CreateSizedList: return "create.sized.list";
        case QoreIROpcode::ListAppend: return "list.append";
        case QoreIROpcode::ListPush: return "list.push";
        case QoreIROpcode::ListSize: return "list.size";
        case QoreIROpcode::ListGetInt: return "list.get.int";
        case QoreIROpcode::ListGetFloat: return "list.get.float";
        case QoreIROpcode::ListGetValue: return "list.get.value";
        case QoreIROpcode::ListSetInt: return "list.set.int";
        case QoreIROpcode::ListSetFloat: return "list.set.float";
        case QoreIROpcode::ListSetValue: return "list.set.value";
        case QoreIROpcode::AddInt: return "add.int";
        case QoreIROpcode::AddFloat: return "add.float";
        case QoreIROpcode::AddAny: return "add.any";
        case QoreIROpcode::AddString: return "add.string";
        case QoreIROpcode::StringConcat: return "string.concat";
        case QoreIROpcode::SubInt: return "sub.int";
        case QoreIROpcode::SubFloat: return "sub.float";
        case QoreIROpcode::SubAny: return "sub.any";
        case QoreIROpcode::MulInt: return "mul.int";
        case QoreIROpcode::MulFloat: return "mul.float";
        case QoreIROpcode::MulAny: return "mul.any";
        case QoreIROpcode::DivInt: return "div.int";
        case QoreIROpcode::DivFloat: return "div.float";
        case QoreIROpcode::DivAny: return "div.any";
        case QoreIROpcode::ModInt: return "mod.int";
        case QoreIROpcode::ModAny: return "mod.any";
        case QoreIROpcode::AndInt: return "and.int";
        case QoreIROpcode::AndAny: return "and.any";
        case QoreIROpcode::OrInt: return "or.int";
        case QoreIROpcode::OrAny: return "or.any";
        case QoreIROpcode::XorInt: return "xor.int";
        case QoreIROpcode::XorAny: return "xor.any";
        case QoreIROpcode::ShlInt: return "shl.int";
        case QoreIROpcode::ShlAny: return "shl.any";
        case QoreIROpcode::ShrInt: return "shr.int";
        case QoreIROpcode::ShrAny: return "shr.any";
        case QoreIROpcode::ShlAssignInt: return "shl.assign.int";
        case QoreIROpcode::ShlAssignAny: return "shl.assign.any";
        case QoreIROpcode::ShrAssignInt: return "shr.assign.int";
        case QoreIROpcode::ShrAssignAny: return "shr.assign.any";
        case QoreIROpcode::AddAssignInt: return "add.assign.int";
        case QoreIROpcode::AddAssignFloat: return "add.assign.float";
        case QoreIROpcode::AddAssignAny: return "add.assign.any";
        case QoreIROpcode::SubAssignInt: return "sub.assign.int";
        case QoreIROpcode::SubAssignFloat: return "sub.assign.float";
        case QoreIROpcode::SubAssignAny: return "sub.assign.any";
        case QoreIROpcode::MulAssignInt: return "mul.assign.int";
        case QoreIROpcode::MulAssignFloat: return "mul.assign.float";
        case QoreIROpcode::MulAssignAny: return "mul.assign.any";
        case QoreIROpcode::DivAssignInt: return "div.assign.int";
        case QoreIROpcode::DivAssignFloat: return "div.assign.float";
        case QoreIROpcode::DivAssignAny: return "div.assign.any";
        case QoreIROpcode::ModAssignInt: return "mod.assign.int";
        case QoreIROpcode::ModAssignAny: return "mod.assign.any";
        case QoreIROpcode::AndAssignInt: return "and.assign.int";
        case QoreIROpcode::AndAssignAny: return "and.assign.any";
        case QoreIROpcode::OrAssignInt: return "or.assign.int";
        case QoreIROpcode::OrAssignAny: return "or.assign.any";
        case QoreIROpcode::XorAssignInt: return "xor.assign.int";
        case QoreIROpcode::XorAssignAny: return "xor.assign.any";
        case QoreIROpcode::LoadLValue: return "load.lvalue";
        case QoreIROpcode::StoreLValue: return "store.lvalue";
        case QoreIROpcode::PreIncLValue: return "preinc.lvalue";
        case QoreIROpcode::PreDecLValue: return "predec.lvalue";
        case QoreIROpcode::PostIncLValue: return "postinc.lvalue";
        case QoreIROpcode::PostDecLValue: return "postdec.lvalue";
        case QoreIROpcode::AddAssignLValue: return "add.assign.lvalue";
        case QoreIROpcode::SubAssignLValue: return "sub.assign.lvalue";
        case QoreIROpcode::MulAssignLValue: return "mul.assign.lvalue";
        case QoreIROpcode::DivAssignLValue: return "div.assign.lvalue";
        case QoreIROpcode::ModAssignLValue: return "mod.assign.lvalue";
        case QoreIROpcode::AndAssignLValue: return "and.assign.lvalue";
        case QoreIROpcode::OrAssignLValue: return "or.assign.lvalue";
        case QoreIROpcode::XorAssignLValue: return "xor.assign.lvalue";
        case QoreIROpcode::ShlAssignLValue: return "shl.assign.lvalue";
        case QoreIROpcode::ShrAssignLValue: return "shr.assign.lvalue";
        case QoreIROpcode::ShiftLValue: return "shift.lvalue";
        case QoreIROpcode::UnshiftLValue: return "unshift.lvalue";
        case QoreIROpcode::PopAny: return "pop.any";
        case QoreIROpcode::PushAny: return "push.any";
        case QoreIROpcode::SpliceLValue: return "splice.lvalue";
        case QoreIROpcode::ExtractAny: return "extract.any";
        case QoreIROpcode::ExtractList: return "extract.list";
        case QoreIROpcode::ExtractString: return "extract.string";
        case QoreIROpcode::ExtractBinary: return "extract.binary";
        case QoreIROpcode::RemoveAny: return "remove.any";
        case QoreIROpcode::RemoveList: return "remove.list";
        case QoreIROpcode::RemoveHash: return "remove.hash";
        case QoreIROpcode::RemoveObject: return "remove.object";
        case QoreIROpcode::RemoveString: return "remove.string";
        case QoreIROpcode::RemoveBinary: return "remove.binary";
        case QoreIROpcode::KeysAny: return "keys.any";
        case QoreIROpcode::KeysList: return "keys.list";
        case QoreIROpcode::KeysHash: return "keys.hash";
        case QoreIROpcode::RegexMatchAny: return "regex.match.any";
        case QoreIROpcode::RegexMatchBool: return "regex.match.bool";
        case QoreIROpcode::RegexNMatchBool: return "regex.nmatch.bool";
        case QoreIROpcode::SwitchRegexMatch: return "switch.regex.match";
        case QoreIROpcode::RegexExtractAny: return "regex.extract.any";
        case QoreIROpcode::RegexExtractList: return "regex.extract.list";
        case QoreIROpcode::RegexSubstAny: return "regex.subst.any";
        case QoreIROpcode::RegexSubstString: return "regex.subst.string";
        case QoreIROpcode::InstanceOfBool: return "instanceof.bool";
        case QoreIROpcode::TrimAny: return "trim.any";
        case QoreIROpcode::TrimString: return "trim.string";
        case QoreIROpcode::ChompAny: return "chomp.any";
        case QoreIROpcode::ChompString: return "chomp.string";
        case QoreIROpcode::TransliterateAny: return "transliterate.any";
        case QoreIROpcode::TransliterateString: return "transliterate.string";
        case QoreIROpcode::BackgroundInt: return "background.int";
        case QoreIROpcode::ListAssignAny: return "list.assign.any";
        case QoreIROpcode::ExistsAny: return "exists.any";
        case QoreIROpcode::ExistsBool: return "exists.bool";
        case QoreIROpcode::ElementsAny: return "elements.any";
        case QoreIROpcode::ElementsInt: return "elements.int";
        case QoreIROpcode::DotEvalAny: return "dot.eval.any";
        case QoreIROpcode::DotEvalInt: return "dot.eval.int";
        case QoreIROpcode::DotEvalFloat: return "dot.eval.float";
        case QoreIROpcode::DotEvalString: return "dot.eval.string";
        case QoreIROpcode::DotEvalDate: return "dot.eval.date";
        case QoreIROpcode::DotEvalList: return "dot.eval.list";
        case QoreIROpcode::DotEvalHash: return "dot.eval.hash";
        case QoreIROpcode::DotEvalObject: return "dot.eval.object";
        case QoreIROpcode::MapSelectList: return "map.select.list";
        case QoreIROpcode::HashMap: return "hash.map";
        case QoreIROpcode::HashMapSelect: return "hash.map.select";
        case QoreIROpcode::Foreach: return "foreach";
        case QoreIROpcode::IteratorCreate: return "iterator.create";
        case QoreIROpcode::IteratorNext: return "iterator.next";
        case QoreIROpcode::OnBlockExit: return "on.block.exit";
        case QoreIROpcode::ThreadExit: return "thread.exit";
        case QoreIROpcode::Debug: return "debug";
        case QoreIROpcode::Assert: return "assert";
        case QoreIROpcode::Context: return "context";
        case QoreIROpcode::Summarize: return "summarize";
        case QoreIROpcode::EqInt: return "eq.int";
        case QoreIROpcode::EqFloat: return "eq.float";
        case QoreIROpcode::EqString: return "eq.string";
        case QoreIROpcode::EqAny: return "eq.any";
        case QoreIROpcode::NeInt: return "ne.int";
        case QoreIROpcode::NeFloat: return "ne.float";
        case QoreIROpcode::NeString: return "ne.string";
        case QoreIROpcode::NeAny: return "ne.any";
        case QoreIROpcode::EqHard: return "eq.hard";
        case QoreIROpcode::NeHard: return "ne.hard";
        case QoreIROpcode::LtInt: return "lt.int";
        case QoreIROpcode::LtFloat: return "lt.float";
        case QoreIROpcode::LtString: return "lt.string";
        case QoreIROpcode::LtAny: return "lt.any";
        case QoreIROpcode::LeInt: return "le.int";
        case QoreIROpcode::LeFloat: return "le.float";
        case QoreIROpcode::LeString: return "le.string";
        case QoreIROpcode::LeAny: return "le.any";
        case QoreIROpcode::GtInt: return "gt.int";
        case QoreIROpcode::GtFloat: return "gt.float";
        case QoreIROpcode::GtString: return "gt.string";
        case QoreIROpcode::GtAny: return "gt.any";
        case QoreIROpcode::GeInt: return "ge.int";
        case QoreIROpcode::GeFloat: return "ge.float";
        case QoreIROpcode::GeString: return "ge.string";
        case QoreIROpcode::GeAny: return "ge.any";
        case QoreIROpcode::CmpInt: return "cmp.int";
        case QoreIROpcode::CmpFloat: return "cmp.float";
        case QoreIROpcode::CmpString: return "cmp.string";
        case QoreIROpcode::CmpAny: return "cmp.any";
        case QoreIROpcode::ToBool: return "to.bool";
        case QoreIROpcode::Not: return "not";
        case QoreIROpcode::IsNullOrNothing: return "is.null-or-nothing";
        case QoreIROpcode::Phi: return "phi";
        case QoreIROpcode::UnaryPlusAny: return "unary.plus.any";
        case QoreIROpcode::UnaryMinusInt: return "unary.minus.int";
        case QoreIROpcode::UnaryMinusFloat: return "unary.minus.float";
        case QoreIROpcode::UnaryMinusAny: return "unary.minus.any";
        case QoreIROpcode::FoldlAny: return "foldl.any";
        case QoreIROpcode::FoldlInt: return "foldl.int";
        case QoreIROpcode::FoldlFloat: return "foldl.float";
        case QoreIROpcode::FoldrAny: return "foldr.any";
        case QoreIROpcode::FoldrInt: return "foldr.int";
        case QoreIROpcode::FoldrFloat: return "foldr.float";
        case QoreIROpcode::FoldlSumInt: return "foldl.sum.int";
        case QoreIROpcode::FoldlSumFloat: return "foldl.sum.float";
        case QoreIROpcode::FoldlProdInt: return "foldl.prod.int";
        case QoreIROpcode::FoldlProdFloat: return "foldl.prod.float";
        case QoreIROpcode::FoldlDiffInt: return "foldl.diff.int";
        case QoreIROpcode::FoldlDiffFloat: return "foldl.diff.float";
        case QoreIROpcode::FoldlMinInt: return "foldl.min.int";
        case QoreIROpcode::FoldlMinFloat: return "foldl.min.float";
        case QoreIROpcode::FoldlMaxInt: return "foldl.max.int";
        case QoreIROpcode::FoldlMaxFloat: return "foldl.max.float";
        case QoreIROpcode::FoldrSumInt: return "foldr.sum.int";
        case QoreIROpcode::FoldrSumFloat: return "foldr.sum.float";
        case QoreIROpcode::FoldrProdInt: return "foldr.prod.int";
        case QoreIROpcode::FoldrProdFloat: return "foldr.prod.float";
        case QoreIROpcode::FoldrDiffInt: return "foldr.diff.int";
        case QoreIROpcode::FoldrDiffFloat: return "foldr.diff.float";
        case QoreIROpcode::FoldrMinInt: return "foldr.min.int";
        case QoreIROpcode::FoldrMinFloat: return "foldr.min.float";
        case QoreIROpcode::FoldrMaxInt: return "foldr.max.int";
        case QoreIROpcode::FoldrMaxFloat: return "foldr.max.float";
        case QoreIROpcode::MapAny: return "map.any";
        case QoreIROpcode::MapInt: return "map.int";
        case QoreIROpcode::MapFloat: return "map.float";
        case QoreIROpcode::MapScaleInt: return "map.scale.int";
        case QoreIROpcode::MapScaleFloat: return "map.scale.float";
        case QoreIROpcode::MapOffsetInt: return "map.offset.int";
        case QoreIROpcode::MapOffsetFloat: return "map.offset.float";
        case QoreIROpcode::MapSquareInt: return "map.square.int";
        case QoreIROpcode::MapSquareFloat: return "map.square.float";
        case QoreIROpcode::MapHashKeyValue: return "map.hashkey.value";
        case QoreIROpcode::MapHashKeyInt: return "map.hashkey.int";
        case QoreIROpcode::MapHashKeyOffsetInt: return "map.hashkey.offset.int";
        case QoreIROpcode::MapHashKeyScaleInt: return "map.hashkey.scale.int";
        case QoreIROpcode::HashMapTwoKeys: return "hashmap.twokeys";
        case QoreIROpcode::SelectAny: return "select.any";
        case QoreIROpcode::SelectInt: return "select.int";
        case QoreIROpcode::SelectFloat: return "select.float";
        case QoreIROpcode::SelectPositiveInt: return "select.positive.int";
        case QoreIROpcode::SelectPositiveFloat: return "select.positive.float";
        case QoreIROpcode::SelectNonZeroInt: return "select.nonzero.int";
        case QoreIROpcode::SelectNonZeroFloat: return "select.nonzero.float";
        case QoreIROpcode::FusedMapSelectScalePositiveInt: return "fused.map.select.scale.positive.int";
        case QoreIROpcode::FusedMapSelectScalePositiveFloat: return "fused.map.select.scale.positive.float";
        case QoreIROpcode::FusedMapSelectOffsetPositiveInt: return "fused.map.select.offset.positive.int";
        case QoreIROpcode::FusedMapSelectOffsetPositiveFloat: return "fused.map.select.offset.positive.float";
        case QoreIROpcode::FusedMapSelectSquarePositiveInt: return "fused.map.select.square.positive.int";
        case QoreIROpcode::FusedMapSelectSquarePositiveFloat: return "fused.map.select.square.positive.float";
        case QoreIROpcode::FusedMapFoldlSumScaleInt: return "fused.map.foldl.sum.scale.int";
        case QoreIROpcode::FusedMapFoldlSumScaleFloat: return "fused.map.foldl.sum.scale.float";
        case QoreIROpcode::FusedMapFoldlSumSquareInt: return "fused.map.foldl.sum.square.int";
        case QoreIROpcode::FusedMapFoldlSumSquareFloat: return "fused.map.foldl.sum.square.float";
        case QoreIROpcode::FusedMapFoldlProdScaleInt: return "fused.map.foldl.prod.scale.int";
        case QoreIROpcode::FusedMapFoldlProdScaleFloat: return "fused.map.foldl.prod.scale.float";
        case QoreIROpcode::MapSelectAny: return "map.select.any";
        case QoreIROpcode::HashMapAny: return "hash.map.any";
        case QoreIROpcode::HashMapSelectAny: return "hash.map.select.any";
        case QoreIROpcode::RangeAny: return "range.any";
        case QoreIROpcode::RangeInt: return "range.int";
        case QoreIROpcode::RangeFloat: return "range.float";
        case QoreIROpcode::RangeDate: return "range.date";
        case QoreIROpcode::RangeSliceAny: return "range.slice.any";
        case QoreIROpcode::RangeSliceInt: return "range.slice.int";
        case QoreIROpcode::RangeSliceFloat: return "range.slice.float";
        case QoreIROpcode::CastAny: return "cast.any";
        case QoreIROpcode::CastList: return "cast.list";
        case QoreIROpcode::CastHash: return "cast.hash";
        case QoreIROpcode::CastObject: return "cast.object";
        case QoreIROpcode::CastEnum: return "cast.enum";
        case QoreIROpcode::Br: return "br";
        case QoreIROpcode::BrIf: return "br.if";
        case QoreIROpcode::SwitchInt: return "switch.int";
        case QoreIROpcode::SwitchString: return "switch.string";
        case QoreIROpcode::Return: return "return";
        case QoreIROpcode::ReturnNothing: return "return.nothing";
        case QoreIROpcode::LoadLocal: return "load.local";
        case QoreIROpcode::StoreLocal: return "store.local";
        case QoreIROpcode::UninstantiateLocal: return "uninstantiate.local";
        case QoreIROpcode::LoadArg: return "load.arg";
        case QoreIROpcode::LoadClosure: return "load.closure";
        case QoreIROpcode::StoreClosure: return "store.closure";
        case QoreIROpcode::LoadGlobal: return "load.global";
        case QoreIROpcode::StoreGlobal: return "store.global";
        case QoreIROpcode::LoadThreadLocal: return "load.threadlocal";
        case QoreIROpcode::StoreThreadLocal: return "store.threadlocal";
        case QoreIROpcode::LoadImplicitArg: return "load.implicit.arg";
        case QoreIROpcode::LoadImplicitArgv: return "load.implicit.argv";
        case QoreIROpcode::LoadImplicitElement: return "load.implicit.element";
        case QoreIROpcode::PushImplicitArg: return "push.implicit.arg";
        case QoreIROpcode::SetImplicitArgv: return "set.implicit.argv";
        case QoreIROpcode::PopImplicitArg: return "pop.implicit.arg";
        case QoreIROpcode::PushImplicitElement: return "push.implicit.element";
        case QoreIROpcode::PopImplicitElement: return "pop.implicit.element";
        case QoreIROpcode::HashKeyAccess: return "hash.key.access";
        case QoreIROpcode::HashKeyAccessInt: return "hash.key.access.int";
        case QoreIROpcode::LoadSelfMember: return "load.self.member";
        case QoreIROpcode::LoadStaticVar: return "load.static.var";
        case QoreIROpcode::NewObject: return "new.object";
        case QoreIROpcode::LoadConstant: return "load.constant";
        case QoreIROpcode::CreateClosure: return "create.closure";
        case QoreIROpcode::CreateCallRef: return "create.call.ref";
        case QoreIROpcode::CreateMethodRef: return "create.method.ref";
        case QoreIROpcode::CreateParseRef: return "create.parse.ref";
        case QoreIROpcode::NewHashDecl: return "new.hash.decl";
        case QoreIROpcode::NewComplexHash: return "new.complex.hash";
        case QoreIROpcode::NewComplexList: return "new.complex.list";
        case QoreIROpcode::VrnConstruct: return "vrn.construct";
        case QoreIROpcode::HashSetKeyValue: return "hash.set.key.value";
        case QoreIROpcode::IteratorCreateReverse: return "iterator.create.reverse";
        case QoreIROpcode::Call: return "call";
        case QoreIROpcode::CallDirect: return "call.direct";
        case QoreIROpcode::CallIndirect: return "call.indirect";
        case QoreIROpcode::CallMethod: return "call.method";
        case QoreIROpcode::CallMethodDirect: return "call.method.direct";
        case QoreIROpcode::InvokeMethodDirect: return "invoke.method.direct";
        case QoreIROpcode::CallStatic: return "call.static";
        case QoreIROpcode::CallStaticDirect: return "call.static.direct";
        case QoreIROpcode::DotEvalMethodDirect: return "dot.eval.method.direct";
        case QoreIROpcode::InvokeDotEvalMethodDirect: return "invoke.dot.eval.method.direct";
        case QoreIROpcode::CallClosureDirect: return "call.closure.direct";
        case QoreIROpcode::Invoke: return "invoke";
        case QoreIROpcode::GuardInt: return "guard.int";
        case QoreIROpcode::GetObjectClass: return "get.object.class";
        case QoreIROpcode::GuardFloat: return "guard.float";
        case QoreIROpcode::GuardType: return "guard.type";
        case QoreIROpcode::GuardNotNothing: return "guard.not-nothing";
        case QoreIROpcode::LandingPad: return "landingpad";
        case QoreIROpcode::CatchException: return "catch.exception";
        case QoreIROpcode::CatchCleanup: return "catch.cleanup";
        case QoreIROpcode::Rethrow: return "rethrow";
        case QoreIROpcode::Throw: return "throw";
        case QoreIROpcode::InvokeSimError: return "invoke.sim.error";
        case QoreIROpcode::Incref: return "incref";
        case QoreIROpcode::Decref: return "decref";
        case QoreIROpcode::DecrefNoThrow: return "decref.nothrow";
        case QoreIROpcode::ScopeEnter: return "scope.enter";
        case QoreIROpcode::ScopeExit: return "scope.exit";
        case QoreIROpcode::RefForeachInit: return "ref.foreach.init";
        case QoreIROpcode::RefForeachSize: return "ref.foreach.size";
        case QoreIROpcode::RefForeachGetEntry: return "ref.foreach.get.entry";
        case QoreIROpcode::RefForeachRecord: return "ref.foreach.record";
        case QoreIROpcode::RefForeachFinalize: return "ref.foreach.finalize";
        case QoreIROpcode::RefForeachCleanup: return "ref.foreach.cleanup";
        case QoreIROpcode::AddNumber: return "add.number";
        case QoreIROpcode::SubNumber: return "sub.number";
        case QoreIROpcode::MulNumber: return "mul.number";
        case QoreIROpcode::DivNumber: return "div.number";
        case QoreIROpcode::HashKeyStore: return "hash.key.store";
        case QoreIROpcode::ListIndexAccess: return "list.index.access";
        case QoreIROpcode::ListIndexStore: return "list.index.store";
        case QoreIROpcode::AddAssignLocalInt: return "add.assign.local.int";
        case QoreIROpcode::IncrementLocalInt: return "increment.local.int";
        case QoreIROpcode::BranchIfLtLocalInt: return "br.if.lt.local.int";
    }
    return "unknown";
}

static void printConstant(const QoreIRConstInstruction& inst, std::ostream& out) {
    switch (inst.constant.kind) {
        case QoreIRConstant::Kind::Int:
            out << inst.constant.int_value;
            break;
        case QoreIRConstant::Kind::Float:
            out << inst.constant.float_value;
            break;
        case QoreIRConstant::Kind::Bool:
            out << (inst.constant.bool_value ? "true" : "false");
            break;
        case QoreIRConstant::Kind::Nothing:
            out << "nothing";
            break;
        case QoreIRConstant::Kind::Null:
            out << "null";
            break;
        case QoreIRConstant::Kind::String:
            out << "\"" << inst.constant.string_value << "\"";
            break;
        case QoreIRConstant::Kind::Date:
            out << (inst.constant.date_is_relative ? "rel:" : "abs:") << inst.constant.date_microseconds;
            break;
        case QoreIRConstant::Kind::Enum:
            if (inst.constant.enum_member) {
                out << inst.constant.enum_member->getEnumDecl()->getNamespacePath()
                    << "::" << inst.constant.enum_member->getName();
            } else {
                out << "<null-enum>";
            }
            break;
    }
}

void QoreIRPrinter::print(const QoreIRFunction& func, std::ostream& out) {
    out << "func @" << func.name << "\n";
    for (const auto& block : func.blocks) {
        out << block->name << ":\n";
        for (const auto& inst : block->instructions) {
            out << "  " << opcodeName(inst->opcode);
            if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (invoke_inst && invoke_inst->invoke_opcode != QoreIROpcode::Invoke) {
                    out << "." << opcodeName(invoke_inst->invoke_opcode);
                }
            }
            if (inst->opcode == QoreIROpcode::LoadLocal || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal
                    || inst->opcode == QoreIROpcode::LoadClosure || inst->opcode == QoreIROpcode::StoreClosure) {
                auto* local_inst = dynamic_cast<const QoreIRLocalInstruction*>(inst.get());
                if (local_inst && local_inst->local) {
                    out << " @" << local_inst->local->getName();
                }
            } else if (inst->opcode == QoreIROpcode::LoadGlobal || inst->opcode == QoreIROpcode::StoreGlobal
                    || inst->opcode == QoreIROpcode::LoadThreadLocal
                    || inst->opcode == QoreIROpcode::StoreThreadLocal) {
                auto* var_inst = dynamic_cast<const QoreIRVarInstruction*>(inst.get());
                if (var_inst && var_inst->var) {
                    out << " $" << var_inst->var->getName();
                }
            } else if (inst->opcode == QoreIROpcode::LoadImplicitArg) {
                auto* impl_arg_inst = dynamic_cast<const QoreIRImplicitArgInstruction*>(inst.get());
                if (impl_arg_inst) {
                    out << " $" << (impl_arg_inst->offset + 1);  // $1 is offset 0, etc.
                }
            } else if (inst->opcode == QoreIROpcode::LoadLValue || inst->opcode == QoreIROpcode::StoreLValue
                    || inst->opcode == QoreIROpcode::PreIncLValue || inst->opcode == QoreIROpcode::PreDecLValue
                    || inst->opcode == QoreIROpcode::PostIncLValue || inst->opcode == QoreIROpcode::PostDecLValue
                    || inst->opcode == QoreIROpcode::AddAssignLValue || inst->opcode == QoreIROpcode::SubAssignLValue
                    || inst->opcode == QoreIROpcode::MulAssignLValue || inst->opcode == QoreIROpcode::DivAssignLValue
                    || inst->opcode == QoreIROpcode::ModAssignLValue || inst->opcode == QoreIROpcode::AndAssignLValue
                    || inst->opcode == QoreIROpcode::OrAssignLValue || inst->opcode == QoreIROpcode::XorAssignLValue
                    || inst->opcode == QoreIROpcode::ShlAssignLValue
                    || inst->opcode == QoreIROpcode::ShrAssignLValue
                    || inst->opcode == QoreIROpcode::ShiftLValue
                    || inst->opcode == QoreIROpcode::UnshiftLValue
                    || inst->opcode == QoreIROpcode::SpliceLValue) {
                auto* lv_inst = dynamic_cast<const QoreIRLValueInstruction*>(inst.get());
                if (lv_inst && lv_inst->lvalue.hasNode()) {
                    out << " <lvalue>";
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyAccess
                    || inst->opcode == QoreIROpcode::HashKeyAccessInt) {
                auto* hka_inst = dynamic_cast<const QoreIRHashKeyAccessInstruction*>(inst.get());
                if (hka_inst) {
                    out << " ." << hka_inst->key_name;
                }
            } else if (inst->opcode == QoreIROpcode::MapHashKeyValue
                    || inst->opcode == QoreIROpcode::MapHashKeyInt
                    || inst->opcode == QoreIROpcode::MapHashKeyOffsetInt
                    || inst->opcode == QoreIROpcode::MapHashKeyScaleInt
                    || inst->opcode == QoreIROpcode::HashMapTwoKeys) {
                auto* mhk_inst = dynamic_cast<const QoreIRMapHashKeyInstruction*>(inst.get());
                if (mhk_inst) {
                    out << " ." << mhk_inst->key1;
                    if (!mhk_inst->key2.empty()) {
                        out << " ." << mhk_inst->key2;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::LoadSelfMember) {
                auto* sm_inst = dynamic_cast<const QoreIRSelfMemberInstruction*>(inst.get());
                if (sm_inst) {
                    out << " @" << sm_inst->member_name;
                }
            } else if (inst->opcode == QoreIROpcode::LoadStaticVar) {
                auto* sv_inst = dynamic_cast<const QoreIRStaticVarInstruction*>(inst.get());
                if (sv_inst) {
                    out << " $" << sv_inst->var_name;
                }
            } else if (inst->opcode == QoreIROpcode::NewObject) {
                auto* no_inst = dynamic_cast<const QoreIRNewObjectInstruction*>(inst.get());
                if (no_inst && no_inst->qc) {
                    out << " @" << no_inst->qc->getName();
                }
            } else if (inst->opcode == QoreIROpcode::LoadConstant) {
                out << " <constant>";
            } else if (inst->opcode == QoreIROpcode::CreateClosure) {
                out << " <closure>";
            } else if (inst->opcode == QoreIROpcode::CreateCallRef) {
                out << " <call-ref>";
            } else if (inst->opcode == QoreIROpcode::CreateMethodRef) {
                out << " <method-ref>";
            } else if (inst->opcode == QoreIROpcode::CreateParseRef) {
                out << " <parse-ref>";
            } else if (inst->opcode == QoreIROpcode::NewHashDecl) {
                auto* nhd_inst = dynamic_cast<const QoreIRNewHashDeclInstruction*>(inst.get());
                if (nhd_inst && nhd_inst->node && nhd_inst->node->hd) {
                    out << " @" << nhd_inst->node->hd->getName();
                }
            } else if (inst->opcode == QoreIROpcode::NewComplexHash) {
                out << " <complex-hash>";
            } else if (inst->opcode == QoreIROpcode::NewComplexList) {
                out << " <complex-list>";
            } else if (inst->opcode == QoreIROpcode::VrnConstruct) {
                out << " <vrn-construct>";
            } else if (inst->opcode == QoreIROpcode::CallMethodDirect) {
                // Print the devirtualized method name
                auto* direct_inst = dynamic_cast<const QoreIRCallMethodDirectInstruction*>(inst.get());
                if (direct_inst && direct_inst->method && direct_inst->qc) {
                    out << " @" << direct_inst->qc->getName() << "::" << direct_inst->method->getName();
                }
            } else if (inst->opcode == QoreIROpcode::InvokeMethodDirect) {
                // Print the devirtualized method name and targets
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst.get());
                if (invoke_inst && invoke_inst->method && invoke_inst->qc) {
                    out << " @" << invoke_inst->qc->getName() << "::" << invoke_inst->method->getName();
                    if (invoke_inst->normal_target) {
                        out << " normal:" << invoke_inst->normal_target->name;
                    }
                    if (invoke_inst->exception_target) {
                        out << " exception:" << invoke_inst->exception_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::CallStaticDirect) {
                auto* direct_inst = dynamic_cast<const QoreIRCallStaticDirectInstruction*>(inst.get());
                if (direct_inst && direct_inst->method) {
                    out << " @" << direct_inst->method->getClass()->getName()
                        << "::" << direct_inst->method->getName();
                }
            } else if (inst->opcode == QoreIROpcode::DotEvalMethodDirect) {
                auto* direct_inst = dynamic_cast<const QoreIRDotEvalMethodDirectInstruction*>(inst.get());
                if (direct_inst && direct_inst->method && direct_inst->qc) {
                    out << " @" << direct_inst->qc->getName() << "::" << direct_inst->method->getName();
                    if (direct_inst->pseudo) {
                        out << " [pseudo]";
                    }
                }
            } else if (inst->opcode == QoreIROpcode::InvokeDotEvalMethodDirect) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get());
                if (invoke_inst && invoke_inst->method && invoke_inst->qc) {
                    out << " @" << invoke_inst->qc->getName() << "::" << invoke_inst->method->getName();
                    if (invoke_inst->pseudo) {
                        out << " [pseudo]";
                    }
                    if (invoke_inst->normal_target) {
                        out << " normal:" << invoke_inst->normal_target->name;
                    }
                    if (invoke_inst->exception_target) {
                        out << " exception:" << invoke_inst->exception_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Call
                    || inst->opcode == QoreIROpcode::CallIndirect
                    || inst->opcode == QoreIROpcode::CallMethod
                    || inst->opcode == QoreIROpcode::CallStatic
                    || inst->opcode == QoreIROpcode::CastAny
                    || inst->opcode == QoreIROpcode::ExtractAny
                    || inst->opcode == QoreIROpcode::ExtractList
                    || inst->opcode == QoreIROpcode::ExtractString
                    || inst->opcode == QoreIROpcode::ExtractBinary
                    || inst->opcode == QoreIROpcode::RemoveAny
                    || inst->opcode == QoreIROpcode::RemoveList
                    || inst->opcode == QoreIROpcode::RemoveHash
                    || inst->opcode == QoreIROpcode::RemoveObject
                    || inst->opcode == QoreIROpcode::RemoveString
                    || inst->opcode == QoreIROpcode::RemoveBinary
                    || inst->opcode == QoreIROpcode::KeysAny
                    || inst->opcode == QoreIROpcode::KeysList
                    || inst->opcode == QoreIROpcode::KeysHash
                    || inst->opcode == QoreIROpcode::DotEvalAny
                    || inst->opcode == QoreIROpcode::DotEvalInt
                    || inst->opcode == QoreIROpcode::DotEvalFloat
                    || inst->opcode == QoreIROpcode::DotEvalString
                    || inst->opcode == QoreIROpcode::DotEvalDate
                    || inst->opcode == QoreIROpcode::DotEvalList
                    || inst->opcode == QoreIROpcode::DotEvalHash
                    || inst->opcode == QoreIROpcode::DotEvalObject
                    || inst->opcode == QoreIROpcode::MapSelectAny
                    || inst->opcode == QoreIROpcode::MapSelectList
                    || inst->opcode == QoreIROpcode::HashMapAny
                    || inst->opcode == QoreIROpcode::HashMap
                    || inst->opcode == QoreIROpcode::HashMapSelectAny
                    || inst->opcode == QoreIROpcode::HashMapSelect) {
                auto* expr_inst = dynamic_cast<const QoreIRExprInstruction*>(inst.get());
                if (expr_inst && expr_inst->expr.hasNode()) {
                    out << " <expr>";
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (invoke_inst && invoke_inst->expr.hasNode()) {
                    out << " <expr>";
                }
            }
            if (inst->result.isValid()) {
                out << " -> %" << inst->result.id;
            }
            if (inst->opcode != QoreIROpcode::Phi && !inst->operands.empty()) {
                out << " ";
                for (size_t i = 0; i < inst->operands.size(); ++i) {
                    if (i) {
                        out << ", ";
                    }
                    out << "%" << inst->operands[i].id;
                }
            }
            if (inst->opcode == QoreIROpcode::ConstInt
                    || inst->opcode == QoreIROpcode::ConstFloat
                    || inst->opcode == QoreIROpcode::ConstBool
                    || inst->opcode == QoreIROpcode::ConstNothing
                    || inst->opcode == QoreIROpcode::ConstNull
                    || inst->opcode == QoreIROpcode::ConstString
                    || inst->opcode == QoreIROpcode::ConstDate
                    || inst->opcode == QoreIROpcode::ConstEnum) {
                auto* cinst = dynamic_cast<const QoreIRConstInstruction*>(inst.get());
                if (cinst) {
                    out << " = ";
                    printConstant(*cinst, out);
                }
            } else if (inst->opcode == QoreIROpcode::Br) {
                auto* br = dynamic_cast<const QoreIRBranchInstruction*>(inst.get());
                if (br && br->target) {
                    out << " -> " << br->target->name;
                }
            } else if (inst->opcode == QoreIROpcode::BrIf) {
                auto* br = dynamic_cast<const QoreIRBranchIfInstruction*>(inst.get());
                if (br) {
                    out << " %" << br->condition.id;
                    if (br->true_target) {
                        out << " then " << br->true_target->name;
                    }
                    if (br->false_target) {
                        out << " else " << br->false_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (invoke_inst) {
                    if (invoke_inst->normal_target) {
                        out << " then " << invoke_inst->normal_target->name;
                    }
                    if (invoke_inst->exception_target) {
                        out << " else " << invoke_inst->exception_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Phi) {
                auto* phi = dynamic_cast<const QoreIRPhiInstruction*>(inst.get());
                if (phi) {
                    out << " ";
                    for (size_t i = 0; i < phi->incoming.size(); ++i) {
                        if (i) {
                            out << ", ";
                        }
                        out << "[%" << phi->incoming[i].value.id;
                        if (phi->incoming[i].block) {
                            out << ", " << phi->incoming[i].block->name;
                        }
                        out << "]";
                    }
                }
            } else if (inst->opcode == QoreIROpcode::Return || inst->opcode == QoreIROpcode::ReturnNothing) {
                auto* ret = dynamic_cast<const QoreIRReturnInstruction*>(inst.get());
                if (ret && ret->has_value) {
                    out << " %" << ret->value.id;
                }
            } else if (inst->opcode == QoreIROpcode::IteratorCreate) {
                auto* iter = dynamic_cast<const QoreIRIteratorCreateInstruction*>(inst.get());
                if (iter) {
                    out << " %" << iter->iterable.id;
                    if (iter->iterator_func) {
                        out << " <functional>";
                    }
                }
            } else if (inst->opcode == QoreIROpcode::IteratorNext) {
                auto* iter = dynamic_cast<const QoreIRIteratorNextInstruction*>(inst.get());
                if (iter) {
                    out << " %" << iter->iterator.id;
                    if (iter->continue_target) {
                        out << " body " << iter->continue_target->name;
                    }
                    if (iter->done_target) {
                        out << " done " << iter->done_target->name;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::RefForeachInit) {
                auto* rfi = dynamic_cast<const QoreIRRefForeachInitInstruction*>(inst.get());
                if (rfi) {
                    out << " <expr>";
                }
            } else if (inst->opcode == QoreIROpcode::SwitchInt) {
                auto* sw = dynamic_cast<const QoreIRSwitchIntInstruction*>(inst.get());
                if (sw) {
                    out << " %" << sw->switch_val.id;
                    if (sw->default_target) {
                        out << " default " << sw->default_target->name;
                    }
                    out << " [";
                    for (size_t i = 0; i < sw->cases.size(); ++i) {
                        if (i) {
                            out << ", ";
                        }
                        out << sw->cases[i].value << " -> " << sw->cases[i].target->name;
                    }
                    out << "]";
                }
            } else if (inst->opcode == QoreIROpcode::SwitchString) {
                auto* sw = dynamic_cast<const QoreIRSwitchStringInstruction*>(inst.get());
                if (sw) {
                    out << " %" << sw->switch_val.id;
                    if (sw->default_target) {
                        out << " default " << sw->default_target->name;
                    }
                    out << " [";
                    for (size_t i = 0; i < sw->cases.size(); ++i) {
                        if (i) {
                            out << ", ";
                        }
                        out << "\"" << sw->cases[i].value << "\" -> " << sw->cases[i].target->name;
                    }
                    out << "]";
                }
            } else if (inst->opcode == QoreIROpcode::LandingPad) {
                auto* lp = dynamic_cast<const QoreIRLandingPadInstruction*>(inst.get());
                if (lp) {
                    out << " scope_depth=" << lp->scope_depth;
                    if (lp->try_scope_id) {
                        out << " try_scope=" << lp->try_scope_id;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::ScopeEnter) {
                auto* se = dynamic_cast<const QoreIRScopeEnterInstruction*>(inst.get());
                if (se) {
                    out << " scope=" << se->scope_id;
                }
            } else if (inst->opcode == QoreIROpcode::ScopeExit) {
                auto* sx = dynamic_cast<const QoreIRScopeExitInstruction*>(inst.get());
                if (sx) {
                    out << " scope=" << sx->scope_id;
                    if (sx->is_error) {
                        out << " error";
                    }
                }
            }
            out << "\n";
        }
    }
}
