/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRVerifier.cpp

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

#include <qore/intern/QoreIRVerifier.h>

#include <unordered_set>

#include <qore/intern/QoreIR.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/QoreOperatorNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>
#include <qore/intern/SelfVarrefNode.h>
#include <qore/intern/ObjectMethodReferenceNode.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/IfStatement.h>
#include <qore/intern/WhileStatement.h>
#include <qore/intern/ForStatement.h>
#include <qore/intern/ForEachStatement.h>
#include <qore/intern/SwitchStatement.h>
#include <qore/intern/TryStatement.h>
#include <qore/intern/ReturnStatement.h>
#include <qore/intern/ThrowStatement.h>
#include <qore/intern/OnBlockExitStatement.h>
#include <qore/intern/DebugStatement.h>
#include <qore/intern/AssertStatement.h>
#include <qore/intern/ExpressionStatement.h>

// isTerminator() is now defined in QoreIR.h

static bool requiresResult(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::ConstInt:
        case QoreIROpcode::ConstFloat:
        case QoreIROpcode::ConstBool:
        case QoreIROpcode::ConstNothing:
        case QoreIROpcode::ConstNull:
        case QoreIROpcode::ConstString:
        case QoreIROpcode::ConstDate:
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::AddString:
        case QoreIROpcode::StringConcat:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::ShrAny:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignFloat:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignFloat:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignFloat:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignFloat:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::EqString:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::NeString:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtString:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeString:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtString:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeString:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpString:
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::Phi:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::FoldlSumInt:
        case QoreIROpcode::FoldlSumFloat:
        case QoreIROpcode::FoldlProdInt:
        case QoreIROpcode::FoldlProdFloat:
        case QoreIROpcode::FoldlDiffInt:
        case QoreIROpcode::FoldlDiffFloat:
        case QoreIROpcode::FoldlMinInt:
        case QoreIROpcode::FoldlMinFloat:
        case QoreIROpcode::FoldlMaxInt:
        case QoreIROpcode::FoldlMaxFloat:
        case QoreIROpcode::FoldrSumInt:
        case QoreIROpcode::FoldrSumFloat:
        case QoreIROpcode::FoldrProdInt:
        case QoreIROpcode::FoldrProdFloat:
        case QoreIROpcode::FoldrDiffInt:
        case QoreIROpcode::FoldrDiffFloat:
        case QoreIROpcode::FoldrMinInt:
        case QoreIROpcode::FoldrMinFloat:
        case QoreIROpcode::FoldrMaxInt:
        case QoreIROpcode::FoldrMaxFloat:
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::MapScaleInt:
        case QoreIROpcode::MapScaleFloat:
        case QoreIROpcode::MapOffsetInt:
        case QoreIROpcode::MapOffsetFloat:
        case QoreIROpcode::MapSquareInt:
        case QoreIROpcode::MapSquareFloat:
        case QoreIROpcode::MapHashKeyValue:
        case QoreIROpcode::MapHashKeyInt:
        case QoreIROpcode::MapHashKeyOffsetInt:
        case QoreIROpcode::MapHashKeyScaleInt:
        case QoreIROpcode::HashMapTwoKeys:
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat:
        case QoreIROpcode::SelectPositiveInt:
        case QoreIROpcode::SelectPositiveFloat:
        case QoreIROpcode::SelectNonZeroInt:
        case QoreIROpcode::SelectNonZeroFloat:
        case QoreIROpcode::FusedMapSelectScalePositiveInt:
        case QoreIROpcode::FusedMapSelectScalePositiveFloat:
        case QoreIROpcode::FusedMapSelectOffsetPositiveInt:
        case QoreIROpcode::FusedMapSelectOffsetPositiveFloat:
        case QoreIROpcode::FusedMapSelectSquarePositiveInt:
        case QoreIROpcode::FusedMapSelectSquarePositiveFloat:
        case QoreIROpcode::FusedMapFoldlSumScaleInt:
        case QoreIROpcode::FusedMapFoldlSumScaleFloat:
        case QoreIROpcode::FusedMapFoldlSumSquareInt:
        case QoreIROpcode::FusedMapFoldlSumSquareFloat:
        case QoreIROpcode::FusedMapFoldlProdScaleInt:
        case QoreIROpcode::FusedMapFoldlProdScaleFloat:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate:
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat:
        case QoreIROpcode::MakeList:
        case QoreIROpcode::MakeHash:
        case QoreIROpcode::CreateEmptyList:
        case QoreIROpcode::CreateSizedList:
        case QoreIROpcode::ListSize:
        case QoreIROpcode::ListGetInt:
        case QoreIROpcode::ListGetFloat:
        case QoreIROpcode::ListGetValue:
        case QoreIROpcode::GetObjectClass:
        case QoreIROpcode::CallClosureDirect:
        case QoreIROpcode::CastAny:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::SwitchRegexMatch:    // Returns bool for switch regex case match
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ListAssignAny:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelect:
        case QoreIROpcode::LoadLocal:
        case QoreIROpcode::LoadArg:
        case QoreIROpcode::LoadClosure:
        case QoreIROpcode::LoadGlobal:
        case QoreIROpcode::LoadThreadLocal:
        case QoreIROpcode::HashKeyAccess:
        case QoreIROpcode::HashKeyAccessInt:
        case QoreIROpcode::LoadSelfMember:
        case QoreIROpcode::LoadStaticVar:
        case QoreIROpcode::NewObject:
        case QoreIROpcode::LoadConstant:
        case QoreIROpcode::CreateClosure:
        case QoreIROpcode::CreateCallRef:
        case QoreIROpcode::CreateMethodRef:
        case QoreIROpcode::CreateParseRef:
        case QoreIROpcode::NewHashDecl:
        case QoreIROpcode::NewComplexHash:
        case QoreIROpcode::NewComplexList:
        case QoreIROpcode::VrnConstruct:
        case QoreIROpcode::IteratorCreateReverse:
        case QoreIROpcode::LoadImplicitArg:
        case QoreIROpcode::LoadImplicitArgv:
        case QoreIROpcode::LoadImplicitElement:
        case QoreIROpcode::PushImplicitArg:     // Returns old context
        case QoreIROpcode::SetImplicitArgv:     // Returns old context (for foldl)
        case QoreIROpcode::PushImplicitElement: // Returns old element
        case QoreIROpcode::LoadLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::CatchException:
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorNext:
            return true;
        default:
            return false;
    }
}

