# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchPipelineDataTypeBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! Base data type for pipeline types
public class ElasticSearchPipelineDataTypeBase inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # body parameters
            "description": {
                "type": StringOrNothingType,
                "desc": "The description of the pipeline",
            },
            "on_failure": {
                "type": new Type("*softlist<hash<string, hash<auto>>>"),
                "desc": "A list of zero or more processors to run immediately after a processor failure",
            },
            "processors": {
                "type": new Type("softlist<hash<string, hash<auto>>>"),
                "desc": "Processors used to perform transformations on documents before indexing",
            },
            "version": {
                "type": IntOrNothingType,
                "desc": "Version number used by external systems to track ingest pipelines",
            },
            "_meta": {
                "type": AutoHashOrNothingType,
                "desc": "Optional metadata about the ingest pipeline",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}
}