# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3RecordIterators.qc paginating record iterators for the AwsS3DataProvider module

/*  AwsS3RecordIterators.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AwsS3DataProvider module
public namespace AwsS3DataProvider {
#! Iterates every object under a prefix, fetching each page from Amazon S3 as it is reached
/** A bucket listing is capped at 1000 keys per response, so anything larger has to be walked with a
    continuation token.  The \c list-objects action exposes that token and leaves the walking to the caller,
    which is right for a single API call but wrong for a record source: a pipeline reading a bucket should not
    have to know how Amazon paginates.

    This iterator does the walking.  A page is fetched only when the previous one has been consumed, so a
    consumer that stops early -- because a \c limit was reached, or because it found what it wanted -- never
    pays for the pages it did not read.

    @note Amazon S3 returns keys in lexical order and guarantees nothing about objects created or removed
    while a listing is in progress; an object added behind the cursor may be missed, and one added ahead of it
    may appear.  That is a property of the service rather than of this iterator.
*/
public class AwsS3ObjectPageIterator inherits Qore::AbstractIterator {
    private {
        #! Performs one listing request
        /** A closure rather than the provider itself: the request plumbing is private to the provider
            hierarchy, and this iterator is not part of it
        */
        code<hash<auto>(hash<auto> query)> fetch;

        #! The bucket being listed
        string bucket;

        #! The query arguments applied to every page
        hash<auto> base_query;

        #! The current page of objects
        list<auto> page = ();

        #! The position within the current page
        int index = -1;

        #! The continuation token for the next page, if any
        *string token;

        #! Whether any page has been fetched yet
        bool started = False;
    }

    #! Creates the iterator
    /** @param fetch a closure performing one listing request against the bucket
        @param bucket the bucket to list
        @param base_query the query arguments applied to every page
    */
    constructor(code<hash<auto>(hash<auto> query)> fetch, string bucket, hash<auto> base_query) {
        self.fetch = fetch;
        self.bucket = bucket;
        self.base_query = base_query;
    }

    #! Advances to the next object, fetching the next page when the current one is exhausted
    bool next() {
        if (++index < page.size()) {
            return True;
        }
        # the listing is finished once a page came back without a continuation token
        if (started && !token.val()) {
            return False;
        }
        fetchPage();
        return index < page.size();
    }

    #! Returns the current object
    auto getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", "the iterator is not pointing at a valid element";
        }
        return page[index];
    }

    #! Returns @ref True if the iterator is positioned on an object
    bool valid() {
        return index >= 0 && index < page.size();
    }

    #! Fetches the next page
    /** @note An empty page that still carries a continuation token is legitimate -- Amazon S3 may return one
        when a prefix and delimiter exclude everything in a range -- so the walk continues rather than stopping
        at the first empty page
    */
    private fetchPage() {
        while (True) {
            hash<auto!> query = base_query;
            if (token.val()) {
                query."continuation-token" = token;
            }
            hash<auto> resp = fetch(query);
            *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListBucketResult;

            list<auto> objects = ();
            foreach hash<auto> obj in (AwsS3DataProviderBase::forceList(doc.Contents)) {
                objects += {
                    "key": obj.Key,
                    "size": obj.Size.val() ? obj.Size.toInt() : NOTHING,
                    "last_modified": obj.LastModified.val() ? date(obj.LastModified) : NOTHING,
                    "etag": AwsS3ListObjectsDataProvider::stripEtag(obj.ETag),
                    "storage_class": obj.StorageClass,
                    "bucket": bucket,
                };
            }

            started = True;
            bool truncated = doc.IsTruncated == "true" || doc.IsTruncated === True;
            token = truncated ? doc.NextContinuationToken : NOTHING;
            page = objects;
            index = 0;

            if (objects || !token.val()) {
                return;
            }
        }
    }
}
}
