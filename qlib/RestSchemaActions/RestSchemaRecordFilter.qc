# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaRecordFilter.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
#! The result of splitting a search predicate into what the API evaluates and what is left over
public hashdecl RestSchemaRecordFilterSplit {
    #! Listing action options carrying the predicates the API will evaluate
    hash<auto> options;

    #! The predicate left for the client side, if any
    /** Evaluated against each record as it arrives, **after** the API's own filtering and **before** the
        caller's limit is counted.
    */
    *hash<DataProviderExpression> client;

    #! The options addressing one record, when the predicate pins every part of its identity
    /** Lets a search be answered by reading one record instead of walking a collection.
    */
    *hash<auto> id;
}

#! Splits a search predicate into the part the API can evaluate and the part left for the client side
/** This is the whole of what "hybrid pushdown" means in this family of applications.  A REST collection is
    not a table: the API evaluates a fixed set of filters expressed as query arguments, or in two cases a
    filter expression language, and it evaluates nothing else.  A record search therefore has to do three
    things at once, and doing any of them alone is wrong:

    |!|!
    |Send what the API can evaluate|so that a collection of ten thousand records behind a page of 100 can be searched at all
    |Evaluate the rest as records arrive|so that the answer is the one the caller asked for and not the API's approximation of it
    |Keep reading pages until the caller's limit is met **by matching records**|so that a search does not silently under-return

    The third is the one that is easy to get wrong, and it is not this class's job: it falls out of applying
    the residual predicate in @ref DataProvider::DefaultRecordIterator "DefaultRecordIterator", which counts
    the limit after filtering, over a page iterator that fetches lazily.

    @section restschemarecordfilter_split What Is Split

    Only the conjuncts of a top-level AND are split independently, which is the same rule
    @ref DataProvider::AbstractDataProvider::splitPushdownableExpression() "splitPushdownableExpression()"
    uses and for the same reason: a disjunct of an OR cannot be dropped from the client side just because one
    of its branches was sent to the API.  A table declaring a filter expression language is the exception,
    because there the whole tree goes over the wire.

    @section restschemarecordfilter_dup Two Predicates, One Option

    Two conjuncts on the same field and operator would map to one option and one of them would silently win,
    so the first is sent and the second is left for the client side.  A search for records created after two
    different dates is unusual but it is not an error, and answering it wrongly would be.
*/
public class RestSchemaRecordFilter {
    public {
        #! The generic search operators a record table answers
        /** Everything here is answered; @ref getExpressionInfo() says which of them the API answers itself.
        */
        const SearchExpressions = cast<hash<string, hash<DataProviderExpressionInfo>>>(
            AbstractDataProvider::GenericExpressions{(
                DP_OP_AND,
                DP_OP_OR,
                DP_SEARCH_OP_NOT,
                DP_SEARCH_OP_EQ,
                DP_SEARCH_OP_NE,
                DP_SEARCH_OP_LT,
                DP_SEARCH_OP_LE,
                DP_SEARCH_OP_GT,
                DP_SEARCH_OP_GE,
                DP_SEARCH_OP_BETWEEN,
                DP_SEARCH_OP_IN,
                DP_SEARCH_OP_REGEX,
                DP_OP_STARTS_WITH,
                DP_OP_ENDS_WITH,
                DP_OP_CONTAINS,
            )}
        );
    }

    private {
        #! The table whose predicates are being split
        hash<RestSchemaRecordTableInfo> table;

        #! Filter rules, keyed by field and operator
        hash<string, hash<RestSchemaRecordFilterInfo>> rules;
    }

    #! Creates the object from a record table declaration
    /** @param table the record table
    */
    constructor(hash<RestSchemaRecordTableInfo> table) {
        self.table = table;
        foreach hash<RestSchemaRecordFilterInfo> f in (table.filters) {
            rules{RestSchemaRecordFilter::ruleKey(f.field, f.operator)} = f;
        }
    }

    #! Returns the key a filter rule is registered under
    static string ruleKey(string field, string operator) {
        return field + " " + operator;
    }

    #! Returns @ref True if the table can push any part of a predicate to the API
    bool hasPushdown() {
        return rules.size() > 0 || exists table.query;
    }

