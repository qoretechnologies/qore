# -*- mode: qore; indent-tabs-mode: nil -*-
#! Compatible image generation using the selected model's pinned contract
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace BflDataProvider {
#! Compatible image form derived from the supported model contracts
public class BflGenerateImageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "prompt": {"display_name": "Prompt", "short_desc": "Text prompt describing the image",
                "type": AbstractDataProviderTypeMap."string", "required": True,
                "desc": "Describe the subject, lighting, and composition. An empty prompt remains an explicit value.",
                "example_value": "A blue ceramic mug on a plain studio background."},
            "model": {"display_name": "Model", "short_desc": "FLUX model identifier",
                "type": AbstractDataProviderTypeMap."*string", "default_value": DefaultImageModel,
                "desc": "Select an image model. Only options supported by that model are accepted.",
                "has_dependents": True, "structural_determinate": True, "on_change": ("refetch",)},
        };
    }
    #! Builds the form after the independent discovery inventory is registered
    constructor() : HashDataType("BflGenerateImageRequest") {
        addQoreFields(Fields);
        getFields().model.setAllowedValues(map <AllowedValueInfo>{"value": $1,
            "display_name": regex_subst(regex_subst(BflApi::Operations{
                BflApi::actionForPath(ImageModelPathMap{$1})}[2], "^Start ", ""), " Image$", "")}, keys ImageModelPathMap);
        hash<string, bool> added = {"prompt": True, "model": True};
        foreach string model in (keys ImageModelPathMap) {
            foreach AbstractDataField field in (BflApi::getActionSet().getOperationEx(
                    BflApi::actionForPath(ImageModelPathMap{model})).getRequestFields().iterator()) {
                string name = field.getName();
                if (added{name} || inlist(name, ("webhook_url", "webhook_secret"))) { continue; }
                hash<auto!> config = field.getConfigInfo() - "default_value";
                config.type = field.getType().getOrNothingType();
                config.attr.required = False;
                config.attr.depends_on = ("model",);
                addField(new QoreDataField(config));
                added{name} = True;
            }
        }
        addQoreFields(BflJobRequestDataType::Controls);
    }
}

#! Preserves generate-image and returns a downloaded image as well as service metadata
public class BflGenerateImageDataProvider inherits BflDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{"name": "generate-image", "type": "BflGenerateImageDataProvider",
            "constructor_options": ConstructorOptions, "supports_request": True};
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = BflCompletedJob;
    }

    private {
        #! The request type, built on first use; see requestType()
        static *BflGenerateImageRequestDataType request_type;
    }
    private {
        #! The ID of the builder of the types; see initializeTypes()
        static int types_id = DataProviderActionCatalog::registerLazyValue(
            auto sub () { return BflGenerateImageDataProvider::buildTypes(); });
    }
    #! Returns the request type, which is built on first use
    /** It is built from the vendored API description, so loading the module does not parse it
    */
    static BflGenerateImageRequestDataType requestType() {
        initializeTypes();
        return request_type;
    }

    #! Returns the response type
    /** Unlike the request type, it is not built from the vendored API description, so it is a constant; this accessor
        gives it the same form as the request type's
    */
    static AbstractDataProviderType responseType() {
        return ResponseType;
    }

    #! Initializes the immutable compatible form once
    private static initializeTypes() {
        if (!request_type) {
            hash<auto> types = DataProviderActionCatalog::getLazyValue(types_id);
            request_type = types.request;
        }
    }

    #! Builds the types; see initializeTypes()
    private static hash<auto> buildTypes() {
        return {"request": new BflGenerateImageRequestDataType()};
    }
    #! Creates the action using an existing authenticated client
    constructor(BflRestClientIo rest) : BflDataProviderCommon(rest) {}
    #! Creates the action from connection options
    constructor(*hash<auto> options) : BflDataProviderCommon(options) {}
    #! Returns the preserved action identity
    string getName() { return ProviderInfo.name; }
    private *AbstractDataProviderType getRequestTypeImpl() { return requestType(); }
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        string model = options.model ?? DefaultImageModel;
        if (!ImageModelPathMap.hasKey(model)) { throw "BFL-INPUT-ERROR", "select a supported image model"; }
        HashDataType type("BflSelectedImageRequest");
        foreach string name in (("prompt", "model")) { type.addField(requestType().getFields(){name}); }
        foreach AbstractDataField field in (BflApi::getActionSet().getOperationEx(
                BflApi::actionForPath(ImageModelPathMap{model})).getRequestFields().iterator()) {
            if (!inlist(field.getName(), ("prompt", "webhook_url", "webhook_secret"))) { type.addField(field); }
        }
        type.addQoreFields(BflJobRequestDataType::Controls);
        return type;
    }
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        return getRequestTypeWithOptionsImpl(req.typeCode() == NT_HASH ? req : NOTHING);
    }
    private *AbstractDataProviderType getResponseTypeImpl() { return responseType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Executes one validated image generation and download
    /** @param req flat model options and overall timeout controls
        @param request_options provider request options
        @return completed job retaining result.sample, seed, cost, and verified media bytes
        @throw BFL-INPUT-ERROR unknown or unsupported model options
        @throw BFL-JOB-TIMEOUT overall deadline expired
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        # Preserve raw integer/null distinctions before model-specific schema validation.
        checkRequest();
        return doRequestImpl(req, validateRequestOptions(request_options));
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.typeCode() != NT_HASH || req.prompt.typeCode() != NT_STRING) {
            throw "BFL-INPUT-ERROR", "generate-image requires a string prompt";
        }
        if (req.hasKey("model") && req.model.typeCode() != NT_STRING) {
            throw "BFL-INPUT-ERROR", "model must be a string when supplied";
        }
        string model = req.model ?? DefaultImageModel;
        if (!ImageModelPathMap.hasKey(model)) { throw "BFL-INPUT-ERROR", "select a supported image model"; }
        return BflApi::generateMedia(rest, model, req - "model");
    }
}
}
