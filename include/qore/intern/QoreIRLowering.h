/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRLowering.h

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

#ifndef _QORE_INTERN_QOREIRLOWERING_H
#define _QORE_INTERN_QOREIRLOWERING_H

#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreParseAnalysis.h>
// AOTConstantReverseMap: the reverse map the lowering resolves constant nodes through
#include "qore/intern/QoreAOTBinary.h"

// Forward declaration for obe_type_e enum
enum obe_type_e;

class QoreParseContext;
class QoreParseListNode;
class QoreListNode;
class QoreValue;
class AbstractQoreNode;
class VarRefNode;
class AbstractStatement;
class StatementBlock;
class QoreTypeInfo;
class LVList;
class QoreMapOperatorNode;
class QoreSelectOperatorNode;
class QoreFoldlOperatorNode;
class QoreFoldrOperatorNode;
class QoreMapSelectOperatorNode;
class QoreHashMapOperatorNode;
class QoreHashMapSelectOperatorNode;
class QoreStreamingOperatorNode;
class QoreSquareBracketsOperatorNode;
class QoreIROnBlockExitInstruction;
class ForEachStatement;
class QoreBackgroundOperatorNode;

class QoreIRLowering;

//! The stack that IR lowering keeps free above the thread's stack limit
/** Lowering recurses with the nesting of the code, and several of its frames are large (much larger again with
    AddressSanitizer), so each check leaves room for a few levels of nesting; code nested more deeply than the thread's
    stack allows for lowering runs on the AST tier, which checks the stack itself and raises \c STACK-LIMIT-EXCEEDED if
    it runs out.
*/
constexpr size_t QORE_IR_LOWERING_STACK_RESERVE = 128 * 1024;

//! The lowering error when the reserve is reached
constexpr const char* QORE_IR_LOWERING_STACK_ERROR = "the code is nested too deeply to be lowered within the stack of "
    "this thread";

//! Returns true if the thread's stack has reached QORE_IR_LOWERING_STACK_RESERVE in a recursive walk of the code
/** For the analyses and walks of the AST or of the IR that lowering and IR optimization make: they recurse with the
    nesting of the code and stop when this returns true.  An analysis then returns its conservative answer; the
    lowering in progress in the thread, if any, fails with QORE_IR_LOWERING_STACK_ERROR (see
    QoreIRLowering::stackLimitReached()) so that a walk that had to be complete cannot leave its result incomplete.
*/
DLLLOCAL bool qore_ir_walk_stack_reserve_reached();

//! Per-callback state passed to plugin IR lowering hooks.
/** Plugin lowering callbacks use this object to publish their lowered IR result
 *  and, on failure, the diagnostic that should be propagated to the caller.
 */
class QoreIRLoweringContext {
public:
    //! Sets the diagnostic for a failed plugin lowering callback.
    DLLLOCAL void setError(const std::string& message);
    //! Sets the diagnostic for a failed plugin lowering callback.
    DLLLOCAL void setError(const char* message);
    //! Returns the current plugin lowering diagnostic.
    DLLLOCAL const std::string& getError() const;
    //! Sets the IR value produced by a successful plugin lowering callback.
    DLLLOCAL void setResult(QoreIRValue value);
    //! Returns the IR value produced by a successful plugin lowering callback.
    DLLLOCAL QoreIRValue getResult() const;
    //! Returns the active lowering pass for advanced callbacks that need it.
    DLLLOCAL QoreIRLowering& getLowering() const;

private:
    friend class QoreIRLowering;

    DLLLOCAL QoreIRLoweringContext(QoreIRLowering& lowering, std::string& error)
            : lowering(lowering), error(error) {
    }

    QoreIRLowering& lowering;
    std::string& error;
    QoreIRValue result;
};

class QoreIRLowering {
public:
    //! Handler metadata for inline compilation at block exit points
    /** Stores on_exit/on_success/on_error handler StatementBlocks along with their type
     *  and location information. These are lowered inline at block exit points rather than
     *  compiled as separate QoreIRFunction objects.
     */
    struct InlineHandler {
        obe_type_e type;                                    //!< handler type (OBE_Unconditional/Success/Error)
        StatementBlock* code;                               //!< handler code block
        const QoreProgramLocation* loc;                     //!< source location for error reporting
        QoreIROnBlockExitInstruction* obe_inst = nullptr;   //!< instruction to attach compiled handler IR
    };

    explicit QoreIRLowering(QoreIRBuilder& builder, QoreParseContext* parse_context = nullptr,
        const AOTConstantReverseMap* constant_reverse_map = nullptr);

    QoreIRValue lowerExpression(const QoreValue& expr, std::string& error);
    bool lowerStatement(const AbstractStatement* stmt, std::string& error);
    bool lowerStatementBlock(const StatementBlock* block, std::string& error);
    void setParseContext(QoreParseContext* parse_context);

    //! Closes the innermost temp scope with a DiscardTemps instruction that has the active exception handler
    /** A temporary's destructor can raise an exception when the scope's temporaries are released at the end of the
        statement; the statement raises it, as in the AST interpreter, so it must branch to an enclosing try block's
        handler in the same function instead of leaving the frame.
    */
    QoreIRInstruction* createDiscardTemps(const QoreProgramLocation* loc);

    //! Compile all handler bodies to separate QoreIRFunction objects and attach to OnBlockExit instructions
    //! @param handlers handlers to compile for this block level
    //! @param parent_func parent function for slot inheritance
    //! @param error receives error message for any compilation failures (non-fatal)
    //! @return number of handlers successfully compiled, -1 on error
    int compileBlockHandlerIRs(const std::vector<InlineHandler>& handlers,
        QoreIRFunction* parent_func, std::string& error);

    //! @param error receives error message for any compilation failures (non-fatal)
    //! @return number of handlers successfully compiled
    int compileAllHandlerIRs(std::string& error);

public:
    //! Returns true if lowering stopped because the thread's stack is too small for the nesting of the code
    /** The code itself can be lowered; the function runs on the AST tier, which checks the stack as it executes.
        See QORE_IR_LOWERING_STACK_RESERVE.
    */
    bool stackLimitReached() const {
        return stack_limit_reached;
    }

