# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantCollectionBulkOperation class definition

/** QdrantCollectionBulkOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! Bulk insert/upsert operation for a Qdrant collection
/** Buffers point records and flushes them as batch upserts via `PUT /collections/{name}/points`.

    This class uses a row-oriented buffer (list of point hashes) rather than the column-oriented buffer
    from the base class, because Qdrant point records contain list-valued fields (vectors) that would be
    misinterpreted as column data by the column-oriented buffering logic.
*/
public class QdrantCollectionBulkOperation inherits DataProvider::AbstractDataProviderBulkOperation {
    private {
        #! The REST client for Qdrant
        RestClientIo::RestClientIo rest;

        #! Pre-built URI for the upsert endpoint
        string uri;

        #! Row-oriented record buffer
        list<hash<auto>> row_buf;

        #! Block size for flushing
        int row_block_size;

        #! Record count
        int row_count = 0;
    }

    #! Creates the bulk operation object
    /** @param provider the data provider for the operation
        @param rest the REST client for Qdrant
        @param collection_name the collection name for the bulk operations
        @param block_size the block size for bulk operations (default: 1000)
    */
    constructor(DataProvider::AbstractDataProvider provider, RestClientIo::RestClientIo rest, string collection_name,
            *int block_size)
            : AbstractDataProviderBulkOperation(provider, "upsert", block_size ?? DefaultBlockSize) {
        self.rest = rest;
        uri = sprintf("/collections/%s/points?wait=true", encode_url(collection_name, True));
        row_block_size = block_size ?? DefaultBlockSize;
    }

    #! Queues a single record for bulk upsert
    /** @param rec the record to queue; must contain `id` and `vector`, optionally `payload`
    */
    queueData(hash<auto> rec) {
        push row_buf, rec;
        if (row_buf.size() >= row_block_size) {
            flushRows();
        }
    }

    #! Queues multiple records for bulk upsert
    /** @param recs a list of records to queue
    */
    queueData(list<hash<auto>> recs) {
        foreach hash<auto> rec in (recs) {
            push row_buf, rec;
            if (row_buf.size() >= row_block_size) {
                flushRows();
            }
        }
    }

    #! Flushes any remaining buffered data
    flush() {
        if (row_buf) {
            flushRows();
        }
    }

    #! Discards any buffered data
    discard() {
        delete row_buf;
    }

    #! Returns the record count
    int getRecordCount() {
        return row_count;
    }

    #! Flushes row buffer to Qdrant
    private flushRows() {
        if (!row_buf) {
            return;
        }

        list<hash<auto>> points;
        foreach hash<auto> rec in (row_buf) {
            hash<auto> point = {"id": rec.id, "vector": rec.vector};
            if (rec.payload) {
                point.payload = rec.payload;
            }
            push points, point;
        }

        rest.restPut(uri, {"points": points});

        int bs = row_buf.size();
        row_count += bs;
        delete row_buf;
    }

    #! Not used — overridden by row-oriented methods above
    private flushImpl() {
    }
}
}