    #! Returns @ref True if the API evaluates whole expression trees itself
    bool hasNativeSearch() {
        return exists table.query;
    }

    #! Splits a search predicate
    /** @param where_cond the predicate, as an expression tree or @ref nothing
        @param reserved listing options the caller is already sending, which no predicate may overwrite

        A caller may narrow a listing with its own search options as well as with a predicate, and the two
        can name the same option: a search for closed records among a listing already restricted to open
        ones.  Overwriting the caller's option would answer with records it excluded, so a predicate whose
        option is @a reserved is not pushed down at all and is evaluated client-side instead - which returns
        the intersection, and for that example returns nothing, which is the right answer.

        @return the listing options carrying what the API will evaluate, and the predicate left over
    */
    hash<RestSchemaRecordFilterSplit> split(*hash<auto> where_cond, *hash<string, bool> reserved) {
        hash<RestSchemaRecordFilterSplit> rv = <RestSchemaRecordFilterSplit>{"options": {}};
        if (!where_cond) {
            return rv;
        }
        if (!(where_cond instanceof hash<DataProviderExpression>)) {
            # a table declaring "supports_search_expressions" is always handed an expression tree, but a
            # caller reaching the *Impl() methods directly is not obliged to know that
            return RestSchemaRecordFilter::plainHashSplit(rv, where_cond);
        }
        hash<DataProviderExpression> cond = cast<hash<DataProviderExpression>>(where_cond);
        rv.id = getSingleId(cond);

        list<auto> conjuncts = getConjuncts(cond);
        list<auto> residual = ();
        list<hash<DataProviderExpression>> pending = ();
        hash<string, bool> used = reserved ?? {};
        foreach auto arg in (conjuncts) {
            if (!(arg instanceof hash<DataProviderExpression>)) {
                # a literal conjunct is not a predicate on a field; the client side evaluates it
                residual += arg;
                continue;
            }
            hash<DataProviderExpression> c = cast<hash<DataProviderExpression>>(arg);
            *hash<auto> applied = applyRule(c, \used);
            if (!applied) {
                pending += c;
                continue;
            }
            rv.options += applied.option;
            if (!applied.exact) {
                residual += c;
            }
        }

        if (pending) {
            if (exists table.query && !used{table.query.option}) {
                list<hash<DataProviderExpression>> renderable = ();
                foreach hash<DataProviderExpression> c in (pending) {
                    if (!canRender(c)) {
                        residual += c;
                        continue;
                    }
                    renderable += c;
                    if (!rendersExactly(c)) {
                        # the language can spell this predicate but does not mean quite the same thing by
                        # it, so it narrows what has to be read and is answered here all the same
                        residual += c;
                    }
                }
                if (renderable) {
                    # wrapped because a rendered query is by construction already what the wire takes; an
                    # option carrying a wire codec would otherwise convert it a second time, and Klaviyo's
                    # converts an unrecognised shape to nothing - which drops the filter and answers the
                    # search with the whole collection
                    rv.options{table.query.option} = <RestSchemaRawValue>{
                        "value": renderable.size() == 1
                            ? render(renderable[0])
                            : render(<DataProviderExpression>{"exp": DP_OP_AND, "args": renderable}),
                    };
                }
            } else {
                residual += pending;
            }
        }

        if (residual.size() == 1 && residual[0] instanceof hash<DataProviderExpression>) {
            rv.client = cast<hash<DataProviderExpression>>(residual[0]);
        } else if (residual) {
            rv.client = <DataProviderExpression>{"exp": DP_OP_AND, "args": residual};
        }
        return rv;
    }

    #! Returns the conjuncts of a predicate, flattening nested AND expressions
    /** <tt>(a AND b) AND c</tt> and <tt>a AND b AND c</tt> are the same predicate, and a caller who wrote
        the first should get the same pushdown as one who wrote the second.  Flattening is what makes that
        true, and it is what
        @ref DataProvider::AbstractDataProvider::splitPushdownableExpression() "splitPushdownableExpression()"
        does as well.

        @param cond the predicate
    */
    private list<auto> getConjuncts(hash<DataProviderExpression> cond) {
        if (cond.exp != DP_OP_AND) {
            return (cond,);
        }
        list<auto> rv = ();
        foreach auto arg in (cond.args) {
            if (arg instanceof hash<DataProviderExpression>
                && cast<hash<DataProviderExpression>>(arg).exp == DP_OP_AND) {
                rv += getConjuncts(cast<hash<DataProviderExpression>>(arg));
            } else {
                rv += arg;
            }
        }
        return rv;
    }

