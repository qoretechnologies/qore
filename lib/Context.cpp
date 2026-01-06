/*
    Context.cc

    Qore programming language

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

#include <qore/Qore.h>
#include "qore/intern/QoreHashNodeIntern.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>

class Templist {
public:
    QoreValue val;
    int pos;
};

struct node_row_list_s {
    QoreValue val;
    int *row_list;
    int num_rows;
    int allocated;
};

#define ROW_BLOCK 40

static int in_list(QoreValue val, node_row_list_s* nlist, int max, int row, ExceptionSink* xsink) {
    int i;

    for (i = 0; i < max; i++) {
        if (!val.isEqualSoft(nlist[i].val, xsink)) {
            if (*xsink)
                return 0;
            // resize array if necessary
            if (nlist[i].num_rows == nlist[i].allocated) {
                printd(5, "%d: old row_list: %p\n", i, nlist[i].row_list);
                int d = nlist[i].allocated >> 2;
                nlist[i].allocated += (d > ROW_BLOCK ? d : ROW_BLOCK);
                nlist[i].row_list = (int*)
                realloc(nlist[i].row_list, sizeof(int) * nlist[i].allocated);
                printd(5, "%d: new row_list: %p\n", i, nlist[i].row_list);
            }
            printd(5, "in_list() row %d added to list for unique value %d (%d)\n", row, i, nlist[i].num_rows);
            nlist[i].row_list[nlist[i].num_rows++] = row;
            return 1;
        }
    }
    return 0;
}

/*
 * if exp == 0, then it is a subcontext.  Calling with exp == 0
 * should only be possible if there is a parent context, so no checks are
 * needed to see if there really is a parent context.
 * The code in summary contexts is only executed once for each discrete value,
 * therefore subcontexts of summary contexts compare all values in the
 * current parent context to create the subcontext row list
 *
 * ROW_BLOCK will be used for normal row lists and summarized value row lists
 * (for now)
 */