static int expectedOperands(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Foreach:
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::OnBlockExit:
        case QoreIROpcode::ThreadExit:
        case QoreIROpcode::Debug:
        case QoreIROpcode::Assert:
        case QoreIROpcode::Context:
        case QoreIROpcode::Summarize:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ListAssignAny:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
            return 0;
        case QoreIROpcode::ListAppend:    // 2 operands: list and value to append
        case QoreIROpcode::ListGetInt:    // 2 operands: list and index
        case QoreIROpcode::ListGetFloat:  // 2 operands: list and index
        case QoreIROpcode::ListGetValue:  // 2 operands: list and index
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::AddString:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::ShrAny:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignFloat:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignFloat:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignFloat:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignFloat:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::EqString:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::NeString:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtString:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeString:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtString:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeString:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpString:
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::FoldlSumInt:
        case QoreIROpcode::FoldlSumFloat:
        case QoreIROpcode::FoldlProdInt:
        case QoreIROpcode::FoldlProdFloat:
        case QoreIROpcode::FoldlDiffInt:
        case QoreIROpcode::FoldlDiffFloat:
        case QoreIROpcode::FoldlMinInt:
        case QoreIROpcode::FoldlMinFloat:
        case QoreIROpcode::FoldlMaxInt:
        case QoreIROpcode::FoldlMaxFloat:
        case QoreIROpcode::FoldrSumInt:
        case QoreIROpcode::FoldrSumFloat:
        case QoreIROpcode::FoldrProdInt:
        case QoreIROpcode::FoldrProdFloat:
        case QoreIROpcode::FoldrDiffInt:
        case QoreIROpcode::FoldrDiffFloat:
        case QoreIROpcode::FoldrMinInt:
        case QoreIROpcode::FoldrMinFloat:
        case QoreIROpcode::FoldrMaxInt:
        case QoreIROpcode::FoldrMaxFloat:
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::MapScaleInt:
        case QoreIROpcode::MapScaleFloat:
        case QoreIROpcode::MapOffsetInt:
        case QoreIROpcode::MapOffsetFloat:
        case QoreIROpcode::MapSquareInt:
        case QoreIROpcode::MapSquareFloat:
        case QoreIROpcode::MapHashKeyOffsetInt:
        case QoreIROpcode::MapHashKeyScaleInt:
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat:
        case QoreIROpcode::SelectPositiveInt:
        case QoreIROpcode::SelectPositiveFloat:
        case QoreIROpcode::SelectNonZeroInt:
        case QoreIROpcode::SelectNonZeroFloat:
        case QoreIROpcode::FusedMapSelectScalePositiveInt:
        case QoreIROpcode::FusedMapSelectScalePositiveFloat:
        case QoreIROpcode::FusedMapSelectOffsetPositiveInt:
        case QoreIROpcode::FusedMapSelectOffsetPositiveFloat:
        case QoreIROpcode::FusedMapSelectSquarePositiveInt:
        case QoreIROpcode::FusedMapSelectSquarePositiveFloat:
        case QoreIROpcode::FusedMapFoldlSumScaleInt:
        case QoreIROpcode::FusedMapFoldlSumScaleFloat:
        case QoreIROpcode::FusedMapFoldlSumSquareInt:
        case QoreIROpcode::FusedMapFoldlSumSquareFloat:
        case QoreIROpcode::FusedMapFoldlProdScaleInt:
        case QoreIROpcode::FusedMapFoldlProdScaleFloat:
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate:
            return 2;
        // These *Any opcodes are used in delegate-to-AST mode with 0 operands
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
            return -1;  // Variable operands (delegate-to-AST stores expr in instruction)
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::HashMap:
            return 3;
        case QoreIROpcode::HashMapSelect:
            return 4;
        case QoreIROpcode::CastAny:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
            return 1;
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
            return 4;
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::SwitchRegexMatch:  // 1 operand: switch value
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::Incref:
        case QoreIROpcode::Decref:
        case QoreIROpcode::DecrefNoThrow:
        case QoreIROpcode::GuardInt:
        case QoreIROpcode::GuardFloat:
        case QoreIROpcode::GuardType:
        case QoreIROpcode::GuardNotNothing:
        case QoreIROpcode::ListSize:          // 1 operand: list
        case QoreIROpcode::CreateSizedList:   // 1 operand: capacity
        case QoreIROpcode::GetObjectClass:    // 1 operand: object value
            return 1;
        case QoreIROpcode::LoadArg:
        case QoreIROpcode::LoadClosure:
            return -1;
        case QoreIROpcode::StoreLocal:
        case QoreIROpcode::StoreClosure:
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal:
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::UnshiftLValue:
            return 1;
        case QoreIROpcode::Throw:
            return 1;
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::LoadImplicitArgv:   // No operands - load entire $argv
        case QoreIROpcode::LoadImplicitElement:// No operands - load current $#
            return 0;
        case QoreIROpcode::LoadImplicitArg:    // Uses QoreIRImplicitArgInstruction with offset field, no operands
        case QoreIROpcode::LoadSelfMember:     // Uses QoreIRSelfMemberInstruction with member_name field, no operands
        case QoreIROpcode::LoadStaticVar:      // Uses QoreIRStaticVarInstruction with vi/var_name fields, no operands
        case QoreIROpcode::NewObject:          // Uses QoreIRNewObjectInstruction with qc/variant/args fields, no operands
        case QoreIROpcode::LoadConstant:       // Uses QoreIRLoadConstantInstruction, no operands
        case QoreIROpcode::CreateClosure:      // Uses QoreIRCreateClosureInstruction, no operands
        case QoreIROpcode::CreateCallRef:      // Uses QoreIRCreateCallRefInstruction, no operands
        case QoreIROpcode::CreateMethodRef:    // Uses QoreIRCreateMethodRefInstruction, no operands
        case QoreIROpcode::CreateParseRef:     // Uses QoreIRCreateParseRefInstruction, no operands
        case QoreIROpcode::NewHashDecl:        // Uses QoreIRNewHashDeclInstruction, no operands
        case QoreIROpcode::NewComplexHash:     // Uses QoreIRNewComplexHashInstruction, no operands
        case QoreIROpcode::NewComplexList:     // Uses QoreIRNewComplexListInstruction, no operands
        case QoreIROpcode::VrnConstruct:       // Uses QoreIRVrnConstructInstruction, no operands
            return 0;
        case QoreIROpcode::HashKeyAccess:         // 1 operand: hash value (key stored in QoreIRHashKeyAccessInstruction)
        case QoreIROpcode::HashKeyAccessInt:      // 1 operand: hash value (key stored in QoreIRHashKeyAccessInstruction)
        case QoreIROpcode::MapHashKeyValue:      // 1 operand: list (key stored in QoreIRMapHashKeyInstruction)
        case QoreIROpcode::MapHashKeyInt:        // 1 operand: list (key stored in QoreIRMapHashKeyInstruction)
        case QoreIROpcode::HashMapTwoKeys:       // 1 operand: list (keys stored in QoreIRMapHashKeyInstruction)
        case QoreIROpcode::IteratorCreateReverse: // 1 operand: iterable
            return 1;
        case QoreIROpcode::HashSetKeyValue:    // 3 operands: hash, key, value
        case QoreIROpcode::ListSetInt:         // 3 operands: list, index, value
        case QoreIROpcode::ListSetFloat:       // 3 operands: list, index, value
        case QoreIROpcode::ListSetValue:       // 3 operands: list, index, value
            return 3;
        case QoreIROpcode::PushImplicitArg:    // 1 operand: value to push as $1
        case QoreIROpcode::SetImplicitArgv:    // 1 operand: list to set as $argv
        case QoreIROpcode::PopImplicitArg:     // 1 operand: old context to restore
        case QoreIROpcode::PushImplicitElement:// 1 operand: index value to push as $#
        case QoreIROpcode::PopImplicitElement: // 1 operand: old element to restore
            return 1;
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat:
        case QoreIROpcode::SpliceLValue:
            return 3;
        case QoreIROpcode::ShiftLValue:
            return 0;
        case QoreIROpcode::MakeList:
        case QoreIROpcode::MakeHash:
        case QoreIROpcode::StringConcat:
            return -1;
        case QoreIROpcode::Call:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::CallClosureDirect:
        case QoreIROpcode::Invoke:
            return -1;
        case QoreIROpcode::Phi:
            return -1;
        default:
            return 0;
    }
}

