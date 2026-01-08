/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    StatementBlock.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QORE_STATEMENT_BLOCK_H

#define _QORE_STATEMENT_BLOCK_H

#include "qore/intern/AbstractStatement.h"
#include <qore/safe_dslist>

#include <set>
#include <vector>
#include <typeinfo>

// all definitions in this file are private to the library and subject to change
class BCAList;
class BCList;

class LVList {
public:
    typedef std::vector<LocalVar*> lv_vec_t;
    lv_vec_t lv;

    DLLLOCAL LVList(int num) {
        add(num);
    }

    DLLLOCAL LVList(const LVList& old) {
        lv.resize(old.size());
        for (unsigned i = 0; i < old.size(); ++i)
            lv[i] = old.lv[i];

        //printd(5, "LVList::LVList() populated with %d vars\n", lv.size());
    }

    DLLLOCAL ~LVList() {
    }

    DLLLOCAL size_t size() const {
        return lv.size();
    }

    DLLLOCAL void add(int num) {
        assert(num > 0);
        unsigned start = lv.size();
        //printd(5, "LVList::add(num: %d) this: %p start: %d\n", num, this, start);
        lv.resize(start + num);
        // pop variables off stack and save in reverse order
        for (int i = (int)(start + num - 1); i >= (int)start; --i) {
            lv[i] = pop_local_var();
            //printd(5, "LVList::add() %d = %p: %s\n", i, lv[i], lv[i]->getName());
        }
    }
};

class LVListInstantiator {
    const LVList* l;
    ExceptionSink* xsink;
    int64 parse_options;

public:
    DLLLOCAL LVListInstantiator(ExceptionSink* xs, const LVList* l, int64 parse_options) : l(l) {
        if (!l) return;
        assert(l->size());
        xsink = xs;
        for (unsigned i = 0; i < l->size(); ++i) {
            //printd(5, "LVListInstantiator::LVListInstantiator() this: %p v: %p %s\n", this, l->lv[i],
            //    l->lv[i]->getName());
            l->lv[i]->instantiate(parse_options);
        }
    }

    DLLLOCAL ~LVListInstantiator() {
        if (!l) return;
        for (int i = (int)l->size() - 1; i >= 0; --i) {
            //printd(5, "LVListInstantiator::~LVListInstantiator() this: %p v: %p %s\n", this, l->lv[i],
            //  l->lv[i]->getName());
            l->lv[i]->uninstantiate(xsink);
        }
    }
};

// forward declaration
class qore_program_private_base;

class StatementBlock : public AbstractStatement {
public:
    DLLLOCAL StatementBlock(int sline, int eline);

    // line numbers on statement blocks are set later
    DLLLOCAL StatementBlock(int sline, int eline, AbstractStatement* s);

    DLLLOCAL virtual ~StatementBlock() {
        del();
    }

    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context);

    DLLLOCAL void del();

    DLLLOCAL void addStatement(AbstractStatement* s);

    using AbstractStatement::exec;
    DLLLOCAL QoreValue exec(ExceptionSink* xsink);

    using AbstractStatement::parseInit;
    DLLLOCAL int parseInit(UserVariantBase* uvb, const QoreClass* class_ctx = nullptr);

    // initialize methods
    DLLLOCAL int parseInitMethod(const QoreTypeInfo* typeInfo, UserVariantBase* uvb);
    DLLLOCAL int parseInitConstructor(const QoreTypeInfo* typeInfo, UserVariantBase* uvb, BCAList* bcal,
            const QoreClass& cls);

    // initialize closure blocks
    DLLLOCAL int parseInitClosure(UserVariantBase* uvb, UserClosureFunction* cf);

    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

    DLLLOCAL void exec();

    DLLLOCAL const LVList* getLVList() const {
        return lvars;
    }

    DLLLOCAL virtual bool hasFinalReturn() const {
        if (statement_list.empty())
            return false;

        return (*statement_list.last())->hasFinalReturn();
    }

    DLLLOCAL void setupLVList(QoreParseContext& parse_context) {
        assert(!lvars);
        if (!parse_context.lvids) {
            return;
        }

        lvars = new LVList(parse_context.lvids);
        parse_context.lvids = 0;
    }

protected:
    typedef safe_dslist<AbstractStatement*> statement_list_t;
    statement_list_t statement_list;
    block_list_t on_block_exit_list;
    LVList* lvars = nullptr;

    // start must be the element before the start position
    DLLLOCAL int parseInitIntern(QoreParseContext& parse_context, statement_list_t::iterator start);
    DLLLOCAL void parseCommitIntern(statement_list_t::iterator start);
    DLLLOCAL bool hasLastReturn(AbstractStatement* as);
    DLLLOCAL int parseCheckReturn();

    DLLLOCAL int execIntern(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL int execIntern(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);

    DLLLOCAL StatementBlock(qore_program_private_base* p);
};

