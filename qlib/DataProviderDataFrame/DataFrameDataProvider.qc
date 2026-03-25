# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataFrameDataProvider class definition

/** DataFrameDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderDataFrame module
public namespace DataProviderDataFrame {

#! Data provider wrapping a DataFrame for record-oriented access
/** Wraps a @ref DataFrame::DataFrame "DataFrame" object as a DataProvider,
    enabling DataFrames to be used in Qorus data pipelines and integrated
    with other DataProvider-based systems.

    @par Example
    @code{.py}
    %requires DataProviderDataFrame

    DataFrame::DataFrame df(
        ({"name": "Alice", "age": 30},
         {"name": "Bob",   "age": 25}));

    DataFrameDataProvider prov(df);
    auto i = prov.searchRecords({"age": {">=": 30}});
    while (i.next()) {
        printf("%y\n", i.getValue());
    }
    @endcode
*/
public class DataFrameDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "dataframe",
            "desc": "DataFrame data provider — wraps a columnar DataFrame as a "
                "record-oriented data source for pipeline integration",
            "type": "DataFrameDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_bulk_read": True,
            "supports_search_expressions": False,
            "supports_create": False,
        };
    }

    private {
        #! The wrapped DataFrame
        DataFrame::DataFrame dataframe;
    }

    #! Creates the provider from a DataFrame
    constructor(DataFrame::DataFrame df) {
        dataframe = df;
    }

    #! Creates the provider from a list of records
    constructor(list<auto> records) {
        dataframe = new DataFrame::DataFrame(records);
    }

    #! Returns the provider name
    string getName() {
        return "dataframe";
    }

    #! Returns the provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the record type based on DataFrame column schema
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        hash<string, AbstractDataField> fields;
        list<string> names = dataframe.columnNames();
        list<string> types = dataframe.dtypes();
        for (int i = 0; i < names.size(); ++i) {
            AbstractDataProviderType dt;
            switch (types[i]) {
                case "int64":
                    dt = AbstractDataProviderTypeMap."int";
                    break;
                case "float64":
                    dt = AbstractDataProviderTypeMap."float";
                    break;
                case "bool":
                    dt = AbstractDataProviderTypeMap."bool";
                    break;
                case "date":
                    dt = AbstractDataProviderTypeMap."date";
                    break;
                default:
                    dt = AbstractDataProviderTypeMap."string";
                    break;
            }
            fields{names[i]} = new QoreDataField(names[i], "", dt);
        }
        return fields;
    }

    #! Returns an iterator over records matching the optional where condition
    private AbstractDataProviderRecordIterator searchRecordsImpl(
            *hash<auto> where_cond, *hash<auto> search_options) {
        return new DataFrameRecordIterator(dataframe, where_cond);
    }

    #! Returns the underlying DataFrame
    DataFrame::DataFrame getDataFrame() {
        return dataframe;
    }
}

#! Iterator over DataFrame records with optional filtering
public class DataFrameRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        #! The records as a list
        list<hash<auto>> records;

        #! Current position (-1 = before first)
        int pos = -1;
    }

    #! Creates the iterator from a DataFrame with optional where conditions
    /** Where conditions use hash format: \c {"column": value} for equality or
        \c {"column": {"op": value}} for comparison operators.
    */
    constructor(DataFrame::DataFrame df, *hash<auto> where_cond) {
        # Apply simple where conditions if provided
        DataFrame::DataFrame filtered = df;
        if (where_cond) {
            HashIterator hi(where_cond);
            while (hi.next()) {
                string col = hi.getKey();
                auto val = hi.getValue();
                if (val.typeCode() == NT_HASH) {
                    # {"col": {">": 30}} style
                    HashIterator oi(val);
                    while (oi.next()) {
                        filtered = filtered.filter(col, oi.getKey(), oi.getValue());
                    }
                } else {
                    # {"col": value} — equality
                    filtered = filtered.filter(col, "==", val);
                }
            }
        }
        records = filtered.toRecords();
    }

    #! Returns True if the iterator is valid
    bool valid() {
        return pos >= 0 && pos < records.size();
    }

    #! Moves to the next record
    private bool nextImpl() {
        return (++pos < records.size());
    }

    #! Returns the current record value
    hash<auto> getValue() {
        return records[pos];
    }
}

}