#pragma GCC diagnostic ignored "-Wclass-memaccess"
Context::Context(char* nme, ExceptionSink* xsink, QoreValue exp, QoreValue cond,
                 int sort_type, QoreValue sort, QoreValue summary,
                 int ignore_key) {
    int allocated = 0;
    //int sense, lcolumn = -1, fcolumn = -1
    //class Key *key = 0;

    QORE_TRACE("Context::Context()");
    //e = ex;

    sub = !exp;
    // set up initial row list and parameters
    if (sub) { // copy subcontext
        // push context on stack
        next = get_context_stack();
        update_context_stack(this);

        name = next->name ? strdup(next->name) : 0;
        value = next->value;
        max_pos = next->max_pos;
        if (max_pos) {
            row_list = (int*)malloc(sizeof(int) * max_pos);
            if (!row_list) {
                xsink->outOfMemory();
                return;
            }
            memcpy(row_list, next->row_list, sizeof(int) * max_pos);
            printd(5, "Context::Context() subcontext: max_pos: %d row_list: %p\n", max_pos, row_list);
        }
    } else { // copy object (query) list
        name = nme ? strdup(nme) : nullptr;
        ValueEvalOptimizedRefHolder rv(exp, xsink);

        // push context on stack
        next = get_context_stack();
        update_context_stack(this);

        if (*xsink)
            return;

        if (rv->getType() != NT_HASH) {
            return;
        }
        value = rv->get<QoreHashNode>();

        QoreValue fkv = qore_hash_private::getFirstKeyValue(value);

        QoreListNode* l = fkv.getType() == NT_LIST ? fkv.get<QoreListNode>() : nullptr;
        if (l) {
            max_pos = l->size();
            row_list = (int*)malloc(sizeof(int) * max_pos);
            if (!row_list) {
                xsink->outOfMemory();
                return;
            }

            for (int i = 0; i < max_pos; i++)
                row_list[i] = i;
            printd(5, "Context::Context() object: max_pos: %d row_list: %p\n", max_pos, row_list);
        }
        else
            max_pos = 0;

        rv.takeReferencedValue();
    }

    printd(5, "Context::Context() %s max_pos: %d row_list: %p\n",
            sub ? "<SUBCONTEXT>" : "<NORMAL>", max_pos, row_list);
    //printd(2, "Context::Context() %p (%s) cond: %p\n", key, key ? sense == K_DIRECT ? "direct" : "reverse" : "none", cond);
    // if there are restrictions, then evaluate each row
    if (cond) {
        // use master_row_list to hold the new row_list
        master_row_list = (int*)malloc(sizeof(int) * max_pos);
        if (!master_row_list) {
            xsink->outOfMemory();
            return;
        }

        // iterate each row in results
        for (pos = 0; pos < max_pos; pos++) {
            //printd(5, "Context::Context() row iteration: %d/%d (%p)\n", pos, max_pos, key);

            // if there are constraints to check
            if (!check_condition(cond, xsink)) {
                master_row_list[pos] = 0;
                printd(5, "Context::Context() row %d REJECTED (!cond)\n", pos);
                continue;
            }
            // add row to row list
            printd(5, "Context::Context() row %d ACCEPTED\n", pos);
            master_row_list[pos] = 1;
        }
        // copy the list over
        pos = 0;
        for (int i = 0; i < max_pos; i++)
            if (master_row_list[i]) {
                if (i != pos)
                    row_list[pos] = row_list[i];
                pos++;
            }
        if (max_pos != pos) {
            max_pos = pos;
            if (max_pos)
                row_list = (int*)realloc(row_list, sizeof(int) * max_pos);
            else {
                free(row_list);
                row_list = 0;
            }
        }
        if (master_row_list) {
            free(master_row_list);
            master_row_list = 0;
        }
    }

    // sort if applicable
    if (sort) {
        sort_xsink = xsink;
        Sort(sort, sort_type);
    }
    if (xsink->isEvent())
        return;

    if (summary) {
        printd(4, "Context::Context() finding unique values for summary context\n");
        master_max_pos = max_pos;
        master_row_list = row_list;
        allocated = 0;
        // find unique values in summary node
        for (pos = 0; pos < master_max_pos; pos++) {
            printd(5, "Context::Context() summary value %d/%d\n", pos, master_max_pos);
            ValueEvalOptimizedRefHolder val(summary, xsink);
            if (*xsink) {
                break;
            }
            if (in_list(*val, group_values, max_group_pos,
                        master_row_list[pos], xsink)) {
                if (*xsink) {
                    break;
                }
                continue;
            }
            if (*xsink) {
                break;
            }
            // resize array if necessary
            if (max_group_pos == allocated) {
                allocated += ROW_BLOCK;
                group_values = (struct node_row_list_s*)
                realloc(group_values,
                        sizeof(struct node_row_list_s) * allocated);
            }
            // insert new value in list
            group_values[max_group_pos].val = val.takeReferencedValue();
            group_values[max_group_pos].num_rows = 1;
            group_values[max_group_pos].allocated = ROW_BLOCK;
            group_values[max_group_pos].row_list = (int*)malloc(sizeof(int) * ROW_BLOCK);
            if (!group_values[max_group_pos].row_list) {
                xsink->outOfMemory();
                return;
            }

            printd(5, "%d: start row_list: %p\n", max_group_pos,
                    group_values[max_group_pos].row_list);
            group_values[max_group_pos].row_list[0] =
                master_row_list[pos];
            printd(4, "Context::Context() row %d creating unique value list %d\n",
                    master_row_list[pos], max_group_pos);
            max_group_pos++;
        }
        // resize array to final size if necessary
        if (max_group_pos != allocated)
            group_values = (struct node_row_list_s*)
                realloc(group_values,
                        sizeof(struct node_row_list_s) * max_group_pos);
        // prepare first context
        if (max_group_pos) {
            row_list = group_values[0].row_list;
            max_pos = group_values[0].num_rows;
        }
    }
    pos = 0;
    printd(5, "Context::Context() max_pos = %d\n", max_pos);
}
#pragma GCC diagnostic pop

Context::~Context() {
    QORE_TRACE("Context::~Context()");

    assert(get_context_stack());
    update_context_stack(get_context_stack()->next);

    if (name) {
        free(name);
    }
    if (master_row_list) {
        free(master_row_list);
        if (group_values) {
            int i;

            for (i = 0; i < max_group_pos; ++i) {
                printd(5, "%d/%d: ", i, max_group_pos);
                group_values[i].val.discard(0);
                printd(5, "row_list: %p (num_rows: %d, allocated: %d): ",
                    group_values[i].row_list,
                    group_values[i].num_rows,
                    group_values[i].allocated);
                free(group_values[i].row_list);
                printd(5, "done\n");
            }
            free(group_values);
        }
    }
    else if (row_list)
        free(row_list);
}

int Context::check_condition(QoreValue cond, ExceptionSink *xsinkx) {
    QORE_TRACE("Context::check_condition()");
    ValueEvalOptimizedRefHolder val(cond, xsinkx);
    return *xsinkx ? -1 : (int)val->getAsBigInt();
}

void Context::deref(ExceptionSink *xsink) {
    if (!sub && value) {
        value->deref(xsink);
    }
    delete this;
}

