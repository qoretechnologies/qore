# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaRecordPageIterator.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! Walks a record table's collection one page at a time, fetching each page only when it is reached
/** Laziness is the whole point of this class.  A record search counts the caller's limit in **matching**
    records, and a predicate the API cannot evaluate is applied to each record as it arrives, so how far into
    a collection a search has to read is not known before it runs.

    An eager reader has to choose between two wrong answers: stop at the limit and under-return, because the
    records it stopped short of are the matching ones; or read the whole collection every time, because it
    cannot know it has enough.  A lazy one has neither problem - it is asked for exactly as many records as
    the filter needs to see, and no more, by whatever is counting the limit.

    @note The records this iterator returns are already in the table's own shape, so that a predicate is
    evaluated against the record the caller sees rather than against the API's response.
*/
public class RestSchemaRecordPageIterator inherits Qore::AbstractIterator {
    private {
        #! The provider executing the listing action
        RestSchemaActionDataProvider lister;

        #! The table being read
        hash<RestSchemaRecordTableInfo> table;

        #! The options sent with every page: the collection's scope, the API-side filter, and any sort
        hash<auto> options;

        #! The records of the page currently being read
        list<hash<auto>> page = ();

        #! The position within @ref page; -1 before the first record
        int index = -1;

        #! The number of the next page to read
        int next_page;

        #! The offset of the next page, for @ref RSRP_OFFSET
        int next_offset = 0;

        #! The cursor identifying the next page, for @ref RSRP_CURSOR and @ref RSRP_AFTER_ID
        *string next_cursor;

        #! Set once the collection is known to be exhausted
        bool exhausted = False;

        #! Set once the first page has been requested
        bool started = False;
    }

    #! Creates the object from the arguments
    /** @param lister the provider executing the listing action
        @param table the table being read
        @param options the options sent with every page, excluding paging
    */
    constructor(RestSchemaActionDataProvider lister, hash<RestSchemaRecordTableInfo> table,
            *hash<auto> options) {
        self.lister = lister;
        self.table = table;
        self.options = options ?? {};
        next_page = table.paging.first_page;
    }

    #! Moves to the next record, reading the next page when the current one runs out
    /** @return @ref False when the collection is exhausted
    */
    bool next() {
        while (True) {
            ++index;
            if (index < page.size()) {
                return True;
            }
            if (exhausted || !readNextPage()) {
                # leave the iterator invalid rather than restarting: a collection walked twice would issue
                # every request again, and a caller that kept the iterator alive has no reason to expect that
                page = ();
                index = 0;
                exhausted = True;
                return False;
            }
            index = -1;
        }
    }

    #! Returns @ref True if the iterator is pointing at a record
    bool valid() {
        return index >= 0 && index < page.size();
    }

    #! Returns the record the iterator is pointing at
    /** @throw INVALID-ITERATOR the iterator is not pointing at a record
    */
    hash<auto> getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", sprintf("the iterator over table %y is not pointing at a record",
                table.name);
        }
        return page[index];
    }

    #! Reads the next page and returns @ref True if it has any records
    private bool readNextPage() {
        if (started && table.paging.mode == RSRP_NONE) {
            return False;
        }
        hash<auto> resp_opts = options + getPagingOptions();
        started = True;
        auto resp = lister.doRequest(resp_opts);
        # the records are put into the table's shape before the next page is worked out, because with
        # @ref RSRP_AFTER_ID the cursor is the last record's identity and that is a **table** field: a table
        # publishing a projection names it in its own terms, which need not be the service's
        list<hash<auto>> records = map formatRecord($1), extractRecords(resp);
        advance(resp, records);
        if (!records) {
            # an empty page ends the walk whatever the API says about there being more, because reading
            # further would repeat the same request forever
            exhausted = True;
            return False;
        }
        page = records;
        return True;
    }

    #! Returns the paging options for the next request
    private hash<auto> getPagingOptions() {
        # "auto!" because a page size is an integer and a cursor is a string, and a hash narrowed to the type
        # of its first value would reject the other
        hash<auto!> rv = {};
        hash<RestSchemaRecordPagingInfo> p = table.paging;
        if (p.size_option.val() && exists p.max_size) {
            rv{p.size_option} = p.max_size;
        }
        if (!p.page_option.val()) {
            return rv;
        }
        switch (p.mode) {
            case RSRP_PAGE:
                rv{p.page_option} = next_page;
                break;
            case RSRP_OFFSET:
                rv{p.page_option} = next_offset;
                break;
            case RSRP_CURSOR:
            case RSRP_AFTER_ID:
                if (next_cursor.val()) {
                    rv{p.page_option} = next_cursor;
                }
                break;
        }
        return rv;
    }

    #! Records where the page after this one begins, and whether there is one
    /** @param resp the listing response
        @param records the records read from it
    */
    private advance(auto resp, list<hash<auto>> records) {
        hash<RestSchemaRecordPagingInfo> p = table.paging;
        ++next_page;
        next_offset += records.size();
        switch (p.mode) {
            case RSRP_NONE:
                exhausted = True;
                return;
            case RSRP_CURSOR: {
                auto value = RestSchemaRecordPageIterator::followPath(resp, p.next_path);
                next_cursor = p.next_cursor ? p.next_cursor(value) : (exists value ? value.toString() : NOTHING);
                if (!next_cursor.val()) {
                    exhausted = True;
                    return;
                }
                break;
            }
            case RSRP_AFTER_ID: {
                if (records) {
                    auto id = records.last(){table.id_field};
                    next_cursor = exists id ? id.toString() : NOTHING;
                }
                if (!next_cursor.val()) {
                    exhausted = True;
                    return;
                }
                break;
            }
        }
        if (exists p.has_more_path) {
            # where the API says whether more records follow, it is authoritative: a full last page is
            # otherwise indistinguishable from a page with more after it
            auto more = RestSchemaRecordPageIterator::followPath(resp, p.has_more_path);
            if (!more.toBool()) {
                exhausted = True;
            }
            return;
        }
        if (exists p.max_size && records.size() < p.max_size) {
            exhausted = True;
        }
    }

    #! Returns the records in a listing response
    private list<hash<auto>> extractRecords(auto resp) {
        auto node = RestSchemaRecordPageIterator::followPath(resp, table.records_path);
        if (!exists node) {
            return ();
        }
        if (node.typeCode() != NT_LIST) {
            throw "REST-SCHEMA-RECORD-ERROR", sprintf("the listing behind table %y returned type %y where a "
                "list of records was expected (records path: %y)", table.name, node.type(),
                table.records_path);
        }
        list<hash<auto>> rv = ();
        foreach auto rec in (node) {
            if (rec.typeCode() != NT_HASH) {
                throw "REST-SCHEMA-RECORD-ERROR", sprintf("the listing behind table %y returned type %y as "
                    "record %d where a hash was expected", table.name, rec.type(), $#);
            }
            rv += rec;
        }
        return rv;
    }

    #! Converts a record from the API's shape into the table's
    private hash<auto> formatRecord(hash<auto> rec) {
        return table.to_record ? table.to_record(rec) : rec;
    }

    #! Returns the value at a path of field names, or the value itself when the path is empty
    /** @param value the value to walk into
        @param path the field names to follow

        @return the value at the path, or @ref nothing if the path does not resolve
    */
    static auto followPath(auto value, *softlist<string> path) {
        foreach string name in (path) {
            if (!exists value) {
                return;
            }
            if (value.typeCode() != NT_HASH) {
                return;
            }
            value = value{name};
        }
        return value;
    }
}
}