    #! Returns the options addressing one record, when the predicate pins every part of its identity
    /** An identity is one value for most tables and more than one for a few - GitHub names a repository by
        its owner **and** its name - so this looks for an equality on **every** part.  Pinning some of them
        is not naming a record, and the search walks the collection as it would for any other predicate.

        The predicate has to be the identity and **nothing else**.  A search for
        <tt>id == 5 AND state == "open"</tt> names a record and also asks a question about it, and reading
        that record without asking the question would answer a search with a record it excluded - which for
        @ref DataProvider::AbstractDataProvider::updateRecords() "updateRecords()" means writing to a record
        the caller did not match.  Such a predicate is answered the ordinary way instead.

        @param cond the predicate

        @return the options addressing the record, or @ref nothing when the predicate does not name one
    */
    private *hash<auto> getSingleId(hash<DataProviderExpression> cond) {
        hash<string, string> parts = RestSchemaTableDataProvider::getIdParts(table);
        list<auto> conjuncts = getConjuncts(cond);
        if (conjuncts.size() != parts.size()) {
            return;
        }
        # the value each identity part is pinned to, keyed by the record field naming that part
        hash<string, auto> found = {};
        foreach auto arg in (conjuncts) {
            if (!(arg instanceof hash<DataProviderExpression>)) {
                return;
            }
            hash<DataProviderExpression> c = cast<hash<DataProviderExpression>>(arg);
            if (c.exp != DP_SEARCH_OP_EQ || c.args.size() != 2) {
                return;
            }
            if (!(c.args[0] instanceof hash<DataProviderFieldReference>)) {
                return;
            }
            if (c.args[1] instanceof hash<DataProviderExpression>
                || c.args[1] instanceof hash<DataProviderFieldReference>) {
                return;
            }
            found{cast<hash<DataProviderFieldReference>>(c.args[0]).field} = c.args[1];
        }
        # "auto!" because a composite identity is a mix of names and numbers, and a hash narrowed to the
        # type of its first part would reject the rest
        hash<auto!> rv = {};
        foreach hash<auto> i in (parts.pairIterator()) {
            if (!found.hasKey(i.value)) {
                return;
            }
            rv{i.key} = found{i.value};
        }
        return rv;
    }

    #! Applies a filter rule to one conjunct, if one matches and its option is still free
    /** @param c the conjunct
        @param used the options already used by this search

        @return the option to send and whether the rule is exact, or @ref nothing if nothing matched
    */
    private *hash<auto> applyRule(hash<DataProviderExpression> c, reference<hash<string, bool>> used) {
        if (!(c.args[0] instanceof hash<DataProviderFieldReference>)) {
            return;
        }
        string field = cast<hash<DataProviderFieldReference>>(c.args[0]).field;
        *hash<RestSchemaRecordFilterInfo> rule = rules{RestSchemaRecordFilter::ruleKey(field, c.exp)};
        if (!rule || used{rule.option}) {
            return;
        }
        auto value;
        if (c.exp == DP_SEARCH_OP_IN) {
            list<auto> values = ();
            for (int j = 1; j < c.args.size(); ++j) {
                if (c.args[j] instanceof hash<DataProviderExpression>
                    || c.args[j] instanceof hash<DataProviderFieldReference>) {
                    return;
                }
                if (c.args[j].typeCode() == NT_LIST) {
                    values += c.args[j];
                } else {
                    values += list(c.args[j]);
                }
            }
            if (!rule.multi) {
                return;
            }
            value = values;
        } else {
            if (c.args.size() != 2) {
                return;
            }
            if (c.args[1] instanceof hash<DataProviderExpression>
                || c.args[1] instanceof hash<DataProviderFieldReference>) {
                # a predicate comparing two fields cannot become a query argument
                return;
            }
            value = c.args[1];
        }
        if (rule.to_wire) {
            value = rule.to_wire(value);
        }
        if (!exists value) {
            return;
        }
        used{rule.option} = True;
        return {"option": {rule.option: value}, "exact": rule.exact};
    }

