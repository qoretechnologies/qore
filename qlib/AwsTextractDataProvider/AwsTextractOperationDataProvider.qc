# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace AwsTextractDataProvider {
#! A typed native AWS JSON operation with explicit single-page result semantics
public class AwsTextractOperationDataProvider inherits AwsTextractDataProviderCommon {
    private {
        string action;
        hash<auto> operation;
    }

    #! Creates one operation on the existing authenticated regional client
    /** @param rest authenticated client
        @param action stable action identifier
        @throw AWS-TEXTRACT-ACTION-ERROR unknown action
    */
    constructor(AwsTextractRestClient::AwsTextractRestClientIo rest, string action)
            : AwsTextractDataProviderCommon(rest) {
        if (!AwsTextractManifest::Operations.hasKey(action)) {
            throw "AWS-TEXTRACT-ACTION-ERROR", "unknown Textract action " + action;
        }
        self.action = action;
        operation = AwsTextractManifest::Operations{action};
    }

    string getName() { return action; }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": action, "type": "AwsTextractOperationDataProvider", "supports_request": True};
    }

    private *AbstractDataProviderType getRequestTypeImpl() { return AwsTextractSchema::getType(operation.input); }
    private *AbstractDataProviderType getResponseTypeImpl() { return AwsTextractSchema::getType(operation.output); }

    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        AwsTextractSchema::validate(operation.input, req);
        return getRequestTypeImpl();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        AwsTextractSchema::validate(operation.input, req);
        return AwsTextractSchema::convert(operation.output,
            doTextractAction(operation.operation, AwsTextractSchema::convert(operation.input, req)), True);
    }
}

#! Idempotent registration for the native operation surface
public class AwsTextractActions {
    #! Registers operations not already provided by the compatible document actions
    /** Metadata is registered through the shared catalog. */
    static registerNativeActions() {
        foreach hash<auto> entry in (AwsTextractManifest::Operations.pairIterator()) {
            if (inlist(entry.key, ("analyze-document", "detect-document-text"))) { continue; }
            hash<auto> op = entry.value;
            hash<string, AbstractDataField> fields = AwsTextractSchema::getType(op.input).getFields();
            hash<string, hash<ActionOptionInfo>> options = DataProviderActionCatalog::getActionOptionFromFields(fields);
            foreach string name in (keys options) {
                # Type-derived date examples use the current time and break repeat registration.
                remove options{name}.example_value;
                if (inlist(name, ("MaxResults", "AdapterId", "AfterCreationTime", "BeforeCreationTime"))) {
                    options{name}.preselected = True;
                }
            }
            if (options.AdapterId) {
                options.AdapterId.ref_data = "adapters";
                options.AdapterId.has_dependents = True;
                options.AdapterId.on_change = ("refetch",);
            }
            if (options.AdapterVersion) {
                options.AdapterVersion.ref_data = "adapter-versions";
                options.AdapterVersion.depends_on = ("AdapterId",);
            }
            DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
                "app": AppName, "action": entry.key, "path": "/" + entry.key,
                "display_name": op.display_name, "short_desc": op.short_desc, "desc": op.desc,
                "action_code": DPAT_API, "options": options,
                "output_type": AwsTextractSchema::getType(op.output),
                "mutates_state": op.operation !~ /^(Get|List|Analyze|Detect)/,
                "groups": (op.operation =~ /Adapter|Tag/ ? "Adapters and Resources" : "Document Extraction",),
            });
        }
    }
}
}