    // Expression handler methods - public for use by QoreIRExprRegistry
    // These are implementation details not meant for external use
    QoreIRValue lowerConstant(const QoreValue& expr, std::string& error);
    QoreIRValue lowerVarRef(const QoreValue& expr, std::string& error);
    QoreIRValue lowerAssignment(const QoreValue& expr, std::string& error);
    //! Returns the result of +=, -=, *= or /= on a variable that is changed by loading it, computing its new value
    //! and storing it
    /** The result is the value the variable holds after the store, as in the AST interpreter, whose +=, -=, *= and
        /= operators return the lvalue's value: the variable's declared type can convert the computed value when it is
        stored (ex: an \c any or \c auto! variable stores a \c hash<HD> as a plain \c hash, a \c softint variable
        stores 17.5 as 17), so the variable is read again when the result is used.  The integer operators (%=, &=,
        |=, ^=, <<=, >>=) apply only to lvalues that accept and return an integer, which store the integer computed
        as it is, so their lowerings return the value they compute.

        @param needs_rv true if the result of the operator is used
        @param var the variable
        @param left the operator's lvalue expression
        @param result the computed value that was stored
        @param error for an error message
        @param context the context of the load for error messages

        @return the result of the operator
    */
    QoreIRValue compoundAssignmentResult(bool needs_rv, const VarRefNode* var, const QoreValue& left,
        QoreIRValue result, std::string& error, const char* context);
    QoreIRValue lowerPlusEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMinusEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMultiplyEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDivideEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerModuloEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerAndEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerXorEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPreIncrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPostIncrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPreDecrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPostDecrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPlus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMinus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMultiplication(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDivision(const QoreValue& expr, std::string& error);
    QoreIRValue lowerModulo(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryAnd(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryOr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryXor(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryNot(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftLeft(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftRight(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftLeftEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftRightEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalNotEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAbsoluteEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAbsoluteNotEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalLessThan(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalLessThanOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalGreaterThan(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalGreaterThanOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalComparison(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnaryPlus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnaryMinus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRange(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSquareBracketsRange(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSquareBrackets(const QoreValue& expr, std::string& error);
    //! Lowers a list of selectors with ranges that can have side effects as ListSelectorStep instructions
    /** Each selector is evaluated only after the previous one has been applied, and none is evaluated for a
        sequence that no selector applies to, as in the AST interpreter (qore#5665)
    */
    QoreIRValue lowerListSelectorSteps(const QoreSquareBracketsOperatorNode* op, QoreIRValue seq,
        const std::vector<QoreValue>& selectors, std::string& error);
    //! Returns true if the sequence of a list of selectors is always a list, a string or a binary object
    bool listSelectorSeqAlwaysApplies(const QoreValue& expr) const;
    //! Returns true if a bound of a range operator can evaluate to NOTHING
    bool rangeBoundMayBeNothing(const QoreValue& expr) const;
    //! A list of selectors with ranges (seq[a..b, c]) that a foreach statement or a functional or streaming
    //! operator iterates lazily, as the AST interpreter does (see QoreIROpcode::IteratorCreateSelectors)
    struct LazySelectorSource {
        const QoreSquareBracketsOperatorNode* op = nullptr;
        std::vector<QoreValue> selectors;

        explicit operator bool() const {
            return op != nullptr;
        }
    };
    //! Returns true and sets \a src if \a expr is a list of selectors with ranges whose lazy iteration can be
    //! observed: a selector with side effects, or a range with negative offsets, which the AST interpreter iterates
    //! index by index instead of as the range of the sequence that it selects
    bool getLazySelectorSource(const QoreValue& expr, LazySelectorSource& src) const;
    //! Returns true if \a expr is a list of selectors with ranges that is iterated lazily (see getLazySelectorSource())
    bool hasLazySelectorSource(const QoreValue& expr) const {
        LazySelectorSource src;
        return getLazySelectorSource(expr, src);
    }
    //! Lowers the source of an iterator: the sequence and IteratorCreateSelectors for a lazy list of selectors, or
    //! the expression and IteratorCreate (or IteratorCreateIterate if \a iterate) otherwise
    /** @param source set to the value iterated: the sequence for a lazy list of selectors
        @param in_temp_scope true to lower the expression in a temp scope of its own

        @return the iterator handle, or an invalid value on error
    */
    QoreIRValue lowerSourceIterator(const QoreValue& expr, const LazySelectorSource& src, bool iterate,
        bool in_temp_scope, const QoreProgramLocation* loc, QoreIRValue& source, std::string& error);
    //! Emits the iteration of a lazy list of selectors in place of IteratorNext
    /** Emits IteratorNextSelectors in the current block, which gives the next value to \a body_block; when the iterator
        has no value, a block asks it which selector it needs (SelectorIteratorPending), evaluates it in a temp scope of
        its own and gives it to the iterator (SelectorIteratorFeed), and returns to IteratorNextSelectors, or releases
        the iterator and branches to \a exit_block when every value has been given.  An exception raised by a selector
        or the iterator goes to \a exception_target, which must release the iterator; if it is null, to a block that
        releases the iterator and branches to the handler in this frame, if any.

        @return the IteratorNextSelectors instruction, whose result is the value, or nullptr on error
    */
    QoreIRIteratorNextInstruction* emitSelectorIteratorNext(const LazySelectorSource& src, QoreIRValue iter,
        QoreIRBasicBlock* exit_block, QoreIRBasicBlock* body_block, QoreIRBasicBlock* exception_target,
        std::string& error);
    //! Returns true if the lazy list of selectors \a src is iterated with the selector-segment loop in native code
    /** The sequence must be a list whose values hold no reference, whose release cannot be observed, so that native
        code can read each value from the list (qore#5711)
    */
    bool useSelectorSegmentLoop(const LazySelectorSource& src) const;
    //! Returns true if evaluating the selector \a selector (or the bounds of a range) can create a node temp
    static bool selectorMayCreateTemp(const QoreValue& selector);
    //! Returns the QoreIRListSelectorFlag flags of the iterator of a lazy list of selectors
    static int64_t selectorIteratorFlags(const LazySelectorSource& src);
    //! Fills the handler of a selector of a remove or delete expression with a list of selectors that raised an
    //! exception: it removes what the \a selector_count selectors before it select, releases the removed values,
    //! and branches with the exceptions to the handler of the expression, as the AST interpreter does
    //! (LValueRemoveHelper::doRemove()); see tryEmitLValuePathOp()
    void emitPartialSliceRemove(QoreIRBasicBlock* partial_block, const std::vector<LVPathStep>& path,
        const std::vector<QoreIRValue>& dyn_vals, size_t selector_count, AssignmentMode mode,
        const QoreProgramLocation* loc);
    QoreIRValue lowerHashObjectDereference(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPop(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShift(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPush(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnshift(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSplice(const QoreValue& expr, std::string& error);
    QoreIRValue lowerExtract(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRemove(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDelete(const QoreValue& expr, std::string& error);
    QoreIRValue lowerKeys(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexMatch(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexNMatch(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexExtract(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexSubst(const QoreValue& expr, std::string& error);
    QoreIRValue lowerInstanceOf(const QoreValue& expr, std::string& error);
    QoreIRValue lowerTrim(const QoreValue& expr, std::string& error);
    QoreIRValue lowerChomp(const QoreValue& expr, std::string& error);
    QoreIRValue lowerTransliteration(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBackground(const QoreValue& expr, std::string& error);
    QoreIRValue lowerListAssignment(const QoreValue& expr, std::string& error);
    QoreIRValue lowerContainerLiteral(const QoreValue& expr, std::string& error);
    QoreIRValue lowerContainerElement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerParseHash(const QoreValue& expr, std::string& error);
    QoreIRValue lowerParseList(const QoreValue& expr, std::string& error);
    QoreIRValue lowerExists(const QoreValue& expr, std::string& error);
    QoreIRValue lowerElements(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDotEval(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldlImpl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldrImpl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMapImpl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSelectImpl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMapSelectImpl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashMapImpl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashMapSelectImpl(const QoreValue& expr, std::string& error);
    //! Lowers a functional operator and releases its source, if it is a temporary, once the operator has run
    template <typename F>
    QoreIRValue lowerWithSourceRelease(const QoreValue& expr, const QoreValue& source,
            const QoreProgramLocation* loc, bool always_ref,
            std::string& error, F&& lower);
    //! Sets the source of the functional operator being lowered to the base source of its fused pipeline
    void setFunctionalBaseSource(const QoreValue& base_source);
    //! The block that an exception leaving the loop of a functional operator goes to (see the definition)
    class FunctionalLoopExceptionCleanup;
    //! The exception target of code lowered with the virtual implicit context, whose runtime implicit argument and
    //! element the lowering sets only if the code turns out to need them (see the definition)
    class ImplicitContextExceptionBlock;
    //! Lowers code that runs with an element as the implicit argument and its index as the implicit element, which
    //! are restored on every path, also when the code raises an exception (see the definition)
    template <typename F>
    QoreIRValue lowerWithImplicitContext(QoreIRValue element, QoreIRValue index, const QoreProgramLocation* loc,
            QoreIRBasicBlock* loop_cleanup, std::string& error, F&& lower);
    QoreIRValue lowerMap(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSelect(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMapSelect(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashMap(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashMapSelect(const QoreValue& expr, std::string& error);
    QoreIRValue lowerIterate(const QoreValue& expr, std::string& error);
    QoreIRValue lowerStreaming(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAnd(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalOr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalNot(const QoreValue& expr, std::string& error);
    QoreIRValue lowerNullCoalescing(const QoreValue& expr, std::string& error);
    QoreIRValue lowerValueCoalescing(const QoreValue& expr, std::string& error);
    QoreIRValue lowerQuestionMark(const QoreValue& expr, std::string& error);
    //! Lowers a typed hash or list construction whose initializer is lowered as the operand \a init
    /** @param type_only a NewComplexHashNode or NewComplexListNode of the type without an initializer */
    QoreIRValue lowerNewComplexFromOperand(QoreIROpcode opcode, const QoreValue& type_only, QoreIRValue init,
            const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerCast(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBuiltinTypeConversion(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFunctionCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerCallReference(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSelfCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerStaticCall(const QoreValue& expr, std::string& error);

private:
    //! set when lowering stopped at QORE_IR_LOWERING_STACK_RESERVE; see stackLimitReached()
    bool stack_limit_reached = false;

    //! the number of walks that stopped at the stack reserve in this thread when lowering started
    /** see qore_ir_walk_stack_reserve_reached()
    */
    unsigned walk_stack_reserve_hits;

    //! Returns true and sets the lowering error if the thread's stack has reached the lowering reserve
    bool stackReserveReached(std::string& error);

    //! Native IR lowering for map operator with implicit argument context
    QoreIRValue lowerMapNative(const QoreMapOperatorNode* map, const QoreValue& expr, std::string& error);
    //! Native IR lowering for select operator with implicit argument context
    QoreIRValue lowerSelectNative(const QoreSelectOperatorNode* select, const QoreValue& expr, std::string& error);
    //! Native IR lowering for foldl operator with implicit argument context
    QoreIRValue lowerFoldlNative(const QoreFoldlOperatorNode* foldl, const QoreValue& expr, std::string& error);
    //! Native IR lowering for foldr operator (reverse iteration)
    QoreIRValue lowerFoldrNative(const QoreFoldrOperatorNode* foldr, const QoreValue& expr, std::string& error);
    //! Native reverse-fold lowering over an already-lowered iterable value.
    QoreIRValue lowerFoldrNativeValue(const QoreFoldrOperatorNode* foldr, QoreIRValue input_list,
        const QoreTypeInfo* element_type, std::string& error);
    //! Insert runtime $1/$2 setup before an AST-delegated fold body.
    QoreIRValue insertFoldImplicitArgvSetup(QoreIRBasicBlock* body, size_t position,
        QoreIRValue accumulator, QoreIRValue element, const QoreProgramLocation* loc);
    //! Native IR lowering for map+select operator
    QoreIRValue lowerMapSelectNative(const QoreMapSelectOperatorNode* ms, const QoreValue& expr, std::string& error);
    //! Native IR lowering for hash map operator
    QoreIRValue lowerHashMapNative(const QoreHashMapOperatorNode* hm, const QoreValue& expr, std::string& error);
    //! Native IR lowering for hash map+select operator
    QoreIRValue lowerHashMapSelectNative(const QoreHashMapSelectOperatorNode* hms, const QoreValue& expr, std::string& error);
    //! Native IR lowering for streaming operators
    QoreIRValue lowerStreamingNative(const QoreStreamingOperatorNode* op, const QoreValue& expr, std::string& error);
    //! Stage descriptor for fused lazy streaming/functional pipelines.
    struct LazyPipelineStage {
        enum Kind {
            StreamTake,
            StreamDrop,
            StreamTakeWhile,
            StreamTakeUntil,
            Select,
            Map,
            MapSelect,
        };

        Kind kind;
        const QoreValue* primary = nullptr;    //!< limit/predicate/map expression
        const QoreValue* secondary = nullptr;  //!< map-select predicate
        const QoreProgramLocation* loc = nullptr;
        //! Map and MapSelect: the element type of the list that the AST interpreter creates for the operator
        const QoreTypeInfo* ast_element_type = nullptr;
    };

    //! Root consumer descriptor for fused lazy pipelines.
    struct LazyPipelineRoot {
        enum Kind {
            Streaming,
            List,
            Foldl,
            MethodJoin,
        };

        Kind kind;
        const QoreStreamingOperatorNode* streaming = nullptr;
        const QoreValue* fold_expr = nullptr;
        const QoreValue* join_expr = nullptr;
        QoreIRValue join_separator;
        const QoreTypeInfo* list_element_type = nullptr;
        const QoreProgramLocation* loc = nullptr;
        bool need_result = true;
        //! List: the last stage is the map, map/select or select operator whose result list is built, which gets
        //! the type that the AST interpreter gives it (see QoreIROpcode::ListSetCommonType)
        bool set_common_type = false;
    };

    //! Collects a supported lazy source chain into ordered stages and an innermost source expression.
    bool collectLazyPipelineStages(const QoreValue& source, QoreValue& base_source,
        std::vector<LazyPipelineStage>& source_stages, std::string& error);
    //! Decides if the list of selectors \a source, which a consumer iterates, is iterated lazily, and records it
    /** It is iterated lazily unless its selectors are stable and the consumer's code - the expressions of \a stages,
        \a consumer_exprs, the lvalue \a written_lvalue and the statements of \a block - does not write the locals that
        they read (qore#5675).  getLazySelectorSource() then gives the decision.

        @return true if the list is iterated lazily
    */
    /** With \a pure_consumer_ok (a map, select, map/select or foldl operator without other stages), a list whose
        selectors have side effects is also evaluated before the iteration when the operator's expressions only
        compute values from numeric elements (qore_ir_consumer_expr_is_pure()), as neither side can observe the other.
        Any other consumer iterates a list of stable selectors over a list whose values hold no reference lazily as
        well (useSelectorSegmentLoop()) when it has at least as many ranges as single indices, which gives the same
        values without building the list of them; a list with more single indices is built first, as a lazy single
        index costs a step of the iteration protocol where the list costs one element (qore#5711).
    */
    bool decideLazySelectorSource(const QoreValue& source, const std::vector<LazyPipelineStage>& stages,
        const std::vector<const QoreValue*>& consumer_exprs, const QoreValue* written_lvalue = nullptr,
        const StatementBlock* block = nullptr, bool pure_consumer_ok = false);
    //! The decisions of decideLazySelectorSource(), by list of selectors
    std::unordered_map<const AbstractQoreNode*, bool> lazy_selector_decisions;

    //! Lowers a pipeline stage with virtual $1/$# values and materializes the runtime context only for AST delegation.
    QoreIRValue lowerLazyPipelineStage(const QoreValue& stage_expr, const QoreProgramLocation* stage_loc,
        QoreIRValue element, QoreIRValue index, std::string& error);

    //! Shared native IR lowering for fused lazy streaming/functional pipelines.
    QoreIRValue lowerLazyPipelineFused(const QoreValue& base_source,
        const std::vector<LazyPipelineStage>& source_stages, const LazyPipelineRoot& root, std::string& error);

    //! Native IR lowering for non-reference foreach over a lazy functional/streaming pipeline.
    bool lowerForeachLazyPipelineFused(const ForEachStatement* foreach_stmt, const QoreValue& base_source,
        const std::vector<LazyPipelineStage>& source_stages, std::string& error);

    //! Fused native IR lowering for nested streaming and functional-operator chains
    QoreIRValue lowerStreamingFused(const QoreStreamingOperatorNode* op, const QoreValue& base_source,
        const std::vector<LazyPipelineStage>& source_stages, std::string& error);
    QoreIRValue lowerListNode(const QoreValue& expr, std::string& error);
    bool guardVarLValue(const QoreValue& exp, std::string& error, bool allow_maybe_nothing = false);
    bool guardLValueBase(const QoreValue& exp, std::string& error, bool allow_maybe_nothing = false);
    QoreIRValue tryEmitLValuePathOp(QoreIROpcode opcode, const QoreValue& lvalue,
        const QoreIRValue* rhs, const QoreProgramLocation* loc, std::string& error,
        AssignmentMode mode, LVCompoundOp compound_op = LVCompoundOp::AddAssign,
        LVUnaryOp unary_op = LVUnaryOp::PreInc,
        LVBinaryMutOp binary_mut_op = LVBinaryMutOp::Push,
        const QoreValue& pattern_expr = QoreValue());
    bool getAnalysis(const QoreValue& expr, QoreParseAnalysis& analysis) const;
    bool isNeverNothingInt(const QoreParseAnalysis& analysis) const;
    bool isNeverNothingFloat(const QoreParseAnalysis& analysis) const;
    bool analysisIndicatesInt(const QoreParseAnalysis& analysis) const;
    bool analysisIndicatesFloat(const QoreParseAnalysis& analysis) const;
    bool analysisIndicatesDate(const QoreParseAnalysis& analysis) const;
    bool guaranteedIntType(const QoreValue* expr) const;
    bool guaranteedFloatType(const QoreValue* expr) const;
    bool guaranteedNumberType(const QoreValue* expr) const;
    bool guaranteedStringType(const QoreValue* expr) const;
    bool guaranteedDateType(const QoreValue* expr) const;
    const QoreTypeInfo* selectAnalysisType(const QoreParseAnalysis& analysis) const;
    QoreIROpcode selectNumericOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op);
    QoreIROpcode selectNumericOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op,
        QoreIROpcode number_op);
    QoreIROpcode selectFoldOpcode(const QoreParseAnalysis& analysis,
        QoreIROpcode any_op, QoreIROpcode int_op, QoreIROpcode float_op) const;
    bool ensureBuilderContext(std::string& error) const;
    QoreIRValue tryPluginLowering(const QoreValue& expr, std::string& error);
    QoreIRBasicBlock* createBlock(const std::string& prefix);
    QoreIRValue loadVarRef(const VarRefNode* var, std::string& error, const char* context,
        const QoreValue& expr);
    //! Gives the load of a variable that can hold a reference the exception target in effect, if any
    /** A variable whose type allows a reference (no type or a reference type) can hold one, and its read reads the
        referenced lvalue, which raises an exception if it no longer exists (OBJECT-ALREADY-DELETED for a member of a
        deleted object, PROGRAM-ERROR for a variable of a deleted Program): the exception is handled by the enclosing
        try or scope cleanup, as in the AST interpreter.  The read of any other variable cannot raise, and its load
        gets no exception edge, which would constrain the optimization of the code around it.

        @param load a LoadLocal, LoadClosure, LoadGlobal or LoadThreadLocal instruction, or an AddAssignLocalInt
        instruction, which reads its source as a load does
        @param type_info the declared type of the variable read
    */
    void setReferenceLoadExceptionTarget(QoreIRInstruction* load, const QoreTypeInfo* type_info);
    bool storeVarRef(const VarRefNode* var, QoreIRValue value, std::string& error, const char* context,
        const QoreValue* expr = nullptr, const QoreProgramLocation* guard_loc = nullptr, AssignmentMode mode = AssignmentMode::Normal,
        QoreIRValue* store_result = nullptr);
    LocalVar* getLocalVarFromValue(const QoreValue& expr) const;
    const QoreTypeInfo* getGuaranteedTypeForValue(const QoreValue* expr, const QoreTypeInfo* fallback) const;
    bool lowerCallArgs(const QoreParseListNode* parse_args, const QoreListNode* args,
        std::vector<QoreIRValue>& lowered, std::string& error);
    bool callArgumentMayPassReference(const QoreValue& arg) const;
    //! Returns true if the value of an assignment to a variable can be a reference that the variable now holds
    /** The value of <tt>t = v</tt> and <tt>t += v</tt> in the AST interpreter is the value the lvalue holds
        afterwards, taken as it is (LValueHelper::getReferencedValue()): if \a v is a reference that a variable of
        no type now holds (an element <tt>h.r</tt> holding <tt>\x</tt>, the value of a call), the value is the
        reference, so that <tt>auto r = (t = h.r); r = 5;</tt> assigns \c x.  Reading the variable again gives the
        referenced value instead, so such an assignment whose value is used is lowered as an lvalue path operation,
        which gives the value as the AST interpreter does.
        @param var the variable assigned
        @param right the value assigned or added
    */
    bool assignmentValueMayBeReference(const VarRefNode* var, const QoreValue& right) const;
    bool callArgsMayPassReferences(const QoreParseListNode* parse_args, const QoreListNode* args) const;
    bool markReferenceArgumentAssignmentsUnknown(const QoreParseListNode* parse_args,
        const QoreListNode* args, std::string& error);
    bool callArgumentMayBeRuntimeNothing(const QoreValue& arg) const;
    bool directCallVariantMayRejectRuntimeNothing(const AbstractQoreFunctionVariant* variant,
        const QoreParseListNode* parse_args, const QoreListNode* args) const;
    bool overloadedDirectCallNeedsRuntimeDispatch(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, const QoreParseListNode* parse_args,
        const QoreListNode* args) const;
    QoreIRValue findLoopInvariantListSize(LocalVar* local) const;
    QoreIRValue findLoopInvariantValue(const QoreValue& expr) const;
    QoreIRValue lowerExprOpOrInvoke(QoreIROpcode op, const QoreValue& expr, const std::vector<QoreIRValue>& operands,
        const QoreProgramLocation* loc, std::string& error, bool has_ref_args = false);
    QoreIRValue lowerExprOpOrInvokeNoGuard(QoreIROpcode op, const QoreValue& expr,
        const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerBinaryOpOrInvoke(QoreIROpcode op, const QoreValue& expr, QoreIRValue left, QoreIRValue right,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerUnaryOpOrInvoke(QoreIROpcode op, const QoreValue& expr, QoreIRValue value,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitHashKeyCompoundOp(const VarRefNode* container_var, const std::string& key_name,
        const QoreValue& key_expr, QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitHashKeyDynamicCompoundOp(const VarRefNode* container_var, const QoreValue& key_expr,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitListKeyCompoundOp(const VarRefNode* container_var, const QoreValue& index_expr,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitHashKeyDirectStore(const VarRefNode* container_var, const std::string& key_name,
        const QoreValue& key_expr, const QoreIRValue& value,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitHashKeyDynamicStore(const VarRefNode* container_var, const QoreValue& key_expr,
        const QoreIRValue& value, const QoreValue& full_expr,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitListIndexDirectStore(const VarRefNode* container_var, const QoreValue& index_expr,
        const QoreIRValue& value, const QoreValue& full_expr,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerConditionValue(const QoreValue& cond, std::string& error);
    bool tryEmitFusedBranchIfLtLocalInt(const QoreValue& cond,
        QoreIRBasicBlock* true_target, QoreIRBasicBlock* false_target);
    QoreIROpcode selectComparisonOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op);
    bool expressionCanThrow(const QoreValue& expr) const;
    void markLocalAssignmentFromExpression(const QoreValue& exp);
    void markLocalUnassignmentFromExpression(const QoreValue& exp);

    //! Lowers the value operand of an assignment, compound assignment, push, or unshift to \a lvalue
    /** When \a lvalue is a container path, or, unless \a assignment, a variable whose value can be a container,
        and the operand's evaluation creates temps besides its value, the operand is lowered in a temp scope of its
        own that is closed with DiscardTempsKeep, so that those temps (for example a load of the container being
        changed) are released before the mutation evaluates copy-on-write, as the AST releases them when the
        operand's evaluation returns; see design/lvalue-loads-in-ir.md.
    */
    QoreIRValue lowerMutationOperand(const QoreValue& lvalue, const QoreValue& expr,
        const QoreProgramLocation* loc, std::string& error, bool assignment = false);
    //! Lowers an expression whose value is used after other code has run, releasing its other temps
    /** When the expression's evaluation creates temps besides its value, it is lowered in a temp scope of its own
        that is closed with DiscardTempsKeep, so that only the value is kept.  Used for the operands of a mutation
        (see lowerMutationOperand(); also the keys and indexes of a mutated path), and for the list of a foreach
        statement and the value of a switch statement, whose temps would otherwise be held while the body changes
        the containers they refer to; see design/lvalue-loads-in-ir.md.  With \a containers_only, as for those
        statements, the scope is used only if a temp besides the value can be or hold a container, so that the
        value of a string or scalar expression stays directly available to the statement's optimizations.
    */
    QoreIRValue lowerExpressionInTempScope(const QoreValue& expr, const QoreProgramLocation* loc,
        std::string& error, bool containers_only = false);
    QoreIRBasicBlock* getCurrentExceptionTarget() const;
    QoreIRBasicBlock* getGuardExceptionTarget() const;
    //! Gives an instruction that can raise an exception the active handler
    /** Without it, an exception raised by the instruction returns from the frame instead of branching to an
        enclosing try/catch in the same function.  Iterator instructions call iterator code (a user
        iterator's next() and getValue(), or a conversion error), which can raise exceptions.
    */
    template <typename T>
    T* withExceptionTarget(T* inst) const {
        inst->exception_target = getCurrentExceptionTarget();
        return inst;
    }
    void setLoopCheckpointExceptionTarget(QoreIRInstruction* inst, QoreIRBasicBlock* target,
        QoreIRBasicBlock* handler = nullptr);
    bool needsNotNothingGuard(const QoreValue& expr) const;
    bool needsNotNothingGuard(const QoreValue* expr, const QoreTypeInfo* target_type,
        bool allow_maybe_nothing = false) const;
    void maybeInsertNotNothingGuard(QoreIRValue value, const QoreValue& expr);
    void maybeInsertNotNothingGuard(QoreIRValue value, const QoreValue* expr,
        const QoreProgramLocation* loc, const QoreTypeInfo* target_type,
        bool allow_maybe_nothing = false);
    const QoreProgramLocation* getExpressionLocation(const QoreValue& expr) const;
    const QoreTypeInfo* getVarRefTypeInfo(const VarRefNode* var) const;
    const QoreProgramLocation* getVarRefLocation(const VarRefNode* var) const;

    //! Flags for emitBlockCleanups controlling cleanup behavior
    enum CleanupFlags : unsigned {
        CF_NONE = 0,
        CF_SKIP_LVARS = 1,       //!< skip local var cleanup (for return - pre-instantiation handles it)
        CF_FILL_REMAINING = 2,   //!< fill remaining elements in ref foreach (for break and return)
    };

    //! Emit block cleanup instructions (handler lowering + ScopeExit + UninstantiateLocal + RefForeach)
    //! from innermost to target depth
    /** Used by return/break/continue to properly unwind on_exit handlers, block-scoped
     *  local variables, and reference foreach state in the correct interleaved order
     *  matching AST mode. Inlines handler code when handlers have been lowered.
     *  @param target_depth the cleanup_stack depth to unwind to
     *  @param error output string for error messages
     *  @param is_error true if exiting due to an exception
     *  @param flags combination of CleanupFlags
     *  @param unwind_temp_depth for a break or continue, the number of temp scopes open where its target was
     *  pushed: the temp scopes above it are released, each before the cleanup of the block that encloses it (see
     *  design/ir-loop-jump-temp-scope.md); SIZE_MAX releases none
     *  @param unwind_loc the location of the break or continue
     *  @return false if lowering failed
     */
    bool emitBlockCleanups(size_t target_depth, std::string& error, bool is_error = false, unsigned flags = CF_NONE,
        size_t unwind_temp_depth = SIZE_MAX, const QoreProgramLocation* unwind_loc = nullptr);

    //! Returns true if what a return statement leaves on its way out of the code depends on an error it raises
    /** A try block catches the error, a catch block and a context clean up, the handlers of a block run by how it
        exits, a reference foreach writes its list back, and the destructor of an object can run when a local variable
        whose value can be or hold one leaves its block: the value of a return statement that leaves any of these
        must be checked before it leaves them, as the AST interpreter checks it in the statement (CheckReturnValue).
        The other local variables leave their blocks without running code, as does the variable whose value is
        returned, which stays referenced, so a check when the code returns is the same.

        @param returned_local the local variable whose value the statement returns, if it returns one as it is
    */
    DLLLOCAL bool returnCleanupObservesError(const LocalVar* returned_local) const;

    //! Determine if a handler should execute for a given exit type
    /** Handlers are filtered by type:
     *  - OBE_Unconditional: always executes
     *  - OBE_Success: executes on normal (non-error) exits
     *  - OBE_Error: executes only on error (exception) exits
     */
    bool shouldRunHandler(const InlineHandler& handler, bool is_error);

    struct BlockCleanupEntry;

    //! Lower all applicable handlers for the current block at an exit point
    /** Lowers handler code inline into the IR in LIFO order.
     *  @param is_error true if this is an error/exception exit
     *  @param start_index start index in block_handlers (for multi-block context)
     *  @param end_index end index in block_handlers (SIZE_MAX = through the end)
     *  @param barrier_depth cleanup_stack depth to set as a hard floor while a
     *    handler body is being inlined — any `emitBlockCleanups` invoked from a
     *    non-local exit (return/break/continue) inside the handler body skips
     *    the firing Scope entry (which still owns this handler range) so it
     *    can't re-enter `lowerHandlersAtExit` on the same handlers. Cleanup
     *    entries below that Scope, such as block locals, still run. SIZE_MAX =
     *    no barrier. Implemented via a
     *    HandlerBarrier entry pushed on cleanup_stack for the duration of the
     *    handler body lowering.
     *  @param scope the Scope entry of the statement block that owns the
     *    handlers, if any; the scope is left at runtime once the handlers have
     *    been lowered (an inline ScopeExit).  Before each handler, a trim drops
     *    it and the handlers registered after it from the runtime scope, and the
     *    handler is lowered with the scope's handler_exception_target, so the
     *    handlers that have not run yet still run, with the exception, if it
     *    raises one, as in the AST interpreter (StatementBlock::execIntern()).
     *  @return false if lowering failed
     */
    bool lowerHandlersAtExit(bool is_error, std::string& error, size_t start_index = 0,
            size_t end_index = SIZE_MAX, size_t barrier_depth = SIZE_MAX, const BlockCleanupEntry* scope = nullptr);

    //! Lowers one handler inline for lowerHandlersAtExit()
    /** @param handler the handler
        @param keep_handlers the number of handlers registered in the scope before this one, which stay registered
        @param error receives the error message if lowering fails
        @param barrier_depth see lowerHandlersAtExit()
        @param scope_id the ID of the runtime scope that owns the handler, or 0 if none
        @param scope_loc the location of the scope
        @param set_exception_target true if the handler is lowered with @p handler_exception_target as its exception
        target
        @param handler_exception_target the exception target for the handler, or nullptr for none (the exception
        leaves the frame)

        @return false if lowering failed
    */
    bool lowerHandlerAtExit(const InlineHandler& handler, size_t keep_handlers, std::string& error,
            size_t barrier_depth, uint32_t scope_id, const QoreProgramLocation* scope_loc, bool set_exception_target,
            QoreIRBasicBlock* handler_exception_target);

    //! Returns true when a concrete constant container can be loaded once instead of rebuilt per read
    bool constantContainerLoadsByReference(const QoreValue& expr) const;

    //! Releases the temporaries of the elements of a list or hash literal once the container holds them
    template <typename F>
    void releaseContainerElementTemps(const std::vector<QoreIRValue>& values, F&& element_expr,
            const QoreProgramLocation* loc);

    //! Lowers an expression (see lowerExpression(), which records the value of a functional operator's source)
    QoreIRValue lowerExpressionImpl(const QoreValue& expr, std::string& error);

    //! The source of a functional operator being lowered and the value it gave, for its release after the operator
    struct FunctionalSourceCapture {
        const AbstractQoreNode* node = nullptr;
        QoreIRValue value;
        //! true if the operator's value can be the source's (a source that is not known to be a list)
        bool may_alias = false;
    };
    //! The sources of the functional operators being lowered, innermost last
    std::vector<FunctionalSourceCapture> functional_source_captures;
    //! Releases the temporary that a functional operator's source gave once the operator has run (see the definition)
    class FunctionalSourceRelease;

    QoreIRBuilder& builder;
    QoreParseContext* parse_context = nullptr;
    const AOTConstantReverseMap* constant_reverse_map = nullptr;
    //! Values known to never be NOTHING (produced by typed opcodes)
    std::unordered_set<uint32_t> never_nothing_values;
    uint32_t block_counter = 0;
    uint32_t scope_counter = 0;  //!< Counter for generating unique scope IDs within a function
    int statement_block_depth = 0;  //!< Nesting depth for debug block markers
    int loop_depth = 0;  //!< Nesting depth of loop bodies (for is_block_exit detection)
    QoreIRBasicBlock* guard_exception_target_override = nullptr;

    //! Exit block of the innermost while/for/do-while loop currently being lowered
    //! (nullptr at top level).  Blocks created via createBlock() while this is set are
    //! stamped with QoreIRBasicBlock::enclosing_loop_exit so the IR interpreter can
    //! resolve a debugger DebugFlowBreak to the correct enclosing loop's exit.
    QoreIRBasicBlock* current_loop_exit = nullptr;

    //! Stack of active scope IDs for tracking nested on_exit handlers
    std::vector<uint32_t> scope_stack;

    //! Entry in the block cleanup stack for interleaved cleanup of on_exit handlers,
    //! block-scoped local variables, and reference foreach state on break/continue/return
    struct BlockCleanupEntry {
        //! HandlerBarrier is a sentinel pushed around a handler body being
        //! inlined by lowerHandlersAtExit().  It has no cleanup effect, but
        //! emitBlockCleanups() uses it to skip the firing Scope entry so a
        //! non-local exit inside the handler body cannot re-enter the same
        //! handler range. Cleanup entries below that Scope still run.
        enum Type { Scope, Lvars, RefForeachRecord, RefForeach, Context, HandlerBarrier, CatchVar,
            ForeachElement, ForeachSource };
        Type type;
        uint32_t scope_id = 0;                    //!< scope ID for Scope entries
        size_t handler_start = 0;                 //!< index into block_handlers at scope entry (for inline lowering)
        const LVList* lvars = nullptr;             //!< local variables for Lvars entries
        const QoreProgramLocation* loc = nullptr;  //!< location for cleanup instructions
        QoreIRValue ref_foreach_state;             //!< state handle for RefForeach/RefForeachRecord
        QoreIRValue old_implicit_element;          //!< saved $# for RefForeachRecord/ForeachElement
        QoreValue var_expr;                        //!< loop variable expression for RefForeachRecord
        QoreIRValue context_state;                 //!< Context* handle for Context entries
        LocalVar* catch_var = nullptr;             //!< single local for CatchVar entries (try-catch exception param)
        QoreIRValue foreach_iterator;              //!< iterator handle released by a ForeachSource entry, if any
        QoreIRValue foreach_list;                  //!< list temporary released by a ForeachSource entry, if any
        QoreIRValue foreach_element;               //!< current element released by a ForeachSource entry, if any
        size_t temp_scope_depth = 0;               //!< temp scopes open when the entry was pushed
        //! true for the Scope entry of a statement block with on_exit/on_success/on_error handlers
        bool block_scope = false;
        //! the exception target of the body of the block of a Scope entry with handlers (block_scope)
        /** An exception raised by one of the scope's handlers lowered inline goes here, while the scope is still
            open at runtime with the handlers that have not run yet: the block's own exception cleanup block (which
            runs them with the exception, then releases the block's locals), or nullptr when nothing in the frame
            encloses the block, and the exception leaves the frame, running them on the way.
        */
        QoreIRBasicBlock* handler_exception_target = nullptr;
    };
    //! Unified cleanup stack tracking scope exits, block-scoped locals, and ref foreach state
    /** Entries are pushed in nesting order. Reverse iteration gives correct cleanup ordering.
     *  For reference foreach, the ordering is:
     *    RefForeach (state) → Scope (try scope) → RefForeachRecord (state, var, $#)
     *  On break, all three are processed (fill remaining + write back).
     *  On return, all three are processed (write back without fill remaining).
     *  On continue, only body inner entries above RefForeachRecord are processed.
     */
    std::vector<BlockCleanupEntry> cleanup_stack;

    //! Handlers registered for current block, to be lowered inline at exit points
    std::vector<InlineHandler> block_handlers;

    //! True while lowering top-level code
    /** Top-level IR is executed without the IR optimization passes, so the analysis that proves a local list
        unique for a native list push (qore_ir_mark_local_list_pushes()) never runs for it; see lowerPush()
    */
    bool lowering_top_level = false;

    //! Local variables of foreach loops assigned each element as it is (AssignmentMode::Element)
    /** Such a variable can hold a weak reference, which a mutation of the variable changes as the AST interpreter
        changes it, through the variable itself; the mutations of these variables therefore use the structured
        lvalue paths instead of the sequences that load and store the variable.
    */
    std::unordered_set<const LocalVar*> foreach_element_locals;

    //! Returns true if an operation that reads and changes the variable must change it through its lvalue
    /** A variable that other threads can change (see isSharedVar() in QoreIRLowering.cpp) is changed in one
        operation, and the variable of a foreach loop (foreach_element_locals) and a variable assigned with := or @=
        (LocalVar::isWeakAssigned()) are changed through the variable itself, as they can hold a weak reference.
    */
    DLLLOCAL bool changesThroughLValue(const VarRefNode* var) const;

    //! Returns true if evaluating \a expr reads, other than as its own value, a local variable that can hold a weak
    //! reference to an object (the variable of a foreach loop, or one assigned with := or @=)
    /** A load of such a variable takes a strong reference of its own, a temporary of the statement that must be
        released before the variable is destroyed when a return statement leaves its block.
    */
    DLLLOCAL bool expressionReadsWeakCapableLocal(const QoreValue& expr) const;

    //! Emits the block that an exception leaving the body of a value foreach loop goes to before \a outer_target
    /** Releases the element (if valid), the values that map expressions gave from it last (the valid ones, in their
        order: the list holds a reference of its own), the partial result (if valid: the list that a functional operator built so far), the
        iterator (if valid), the list and the loop variables, in this order, as the AST's foreach statement and
        functional operators do when an exception unwinds them, before the handler of the enclosing try block or block
        with handlers runs.  Without \a outer_target, the exception then leaves the function (a synthetic rethrow).
    */
    //! Releases the value of a functional operator's predicate once it has been converted to a bool, if it can
    //! hold an object
    /** The AST interpreter holds the value of the predicate for the element (ValueEvalOptimizedRefHolder) and
        releases it when it has decided on the element; as a temporary of the statement, it was released when the
        next element's value was, or with the statement's temporaries after the operator's values when an exception
        left the operator.  Nothing is emitted for a value that cannot hold an object, whose release cannot be
        observed, or that is the element (\a element) itself.
    */
    DLLLOCAL void releasePredicateValue(QoreIRValue value, const QoreValue& expr, QoreIRValue element,
            const QoreProgramLocation* loc);

    DLLLOCAL void emitForeachExceptionCleanup(QoreIRBasicBlock* cleanup_block, QoreIRBasicBlock* outer_target,
            QoreIRValue iterator, QoreIRValue list, QoreIRValue element, const LVList* lvars,
            const QoreProgramLocation* loc, QoreIRValue partial_result = QoreIRValue(),
            const std::vector<QoreIRValue>& element_values = {});

    //! Top-level handlers saved before block_handlers is erased
    /** Used by compileAllHandlerIRs() to iterate over handlers that would otherwise
     *  be lost when lowerStatementBlock() erases block_handlers. Only populated at
     *  the top-level block scope (block_handler_start == 0).
     */
    std::vector<InlineHandler> saved_top_level_handlers;

    //! Stack of handler vector sizes at each block entry for nested block tracking
    /** When entering a nested block, push the current block_handlers.size().
     *  On exit, pop and restore to enable proper handler isolation between block scopes.
     */
    std::stack<size_t> handler_stack;

    class GuardExceptionTargetOverrideScope {
    public:
        GuardExceptionTargetOverrideScope(QoreIRLowering& lowering, QoreIRBasicBlock* target);
        ~GuardExceptionTargetOverrideScope();

    private:
        QoreIRLowering& lowering;
        QoreIRBasicBlock* previous;
    };

    struct FlowTarget {
        QoreIRBasicBlock* break_target = nullptr;
        QoreIRBasicBlock* continue_target = nullptr;
        bool is_switch = false;
        int catch_cleanup_depth = 0;   //!< catch_cleanup_depth when this flow target was created
        size_t cleanup_stack_depth = 0;  //!< cleanup_stack depth when this flow target was created
        QoreIRValue old_implicit_element;  //!< saved $# value for foreach loops (for break/continue cleanup)
        size_t temp_scope_depth = 0;     //!< temp scopes open when this flow target was created
        //! the exception target in force when this flow target was created: a continue or break that branches to a
        //! block that checks for cancellation raises it in the scope of the loop, as the loop's own branches do,
        //! and not in a try block that the statement is in inside the loop
        QoreIRBasicBlock* loop_exception_target = nullptr;
    };
    std::vector<FlowTarget> flow_stack;
    //! Push a block cleanup entry, recording the number of temp scopes open where it is pushed
    void pushBlockCleanup(BlockCleanupEntry entry);
    //! Push a break / continue target, recording the number of temp scopes open where it is pushed
    void pushFlowTarget(FlowTarget target);
    std::vector<QoreIRBasicBlock*> exception_stack;
    //! Scope stack depth at each exception handler's entry point.
    //! Parallel to exception_stack — records scope_stack.size() when each handler was pushed.
    //! Used to set QoreIRInvokeInstruction::exception_scope_depth so the IR interpreter
    //! can execute on_error/on_exit handlers when an invoke throws.
    std::vector<size_t> exception_scope_depth_stack;

    //! Phase 3b: Maps OnBlockExitStatement to compiled handler IR
    //! Stores compiled handlers during lowering, ready for Phase 4 execution
    std::unordered_map<const OnBlockExitStatement*, std::unique_ptr<QoreIRFunction>> compiled_handlers;

    //! Depth of active catch scopes that need CatchCleanup on non-local exit
    int catch_cleanup_depth = 0;

    //! catch_cleanup_depth at the body of each enclosing try statement, innermost last
    /** An exception raised in a try body lands in that try statement's catch block, so a throw or rethrow there
        leaves only the catch scopes entered since the try body began; the catch scopes that enclose the try statement
        stay active (see catchScopesLeftByThrow()).
    */
    std::vector<int> try_catch_cleanup_depth_stack;

    //! Returns the number of active catch scopes that a throw or rethrow statement at the current point leaves
    /** These are the catch scopes entered since the body of the innermost enclosing try statement began, or all of
        them when there is none.  Popping more would delete the exception of a catch block that is still running, and
        that a rethrow in it reads after the inner try statement.
    */
    int catchScopesLeftByThrow() const {
        return catch_cleanup_depth - (try_catch_cleanup_depth_stack.empty() ? 0 : try_catch_cleanup_depth_stack.back());
    }

    //! Virtual implicit argument context - eliminates push/pop runtime calls for map/select/foldl/foldr
    /** When active, $1/$2/$# references in lowerExpression() use these IR values directly
     * instead of emitting LoadImplicitArg/LoadImplicitElement runtime calls.
     * This eliminates 6+ runtime calls (push/pop/load for arg and element) per loop iteration.
     */
    struct VirtualImplicitContext {
        QoreIRValue arg0;      //!< virtual $1 value (offset 0)
        QoreIRValue arg1;      //!< virtual $2 value (offset 1, for foldl/foldr)
        QoreIRValue element;   //!< virtual $# value
        bool active = false;   //!< true when virtual context is in effect
    };
    VirtualImplicitContext virtual_implicit;

    //! The state of the lowering of a background helper function; see lowerBackgroundHelper()
    struct BackgroundHelperLowering {
        //! the values other than local variables that the expression takes when the thread starts
        /** Implicit arguments of the starting code, member reads, find expressions, closures and references; each is
            held by the parameter with the same index in \a params
        */
        std::vector<QoreValue> leaves;
        //! the parameter that holds each value in \a leaves
        std::vector<std::unique_ptr<LocalVar, QoreIROwnedLocalVarDeleter>> params;
        //! maps the key of a value in \a leaves to its index, so that a value used twice is taken once
        std::unordered_map<std::string, size_t> leaf_index;
        //! true if the expression calls a method of the object of the starting code
        bool uses_self = false;
    };
    //! Set while the expression of a background helper is lowered, null otherwise
    BackgroundHelperLowering* bg_helper = nullptr;

    //! The number of background helpers lowered by this lowering and the lowerings nested in it
    unsigned bg_helper_count_own = 0;
    //! Counts background helpers for the outermost lowering, so that the names of their parameters are unique and
    //! deterministic within the function; nested lowerings (handlers, helpers) share their parent's counter
    unsigned* bg_helper_count = &bg_helper_count_own;
    //! The number of the background helper this lowering lowers, if any (see bg_helper)
    unsigned bg_helper_index = 0;

    //! Lowers a value that a background expression takes when the thread starts, while lowering its helper
    /** @param expr the expression to lower
        @param rv the load of the parameter holding the value (for a reference, a reference to the parameter), if
        \a expr is such a value; invalid if an error was raised
        @param error the error message, if lowering the use of the parameter fails

        @return true if \a expr is such a value, false if not
    */
    bool lowerBackgroundLeaf(const QoreValue& expr, QoreIRValue& rv, std::string& error);

    //! Returns the parameter of the background helper being lowered that holds a value the expression takes when the
    //! thread starts, created for the key if it does not exist yet (see lowerBackgroundLeaf())
    LocalVar* backgroundLeafParam(std::string key, const QoreValue& expr);

    //! Extracts the access path of an lvalue (the file-level extractLValuePath() function)
    /** In a background helper, the root of a path that is a member of the object of the code that starts the thread is
        the value of the member that the thread took when it started, as for the AST interpreter, which the helper
        holds in a parameter: the new thread has no object context (issue #5723: <tt>background (o.x = 1)</tt> in a
        method of a class with an object member \c o raised "no object context for self member access")
    */
    bool extractLValuePath(const QoreValue& expr, std::vector<LVPathStep>& path,
            std::vector<QoreValue>& dynamic_operands, bool allow_slice = false);

    //! Lowers a background expression to a helper function and the instruction starting the thread with it
    /** See QoreIRBackgroundInstruction.
    */
    QoreIRValue lowerBackgroundHelper(const QoreValue& expr, const QoreBackgroundOperatorNode* op,
        std::string& error);

    //! Counter for AST-delegated instructions (Call/Invoke with expr, DotEval* except
    //! DotEvalMethodDirect/HashKeyAccess).  Used by lowerMapNative to determine whether
    //! push/pop implicit arg calls are needed for the map body.
    int ast_delegate_count = 0;

    //! Hoisted per-loop values for safe, invariant <list>.size() expressions.
    std::vector<std::unordered_map<LocalVar*, QoreIRValue>> loop_invariant_list_sizes;

    //! Hoisted per-loop values for safe invariant expressions keyed by AST node.
    std::vector<std::unordered_map<const AbstractQoreNode*, QoreIRValue>> loop_invariant_values;
};

#endif
