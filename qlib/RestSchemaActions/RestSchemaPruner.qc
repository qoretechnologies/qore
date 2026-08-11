# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/** RestSchemaPruner.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Identifies one operation in a REST schema
public hashdecl SchemaOperationInfo {
    #! The HTTP method; matched case-insensitively and normalized to lower case
    string method;

    #! The path as it appears in the schema, including any \c "{template}" segments
    string path;
}

#! The result of pruning a schema
public hashdecl PruneResultInfo {
    #! The pruned schema
    hash<auto> schema;

    #! The number of paths kept
    int paths_kept;

    #! The number of paths the source schema declared
    int paths_total;

    #! The number of method/path operations kept
    int operations_kept;

    #! The number of distinct local references reachable from the kept operations
    int refs_kept;
}

#! Prunes a REST schema to an explicitly selected set of operations
/** Parsing a large schema dominates load time and memory - Stripe's OpenAPI 3 document declares
    several hundred paths, of which an application typically exposes a couple of dozen - so the
    document an application builds its actions from should contain the operations it selected and
    nothing else.

    @par What is preserved

    The pruned document is a valid schema in its own right:
    - the selected method/path operations, plus each kept path's **path-level** \c parameters, which
      apply to every operation under it and would otherwise be lost;
    - the **complete transitive closure** of local \c "$ref" pointers reachable from those
      operations, computed as a worklist so that a reference chain of any depth is followed and a
      recursive schema terminates;
    - document metadata (\c openapi, \c info, \c servers, \c security, \c tags, …) wholesale;
    - security schemes wholesale, because they are referenced **by name** from \c security rather
      than by \c "$ref", so reachability analysis would wrongly discard them.

    Everything else - every unreferenced component, definition and vendor extension blob - is
    dropped.

    @par What is guaranteed

    - the input document is **never modified**: pruning builds a new document;
    - the output is **deterministic**: members are emitted in the source document's own order, so
      the same input and selection always produce byte-identical output;
    - the output is **self-validated**: a pruning that produced a dangling local reference is a bug
      in this code or a defect in the source document, and either way it throws rather than
      returning a schema that fails later, somewhere less obvious.

    @note This is a structural transformation only.  It never edits a schema to work around an
    upstream defect or an unsupported construct: an invalid document belongs in a normalization step
    of its own, and a valid construct %Qore does not yet support belongs in
    @ref openapi3intro "OpenApi3".
*/
public class RestSchemaPruner {
    public {
        #! Document members kept wholesale rather than pruned to the reachable set
        /** These are metadata rather than reference containers.  \c securityDefinitions (Swagger 2)
            is included because a security scheme is referenced **by name** from \c security, not by
            \c "$ref"; \c components.securitySchemes is handled separately for the same reason.
        */
        const MetadataMembers = {
            "basePath": True,
            "consumes": True,
            "externalDocs": True,
            "host": True,
            "info": True,
            "openapi": True,
            "produces": True,
            "schemes": True,
            "security": True,
            "securityDefinitions": True,
            "servers": True,
            "swagger": True,
            "tags": True,
        };

        #! Component containers kept wholesale rather than pruned to the reachable set
        const WholesaleComponents = {
            "securitySchemes": True,
        };
    }