// Collect all branch targets (blocks that can be reached)
static std::unordered_set<const QoreIRBasicBlock*> collectBranchTargets(const QoreIRFunction& func) {
    std::unordered_set<const QoreIRBasicBlock*> targets;
    // Entry block is always reachable
    if (!func.blocks.empty()) {
        targets.insert(func.blocks[0].get());
    }
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            // Collect branch targets
            if (auto* br = dynamic_cast<const QoreIRBranchInstruction*>(inst.get())) {
                if (br->target) {
                    targets.insert(br->target);
                }
            } else if (auto* brif = dynamic_cast<const QoreIRBranchIfInstruction*>(inst.get())) {
                if (brif->true_target) {
                    targets.insert(brif->true_target);
                }
                if (brif->false_target) {
                    targets.insert(brif->false_target);
                }
            } else if (auto* invoke = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get())) {
                if (invoke->normal_target) {
                    targets.insert(invoke->normal_target);
                }
                if (invoke->exception_target) {
                    targets.insert(invoke->exception_target);
                }
            } else if (auto* sw_int = dynamic_cast<const QoreIRSwitchIntInstruction*>(inst.get())) {
                for (const auto& case_info : sw_int->cases) {
                    if (case_info.target) {
                        targets.insert(case_info.target);
                    }
                }
                if (sw_int->default_target) {
                    targets.insert(sw_int->default_target);
                }
            } else if (auto* sw_str = dynamic_cast<const QoreIRSwitchStringInstruction*>(inst.get())) {
                for (const auto& case_info : sw_str->cases) {
                    if (case_info.target) {
                        targets.insert(case_info.target);
                    }
                }
                if (sw_str->default_target) {
                    targets.insert(sw_str->default_target);
                }
            } else if (auto* thr = dynamic_cast<const QoreIRThrowInstruction*>(inst.get())) {
                if (thr->exception_target) {
                    targets.insert(thr->exception_target);
                }
            } else if (auto* inv_md = dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst.get())) {
                if (inv_md->normal_target) {
                    targets.insert(inv_md->normal_target);
                }
                if (inv_md->exception_target) {
                    targets.insert(inv_md->exception_target);
                }
            } else if (auto* inv_de = dynamic_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get())) {
                if (inv_de->normal_target) {
                    targets.insert(inv_de->normal_target);
                }
                if (inv_de->exception_target) {
                    targets.insert(inv_de->exception_target);
                }
            }
            // Also check base class exception_target for other opcodes
            if (inst->exception_target) {
                targets.insert(inst->exception_target);
            }
        }
    }
    return targets;
}