QoreValue eval_context_ref(const char* key, ExceptionSink* xsink) {
    Context* c = get_context_stack();
    return c->eval(key, xsink);
}

QoreHashNode* eval_context_row(ExceptionSink *xsink) {
    return get_context_stack()->getRow(xsink);
}

QoreValue Context::eval(const char* field, ExceptionSink* xsink) {
    if (!value) {
        return QoreValue();
    }

    bool exists;

    ValueHolder val(qore_hash_private::get(*value)->getReferencedKeyValueIntern(field, exists), xsink);
    if (!exists) {
        xsink->raiseException("CONTEXT-EXCEPTION", "\"%s\" is not a valid key for this context", field);
        return QoreValue();
    }
    if (val->getType() != NT_LIST) {
        return QoreValue();
    }

    QoreValue rv = val->get<QoreListNode>()->retrieveEntry(row_list[pos]);
    //printd(5, "Context::eval(%s) this: %p pos: %d rv: %p %s %lld\n", field, this, pos, rv, rv.getTypeName(), rv.getType() == NT_INT ? rv.getAsBigInt() : -1);
    //printd(5, "Context::eval(%s) pos: %d, val: %s\n", field, pos, rv.getType() == NT_STRING ? rv.get<const QoreStringNode>()->c_str() : "?");
    return rv.refSelf();
}

QoreHashNode* Context::getRow(ExceptionSink *xsink) {
    printd(5, "Context::getRow() value: %p %s\n", value, value ? value->getTypeName() : "NULL");
    if (!value)
        return nullptr;

    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

    qore_hash_private* hp = qore_hash_private::get(**h);

    HashIterator hi(value);
    while (hi.next()) {
        const char* key = hi.getKey();
        printd(5, "Context::getRow() key: %s\n", key);
        // try to get list from hash
        QoreValue v = hi.get();

        // if the hash key does not contain a list, then set the value to NOTHING
        if (v.getType() != NT_LIST)
            hp->setKeyValueIntern(key, QoreValue());
        else {
            // set key value to list entry
            QoreListNode* l = v.get<QoreListNode>();
            hp->setKeyValueIntern(key, l->getReferencedEntry(row_list[pos]));
        }
    }

    return h.release();
}

// to sort non-existing values last
static int compare_templist(Templist t1, Templist t2) {
    //printd(5, "t1.node: %p pos: %d t2.node: %p pos: %d\n", t1.node, t1.pos, t2.node, t2.pos);

    if (t1.val.isNothing()) {
        return 0;
    }
    if (t2.val.isNothing()) {
        return 1;
    }

    ExceptionSink xsink;
    return QoreLogicalLessThanOperatorNode::doLessThan(t1.val, t2.val, &xsink);
}

void Context::Sort(QoreValue snode, int sort_type) {
    int sense = 1;

    QORE_TRACE("Context::Sort()");

    printd(5, "sorting context (%d row(s)) (type: %d)\n", max_pos, sort_type);
    Templist* list = new Templist[max_pos];
    // NOTE: Solaris CC doesn't allow non-constant array sizes
    //Templist list[max_pos];
    // get list of results to be sorted
    for (pos = 0; pos < max_pos; ++pos) {
        ValueEvalOptimizedRefHolder val(snode, sort_xsink);
        if (*sort_xsink) {
            delete [] list;
            return;
        }

        list[pos].val = val.takeReferencedValue();
        printd(5, "Context::Sort() eval(): max: %d list[%d].val = '%s' pos: %d\n",
                max_pos, pos, list[pos].val.getTypeName(),
                row_list[pos]);
        list[pos].pos = row_list[pos];
    }

    // sort the list with STL sort
    std::sort(list, list + max_pos, compare_templist);

    int i;
    // assign sorted row list and delete temporary results
    if (sort_type == CM_SORT_DESCENDING) {
        i = max_pos - 1;
        sense = -1;
    }
    else
        i = 0;
    for (pos = 0; pos < max_pos; pos++) {
        row_list[pos] = list[i].pos;
        printd(5, "Context::Sort() deref(): max: %d list[%d].val = '%s'\n",
                max_pos, i, list[i].val.getTypeName());
        list[i].val.discard(sort_xsink);
        i += sense;
    }

    delete [] list;
}

int Context::next_summary() {
    printd(5, "Context::next_summary() %p %d/%d\n", this, group_pos, max_group_pos);
    ++group_pos;
    if (group_pos == max_group_pos)
        return 0;
    max_pos = group_values[group_pos].num_rows;
    row_list = group_values[group_pos].row_list;
    return 1;
}