    #! Prunes a schema to the given operations
    /** @param schema the source schema; not modified
        @param operations the operations to keep

        @return the pruned schema and a summary of what was kept

        @throw REST-SCHEMA-PRUNE-ERROR no operations were given, an operation is not present in the
        schema, or pruning produced a dangling local reference
    */
    static hash<PruneResultInfo> prune(hash<auto> schema, list<hash<SchemaOperationInfo>> operations) {
        if (!operations) {
            throw "REST-SCHEMA-PRUNE-ERROR", "no operations were selected; pruning a schema to "
                "nothing is never intended, so it is an error rather than an empty result";
        }

        # path -> ordered set of lower-case methods to keep
        hash<string, hash<string, bool>> keep = {};
        foreach hash<SchemaOperationInfo> op in (operations) {
            string method = op.method.lwr();
            if (!exists schema.paths{op.path}) {
                throw "REST-SCHEMA-PRUNE-ERROR", sprintf("operation %s %y was selected, but the "
                    "schema declares no such path; %d path(s) are declared", op.method.upr(),
                    op.path, (keys (schema.paths ?? {})).size());
            }
            # membership of the method set, not mere presence of the key: "summary", "servers" and
            # a vendor extension are all members of a path item but none of them is an operation,
            # and accepting one would silently keep nothing while reporting success
            if (!inlist(method, RestSchemaPruner::methodsOf(schema.paths{op.path}))) {
                throw "REST-SCHEMA-PRUNE-ERROR", sprintf("operation %s %y was selected, but the "
                    "schema declares only %y for that path", op.method.upr(), op.path,
                    RestSchemaPruner::methodsOf(schema.paths{op.path}));
            }
            keep{op.path}{method} = True;
        }

        # the kept operations, plus every non-operation member of each kept path item.  Iterated in
        # the SOURCE document's order, not the selection's, so that reordering a manifest cannot
        # change the artifact
        hash<auto> pruned_paths = {};
        int operations_kept = 0;
        foreach string path in (keys schema.paths) {
            if (!keep{path}) {
                continue;
            }
            hash<auto> item = schema.paths{path};
            hash<string, bool> methods = map {$1: True}, RestSchemaPruner::methodsOf(item);
            hash<auto> out = {};
            foreach string key in (keys item) {
                if (!methods{key}) {
                    # a non-operation member of the path item: "parameters", which applies to every
                    # operation under the path and whose loss would silently remove required inputs;
                    # "servers", whose loss would send the request to the wrong host entirely; plus
                    # "summary", "description", "$ref" and any vendor extension
                    out{key} = item{key};
                } else if (keep{path}{key}) {
                    out{key} = item{key};
                    ++operations_kept;
                }
            }
            pruned_paths{path} = out;
        }

        # transitive reference closure, seeded from the kept operations and the kept metadata but
        # NOT from the reference containers themselves -- seeding from those would mark every
        # component reachable and prune nothing
        hash<string, bool> wanted = {};
        RestSchemaPruner::collectRefs(pruned_paths, \wanted);
        foreach hash<auto> i in (schema.pairIterator()) {
            if (i.key != "paths" && MetadataMembers{i.key}) {
                RestSchemaPruner::collectRefs(i.value, \wanted);
            }
        }
        list<string> frontier = keys wanted;
        while (frontier) {
            list<string> next = ();
            foreach string ref in (frontier) {
                hash<string, bool> found = {};
                RestSchemaPruner::collectRefs(RestSchemaPruner::resolveRef(schema, ref), \found);
                foreach string r in (keys found) {
                    if (!wanted{r}) {
                        wanted{r} = True;
                        push next, r;
                    }
                }
            }
            frontier = next;
        }

        # rebuild the document, in the source document's member order
        hash<auto> rv = {};
        foreach hash<auto> i in (schema.pairIterator()) {
            if (i.key == "paths") {
                rv.paths = pruned_paths;
            } else if (MetadataMembers{i.key}) {
                rv{i.key} = i.value;
            } else if (i.key == "components") {
                hash<auto> kept_components = {};
                foreach hash<auto> c in ((i.value ?? {}).pairIterator()) {
                    if (WholesaleComponents{c.key}) {
                        kept_components{c.key} = c.value;
                        continue;
                    }
                    hash<auto> members = RestSchemaPruner::keepReachable(c.value,
                        sprintf("#/components/%s/", c.key), wanted);
                    if (members) {
                        kept_components{c.key} = members;
                    }
                }
                if (kept_components) {
                    rv.components = kept_components;
                }
            } else {
                # any other reference container: a Swagger 2 "definitions" block, or a vendor blob
                # such as "x-webhooks".  Keep only what is reachable, and drop the container
                # entirely when nothing in it is
                hash<auto> members = RestSchemaPruner::keepReachable(i.value,
                    sprintf("#/%s/", i.key), wanted);
                if (members) {
                    rv{i.key} = members;
                }
            }
        }

        # self-validation: a dangling reference means this code dropped something it should have
        # kept, or the source document was already broken
        hash<string, bool> final_refs = {};
        RestSchemaPruner::collectRefs(rv, \final_refs);
        list<string> dangling = ();
        foreach string ref in (keys final_refs) {
            if (!exists RestSchemaPruner::resolveRef(rv, ref)) {
                push dangling, ref;
            }
        }
        if (dangling) {
            throw "REST-SCHEMA-PRUNE-ERROR", sprintf("pruning produced %d dangling local "
                "reference(s), so the result is not a valid schema; the first are: %y",
                dangling.size(), dangling[0..(min(dangling.size(), 3) - 1)]);
        }

        return <PruneResultInfo>{
            "schema": rv,
            "paths_kept": (keys pruned_paths).size(),
            "paths_total": (keys (schema.paths ?? {})).size(),
            "operations_kept": operations_kept,
            "refs_kept": (keys wanted).size(),
        };
    }

