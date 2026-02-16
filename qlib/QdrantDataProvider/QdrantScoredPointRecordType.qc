# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantScoredPointRecordType class definition

/** QdrantScoredPointRecordType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Shared record type for Qdrant scored point results
/** Defines the fields returned by Qdrant query/search endpoints for each scored point
*/
public class QdrantScoredPointRecordType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": AutoType,
                "display_name": "ID",
                "short_desc": "Point ID",
                "desc": "The point ID (integer or UUID string)",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "Point version",
                "desc": "The point version number",
            },
            "score": {
                "type": FloatOrNothingType,
                "display_name": "Score",
                "short_desc": "Similarity score",
                "desc": "The similarity score for the point",
            },
            "payload": {
                "type": AutoHashOrNothingType,
                "display_name": "Payload",
                "short_desc": "Point payload",
                "desc": "The point payload data",
            },
            "vector": {
                "type": AutoType,
                "display_name": "Vector",
                "short_desc": "Vector data",
                "desc": "The vector data if requested",
            },
            "shard_key": {
                "type": AutoType,
                "display_name": "Shard Key",
                "short_desc": "Shard key",
                "desc": "The shard key if applicable",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