class TopLevelStatementBlock : public StatementBlock {
public:
    DLLLOCAL TopLevelStatementBlock(qore_program_private_base* p) : StatementBlock(p), hwm(statement_list.end()),
            ehwm(statement_list.end()), first(true) {
    }

    DLLLOCAL virtual ~TopLevelStatementBlock() {
    }

    using StatementBlock::parseInit;
    DLLLOCAL int parseInit();

    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

    DLLLOCAL void parseRollback() {
        // delete all statements after the high water mark (hwm) to the end of the list
        statement_list_t::iterator start = hwm;
        if (start != statement_list.end())
            ++start;
        else
            start = statement_list.begin();

        for (statement_list_t::iterator i = start, e = statement_list.end(); i != e; ++i)
            delete *i;

        statement_list.erase_to_end(hwm);
    }

    // local vars are not instantiated here because they are instantiated by the QoreProgram object
    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);

    // assign inherited local var list from parent program
    DLLLOCAL void assignLocalVars(const LVList* lvl) {
        assert(!lvars);
        lvars = new LVList(*lvl);
    }

    DLLLOCAL void setupLVList(QoreParseContext& parse_context) {
        if (!parse_context.lvids) {
            return;
        }

        if (lvars) {
            lvars->add(parse_context.lvids);
        } else {
            lvars = new LVList(parse_context.lvids);
        }
        parse_context.lvids = 0;
    }

protected:
    // iterator to last commit element in statement list (for parsing)
    statement_list_t::iterator hwm;
    // iterator to last executed element in statement list (for REPARSE mode execution)
    statement_list_t::iterator ehwm;
    // true only the first time parseInit() is called
    bool first;
};

// parse variable stack
class VNode {
public:
    LocalVar* lvar;
    VNode* next;

    DLLLOCAL VNode(LocalVar* lv, const QoreProgramLocation* n_loc = nullptr, int n_refs = 0,
            bool n_top_level = false);

    DLLLOCAL ~VNode();

    DLLLOCAL void appendLocation(QoreString& str);

    DLLLOCAL void setRef() {
        ++refs;
    }

    DLLLOCAL bool setBlockStart(bool bs = true) {
        bool rc = block_start;
        block_start = bs;
        return rc;
    }

    DLLLOCAL bool isBlockStart() const {
        return block_start;
    }

    DLLLOCAL bool isReferenced() const {
        return refs;
    }

    DLLLOCAL int refCount() const {
        return refs;
    }

    DLLLOCAL bool isTopLevel() const {
        return top_level;
    }

    DLLLOCAL const char* getName() const;

    // searches to marker and then jumps to global thread-local variables
    DLLLOCAL VNode* nextSearch() const;

protected:
    // # of times this variable is referenced in code
    int refs;

    // to store parse location in case of errors
    const QoreProgramLocation* loc;
    bool block_start;
    bool top_level;
};

class CatchExceptionHelper {
private:
    QoreException* e;

public:
    DLLLOCAL CatchExceptionHelper(QoreException* n_e) : e(catch_swap_exception(n_e)) {
    }

    DLLLOCAL ~CatchExceptionHelper() {
        catch_swap_exception(e);
    }
};

//! Helper class to save and restore narrowed types for branch tracking
/** This class is used during if/else and switch statement parsing to track
    narrowed types across branches. It saves the current narrowed types before
    parsing a branch and can restore them after, allowing each branch to
    have independent type narrowing while ensuring the correct merged type
    is used after the branching statement.
*/
class NarrowedTypeHelper {
private:
    typedef std::vector<std::pair<LocalVar*, const QoreTypeInfo*>> type_map_t;
    type_map_t saved_types;
    std::vector<type_map_t> branch_types;

public:
    //! Save the current narrowed types of all local auto variables
    DLLLOCAL void saveState();

    //! Restore the narrowed types to the saved state
    DLLLOCAL void restoreState();

    //! Record current branch types and restore to saved state
    /** Call this after parsing each branch to record its final types
        and restore to the pre-branch state for the next branch
    */
    DLLLOCAL void recordBranchAndRestore();

    //! Merge all recorded branch types and apply the common type
    /** Call this after all branches have been parsed to compute and
        apply the common type across all branches
    */
    DLLLOCAL void mergeAndApply();

    //! Record the saved state as an implicit branch (for if without else)
    /** When there is no else branch, the implicit else path preserves the
        original types. This method records the saved types as a branch so
        that mergeAndApply() considers both paths.
    */
    DLLLOCAL void recordSavedAsImplicitBranch();
};

#endif // _QORE_STATEMENT_BLOCK_H
