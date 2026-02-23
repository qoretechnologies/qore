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

#include <string>
#include <unordered_set>
#include <vector>

#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreParseAnalysis.h>

class QoreParseContext;
class QoreParseListNode;
class QoreListNode;
class QoreValue;
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

class QoreIRLowering {
public:
    explicit QoreIRLowering(QoreIRBuilder& builder, QoreParseContext* parse_context = nullptr);

    QoreIRValue lowerExpression(const QoreValue& expr, std::string& error);
    bool lowerStatement(const AbstractStatement* stmt, std::string& error);
    bool lowerStatementBlock(const StatementBlock* block, std::string& error);
    void setParseContext(QoreParseContext* parse_context);

private:
    QoreIRValue lowerConstant(const QoreValue& expr, std::string& error);
    QoreIRValue lowerVarRef(const QoreValue& expr, std::string& error);
    QoreIRValue lowerAssignment(const QoreValue& expr, std::string& error);
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
    QoreIRValue lowerLogicalEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalNotEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAbsoluteEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAbsoluteNotEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalLessThan(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalLessThanOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalGreaterThan(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalGreaterThanOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalComparison(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAnd(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalOr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalNot(const QoreValue& expr, std::string& error);
    QoreIRValue lowerNullCoalescing(const QoreValue& expr, std::string& error);
    QoreIRValue lowerValueCoalescing(const QoreValue& expr, std::string& error);
    QoreIRValue lowerQuestionMark(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnaryPlus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnaryMinus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryNot(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMultiplication(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDivision(const QoreValue& expr, std::string& error);
    QoreIRValue lowerModulo(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryAnd(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryOr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryXor(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftLeft(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftRight(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftLeftEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftRightEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRange(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSquareBracketsRange(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSquareBrackets(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashObjectDereference(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShift(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnshift(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPop(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPush(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSplice(const QoreValue& expr, std::string& error);
    QoreIRValue lowerExtract(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRemove(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDelete(const QoreValue& expr, std::string& error);
    QoreIRValue lowerKeys(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexNMatch(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexMatch(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexExtract(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRegexSubst(const QoreValue& expr, std::string& error);
    QoreIRValue lowerParseHash(const QoreValue& expr, std::string& error);
    QoreIRValue lowerParseList(const QoreValue& expr, std::string& error);
    QoreIRValue lowerInstanceOf(const QoreValue& expr, std::string& error);
    QoreIRValue lowerTrim(const QoreValue& expr, std::string& error);
    QoreIRValue lowerChomp(const QoreValue& expr, std::string& error);
    QoreIRValue lowerTransliteration(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBackground(const QoreValue& expr, std::string& error);
    QoreIRValue lowerListAssignment(const QoreValue& expr, std::string& error);
    QoreIRValue lowerExists(const QoreValue& expr, std::string& error);
    QoreIRValue lowerElements(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDotEval(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMap(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSelect(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMapSelect(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashMap(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashMapSelect(const QoreValue& expr, std::string& error);

    //! Native IR lowering for map operator with implicit argument context
    QoreIRValue lowerMapNative(const QoreMapOperatorNode* map, const QoreValue& expr, std::string& error);
    //! Native IR lowering for select operator with implicit argument context
    QoreIRValue lowerSelectNative(const QoreSelectOperatorNode* select, const QoreValue& expr, std::string& error);
    //! Native IR lowering for foldl operator with implicit argument context
    QoreIRValue lowerFoldlNative(const QoreFoldlOperatorNode* foldl, const QoreValue& expr, std::string& error);
    //! Native IR lowering for foldr operator (reverse iteration)
    QoreIRValue lowerFoldrNative(const QoreFoldrOperatorNode* foldr, const QoreValue& expr, std::string& error);
    //! Native IR lowering for map+select operator
    QoreIRValue lowerMapSelectNative(const QoreMapSelectOperatorNode* ms, const QoreValue& expr, std::string& error);
    //! Native IR lowering for hash map operator
    QoreIRValue lowerHashMapNative(const QoreHashMapOperatorNode* hm, const QoreValue& expr, std::string& error);
    //! Native IR lowering for hash map+select operator
    QoreIRValue lowerHashMapSelectNative(const QoreHashMapSelectOperatorNode* hms, const QoreValue& expr, std::string& error);
    QoreIRValue lowerCast(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFunctionCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerCallReference(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSelfCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerStaticCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerListNode(const QoreValue& expr, std::string& error);
    bool guardVarLValue(const QoreValue& exp, std::string& error, bool allow_maybe_nothing = false);
    bool guardLValueBase(const QoreValue& exp, std::string& error, bool allow_maybe_nothing = false);
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
    QoreIRBasicBlock* createBlock(const std::string& prefix);
    QoreIRValue loadVarRef(const VarRefNode* var, std::string& error, const char* context,
        const QoreValue& expr);
    bool storeVarRef(const VarRefNode* var, QoreIRValue value, std::string& error, const char* context,
        const QoreValue* expr = nullptr, const QoreProgramLocation* guard_loc = nullptr);
    LocalVar* getLocalVarFromValue(const QoreValue& expr) const;
    const QoreTypeInfo* getGuaranteedTypeForValue(const QoreValue* expr, const QoreTypeInfo* fallback) const;
    bool lowerCallArgs(const QoreParseListNode* parse_args, const QoreListNode* args,
        std::vector<QoreIRValue>& lowered, std::string& error);
    QoreIRValue lowerExprOpOrInvoke(QoreIROpcode op, const QoreValue& expr, const std::vector<QoreIRValue>& operands,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerExprOpOrInvokeNoGuard(QoreIROpcode op, const QoreValue& expr,
        const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerBinaryOpOrInvoke(QoreIROpcode op, const QoreValue& expr, QoreIRValue left, QoreIRValue right,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerUnaryOpOrInvoke(QoreIROpcode op, const QoreValue& expr, QoreIRValue value,
        const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitHashKeyCompoundOp(const VarRefNode* container_var, const std::string& key_name,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue emitListKeyCompoundOp(const VarRefNode* container_var, const QoreValue& index_expr,
        QoreIROpcode arith_op, const QoreIRValue& right,
        const QoreValue& full_expr, const QoreProgramLocation* loc, std::string& error);
    QoreIRValue lowerConditionValue(const QoreValue& cond, std::string& error);
    QoreIROpcode selectComparisonOpcode(const QoreValue& left, const QoreValue& right,
        QoreIROpcode int_op, QoreIROpcode float_op, QoreIROpcode any_op);
    bool expressionCanThrow(const QoreValue& expr) const;
    void markLocalAssignmentFromExpression(const QoreValue& exp);
    void markLocalUnassignmentFromExpression(const QoreValue& exp);

    QoreIRBasicBlock* getCurrentExceptionTarget() const;
    QoreIRBasicBlock* getGuardExceptionTarget() const;
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

    //! Emit ScopeExit instructions for active scopes down to target depth
    /** Used by return/break/continue to properly unwind on_exit handlers
     * @param target_depth the scope_stack depth to unwind to (0 for return, loop's depth for break/continue)
     * @param is_error true if exiting due to an exception
     */
    void emitScopeExits(size_t target_depth, bool is_error = false);

    //! Flags for emitBlockCleanups controlling cleanup behavior
    enum CleanupFlags : unsigned {
        CF_NONE = 0,
        CF_SKIP_LVARS = 1,       //!< skip local var cleanup (for return - pre-instantiation handles it)
        CF_FILL_REMAINING = 2,   //!< fill remaining elements in ref foreach (for break)
    };

    //! Emit block cleanup instructions (ScopeExit + UninstantiateLocal + RefForeach)
    //! from innermost to target depth
    /** Used by return/break/continue to properly unwind on_exit handlers, block-scoped
     *  local variables, and reference foreach state in the correct interleaved order
     *  matching AST mode.
     *  @param target_depth the cleanup_stack depth to unwind to
     *  @param is_error true if exiting due to an exception
     *  @param flags combination of CleanupFlags
     */
    void emitBlockCleanups(size_t target_depth, bool is_error = false, unsigned flags = CF_NONE);

    QoreIRBuilder& builder;
    QoreParseContext* parse_context = nullptr;
    //! Values known to never be NOTHING (produced by typed opcodes)
    std::unordered_set<uint32_t> never_nothing_values;
    uint32_t block_counter = 0;
    uint32_t scope_counter = 0;  //!< Counter for generating unique scope IDs within a function
    QoreIRBasicBlock* guard_exception_target_override = nullptr;

    //! Stack of active scope IDs for tracking nested on_exit handlers
    std::vector<uint32_t> scope_stack;

    //! Entry in the block cleanup stack for interleaved cleanup of on_exit handlers,
    //! block-scoped local variables, and reference foreach state on break/continue/return
    struct BlockCleanupEntry {
        enum Type { Scope, Lvars, RefForeachRecord, RefForeach };
        Type type;
        uint32_t scope_id = 0;                    //!< scope ID for Scope entries
        const LVList* lvars = nullptr;             //!< local variables for Lvars entries
        const QoreProgramLocation* loc = nullptr;  //!< location for cleanup instructions
        QoreIRValue ref_foreach_state;             //!< state handle for RefForeach/RefForeachRecord
        QoreIRValue old_implicit_element;          //!< saved $# for RefForeachRecord
        QoreValue var_expr;                        //!< loop variable expression for RefForeachRecord
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
    };
    std::vector<FlowTarget> flow_stack;
    std::vector<QoreIRBasicBlock*> exception_stack;
    //! Scope stack depth at each exception handler's entry point.
    //! Parallel to exception_stack — records scope_stack.size() when each handler was pushed.
    //! Used to set QoreIRInvokeInstruction::exception_scope_depth so the IR interpreter
    //! can execute on_error/on_exit handlers when an invoke throws.
    std::vector<size_t> exception_scope_depth_stack;

    //! Depth of active catch scopes that need CatchCleanup on non-local exit
    int catch_cleanup_depth = 0;

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

    //! Counter for AST-delegated instructions (Call/Invoke with expr, DotEval* except
    //! DotEvalMethodDirect/HashKeyAccess).  Used by lowerMapNative to determine whether
    //! push/pop implicit arg calls are needed for the map body.
    int ast_delegate_count = 0;
};

#endif