bool QoreIRVerifier::verify(const QoreIRFunction& func, std::string& error) {
    if (func.blocks.empty()) {
        error = "function has no basic blocks";
        return false;
    }

    // Collect branch targets to identify reachable blocks
    std::unordered_set<const QoreIRBasicBlock*> reachable = collectBranchTargets(func);

    std::unordered_set<const QoreIRBasicBlock*> block_set;
    for (const auto& block : func.blocks) {
        if (!block_set.insert(block.get()).second) {
            error = "duplicate basic block pointer";
            return false;
        }
        // Skip unreachable blocks - they may be empty/unterminated dead code
        if (reachable.find(block.get()) == reachable.end()) {
            continue;
        }
        if (block->instructions.empty()) {
            error = "basic block '" + block->name + "' has no instructions";
            return false;
        }
        const QoreIRInstruction* last = block->instructions.back().get();
        if (!isTerminator(last->opcode)) {
            error = "basic block '" + block->name + "' missing terminator";
            if (getenv("QORE_AOT_DEBUG")) {
                fprintf(stderr, "VERIFIER: block '%s' has %zu instructions, last opcode=%d\n",
                    block->name.c_str(), block->instructions.size(), static_cast<int>(last->opcode));
                for (size_t i = 0; i < block->instructions.size(); ++i) {
                    fprintf(stderr, "  inst[%zu]: opcode=%d\n", i,
                        static_cast<int>(block->instructions[i]->opcode));
                }
                // Also dump all blocks
                for (const auto& b : func.blocks) {
                    fprintf(stderr, "BLOCK '%s' (%zu instructions):", b->name.c_str(), b->instructions.size());
                    for (const auto& inst : b->instructions) {
                        fprintf(stderr, " %d", static_cast<int>(inst->opcode));
                    }
                    fprintf(stderr, "\n");
                }
            }
            return false;
        }
    }
    std::unordered_set<uint32_t> value_ids;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (requiresResult(inst->opcode)) {
                if (!inst->result.isValid()) {
                    error = "instruction missing result value: opcode=" + std::to_string(static_cast<int>(inst->opcode))
                        + " in block '" + block->name + "'";
                    return false;
                }
                if (!value_ids.insert(inst->result.id).second) {
                    error = "duplicate result value id";
                    return false;
                }
            } else if (inst->result.isValid()) {
                error = "unexpected result value";
                return false;
            }
        }
    }
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke missing valid targets";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::InvokeMethodDirect) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke.method.direct missing valid targets";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::InvokeDotEvalMethodDirect) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke.dot_eval_method.direct missing valid targets";
                    return false;
                }
            }
            int expected = expectedOperands(inst->opcode);
            if (expected >= 0 && expected != static_cast<int>(inst->operands.size())) {
                error = "unexpected operand count for opcode " + std::to_string(static_cast<int>(inst->opcode))
                    + " (expected " + std::to_string(expected) + ", got " + std::to_string(inst->operands.size()) + ")";
                return false;
            }
            if ((inst->opcode == QoreIROpcode::LoadArg || inst->opcode == QoreIROpcode::LoadClosure)
                    && inst->operands.size() > 1) {
                error = "load.arg/load.closure only support zero or one operand";
                return false;
            }
            for (const auto& op : inst->operands) {
                if (!op.isValid()) {
                    error = "invalid operand value id";
                    return false;
                }
                if (value_ids.find(op.id) == value_ids.end()) {
                    error = "operand references undefined value";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::Br) {
                auto* br = dynamic_cast<const QoreIRBranchInstruction*>(inst.get());
                if (!br || !br->target) {
                    error = "branch missing target";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::BrIf) {
                auto* br = dynamic_cast<const QoreIRBranchIfInstruction*>(inst.get());
                if (!br || !br->condition.isValid()) {
                    error = "branch-if missing condition";
                    return false;
                }
                if (!br->true_target || !br->false_target) {
                    error = "branch-if missing target";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::SwitchInt) {
                auto* sw = dynamic_cast<const QoreIRSwitchIntInstruction*>(inst.get());
                if (!sw || !sw->switch_val.isValid()) {
                    error = "switch.int missing switch value";
                    return false;
                }
                if (!sw->default_target) {
                    error = "switch.int missing default target";
                    return false;
                }
                for (const auto& c : sw->cases) {
                    if (!c.target) {
                        error = "switch.int case missing target";
                        return false;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::IteratorCreate) {
                auto* iter = dynamic_cast<const QoreIRIteratorCreateInstruction*>(inst.get());
                if (!iter || !iter->iterable.isValid()) {
                    error = "iterator.create missing iterable";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::IteratorNext) {
                auto* iter = dynamic_cast<const QoreIRIteratorNextInstruction*>(inst.get());
                if (!iter || !iter->iterator.isValid()) {
                    error = "iterator.next missing iterator";
                    return false;
                }
                if (!iter->done_target || !iter->continue_target) {
                    error = "iterator.next missing targets";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Return
                    || inst->opcode == QoreIROpcode::ReturnNothing) {
                auto* ret = dynamic_cast<const QoreIRReturnInstruction*>(inst.get());
                if (!ret) {
                    error = "return instruction malformed";
                    return false;
                }
                if (inst->opcode == QoreIROpcode::Return && !ret->has_value) {
                    error = "return missing value";
                    return false;
                }
                if (inst->opcode == QoreIROpcode::ReturnNothing && ret->has_value) {
                    error = "return.nothing has value";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Phi) {
                auto* phi = dynamic_cast<const QoreIRPhiInstruction*>(inst.get());
                if (!phi) {
                    error = "phi instruction malformed";
                    return false;
                }
                if (phi->incoming.empty()) {
                    error = "phi missing incoming values";
                    return false;
                }
                if (phi->incoming.size() != inst->operands.size()) {
                    error = "phi operands do not match incoming values";
                    return false;
                }
                for (const auto& incoming : phi->incoming) {
                    if (!incoming.block || block_set.find(incoming.block) == block_set.end()) {
                        error = "phi references unknown block";
                        return false;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::LoadClosure
                    || inst->opcode == QoreIROpcode::StoreClosure) {
                auto* local_inst = dynamic_cast<const QoreIRLocalInstruction*>(inst.get());
                if (!local_inst || !local_inst->local) {
                    error = "local instruction missing local";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LoadGlobal
                    || inst->opcode == QoreIROpcode::StoreGlobal
                    || inst->opcode == QoreIROpcode::LoadThreadLocal
                    || inst->opcode == QoreIROpcode::StoreThreadLocal) {
                auto* var_inst = dynamic_cast<const QoreIRVarInstruction*>(inst.get());
                if (!var_inst || !var_inst->var) {
                    error = "variable instruction missing var";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LoadLValue
                    || inst->opcode == QoreIROpcode::StoreLValue
                    || inst->opcode == QoreIROpcode::PreIncLValue
                    || inst->opcode == QoreIROpcode::PreDecLValue
                    || inst->opcode == QoreIROpcode::PostIncLValue
                    || inst->opcode == QoreIROpcode::PostDecLValue
                    || inst->opcode == QoreIROpcode::AddAssignLValue
                    || inst->opcode == QoreIROpcode::SubAssignLValue
                    || inst->opcode == QoreIROpcode::MulAssignLValue
                    || inst->opcode == QoreIROpcode::DivAssignLValue
                    || inst->opcode == QoreIROpcode::ModAssignLValue
                    || inst->opcode == QoreIROpcode::AndAssignLValue
                    || inst->opcode == QoreIROpcode::OrAssignLValue
                    || inst->opcode == QoreIROpcode::XorAssignLValue
                    || inst->opcode == QoreIROpcode::ShlAssignLValue
                    || inst->opcode == QoreIROpcode::ShrAssignLValue) {
                auto* lv_inst = dynamic_cast<const QoreIRLValueInstruction*>(inst.get());
                if (!lv_inst || !lv_inst->lvalue.hasNode()) {
                    error = "lvalue instruction missing lvalue";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Call
                    || inst->opcode == QoreIROpcode::CallIndirect
                    || inst->opcode == QoreIROpcode::CallMethod
                    || inst->opcode == QoreIROpcode::CallStatic
                    || inst->opcode == QoreIROpcode::CastAny
                    || inst->opcode == QoreIROpcode::CastList
                    || inst->opcode == QoreIROpcode::CastHash
                    || inst->opcode == QoreIROpcode::CastObject
                    || inst->opcode == QoreIROpcode::CastEnum
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
                    || inst->opcode == QoreIROpcode::RegexMatchAny
                    || inst->opcode == QoreIROpcode::RegexMatchBool
                    || inst->opcode == QoreIROpcode::RegexNMatchBool
                    || inst->opcode == QoreIROpcode::RegexExtractAny
                    || inst->opcode == QoreIROpcode::RegexExtractList
                    || inst->opcode == QoreIROpcode::RegexSubstAny
                    || inst->opcode == QoreIROpcode::RegexSubstString
                    || inst->opcode == QoreIROpcode::InstanceOfBool
                    || inst->opcode == QoreIROpcode::TrimAny
                    || inst->opcode == QoreIROpcode::TrimString
                    || inst->opcode == QoreIROpcode::ChompAny
                    || inst->opcode == QoreIROpcode::ChompString
                    || inst->opcode == QoreIROpcode::TransliterateAny
                    || inst->opcode == QoreIROpcode::TransliterateString
                    || inst->opcode == QoreIROpcode::BackgroundInt
                    || inst->opcode == QoreIROpcode::ListAssignAny
                    || inst->opcode == QoreIROpcode::PopAny
                    || inst->opcode == QoreIROpcode::PushAny
                    || inst->opcode == QoreIROpcode::ExistsAny
                    || inst->opcode == QoreIROpcode::ExistsBool
                    || inst->opcode == QoreIROpcode::ElementsAny
                    || inst->opcode == QoreIROpcode::ElementsInt
                    || inst->opcode == QoreIROpcode::DotEvalAny
                    || inst->opcode == QoreIROpcode::DotEvalInt
                    || inst->opcode == QoreIROpcode::DotEvalFloat
                    || inst->opcode == QoreIROpcode::DotEvalString
                    || inst->opcode == QoreIROpcode::DotEvalDate
                    || inst->opcode == QoreIROpcode::DotEvalList
                    || inst->opcode == QoreIROpcode::DotEvalHash
                    || inst->opcode == QoreIROpcode::DotEvalObject) {
                auto* expr_inst = dynamic_cast<const QoreIRExprInstruction*>(inst.get());
                if (!expr_inst || !expr_inst->expr.hasNode()) {
                    error = "expr instruction missing expr";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::SwitchRegexMatch) {
                auto* regex_inst = dynamic_cast<const QoreIRSwitchRegexMatchInstruction*>(inst.get());
                if (!regex_inst || !regex_inst->regex_case) {
                    error = "SwitchRegexMatch instruction missing regex_case";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->expr.hasNode()) {
                    error = "invoke instruction missing expr";
                    return false;
                }
            }
        }
    }
    return true;
}

// --- IR-only local classification ---

//! Returns true if the opcode is a delegate-to-AST statement that executes through
//! StatementBlock::exec() and could access any local on the thread-local variable stack.
static bool isDelegateToASTStatement(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Foreach:       // Reference foreach delegates to AST
        // OnBlockExit: handler body locals are now analyzed via collectLocalsFromStatementBlock()
        // Debug: now lowered inline (expression or block form)
        // Assert: condition is now lowered inline; Assert opcode only on failure path
        case QoreIROpcode::Context:       // Context statement delegates to AST
        case QoreIROpcode::Summarize:     // Summarize statement delegates to AST
        case QoreIROpcode::MapSelectAny:  // Delegate-to-AST functional operator
        case QoreIROpcode::HashMapAny:    // Delegate-to-AST functional operator
        case QoreIROpcode::HashMapSelectAny: // Delegate-to-AST functional operator
            return true;
        default:
            return false;
    }
}

//! Returns true if the opcode is a lvalue operation that modifies variables through
//! the runtime stack (not through StoreLocal).
static bool isLValueOp(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::LoadLValue:
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::SpliceLValue:
            return true;
        default:
            return false;
    }
}

//! Recursively walk an AST expression tree and collect all local variable keys
//! (LocalVar* as void*) referenced by VarRefNode leaves.
//!
//! IMPORTANT: This walker is conservative — for any unrecognized node type,
//! it sets `unknown_node_found` to true, which causes the caller to treat
//! ALL locals as AST-visible.  This ensures correctness even if new AST node
//! types are added to Qore.
static void collectLocalsFromExpr(const QoreValue& expr,
        std::unordered_set<const void*>& ast_locals, bool& unknown_node_found) {
    if (!expr.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return;
    }

    qore_type_t ntype = expr.getType();

    // VarRefNode — leaf node, check if it's a local variable
    if (ntype == NT_VARREF) {
        auto* var_ref = reinterpret_cast<const VarRefNode*>(node);
        qore_var_t type = var_ref->getType();
        if ((type == VT_LOCAL || type == VT_LOCAL_TS) && var_ref->ref.id) {
            ast_locals.insert(reinterpret_cast<const void*>(var_ref->ref.id));
        }
        // VarRefNewObjectNode inherits from both VarRefNode and FunctionCallBase;
        // its constructor args may reference locals that are evaluated through AST
        if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
            if (const QoreParseListNode* pargs = vrn->getParseArgs()) {
                for (size_t i = 0; i < pargs->size(); ++i) {
                    collectLocalsFromExpr(pargs->get(i), ast_locals, unknown_node_found);
                }
            }
            if (const QoreListNode* eargs = vrn->getArgs()) {
                ConstListIterator li(eargs);
                while (li.next()) {
                    collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found);
                }
            }
        }
        return;
    }

    // Self variable reference — leaf node (not a local, no children)
    if (ntype == NT_SELF_VARREF) {
        return;
    }

    // Constants and literals — leaf nodes
    if (ntype == NT_STRING || ntype == NT_INT || ntype == NT_FLOAT || ntype == NT_BOOLEAN
            || ntype == NT_NOTHING || ntype == NT_NULL || ntype == NT_NUMBER
            || ntype == NT_DATE || ntype == NT_BINARY || ntype == NT_HASH
            || ntype == NT_LIST || ntype == NT_BACKQUOTE) {
        return;
    }

    // Binary operators: recurse left and right
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        collectLocalsFromExpr(binop->getLeft(), ast_locals, unknown_node_found);
        collectLocalsFromExpr(binop->getRight(), ast_locals, unknown_node_found);
        return;
    }

    // Binary int-specific operators (inherit separately from QoreBinaryOperatorNode<>)
    if (auto* binop = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node)) {
        collectLocalsFromExpr(binop->getLeft(), ast_locals, unknown_node_found);
        collectLocalsFromExpr(binop->getRight(), ast_locals, unknown_node_found);
        return;
    }

    // Unary/single expression operators: recurse expression
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        collectLocalsFromExpr(unop->getExp(), ast_locals, unknown_node_found);
        return;
    }

    // LValue single expression operators
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        collectLocalsFromExpr(unop->getExp(), ast_locals, unknown_node_found);
        return;
    }

    // Dot eval operator (method call on object): left.method()
    if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        collectLocalsFromExpr(dot->getExpression(), ast_locals, unknown_node_found);
        // Method call node has args that might reference locals
        if (const MethodCallNode* m = dot->getMethodCall()) {
            if (const QoreParseListNode* pargs = m->getParseArgs()) {
                for (size_t i = 0; i < pargs->size(); ++i) {
                    collectLocalsFromExpr(pargs->get(i), ast_locals, unknown_node_found);
                }
            }
            // Also check resolved args — after parse resolution, locals may only
            // be in the resolved args list
            if (const QoreListNode* rargs = m->getArgs()) {
                ConstListIterator li(rargs);
                while (li.next()) {
                    collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found);
                }
            }
        }
        return;
    }

    // Function calls (FunctionCallNode, SelfFunctionCallNode, StaticMethodCallNode,
    // MethodCallNode — all inherit from FunctionCallBase)
    if (auto* call = dynamic_cast<const FunctionCallBase*>(node)) {
        if (const QoreParseListNode* args = call->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalsFromExpr(args->get(i), ast_locals, unknown_node_found);
            }
        }
        if (const QoreListNode* args = call->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found);
            }
        }
        return;
    }

    // Call reference calls: adder(32)
    if (auto* crc = dynamic_cast<const CallReferenceCallNode*>(node)) {
        collectLocalsFromExpr(crc->getExp(), ast_locals, unknown_node_found);
        if (const QoreParseListNode* args = crc->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalsFromExpr(args->get(i), ast_locals, unknown_node_found);
            }
        }
        if (const QoreListNode* args = crc->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found);
            }
        }
        return;
    }

    // Object method reference: \obj.method() — recurse into the object expression
    if (ntype == NT_OBJMETHREF) {
        if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
            collectLocalsFromExpr(omr->getExp(), ast_locals, unknown_node_found);
        }
        // ParseSelfMethodReferenceNode, ParseScopedSelfMethodReferenceNode,
        // StaticMethodReferenceNode have no local variable references (self-based)
        return;
    }

    // Parse reference: \var — recurse into the lvalue expression
    if (ntype == NT_PARSEREFERENCE) {
        auto* pref = reinterpret_cast<const ParseReferenceNode*>(node);
        collectLocalsFromExpr(pref->getLVExp(), ast_locals, unknown_node_found);
        return;
    }

    // Parse list: recurse all elements
    if (ntype == NT_PARSE_LIST) {
        auto* plist = expr.get<const QoreParseListNode>();
        for (size_t i = 0; i < plist->size(); ++i) {
            collectLocalsFromExpr(plist->get(i), ast_locals, unknown_node_found);
        }
        return;
    }

    // Unknown node type — conservatively mark for fallback
    unknown_node_found = true;
}

