# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvHelper class definition

/*  CsvHelper.qm Copyright 2012 - 2024 Qore Technologies, s.r.o.

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

# assume local var scope, do not use "$" for vars, members, and method calls
%modern

class CsvHelper {
    private {
        const C_OPT1 = 0x1;
        const C_OPT2 = 0x2;
        #! supported type codes (hash for quick lookups)
        const Types = {
            "bool": True,
            "*bool": True,
            "int": True,
            "*int": True,
            "float": True,
            "*float": True,
            "number": True,
            "*number": True,
            "string": True,
            "*string": True,
            "date": True,
            "*date": True,
        };

        #! supported field description attribute codes
        const FieldAttrs = ("type", "format", "timezone", "code", "header");

        #! flag to convert all header names to lower case
        bool tolwr = False;

        #! default date->string format
        string date_format;

        #! default {int|float|number}->string format
        string number_format;

        #! hash of field information (types, formats, and possible code), hash key = column name or number (starting with 0)
        hash m_specs;

        #! the exeption \c "err" string
        string errname;

        # reorder data according headers set by options.headers or read from CsvHeader
        bool headerReorder = True;
    }

    #! @param n_errname class name used in exception to identify class
    constructor (string n_errname) {
        errname = n_errname;
    }

    #! Returns the description of the record type, if any
    *hash<string, AbstractDataField> getRecordType() {
        return map {
            $1.key: new QoreDataField($1.key, NOTHING, AbstractDataProviderType::get(
                    new Type(OptimalQoreSoftDataTypeMap{$1.value.type} ?? "string"
                ).getOrNothingType())),
        }, m_specs.firstValue().pairIterator();
    }

    #! returns True if specification hash defines more types
    private bool isMultiType() {
        return m_specs && m_specs.size() > 1;
    }

    #! validate field type
    private checkType(string fld_errs, string key, string value) {
        if (!Types{value})
            throw "CSV-TYPE-ERROR", sprintf("%s: unknown field type %y for field %y (supported types: %y)", fld_errs, value, key, Types.keys());
    }

    # get spec from options.fields for old Csv. Check spec param for new Csv
    private hash getSpec(*hash fields, string fld_errs, int C_OPTx) {
        hash spec = hash();
        if (fields) {
            if (fields.typeCode() != NT_HASH)
                throw errname, sprintf("%s: expecting a hash value; got %y (type %s) instead", fld_errs, fields, fields.type());

            hash checkIdx = hash();
            # iterate hash to process field descriptions
            foreach hash hi in (fields.pairIterator()) {
                hash h;
                h.idx = $#;
                h.header = hi.key;
                switch (hi.value.typeCode()) {
                    case NT_STRING: {
                        checkType(fld_errs, hi.key, hi.value);
                        h.type = hi.value;
                        break;
                    }
                    case NT_HASH: {
                        if (hi.value.empty())
                            throw errname, sprintf("%s: empty hash passed as description for field %y", fld_errs, hi.key);
                        foreach hash fh in (hi.value.pairIterator()) {
                            bool found = True;
                            switch (fh.key) {
                                case "type": {
                                    checkType(fld_errs, hi.key, fh.value);
                                    h.type = fh.value;
                                    break;
                                }
                                case "header":
                                case "format": {
                                    if (fh.value.typeCode() != NT_STRING)
                                        throw errname, sprintf("%s: field %y \"%s\" attributes expects a string value; got %y instead", fld_errs, hi.key, fh.key, fh.value.type());
                                    h{fh.key} = fh.value;
                                    break;
                                }
                                case "timezone": {
                                    h.timezone = new TimeZone(fh.value);
                                    break;
                                }
                                case "code": {
                                    if (!fh.value.callp())
                                        throw errname, sprintf("%s: field %y \"%s\" attributes expects a callable value to process the field value; got %y instead", fld_errs, hi.key, fh.key, fh.value.type());
                                    h.code = fh.value;
                                    break;
                                }
                                default: {
                                    found = False;
                                }
                            }
                            if (!found && C_OPTx == C_OPT2) {
                                found = True;
                                switch (fh.key) {
                                    case "value":
                                    case "default":
                                        h{fh.key} = fh.value;
                                        break;
                                    case "index":
                                        if (int(fh.value) != fh.value)
                                            throw errname, sprintf("%s: field %y \"%s\" does not contain an integer value; %n vs. %n", fld_errs, hi.key, fh.key, int(fh.value), fh.value);
                                        h.idx = int(fh.value);
                                        if (h.idx < 0 || h.idx >= fields.size()) {
                                            throw errname, sprintf("%s: field %y \"%s\" has value %d out off range 0..%d", fld_errs, hi.key, fh.key, h.idx, fields.size()-1);
                                        }
                                        break;
                                    case "regex":
                                        if (fh.value.typeCode() != NT_STRING) {
                                            throw errname, sprintf("%s: field %y \"%s\" attributes expects a string value; got %y instead", fld_errs, hi.key, fh.key, fh.value.type());
                                        }
                                        h{fh.key} = fh.value;
                                        break;
                                    default:
                                        found = False;
                                }
                            }
                            if (!found) {
                                throw errname, sprintf("%s: unknown field attribute value %y given for field %y (supported attribute values: %y)", fld_errs, fh.key, hi.key, FieldAttrs);

                            }
                        }
                        if (!h.type)
                            h.type = "*string";
                        if (h.type != "date" && h.type != "*date") {
                            foreach string f in ("timezone", "format") {
                                if (h{f})
                                    throw errname, sprintf("%s: field %y is type %y, but the %y attribute was also given, which is only valid for \"date\" fields", fld_errs, hi.key, h.type, f);
                            }
                        }
                        break;
                    }
                    default: {
                        throw errname, sprintf("%s: invalid value passed as the field description for field %y; expecting \"string\" or \"hash\"; got %y instead", fld_errs, hi.key, hi.value.type());
                    }
                }
                if (exists checkIdx{h.idx}) {
                    throw errname, sprintf("%s: field %y \"index\" causes index violation %d", fld_errs, hi.key, h.idx);
                }
                if (tolwr) {
                    h.header = h.header.lwr();
                }

                checkIdx{h.idx} = True;
                spec.(hi.key) = h;
            }
        }
        return spec;
    }

    private hash getSpec1(*hash fields) {
        return (CSV_TYPE_SINGLE: getSpec(fields, "while processing the hash value for option key \"fields\"", C_OPT1));
    }

    private hash getSpec2(hash spec) {
        spec = spec ?? (CSV_TYPE_SINGLE: hash());
        foreach string i in (spec.keyIterator()) {
            spec{i} = getSpec(spec{i}, sprintf("while processing the hash value for spec key \"%s\"", i), C_OPT2);
        }
        return spec;
    }

    /** Process headers and add missing field specification, consider reordering fields in specification

        @return a list of field names in order of csv record
    */
    private list<string> adjustFieldsFromHeaders(string type, *list<auto> headers, bool check = False) {
        hash<auto> spec = m_specs{type} ?? {};
        # issue #2179: ensure that all field specifications match headers set
        if (check && spec && headers) {
            #printf("type: %y spec: %y headers: %y\n", type, spec, headers);
            # get spec with actual header names
            hash spec_copy = spec;
            foreach hash h in (spec_copy.pairIterator()) {
                if (h.value.header)
                    spec_copy{h.value.header} = remove spec_copy{h.key};
            }
            *hash h1 = spec_copy - headers;
            if (h1) {
                throw errname, sprintf("field specification for record %y refers to the following fields not "
                    "reflected in the header: %y; known fields: %y", type, keys h1, headers);
            }
        }
        list<string> csv_order = ();
        if (headers && tolwr)
            headers = (map $1.lwr(), headers);

        # look for header name if specified
        hash<auto> fld_by_hdr = {};
        hash<string, int> hdr_idxs = map {$1: $#}, headers;

        m_specs{type} = {};

        foreach string f in (keys spec) {
            fld_by_hdr{spec{f}.header} = f;
            if (!headerReorder) {
                # keep existing spec
                if (exists hdr_idxs{spec{f}.header}) {
                    spec{f}.idx = hdr_idxs{spec{f}.header};
                    m_specs{type}{f} = spec{f};
                    push csv_order, f;
                    remove hdr_idxs{spec{f}.header};
                }
            }
        }

        # change field specificiation to follow header order
        foreach string h in (headers) {
            if (!exists hdr_idxs{h}) {
                # header already in spec
                continue;
            }
            string f = fld_by_hdr{h} ?? h;
            m_specs{type}{f} = spec{f} ?? ("type": "*string", "header": h);
            m_specs{type}{f}.idx = hdr_idxs{h};
            push csv_order, f;
            remove hdr_idxs{h};
        }

        return csv_order;
    }
} # class CsvHelper
