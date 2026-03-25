# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDataProfileProcessor class definition

/** QoreDataProfileProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that accumulates records and produces a data quality profile report
/** Accumulates records in batches and computes a data quality profile for each specified
    field: missing rates, type distribution, value ranges, cardinality, and outlier detection.
    On flush (or when the batch is full), emits one DataProfileFieldInfo event per field.

*/
public class QoreDataProfileProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreDataProfileProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Profile",
                "short_desc": "Which fields to analyze for data quality",
                "desc": "The fields from your data to profile. For each field, the processor "
                    "computes missing rates, type distribution, value ranges, cardinality, and "
                    "outlier counts.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to profile per batch",
                "desc": "How many records to collect before computing a profile report. All "
                    "records in the batch are analyzed together.\n\n"
                    "**Tip**: use at least `50` for meaningful statistics; `200`+ for reliable "
                    "outlier detection",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 1; got %d", window_size);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "data-profile";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Profiles data quality: computes missing rates, type distribution, value ranges, "
            "cardinality, and outlier detection for each field";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits profile when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreDataProfileProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    #! Flushes remaining buffered data
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= 1) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records to produce a data profile
    private list<auto> processWindow(list<hash<auto>> records) {
        int count = records.lsize();
        list<auto> events;

        foreach string field in (fields) {
            # collect values and track types
            int missing_count = 0;
            hash<string, int> type_counts;
            list<auto> non_null_values = ();

            foreach hash<auto> rec in (records) {
                auto val = DataProvider::AbstractDataProvider::getFieldValue(rec, field);
                if (!exists val) {
                    ++missing_count;
                } else {
                    string val_type = val.type();
                    type_counts{val_type} = (type_counts{val_type} ?? 0) + 1;
                    push non_null_values, val;
                }
            }

            # determine most common type
            string most_common_type = "unknown";
            int max_type_count = 0;
            foreach string t in (keys type_counts) {
                if (type_counts{t} > max_type_count) {
                    max_type_count = type_counts{t};
                    most_common_type = t;
                }
            }

            # compute cardinality
            hash<auto> unique_tracker;
            foreach auto v in (non_null_values) {
                unique_tracker{string(v)} = True;
            }
            int cardinality = unique_tracker.size();

            # check if field is numeric
            bool is_numeric = (most_common_type == "integer" || most_common_type == "float");

            *float min_val;
            *float max_val;
            *float mean_val;
            *float std_val;
            *int outlier_count;

            if (is_numeric && non_null_values.lsize() > 0) {
                # compute numeric statistics
                list<float> numeric_values = ();
                foreach auto v in (non_null_values) {
                    push numeric_values, v.toFloat();
                }

                int n = numeric_values.lsize();

                # min and max
                float mn = numeric_values[0];
                float mx = numeric_values[0];
                float total = 0.0;
                foreach float nv in (numeric_values) {
                    if (nv < mn) {
                        mn = nv;
                    }
                    if (nv > mx) {
                        mx = nv;
                    }
                    total += nv;
                }
                min_val = mn;
                max_val = mx;
                mean_val = total / n;

                # standard deviation
                float sq_sum = 0.0;
                foreach float nv in (numeric_values) {
                    float diff = nv - mean_val;
                    sq_sum += diff * diff;
                }
                std_val = sqrt(sq_sum / n);

                # outlier detection via IQR (only for >= 10 values)
                if (n >= 10) {
                    # sort values for quartile computation
                    list<float> sorted = sort(numeric_values);
                    float q1 = sorted[(n * 0.25).toInt()];
                    float q3 = sorted[(n * 0.75).toInt()];
                    float iqr = q3 - q1;
                    float lower_bound = q1 - 1.5 * iqr;
                    float upper_bound = q3 + 1.5 * iqr;

                    int oc = 0;
                    foreach float nv in (numeric_values) {
                        if (nv < lower_bound || nv > upper_bound) {
                            ++oc;
                        }
                    }
                    outlier_count = oc;
                }
            }

            push events, <DataProfileFieldInfo>{
                "event_type": "data-profile",
                "field_name": field,
                "count": count,
                "missing_count": missing_count,
                "missing_rate": missing_count.toFloat() / count,
                "type": most_common_type,
                "cardinality": cardinality,
                "min": min_val,
                "max": max_val,
                "mean": mean_val,
                "std": std_val,
                "outlier_count": outlier_count,
            };
        }

        return events;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns the output type
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::DataProfileFieldInfo"));
    }
}
}
