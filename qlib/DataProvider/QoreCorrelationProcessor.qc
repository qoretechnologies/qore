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

    A field given more than once in \c fields is one field.  The correlation of each pair is given in the event under
    the key of its two field paths joined with \c _ (@ref CorrelationEventInfo); two pairs that would give the same
    key, such as \c a with \c b_c and \c a_b with \c c, are refused when the processor is created.

    @since %DataProvider 3.3
*/
public class QoreCorrelationProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "type": "QoreCorrelationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const hash<string, hash<DataProviderOptionInfo>> ConstructorOptions = {
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

        #! Preferred shape for pending flush output
        DataProviderShape pending_output_shape = DataProviderShape::RowRecords;

        #! True if the pending output shape has been set for buffered records
        bool pending_output_shape_set = False;

        #! True while a shaped bulk block is dispatching through processRecordImpl()
        bool processing_shaped_bulk = False;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        # a field given twice is one field
        hash<string, bool> given;
        foreach string field in (AbstractDataProvider::parseFieldList(
                AbstractDataProvider::getFieldListOption(options, copts, "fields"))) {
            if (!given{field}) {
                push fields, field;
                given{field} = True;
            }
        }
        AbstractDataProvider::checkFieldPathOption("CONSTRUCTOR-ERROR", "fields", fields);
        if (fields.lsize() < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("at least 2 fields are required for correlation; got %d", fields.lsize());
        }
        QoreCorrelationProcessor::checkCorrelationKeys(fields);
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2 for correlation; got %d", window_size);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the key of the correlation of two fields in the event: their field paths joined with \c _
    private static string getCorrelationKey(string field_a, string field_b) {
        return field_a + "_" + field_b;
    }

    #! Checks that each pair of fields gives its own key of the correlations in the event
    /** The key of a pair is its two field paths joined with \c _, so two pairs can give the same key: the fields
        \c a, \c a_b, \c b_c and \c c give \c a_b_c for \c a with \c b_c and for \c a_b with \c c, and the
        correlation of one pair would replace the other

        @param fields the field paths, each given once, in the order given

        @throw CONSTRUCTOR-ERROR two pairs of fields give the same key; the message names both pairs
    */
    private static checkCorrelationKeys(list<string> fields) {
        # key -> the pair that gives it
        hash<string, list<string>> pairs;
        for (int i = 0; i < fields.lsize(); ++i) {
            for (int j = i + 1; j < fields.lsize(); ++j) {
                string key = QoreCorrelationProcessor::getCorrelationKey(fields[i], fields[j]);
                *list<string> pair = pairs{key};
                if (pair) {
                    # the paths are given as they are written
                    throw "CONSTRUCTOR-ERROR", sprintf("option \"fields\": the pairs of fields \"%s\" and \"%s\", and "
                        "\"%s\" and \"%s\", both give the correlation key \"%s\", so one correlation would replace the "
                        "other; remove one of these fields from the option", pair[0], pair[1], fields[i], fields[j],
                        key);
                }
                pairs{key} = (fields[i], fields[j]);
            }
        }
    }

    #! Records the shape of accumulated input for shape-preserving flushes
    private updatePendingOutputShape(DataProviderShape shape) {
        if (!pending_output_shape_set) {
            pending_output_shape = cast<enum<DataProviderShape>>(shape);
            pending_output_shape_set = True;
            return;
        }
        if (pending_output_shape != shape) {
            pending_output_shape = cast<enum<DataProviderShape>>(DataProviderShape::RowRecords);
        }
    }

    #! Clears pending flush shape state
    private clearPendingOutputShape() {
        pending_output_shape = cast<enum<DataProviderShape>>(DataProviderShape::RowRecords);
        pending_output_shape_set = False;
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
        if (!processing_shaped_bulk) {
            updatePendingOutputShape(DataProviderShape::RowRecords);
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            hash<auto> result = computeCorrelation(buffer);
            buffer = ();
            return result;
        }
        return;
    }

    #! Builds remaining buffered results
    private *list<auto> buildFlushRows() {
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

    #! Flushes remaining buffered data as row records for direct callers
    private *list<auto> flushRecordsImpl() {
        *list<auto> results = buildFlushRows();
        clearPendingOutputShape();
        return results;
    }

    #! Flushes remaining buffered data in the negotiated pipeline shape
    private flushImpl(code enqueue) {
        *list<auto> results = buildFlushRows();
        DataProviderShape output_shape = pending_output_shape;
        clearPendingOutputShape();
        AbstractDataProcessor::emitRecordListShape(results, output_shape, enqueue);
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
                    correlations{QoreCorrelationProcessor::getCorrelationKey(fi, fj)} = r;
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

    #! Processes records in a given bulk shape
    private *hash<auto> processRecordsBulkShape(hash<auto> records, DataProviderShape shape) {
        updatePendingOutputShape(shape);
        list<auto> results;
        processing_shaped_bulk = True;
        on_exit {
            processing_shaped_bulk = False;
        }
        HashListIterator it(records);
        while (it.next()) {
            *auto result = processRecordImpl(it.getValue());
            if (exists result) {
                if (result.typeCode() == NT_LIST) {
                    results += result;
                } else {
                    push results, result;
                }
            }
        }
        if (!results) {
            return;
        }
        return AbstractDataProcessor::recordsToHashList(results);
    }

    #! Processes records in bulk (hash-of-lists) format
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        return processRecordsBulkShape(records, DataProviderShape::HashOfLists);
    }

    #! Processes DataFrame or BufferColumns input from selected top-level columns only
    private *hash<auto> processColumnarRecordsShape(auto records, DataProviderShape shape, hash<auto> columns) {
        updatePendingOutputShape(shape);
        int record_count = AbstractDataProcessor::shapeRecordCount(records, shape);
        list<auto> results;
        processing_shaped_bulk = True;
        on_exit {
            processing_shaped_bulk = False;
        }
        for (int i = 0; i < record_count; ++i) {
            hash<auto> record;
            foreach string field in (fields) {
                record{field} = columns{field}[i];
            }
            *auto result = processRecordImpl(record);
            if (exists result) {
                if (result.typeCode() == NT_LIST) {
                    results += result;
                } else {
                    push results, result;
                }
            }
        }
        if (!results) {
            return;
        }
        return AbstractDataProcessor::recordsToHashList(results);
    }

    #! Processes data in an explicitly negotiated shape
    private *auto processRecordsShapeImpl(auto records, DataProviderShape shape) {
        switch (shape) {
            case DataProviderShape::HashOfLists:
                return processRecordsBulkShape(records, shape);
            case DataProviderShape::BufferColumns:
            case DataProviderShape::DataFrameBlock: {
                *hash<auto> columns = AbstractDataProcessor::getTopLevelShapeColumns(records, shape, fields);
                if (columns && columns.size() == fields.size()) {
                    return AbstractDataProcessor::convertHashListResultShape(
                        processColumnarRecordsShape(records, shape, columns), shape);
                }
                return AbstractDataProcessor::convertHashListResultShape(
                    processRecordsBulkShape(AbstractDataProcessor::convertShape(
                        records, shape, DataProviderShape::HashOfLists)[0], shape), shape);
            }
            case DataProviderShape::RowRecords:
                return processRecordImpl(records);
        }
        throw "PROCESSOR-SHAPE-ERROR", sprintf("%s cannot process unsupported data shape %s",
            self.className(), AbstractDataProcessor::shapeName(shape));
    }

    #! Supports bulk API
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns supported input shapes
    private list<DataProviderShape> supportedInputShapesImpl() {
        return (DataProviderShape::DataFrameBlock, DataProviderShape::BufferColumns,
            DataProviderShape::HashOfLists, DataProviderShape::RowRecords);
    }

    #! Returns supported output shapes
    private list<DataProviderShape> supportedOutputShapesImpl() {
        return (DataProviderShape::DataFrameBlock, DataProviderShape::BufferColumns,
            DataProviderShape::HashOfLists, DataProviderShape::RowRecords);
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
