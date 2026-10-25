# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataTypeGraphBuilder class definition


/** DataTypeGraphBuilder.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Describes types with one type table, in which each type with fields and each union type is described once
/** Each type with fields and each union type that an added type refers to, directly or through other types, is
    described once in the type table under a key, and every reference to it is a description that gives the key in
    @ref DataProvider::DataTypeInfo::type_ref "type_ref"; other types are described in place.  Types added later share
    the table: a type that an earlier type already refers to is not described again, so the descriptions of many
    types, such as the option types of all actions of an application, take as much as the distinct types do.

    The keys are \c "1", \c "2", ... in the order in which the types are first referred to, and types and fields
    described in place have an empty \c unique_key, so the same types added in the same order are always described
    identically.

    @ref DataProvider::AbstractDataProviderType::getInfoGraph() "AbstractDataProviderType::getInfoGraph()" describes
    one type with a table of its own.

    @par Example:
    @code{.py}
DataTypeGraphBuilder builder();
hash<string, hash<DataTypeInfo>> options = map {$1.key: builder.add($1.value.type)}, action.options.pairIterator();
hash<string, hash<DataTypeInfo>> types = builder.getTypes();
# restore an option type
AbstractDataProviderType t = AbstractDataProviderType::getFromGraph(<DataTypeGraphInfo>{
    "type": options.customer,
    "types": types,
});
    @endcode

    @note Only one type graph can be described at a time in a thread

    @since DataProvider 3.11
*/
public class DataTypeGraphBuilder {
    private {
        #! The key of each type in the graph, by unique hash
        hash<string, string> key_map = {};

        #! The descriptions of the types with fields and the union types, by key
        hash<string, hash<DataTypeInfo>> types = {};

        #! If simplified type names are returned
        *bool simple;
    }

    #! Creates the object
    /** @param simple if @ref True, the descriptions have simplified type names in \c types_accepted and
        \c types_returned
    */
    constructor(*bool simple) {
        self.simple = simple;
    }

    #! Adds a type to the graph and returns its description
    /** Every type with fields and every union type that the type refers to, directly or through other types, and
        that the table does not describe yet, is described in the table

        @param type the type to describe

        @return the description of the type: a reference if it is a type with fields or a union type, otherwise the
        description of the type, which refers to the types it is made of

        @throw DATA-TYPE-GRAPH-ERROR a type graph is already being described in this thread
    */
    hash<DataTypeInfo> add(AbstractDataProviderType type) {
        if (tgraph) {
            throw "DATA-TYPE-GRAPH-ERROR", "a type graph is already being described in this thread";
        }
        tgraph = <TypeGraphState>{
            "key_map": key_map,
        };
        on_exit remove tgraph;
        # the table is only extended when every new type is described
        hash<string, hash<DataTypeInfo>> new_types = {};
        hash<DataTypeInfo> rv = type.getInfo(simple);
        while (tgraph.queue) {
            hash<TypeGraphItem> item = shift tgraph.queue;
            tgraph.build = item.uh;
            new_types{item.key} = item.type.getInfo(simple);
        }
        key_map = tgraph.key_map;
        types += new_types;
        return rv;
    }

    #! Returns the descriptions of the types with fields and the union types, by key
    hash<string, hash<DataTypeInfo>> getTypes() {
        return types;
    }

    #! Returns a type graph description of a description returned by add()
    /** @param type a description returned by add()

        @return a type graph description with the type table of all types added
    */
    hash<DataTypeGraphInfo> getGraph(hash<DataTypeInfo> type) {
        return <DataTypeGraphInfo>{
            "type": type,
            "types": types,
        };
    }
}
}