//! Recursively walks a StatementBlock AST tree to find all local variable references.
//! This is used to determine which locals are referenced by on_block_exit handler bodies
//! so they can be excluded from IR-only classification.
static void collectLocalsFromStatementBlock(const StatementBlock* block,
        std::unordered_set<const void*>& ast_locals, bool& unknown_node_found) {
    if (!block) {
        return;
    }
    for (const auto* stmt : block->getStatements()) {
        if (!stmt) {
            continue;
        }
        // ExpressionStatement: walk expression
        if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            collectLocalsFromExpr(expr_stmt->getExpression(), ast_locals, unknown_node_found);
            continue;
        }
        // IfStatement: walk condition, then-block, else-block
        if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
            collectLocalsFromExpr(if_stmt->getCond(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(if_stmt->getIfCode(), ast_locals,
                    unknown_node_found);
            collectLocalsFromStatementBlock(if_stmt->getElseCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // WhileStatement / DoWhileStatement: walk condition, body
        if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
            collectLocalsFromExpr(while_stmt->getCond(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(while_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // ForStatement: walk init, condition, increment, body
        if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
            collectLocalsFromExpr(for_stmt->getAssignment(), ast_locals, unknown_node_found);
            collectLocalsFromExpr(for_stmt->getCond(), ast_locals, unknown_node_found);
            collectLocalsFromExpr(for_stmt->getIterator(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(for_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // ForEachStatement: walk variable, list, body
        if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
            collectLocalsFromExpr(foreach_stmt->getVar(), ast_locals, unknown_node_found);
            collectLocalsFromExpr(foreach_stmt->getList(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(foreach_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // SwitchStatement: walk switch expression, case expressions and blocks
        if (auto* switch_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
            collectLocalsFromExpr(switch_stmt->getSwitchExp(), ast_locals,
                    unknown_node_found);
            for (const CaseNode* cn = switch_stmt->getCases(); cn; cn = cn->next) {
                collectLocalsFromExpr(cn->val, ast_locals, unknown_node_found);
                collectLocalsFromStatementBlock(cn->code, ast_locals, unknown_node_found);
            }
            continue;
        }
        // TryStatement: walk try block, catch block
        if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
            collectLocalsFromStatementBlock(try_stmt->getTryBlock(), ast_locals,
                    unknown_node_found);
            collectLocalsFromStatementBlock(try_stmt->getCatchBlock(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // ReturnStatement: walk return expression
        if (auto* ret_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
            collectLocalsFromExpr(ret_stmt->getExpression(), ast_locals, unknown_node_found);
            continue;
        }
        // ThrowStatement: walk throw args
        if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(stmt)) {
            collectLocalsFromExpr(throw_stmt->getArgs(), ast_locals, unknown_node_found);
            continue;
        }
        // OnBlockExitStatement: walk handler body
        if (auto* obe_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
            collectLocalsFromStatementBlock(obe_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // DebugStatement: walk expression or block
        if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
            if (debug_stmt->getBlock()) {
                collectLocalsFromStatementBlock(debug_stmt->getBlock(), ast_locals,
                        unknown_node_found);
            }
            if (debug_stmt->getExpression()) {
                collectLocalsFromExpr(debug_stmt->getExpression(), ast_locals,
                        unknown_node_found);
            }
            continue;
        }
        // AssertStatement: walk condition expression
        if (auto* assert_stmt = dynamic_cast<const AssertStatement*>(stmt)) {
            collectLocalsFromExpr(assert_stmt->getCondition(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // Nested StatementBlock: recurse
        if (auto* sub_block = dynamic_cast<const StatementBlock*>(stmt)) {
            collectLocalsFromStatementBlock(sub_block, ast_locals, unknown_node_found);
            continue;
        }
        // Unknown statement type — conservatively mark for fallback
        unknown_node_found = true;
    }
}

//! Extracts the AST expression from an instruction that stores one.
//! Returns nullptr if the instruction type doesn't have an expr field.
static const QoreValue* getInstructionExpr(const QoreIRInstruction* inst) {
    switch (inst->opcode) {
        // QoreIRExprInstruction (Call, CallIndirect, CallMethod, CallStatic, CallClosureDirect, and other expr-based ops)
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallClosureDirect:
            return &static_cast<const QoreIRExprInstruction*>(inst)->expr;

        // CallDirect has its own instruction class with an expr field
        case QoreIROpcode::CallDirect:
            return &static_cast<const QoreIRCallDirectInstruction*>(inst)->expr;

        // Invoke has its own instruction class
        case QoreIROpcode::Invoke:
            return &static_cast<const QoreIRInvokeInstruction*>(inst)->expr;

        // Direct static method call with expr field
        case QoreIROpcode::CallStaticDirect:
            return &static_cast<const QoreIRCallStaticDirectInstruction*>(inst)->expr;

        // Direct dot-eval method call with expr field
        case QoreIROpcode::DotEvalMethodDirect:
            return &static_cast<const QoreIRDotEvalMethodDirectInstruction*>(inst)->expr;
        case QoreIROpcode::InvokeDotEvalMethodDirect:
            return &static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst)->expr;

        // Native access opcodes with expr fields
        case QoreIROpcode::LoadStaticVar:
            return &static_cast<const QoreIRStaticVarInstruction*>(inst)->expr;
        case QoreIROpcode::NewObject:
            return &static_cast<const QoreIRNewObjectInstruction*>(inst)->expr;
        case QoreIROpcode::LoadConstant:
            return &static_cast<const QoreIRLoadConstantInstruction*>(inst)->expr;

        // Closure/reference creation opcodes with expr fields
        case QoreIROpcode::CreateClosure:
            return &static_cast<const QoreIRCreateClosureInstruction*>(inst)->expr;
        case QoreIROpcode::CreateCallRef:
            return &static_cast<const QoreIRCreateCallRefInstruction*>(inst)->expr;
        case QoreIROpcode::CreateMethodRef:
            return &static_cast<const QoreIRCreateMethodRefInstruction*>(inst)->expr;
        case QoreIROpcode::CreateParseRef:
            return &static_cast<const QoreIRCreateParseRefInstruction*>(inst)->expr;

        // Container construction opcodes with expr fields
        case QoreIROpcode::NewHashDecl:
            return &static_cast<const QoreIRNewHashDeclInstruction*>(inst)->expr;
        case QoreIROpcode::NewComplexHash:
            return &static_cast<const QoreIRNewComplexHashInstruction*>(inst)->expr;
        case QoreIROpcode::NewComplexList:
            return &static_cast<const QoreIRNewComplexListInstruction*>(inst)->expr;

        // VarRefNewObjectNode construction
        case QoreIROpcode::VrnConstruct:
            return &static_cast<const QoreIRVrnConstructInstruction*>(inst)->expr;

        // DotEval opcodes delegate method calls to AST — the expr contains the
        // full QoreDotEvalOperatorNode including argument expressions that may
        // reference locals.
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        // Regex ops delegate to AST
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
        // Lvalue compound assignment and list assignment delegate to AST
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::ListAssignAny:
        // Elements/size ops delegate to AST
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        // Map/select/cast ops delegate to AST
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelect:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::CastAny:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::InvokeSimError:
            return &static_cast<const QoreIRExprInstruction*>(inst)->expr;

        default:
            return nullptr;
    }
}

//! Returns true if the opcode is a call/invoke that could trigger AST evaluation
//! and potentially access locals through the thread-local variable stack.
static bool isCallOrInvoke(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Call:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::Invoke:
            return true;
        default:
            return false;
    }
}

void QoreIRFunction::computeIROnlyLocals() {
    ir_only_locals.clear();

    // Collect all locals referenced by LoadLocal/StoreLocal/UninstantiateLocal
    std::unordered_set<const void*> all_locals;

    // Collect all locals referenced by AST expression trees in Call/Invoke/Lvalue
    // instructions.  These locals are accessed through the runtime stack (not through
    // LoadLocal/StoreLocal) and MUST remain AST-visible.
    std::unordered_set<const void*> ast_referenced_locals;

    // Track whether the function has any delegate-to-AST statements
    bool has_delegate_to_ast = false;

    // If the AST walker encounters an unknown node type, we conservatively
    // mark ALL locals as AST-visible.
    bool unknown_node_found = false;

    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            // Collect locals from LoadLocal/StoreLocal/UninstantiateLocal
            if (inst->opcode == QoreIROpcode::LoadLocal ||
                    inst->opcode == QoreIROpcode::StoreLocal ||
                    inst->opcode == QoreIROpcode::UninstantiateLocal) {
                auto* linst = static_cast<const QoreIRLocalInstruction*>(inst.get());
                if (linst->local) {
                    all_locals.insert(reinterpret_cast<const void*>(linst->local));
                }
            }

            // Lvalue operations: walk the lvalue AST expression for local references
            if (isLValueOp(inst->opcode)) {
                auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst.get());
                collectLocalsFromExpr(lvinst->lvalue, ast_referenced_locals,
                        unknown_node_found);
            }

            // Walk the expr AST tree for any instruction type that stores one
            if (const QoreValue* expr = getInstructionExpr(inst.get())) {
                collectLocalsFromExpr(*expr, ast_referenced_locals,
                        unknown_node_found);
            }

            if (isDelegateToASTStatement(inst->opcode)) {
                has_delegate_to_ast = true;
            }

            // OnBlockExit: walk handler body AST to find referenced locals.
            // Only those locals must remain AST-visible — others can be IR-only.
            if (inst->opcode == QoreIROpcode::OnBlockExit) {
                auto* obe_inst = static_cast<const QoreIROnBlockExitInstruction*>(inst.get());
                if (obe_inst->stmt) {
                    collectLocalsFromStatementBlock(obe_inst->stmt->getCode(),
                            ast_referenced_locals, unknown_node_found);
                }
            }
        }
    }

    // Store total local count for Phase 4 optimization (selective reload skip)
    total_local_count = all_locals.size();

    // If the function has delegate-to-AST statements, ALL locals are AST-visible
    // because the AST execution could access any local through the thread-local stack.
    if (has_delegate_to_ast) {
        printd(5, "computeIROnlyLocals '%s': has_delegate_to_ast, all AST-visible\n", name.c_str());
        return;  // ir_only_locals stays empty — all locals are AST-visible
    }

    // If the AST walker hit an unknown node type, conservatively treat all locals
    // as AST-visible.  This ensures correctness even for unhandled AST structures.
    if (unknown_node_found) {
        printd(5, "computeIROnlyLocals '%s': unknown_node_found, all AST-visible\n", name.c_str());
        return;
    }

    printd(5, "computeIROnlyLocals '%s': all_locals=%d ast_referenced=%d\n",
        name.c_str(), (int)all_locals.size(), (int)ast_referenced_locals.size());

    // A local is IR-only if:
    // 1. It appears in LoadLocal/StoreLocal/UninstantiateLocal (the IR access path)
    // 2. It is NOT referenced by any AST expression tree (Call/Invoke/Lvalue exprs)
    // 3. It is NOT a reference type (which can alias other variables)
    for (const void* key : all_locals) {
        const LocalVar* lv = reinterpret_cast<const LocalVar*>(key);
        // Local appears in AST expression trees — must stay AST-visible
        if (ast_referenced_locals.count(key)) {
            printd(5, "  local '%s' (%p): AST-referenced (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // Closure-captured locals use the closure variable stack (not the regular
        // local variable stack).  qore_rt_assign_local routes through the closure
        // stack when closure_use is true, so we must NOT skip it.
        if (lv->closureUse()) {
            printd(5, "  local '%s' (%p): closure-captured (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // Reference-type locals can alias other variables — must stay AST-visible
        if (QoreTypeInfo::isReference(lv->getTypeInfo())) {
            printd(5, "  local '%s' (%p): reference type (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        printd(5, "  local '%s' (%p): IR-ONLY\n", lv->getName(), key);
        ir_only_locals.insert(key);
    }

}
