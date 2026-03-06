# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSelectFieldsProcessor class definition

/** QoreSelectFieldsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that selects and optionally renames fields from records
public class QoreSelectFieldsProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSelectFieldsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Field List",
                "short_desc": "Field specs to select",
                "desc": "A list of field specifications; use `name` to keep a field as-is, "
                    "or `new_name=old_name` to rename a field (e.g. `name`, `email`, "
                    "`customer_name=name`)",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
        };
    }

    private {
        #! Field mapping: output_name => input_name
        hash<string, string> field_map;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.fields.val()) {
            field_map = QoreSelectFieldsProcessor::parseFieldSpec(copts.fields);
        }
    }

    #! Parses a field specification from either a string or list into a mapping hash
    static hash<string, string> parseFieldSpec(auto fields) {
        # normalize to a flat list of spec strings
        list<string> specs = AbstractDataProvider::parseFieldList(fields);
        hash<string, string> result;
        foreach string spec in (specs) {
            int eq_pos = spec.find("=");
            if (eq_pos >= 0) {
                string output_name = trim(spec.substr(0, eq_pos));
                string input_name = trim(spec.substr(eq_pos + 1));
                if (output_name.val() && input_name.val()) {
                    result{output_name} = input_name;
                }
            } else {
                result{spec} = spec;
            }
        }
        if (!result) {
            throw "CONSTRUCTOR-ERROR", "no valid field specifications found in 'fields' option";
        }
        return result;
    }

    #! Returns the name of this data provider
    string getName() {
        return "select-fields";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Selects and optionally renames fields from records; only the specified fields appear in the "
            "output, and fields can be renamed using the `new_name=old_name` syntax";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!field_map) {
            throw "PROCESSOR-ERROR", "QoreSelectFieldsProcessor: no field specification configured; "
                "provide the 'fields' option";
        }
        hash<auto> result;
        foreach hash<auto> i in (field_map.pairIterator()) {
            result{i.key} = AbstractDataProvider::getFieldValue(record, i.value);
        }
        return result;
    }

    #! Computes the output type based on the input type and the configured field selection
    /** @param input_type the output type from the preceding pipeline state

        @return a HashDataType with only the selected (and optionally renamed) fields from the input,
        or NOTHING if the input type has no fields or the processor is not configured

        @since %DataProvider 3.4
    */
    private *AbstractDataProviderType getPipelineProcessorOutputTypeWithInputTypeImpl(
            *AbstractDataProviderType input_type) {
        if (!field_map || !input_type) {
            return;
        }
        *hash<string, AbstractDataField> input_fields = input_type.getFields();
        if (!input_fields) {
            return;
        }
        HashDataType rv(input_type.getName() + " (selected)");
        foreach hash<auto> i in (field_map.pairIterator()) {
            # i.key = output name, i.value = input name
            if (*AbstractDataField field = input_fields{i.value}) {
                if (i.key != i.value) {
                    # renamed field: create new field with output name and same type
                    rv.addField(new QoreDataField(i.key, field.getDescription(), field.getType()));
                } else {
                    rv.addField(field);
                }
            }
        }
        return rv.getFields() ? rv : NOTHING;
    }

    #! Indicates that this processor does not support the bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

}
}