    #! Returns @ref True if the whole expression can be rendered into the table's query language
    /** @param c the expression
    */
    bool canRender(hash<DataProviderExpression> c) {
        if (!exists table.query) {
            return False;
        }
        hash<RestSchemaQuerySyntaxInfo> syn = table.query.syntax;
        switch (c.exp) {
            case DP_OP_AND:
            case DP_OP_OR:
            case DP_SEARCH_OP_NOT: {
                # a language that does not have a connective cannot render one; the expression stays whole
                # and is evaluated client-side, which is the only answer that is not wrong.  Sending the
                # arguments of an OR joined by anything else would answer with records the caller excluded
                if (c.exp == DP_OP_OR && !syn.or_op.val()) {
                    return False;
                }
                if (c.exp == DP_SEARCH_OP_NOT && !syn.not_op.val()) {
                    return False;
                }
                foreach auto arg in (c.args) {
                    if (!(arg instanceof hash<DataProviderExpression>)) {
                        return False;
                    }
                    if (!canRender(cast<hash<DataProviderExpression>>(arg))) {
                        return False;
                    }
                }
                return True;
            }
        }
        if (!(c.args[0] instanceof hash<DataProviderFieldReference>)) {
            return False;
        }
        string field = cast<hash<DataProviderFieldReference>>(c.args[0]).field;
        if (exists table.query.field_ops) {
            # the finer matrix: this API indexes a field for some comparisons and not others, and answers a
            # filter using one it does not allow on that field with an error rather than with records
            *hash<string, bool> allowed = table.query.field_ops{field};
            if (!allowed{c.exp}
                && !(c.exp == DP_SEARCH_OP_IN && allowed{DP_SEARCH_OP_EQ})) {
                return False;
            }
        } else if (exists table.query.fields && !table.query.fields{field}) {
            return False;
        }
        for (int j = 1; j < c.args.size(); ++j) {
            if (c.args[j] instanceof hash<DataProviderExpression>
                || c.args[j] instanceof hash<DataProviderFieldReference>) {
                return False;
            }
        }
        if (RestSchemaRecordFilter::hasOp(syn, c.exp)) {
            return True;
        }
        # a language with no "in" spells it as a disjunction of equalities, which is exact
        if (c.exp != DP_SEARCH_OP_IN || !RestSchemaRecordFilter::hasOp(syn, DP_SEARCH_OP_EQ)) {
            return False;
        }
        # more than one value needs a disjunction, and a language with no OR cannot write one
        return RestSchemaRecordFilter::getInValues(c).size() < 2 || syn.or_op.val();
    }

    #! Returns every operator the API accepts on some field, or @ref nothing where the table does not say
    /** @ref RestSchemaActions::RestSchemaRecordQueryInfo::field_ops "field_ops" states the matrix per field;
        an operator no field accepts cannot be pushed down whatever the language can spell.
    */
    private *hash<string, bool> getApiOperators() {
        if (!exists table.query.field_ops) {
            return;
        }
        hash<string, bool> rv = {};
        foreach hash<auto> i in (table.query.field_ops.pairIterator()) {
            map rv{$1} = True, keys i.value;
        }
        # an "in" is answered wherever equality is, by expanding it into a disjunction of equalities
        if (rv{DP_SEARCH_OP_EQ}) {
            rv{DP_SEARCH_OP_IN} = True;
        }
        return rv;
    }

    #! Returns @ref True if the syntax can render an operator in any of its three forms
    /** @param syn the query language
        @param op the generic search operator

        @see @ref restschemaquerysyntax_forms
    */
    private static bool hasOp(hash<RestSchemaQuerySyntaxInfo> syn, string op) {
        return exists syn.binary_ops{op} || exists syn.call_ops{op} || exists syn.method_ops{op};
    }

