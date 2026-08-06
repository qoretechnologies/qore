# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxRecordIterators.qc paginating record iterators for the DropboxDataProvider module

/*  DropboxRecordIterators.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! Iterates every entry of a Dropbox listing or search, fetching each page as it is reached
/** Both \c list-folder and \c search return a page with a cursor, and both continue through an endpoint of
    their own.  The actions expose the cursor and leave the walking to the caller, which is right for a single
    API call; a record source must not make its consumer do the paginating, so this walks the whole result.

    A page is fetched only when the previous one has been consumed, so a consumer that stops early never pays
    for the pages it did not read.
*/
public class DropboxEntryPageIterator inherits Qore::AbstractIterator {
    private {
        #! Fetches the first page
        code<hash<auto>()> fetch_first;

        #! Fetches a following page from a cursor
        code<hash<auto>(string)> fetch_next;

        #! Converts one entry into a record, or returns nothing to drop it
        code<*hash<auto>(hash<auto>)> convert;

        #! The current page of entries
        list<auto> page = ();

        #! The position within the current page
        int index = -1;

        #! The cursor for the next page, if any
        *string cursor;

        #! Whether the service reported more pages
        bool has_more = False;

        #! Whether any page has been fetched yet
        bool started = False;
    }

    #! Creates the iterator
    /** @param fetch_first fetches the first page
        @param fetch_next fetches a following page from a cursor
        @param convert converts one entry into a record, or returns @ref nothing to drop it
    */
    constructor(code<hash<auto>()> fetch_first, code<hash<auto>(string)> fetch_next,
            code<*hash<auto>(hash<auto>)> convert) {
        self.fetch_first = fetch_first;
        self.fetch_next = fetch_next;
        self.convert = convert;
    }

    #! Advances to the next entry, fetching the next page when the current one is exhausted
    bool next() {
        if (++index < page.size()) {
            return True;
        }
        if (started && (!has_more || !cursor.val())) {
            return False;
        }
        fetchPage();
        return index < page.size();
    }

    #! Returns the current entry
    auto getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", "the iterator is not pointing at a valid element";
        }
        return page[index];
    }

    #! Returns @ref True if the iterator is positioned on an entry
    bool valid() {
        return index >= 0 && index < page.size();
    }

    #! Fetches the next page
    /** @note A page whose entries are all dropped by the converter -- a search match with no metadata, a
        deletion in a listing -- is legitimate and does not mean the walk is over, so it continues to the next
        page rather than stopping
    */
    private fetchPage() {
        while (True) {
            hash<auto> body = started ? fetch_next(cursor) : fetch_first();
            list<auto> records = ();
            foreach hash<auto> e in (body.entries ?? body.matches) {
                *hash<auto> rec = convert(e);
                if (rec) {
                    records += rec;
                }
            }
            started = True;
            cursor = body.cursor;
            has_more = body.has_more ?? False;
            page = records;
            index = 0;
            if (records || !has_more || !cursor.val()) {
                return;
            }
        }
    }
}
}
