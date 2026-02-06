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

static bool isTerminator(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Invoke:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::SwitchInt:
        case QoreIROpcode::SwitchString:
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Throw:
        case QoreIROpcode::Rethrow:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::ThreadExit:
            return true;
        default:
            return false;
    }
}

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
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::MapScaleInt:
        case QoreIROpcode::MapScaleFloat:
        case QoreIROpcode::MapOffsetInt:
        case QoreIROpcode::MapOffsetFloat:
        case QoreIROpcode::MapSquareInt:
        case QoreIROpcode::MapSquareFloat:
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
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
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
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::MapScaleInt:
        case QoreIROpcode::MapScaleFloat:
        case QoreIROpcode::MapOffsetInt:
        case QoreIROpcode::MapOffsetFloat:
        case QoreIROpcode::MapSquareInt:
        case QoreIROpcode::MapSquareFloat:
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
            return 0;
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
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
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
                    || inst->opcode == QoreIROpcode::SwitchRegexMatch
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
