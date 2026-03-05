# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreCorrelationProcessor class definition

/** QoreCorrelationProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Pipeline processor that computes Pearson correlation between multiple numeric fields
/** Accumulates records in windows and computes the Pearson correlation coefficient
    between all pairs of configured fields when the window is full or on flush.

    @since %DataProvider 3.3
*/
public class QoreCorrelationProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreCorrelationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field Names",
                "short_desc": "Numeric field names (at least 2)",
                "desc": "A list of numeric field names to compute correlations "
                    "between; at least 2 fields are required; "
                    "supports dot-notation for nested fields (e.g. `sensor.temperature`)",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per correlation window",
                "desc": "The number of records to accumulate before computing the correlation "
                    "matrix; the window is cleared after each computation.\n\n"
                    "**Sizing guidance**: correlation needs enough data for stable results; "
                    "use at least `30` for basic correlation, `100`+ for reliable results",
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

        #! Accumulated records (list of hashes)
        list<hash<auto>> buffer;

        #! Window counter
        int window_count = 0;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        fields = AbstractDataProvider::parseFieldList(copts.fields);
        if (fields.lsize() < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("at least 2 fields are required for correlation; got %d", fields.lsize());
        }
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2 for correlation; got %d", window_size);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "correlation";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes Pearson correlation coefficients between all pairs of configured "
            "numeric fields over a window of records; emits a correlation matrix when the "
            "window is full or on flush";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits correlation matrix when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR", "QoreCorrelationProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            hash<auto> result = computeCorrelation(buffer);
            buffer = ();
            return result;
        }
        return;
    }

    #! Flushes remaining buffered data
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= 2) {
            hash<auto> result = computeCorrelation(buffer);
            buffer = ();
            return (result,);
        }
        buffer = ();
        return;
    }

    #! Computes correlation matrix from buffered records
    private hash<auto> computeCorrelation(list<hash<auto>> records) {
        window_count++;
        int n = records.lsize();

        # extract values per field
        hash<string, list<float>> field_values;
        foreach string f in (fields) {
            field_values{f} = ();
        }
        foreach hash<auto> rec in (records) {
            foreach string f in (fields) {
                auto val = AbstractDataProvider::getFieldValue(rec, f);
                push field_values{f}, exists val ? val.toFloat() : 0.0;
            }
        }

        # compute means
        hash<string, float> means;
        foreach string f in (fields) {
            float sum = 0.0;
            foreach float v in (field_values{f}) {
                sum += v;
            }
            means{f} = sum / n;
        }

        # compute stddevs
        hash<string, float> stddevs;
        foreach string f in (fields) {
            float sum_sq = 0.0;
            foreach float v in (field_values{f}) {
                float diff = v - means{f};
                sum_sq += diff * diff;
            }
            stddevs{f} = sqrt(sum_sq / (n - 1));
        }

        # compute correlation matrix
        hash<auto> matrix;
        hash<auto> correlations;
        for (int i = 0; i < fields.lsize(); ++i) {
            string fi = fields[i];
            matrix{fi} = {};
            for (int j = 0; j < fields.lsize(); ++j) {
                string fj = fields[j];
                float r;
                if (i == j) {
                    r = 1.0;
                } else if (stddevs{fi} == 0.0 || stddevs{fj} == 0.0) {
                    r = 0.0;
                } else {
                    float cov = 0.0;
                    for (int k = 0; k < n; ++k) {
                        cov += (field_values{fi}[k] - means{fi})
                            * (field_values{fj}[k] - means{fj});
                    }
                    cov /= (n - 1);
                    r = cov / (stddevs{fi} * stddevs{fj});
                }
                matrix{fi}{fj} = r;
                if (i < j) {
                    correlations{fi + "_" + fj} = r;
                }
            }
        }

        return <CorrelationEventInfo>{
            "event_type": "correlation",
            "window_size": window_size,
            "window_count": window_count,
            "correlations": correlations,
            "matrix": matrix,
            "count": n,
        };
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
    private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::CorrelationEventInfo"));
    }
}
}
