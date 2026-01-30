# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeneratorGenerateFromTemplateDataProvider class definition

/** GeneratorGenerateFromTemplateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeneratorDataProvider module
public namespace GeneratorDataProvider {

#! Data provider for the generate-from-template action
/** Generates a record stream by varying a template record with optional randomization for testing data pipelines.

    Generation parameters are set via constructor options; records are read via the record-based read API
    (searchRecords / searchRecordsBulk / getBulkRecordInterface).
*/
public class GeneratorGenerateFromTemplateDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options for configuring generation
        const ConstructorOptions = {
            "template": <DataProviderOptionInfo>{
                "display_name": "Template Record",
                "short_desc": "A sample record to use as a template",
                "desc": "A hash representing a single record whose structure and values will be used as the "
                    "basis for all generated records; field types are inferred from the template values; each "
                    "generated record varies the template values based on the record index or randomly",
                "type": AbstractDataProviderTypeMap."hash",
                "required": True,
            },
            "count": <DataProviderOptionInfo>{
                "display_name": "Record Count",
                "short_desc": "Number of records to generate",
                "desc": "The total number of records to generate from the template; must be a non-negative "
                    "integer; defaults to 10",
                "type": AbstractDataProviderTypeMap."int",
                "required": False,
                "default_value": 10,
            },
            "strategy": <DataProviderOptionInfo>{
                "display_name": "Generation Strategy",
                "short_desc": "How template values are varied",
                "desc": "The strategy for varying template values across generated records; 'sequential' adds "
                    "the record index to numeric values and appends it as a suffix to strings; 'random' "
                    "applies random variations; defaults to 'sequential'",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": GeneratorStrategy::Sequential,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": GeneratorStrategy::Sequential,
                        "desc": "Vary template values deterministically by record index",
                    },
                    <AllowedValueInfo>{
                        "value": GeneratorStrategy::Random,
                        "desc": "Apply random variations to template values",
                    },
                ),
            },
            "include_nulls": <DataProviderOptionInfo>{
                "display_name": "Include Nulls",
                "short_desc": "Whether to randomly include null values",
                "desc": "When True and strategy is 'random', approximately 10% of field values will be set to "
                    "NOTHING (null); useful for testing null handling in data pipelines; defaults to False",
                "type": AbstractDataProviderTypeMap."bool",
                "default_value": False,
            },
            "seed": <DataProviderOptionInfo>{
                "display_name": "Random Seed",
                "short_desc": "Seed for reproducible random generation",
                "desc": "An integer seed for the random number generator for reproducible output; only "
                    "effective with 'random' strategy",
                "type": AbstractDataProviderTypeMap."int",
            },
            "base_date": <DataProviderOptionInfo>{
                "display_name": "Base Date",
                "short_desc": "Fixed base date for date field generation",
                "desc": "A fixed date/time value used as the reference point for generating date fields; "
                    "when provided with a seed, this ensures full cross-run reproducibility of date "
                    "fields; if not provided, the current date/time at the start of generation is used",
                "type": AbstractDataProviderTypeMap."date",
            },
        };

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate-from-template",
            "desc": "Generate a record stream by repeating and varying a template record with optional "
                "randomization for testing data pipelines",
            "type": "GeneratorGenerateFromTemplateDataProvider",
            "supports_read": True,
            "supports_bulk_read": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": GenericRecordSearchOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Template record
        hash<auto> template_record;

        #! Inferred field definitions
        list<auto> fields;

        #! Record count
        int count = 10;

        #! Generation strategy
        string strategy = GeneratorStrategy::Sequential;

        #! Include nulls flag
        bool include_nulls = False;

        #! Random seed
        *int seed;

        #! Base date for date generation
        *date base_date;
    }

    #! Creates the object
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (!exists copts.template) {
            throw "CONSTRUCTOR-ERROR", "the 'template' option is required";
        }
        template_record = copts.template;

        # Infer field types from template values
        fields = ();
        foreach hash<auto> pair in (template_record.pairIterator()) {
            fields += {"name": pair.key, "type": inferType(pair.value)};
        }

        if (exists copts.count) {
            if (copts.count < 0) {
                throw "CONSTRUCTOR-ERROR", sprintf("'count' must be a non-negative integer; got %d", copts.count);
            }
            count = copts.count;
        }
        if (exists copts.strategy) {
            strategy = copts.strategy;
        }
        if (exists copts.include_nulls) {
            include_nulls = copts.include_nulls;
        }
        seed = copts.seed;
        base_date = copts.base_date;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Infers a GeneratorFieldType string from a Qore runtime value
    static string inferType(auto val) {
        switch (val.type()) {
            case "integer":
                return GeneratorFieldType::Int;
            case "float":
            case "number":
                return GeneratorFieldType::Float;
            case "bool":
                return GeneratorFieldType::Bool;
            case "date":
                return GeneratorFieldType::Date;
            case "string":
            case "NULL":
                return GeneratorFieldType::String;
        }
        throw "TEMPLATE-ERROR", sprintf("unsupported template value type %y; supported types: string, int, "
            "float, bool, date", val.type());
    }

    #! Creates a record iterator for the configured generation parameters
    private GeneratorRecordIterator makeIterator() {
        return new GeneratorRecordIterator(count, fields, strategy, include_nulls, seed, template_record,
            base_date);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond ignored; generation does not filter
        @param search_options search options (supports generic options like \c max_records and \c limit)

        @return a record iterator that generates the configured number of records
    */
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return makeIterator();
    }

    #! Returns the record type for the generated records
    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return makeIterator().getRecordType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