    #! Returns the values of an "in" predicate, flattening a list argument into its elements
    /** @param c the predicate
    */
    private static list<auto> getInValues(hash<DataProviderExpression> c) {
        list<auto> rv = ();
        for (int j = 1; j < c.args.size(); ++j) {
            rv += c.args[j].typeCode() == NT_LIST ? c.args[j] : list(c.args[j]);
        }
        return rv;
    }

    #! Returns @ref True if the rendered query means exactly what the expression means
    /** @param c the expression; must satisfy @ref canRender()

        @see @ref RestSchemaActions::RestSchemaRecordQueryInfo::inexact_ops "inexact_ops"
    */
    bool rendersExactly(hash<DataProviderExpression> c) {
        if (table.query.inexact_ops{c.exp}) {
            return False;
        }
        foreach auto arg in (c.args) {
            if (arg instanceof hash<DataProviderExpression>
                && !rendersExactly(cast<hash<DataProviderExpression>>(arg))) {
                return False;
            }
        }
        return True;
    }

    #! Renders an expression into the table's query language
    /** @param c the expression; must satisfy @ref canRender()

        @return the value to place on the query option.  Normally the rendered query string; a syntax
        declaring @ref RestSchemaActions::RestSchemaQuerySyntaxInfo::render_query "render_query" returns
        whatever that hook returns, which need not be a string - Intercom's search takes a hash

        @throw REST-SCHEMA-RECORD-ERROR the expression cannot be rendered
    */
    auto render(hash<DataProviderExpression> c) {
        if (!exists table.query) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("table %y declares no filter expression language",
                table.name);
        }
        if (*code<auto(hash<DataProviderExpression>)> hook = table.query.syntax.render_query) {
            return hook(c);
        }
        return renderExp(c);
    }

    #! Renders an expression into the table's query language as a string
    /** @param c the expression; must satisfy @ref canRender()

        @return the rendered query

        @throw REST-SCHEMA-RECORD-ERROR the expression cannot be rendered
    */
    private string renderExp(hash<DataProviderExpression> c) {
        hash<RestSchemaQuerySyntaxInfo> syn = table.query.syntax;
        switch (c.exp) {
            case DP_OP_AND:
            case DP_OP_OR: {
                list<string> parts = map renderExp(cast<hash<DataProviderExpression>>($1)), c.args;
                return joinGroup(c.exp == DP_OP_AND, parts);
            }
            case DP_SEARCH_OP_NOT: {
                return syn.not_op + " " + renderExp(cast<hash<DataProviderExpression>>(c.args[0]));
            }
        }
        string field = cast<hash<DataProviderFieldReference>>(c.args[0]).field;
        string wire_field = table.query.field_map{field} ?? field;
        if (syn.render_field) {
            wire_field = syn.render_field(wire_field);
        }
        if (c.exp == DP_SEARCH_OP_IN) {
            # an "in" carries its values as the arguments after the field, and any of them may itself be a
            # list; rendering only the second argument would send a set predicate matching one value
            list<auto> values = RestSchemaRecordFilter::getInValues(c);
            if (RestSchemaRecordFilter::hasOp(syn, DP_SEARCH_OP_IN)) {
                # the language spells set membership itself, and takes the whole set as the value
                return renderPredicate(DP_SEARCH_OP_IN, wire_field, field, values);
            }
            # a language with no "in" spells it as a disjunction of equalities
            list<string> parts = map renderPredicate(DP_SEARCH_OP_EQ, wire_field, field, $1), values;
            return joinGroup(False, parts);
        }
        return renderPredicate(c.exp, wire_field, field, c.args[1]);
    }

    #! Joins the rendered arguments of a connective
    /** @param and_group @ref True for an AND, @ref False for an OR
        @param parts the rendered arguments

        @see @ref restschemaquerysyntax_connectives
    */
    private string joinGroup(bool and_group, list<string> parts) {
        if (parts.size() == 1) {
            return parts[0];
        }
        hash<RestSchemaQuerySyntaxInfo> syn = table.query.syntax;
        string sep = and_group
            ? (syn.and_sep ?? " " + syn.and_op + " ")
            : (syn.or_sep ?? " " + syn.or_op + " ");
        string rv = foldl $1 + sep + $2, parts;
        return syn.parenthesize ? "(" + rv + ")" : rv;
    }

    #! Renders one predicate in whichever of the three forms the syntax declares for its operator
    /** @param op the generic search operator
        @param wire_field the field name as the query language spells it
        @param field the record field, passed to the value renderer
        @param value the value to compare against

        @return the rendered predicate

        @throw REST-SCHEMA-RECORD-ERROR the syntax has no rendering for the operator

        @see @ref restschemaquerysyntax_forms
    */
    private string renderPredicate(string op, string wire_field, string field, auto value) {
        hash<RestSchemaQuerySyntaxInfo> syn = table.query.syntax;
        if (*string method = syn.method_ops{op}) {
            return sprintf("%s.%s(%s)", wire_field, method, syn.render_value(value, field));
        }
        if (*string call = syn.call_ops{op}) {
            return sprintf("%s(%s,%s)", call, wire_field, syn.render_value(value, field));
        }
        *string token = syn.binary_ops{op};
        if (!token.val()) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("the %y query language has no rendering for operator "
                "%y; renderable operators: %y", syn.name, op,
                sort(keys (syn.binary_ops + (syn.call_ops ?? {}) + (syn.method_ops ?? {}))));
        }
        return sprintf("%s %s %s", wire_field, token, syn.render_value(value, field));
    }

    #! Splits a plain search hash, which carries no operators and is therefore all client-side
    private static hash<RestSchemaRecordFilterSplit> plainHashSplit(hash<RestSchemaRecordFilterSplit> rv,
            hash<auto> where_cond) {
        list<hash<DataProviderExpression>> args = map <DataProviderExpression>{
            "exp": DP_SEARCH_OP_EQ,
            "args": (<DataProviderFieldReference>{"field": $1.key}, $1.value),
        }, where_cond.pairIterator();
        rv.client = args.size() == 1 ? args[0] : <DataProviderExpression>{"exp": DP_OP_AND, "args": args};
        return rv;
    }

    #! Returns the expression metadata the table publishes, annotated with what the API evaluates
    /** Every generic search operator is published, because a predicate the API cannot evaluate is still
        answered - on the client side, over the records the listing returns.  What varies by table is
        @ref DataProvider::DataProviderExpressionInfo::supports_pushdown "supports_pushdown", which says
        whether the API can do the work itself.
    */
    hash<string, hash<DataProviderExpressionInfo>> getExpressionInfo() {
        hash<string, bool> pushdown = {};
        map pushdown{$1.operator} = True, table.filters;
        if (exists table.query) {
            hash<RestSchemaQuerySyntaxInfo> syn = table.query.syntax;
            # where the API's matrix is declared per field, an operator the language can spell but no field
            # accepts is not pushed down on this table at all
            *hash<string, bool> api_ops = getApiOperators();
            map pushdown{$1} = True, keys syn.binary_ops, !exists api_ops || api_ops{$1};
            map pushdown{$1} = True, keys syn.call_ops, !exists api_ops || api_ops{$1};
            map pushdown{$1} = True, keys syn.method_ops, !exists api_ops || api_ops{$1};
            pushdown{DP_OP_AND} = True;
            # a connective the language does not have is never pushed down; claiming it here would say the
            # API can answer a search it would in fact have to be answered client-side
            if (syn.or_op.val()) {
                pushdown{DP_OP_OR} = True;
            }
            if (syn.not_op.val()) {
                pushdown{DP_SEARCH_OP_NOT} = True;
            }
            if (RestSchemaRecordFilter::hasOp(syn, DP_SEARCH_OP_EQ)
                && (!exists api_ops || api_ops{DP_SEARCH_OP_IN})) {
                pushdown{DP_SEARCH_OP_IN} = True;
            }
        }
        hash<string, hash<DataProviderExpressionInfo>> rv = {};
        foreach hash<auto> i in (RestSchemaRecordFilter::SearchExpressions.pairIterator()) {
            rv{i.key} = cast<hash<DataProviderExpressionInfo>>(i.value + {
                "supports_pushdown": exists pushdown{i.key},
            });
        }
        return rv;
    }
}
}
