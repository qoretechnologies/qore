# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveRecordIterators.qc paginating record iterators for the GoogleDriveDataProvider module

/*  GoogleDriveRecordIterators.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleDriveDataProvider module
public namespace GoogleDriveDataProvider {
#! Iterates every file matching a query, fetching each page from Drive as it is reached
/** The \c list-files action answers one page and hands back a page token, which is right for a single API
    call; a record source must not make its consumer do the paginating, so this walks the whole result.

    A page is fetched only when the previous one has been consumed, so a consumer that stops early never pays
    for the pages it did not read.
*/
public class GoogleDriveFilePageIterator inherits Qore::AbstractIterator {
    private {
        #! Performs one listing request
        /** A closure rather than the provider itself: the request plumbing is private to the provider
            hierarchy, and this iterator is not part of it
        */
        code<hash<auto>(*string)> fetch;

        #! The current page of files
        list<auto> page = ();

        #! The position within the current page
        int index = -1;

        #! The token for the next page, if any
        *string token;

        #! Whether any page has been fetched yet
        bool started = False;
    }

    #! Creates the iterator
    /** @param fetch a closure performing one listing request for the given page token
    */
    constructor(code<hash<auto>(*string)> fetch) {
        self.fetch = fetch;
    }

    #! Advances to the next file, fetching the next page when the current one is exhausted
    bool next() {
        if (++index < page.size()) {
            return True;
        }
        if (started && !token.val()) {
            return False;
        }
        fetchPage();
        return index < page.size();
    }

    #! Returns the current file
    auto getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", "the iterator is not pointing at a valid element";
        }
        return page[index];
    }

    #! Returns @ref True if the iterator is positioned on a file
    bool valid() {
        return index >= 0 && index < page.size();
    }

    #! Fetches the next page
    /** @note Drive may return an empty page while still handing back a page token -- a page whose files were
        all filtered out by the query is legitimate -- so the walk continues rather than stopping there
    */
    private fetchPage() {
        while (True) {
            hash<auto> body = fetch(token).body;
            list<auto> files = ();
            foreach hash<auto> f in (body.files) {
                files += GoogleDriveDataProviderBase::getFileInfo(f);
            }
            started = True;
            token = body.nextPageToken;
            page = files;
            index = 0;
            if (files || !token.val()) {
                return;
            }
        }
    }
}
}