    #! Returns the HTTP methods a path item declares, in the order the document declares them
    /** Everything that is not a path-item keyword is a method; that is how OpenAPI models a path
        item, and enumerating a fixed method list instead would silently ignore a method a future
        revision adds.
    */
    static list<string> methodsOf(hash<auto> path_item) {
        list<string> rv = ();
        foreach string key in (keys path_item) {
            if (key == "parameters" || key == "summary" || key == "description"
                || key == "servers" || key == "$ref" || key.equalPartial("x-")) {
                continue;
            }
            push rv, key;
        }
        return rv;
    }

    #! Collects every local \c "$ref" reachable from a node
    /** Only \c "#/..." pointers are collected: an external reference names another document, which
        this transformation neither reads nor rewrites.

        @param node the node to walk
        @param acc a reference to the accumulating set
    */
    static collectRefs(auto node, reference<hash<string, bool>> acc) {
        switch (node.typeCode()) {
            case NT_LIST: {
                foreach auto v in (node) {
                    RestSchemaPruner::collectRefs(v, \acc);
                }
                break;
            }
            case NT_HASH: {
                foreach hash<auto> i in (node.pairIterator()) {
                    if (i.key == "$ref" && i.value.typeCode() == NT_STRING
                        && i.value.equalPartial("#/")) {
                        acc{i.value} = True;
                    } else {
                        RestSchemaPruner::collectRefs(i.value, \acc);
                    }
                }
                break;
            }
        }
    }

    #! Resolves a local JSON-pointer reference against a document
    /** @param root the document
        @param ref a \c "#/..." reference

        @return the referenced node, or @ref nothing if the pointer does not resolve

        @note the pointer's escape sequences are decoded: \c "~1" is a literal \c "/" in a member
        name and \c "~0" a literal \c "~", so a component named \c "a/b" is addressed as
        \c "#/components/schemas/a~1b".  Decoding \c "~1" before \c "~0" would turn \c "~01" into
        \c "/" instead of \c "~1"
    */
    static auto resolveRef(hash<auto> root, string ref) {
        if (!ref.equalPartial("#/")) {
            return;
        }
        auto node = root;
        # NOTE: the third argument of split() is "with_separator", not "keep_trailing_empty" --
        # passing True there returns the tokens with their separators still attached, and every
        # pointer then fails to resolve.  A trailing empty token is a member literally named "",
        # which is a legal JSON pointer, so it is kept
        foreach string raw in (split("/", ref.substr(2), False, True)) {
            string token = raw;
            token =~ s/~1/\//g;
            token =~ s/~0/~/g;
            if (node.typeCode() == NT_HASH) {
                if (!node.hasKey(token)) {
                    return;
                }
                node = node{token};
            } else if (node.typeCode() == NT_LIST) {
                if (token !~ /^[0-9]+$/) {
                    return;
                }
                int index = token.toInt();
                if (index >= node.size()) {
                    return;
                }
                node = node[index];
            } else {
                return;
            }
        }
        return node;
    }

    #! Returns the members of a reference container that the closure reached
    /** @param members the container's members
        @param prefix the reference prefix addressing them, e.g. \c "#/components/schemas/"
        @param wanted the reachable reference set

        @return the reachable members, in the container's own order
    */
    static hash<auto> keepReachable(auto members, string prefix, hash<string, bool> wanted) {
        if (members.typeCode() != NT_HASH) {
            return {};
        }
        hash<auto> rv = {};
        foreach string name in (keys members) {
            # the member name is escaped the same way a pointer to it would be
            string escaped = name;
            escaped =~ s/~/~0/g;
            escaped =~ s/\//~1/g;
            if (wanted{prefix + escaped}) {
                rv{name} = members{name};
            }
        }
        return rv;
    }
}
}
