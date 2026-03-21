# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreClassificationMetricsProcessor class definition

/** QoreClassificationMetricsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that measures how accurate classification predictions are
/** Accumulates records containing predicted and true class labels, then computes
    accuracy, macro F1, weighted F1, and per-class precision/recall/F1 metrics
    using the ML::ml_classification_report() function.

    On flush (or when the window is full), emits a single
    ClassificationMetricsEventInfo event summarizing the batch.

*/
public class QoreClassificationMetricsProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreClassificationMetricsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "predicted_field": <DataProviderOptionInfo>{
                "display_name": "Predicted Category Field",
                "short_desc": "Which field has the model's predicted category",
                "desc": "The field in each record that contains what your model predicted. "
                    "For example, if your spam filter writes its prediction to a field called "
                    "`predicted_category` with values like `\"spam\"` or `\"not_spam\"`, "
                    "enter `predicted_category` here",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "true_field": <DataProviderOptionInfo>{
                "display_name": "Correct Answer Field",
                "short_desc": "Which field has the actual correct category",
                "desc": "The field in each record that contains the correct answer — what the "
                    "category actually was. For example, if a human reviewer labeled each email "
                    "in a `true_category` field, enter `true_category` here",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before scoring",
                "desc": "How many records to collect before computing a quality score. All "
                    "records in the batch are evaluated together.\n\n"
                    "**Tip**: use at least `50` for meaningful scores; `200`+ for a detailed "
                    "per-category breakdown",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
        };
    }

    private {
        #! Configured predicted field name
        string predicted_field;

        #! Configured true field name
        string true_field;

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
        predicted_field = copts.predicted_field;
        true_field = copts.true_field;
        window_size = copts.window_size;
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "classification-metrics";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Compares predicted labels against actual labels to measure model accuracy; "
            "accumulates records in batches and emits accuracy, macro F1, weighted F1, and "
            "per-class precision/recall/F1 metrics";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits metrics when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreClassificationMetricsProcessor: not configured; provide constructor options";
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
        if (buffer.lsize() >= 2) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records through classification metrics
    private list<auto> processWindow(list<hash<auto>> records) {
        list<auto> y_true;
        list<auto> y_pred;
        foreach hash<auto> rec in (records) {
            push y_true, DataProvider::AbstractDataProvider::getFieldValue(rec, true_field);
            push y_pred, DataProvider::AbstractDataProvider::getFieldValue(rec, predicted_field);
        }

        hash<ML::ClassificationReport> report = ML::ml_classification_report(y_true, y_pred);

        # convert per_class list to list<hash<auto>> for the event
        list<hash<auto>> per_class;
        foreach hash<ML::ClassMetrics> cls in (report.per_class) {
            push per_class, {
                "label": cls.label,
                "precision": cls.precision,
                "recall": cls.recall,
                "f1": cls.f1,
                "support": cls.support,
            };
        }

        return (<ClassificationMetricsEventInfo>{
            "event_type": "classification-metrics",
            "accuracy": report.accuracy,
            "macro_f1": report.macro_f1,
            "weighted_f1": report.weighted_f1,
            "per_class": per_class,
            "sample_count": records.lsize(),
        },);
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
            TypedHash::forName("DataProviderML::ClassificationMetricsEventInfo"));
    }
}
}
