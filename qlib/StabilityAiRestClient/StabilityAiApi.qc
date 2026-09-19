# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).

namespace Priv {
    # Read immutable packaged resources while the module initialization sandbox permits it.
    const StabilitySchemaText = File::readTextFile(get_script_dir() + "/stability-openapi.json");
    const StabilityPresentationText = File::readTextFile(get_script_dir() + "/stability-presentation.json");
}

public namespace StabilityAiRestClient {
#! Pinned service contracts shared by app actions and the media facade
public class StabilityAiApi {
    public {
        # Lightweight identities are available without parsing or connecting.
        const Operations = {
            "generate-ultra": ("POST", "/v2beta/stable-image/generate/ultra", "Generate Ultra Image", "Create detailed marketing images with Stable Image Ultra.", "image"),
            "generate-core": ("POST", "/v2beta/stable-image/generate/core", "Generate Core Image", "Create affordable concept images with Stable Image Core.", "image"),
            "generate-sd3": ("POST", "/v2beta/stable-image/generate/sd3", "Generate SD3.5 Image", "Create or transform images with a selected SD3.5 model.", "image"),
            "upscale-fast": ("POST", "/v2beta/stable-image/upscale/fast", "Upscale Image Quickly", "Increase image resolution for reuse in larger layouts.", "image"),
            "upscale-conservative": ("POST", "/v2beta/stable-image/upscale/conservative", "Upscale Image Conservatively", "Enlarge an image while preserving its original appearance.", "image"),
            "start-creative-upscale": ("POST", "/v2beta/stable-image/upscale/creative", "Start Creative Upscale", "Start a detailed upscale and retain its job ID for retrieval.", "image-job"),
            "get-upscale-result": ("GET", "/v2beta/stable-image/upscale/creative/result/{id}", "Get Creative Upscale Result", "Retrieve a completed upscale or its pending status.", "image-result"),
            "erase-object": ("POST", "/v2beta/stable-image/edit/erase", "Erase Object", "Remove unwanted objects from a masked image area.", "image"),
            "inpaint-image": ("POST", "/v2beta/stable-image/edit/inpaint", "Inpaint Image", "Replace a masked image area using a descriptive prompt.", "image"),
            "outpaint-image": ("POST", "/v2beta/stable-image/edit/outpaint", "Extend Image", "Extend image borders to fit a new composition.", "image"),
            "search-and-replace": ("POST", "/v2beta/stable-image/edit/search-and-replace", "Search and Replace", "Replace an object selected by a text description.", "image"),
            "search-and-recolor": ("POST", "/v2beta/stable-image/edit/search-and-recolor", "Search and Recolor", "Recolor an object selected by a text description.", "image"),
            "remove-background": ("POST", "/v2beta/stable-image/edit/remove-background", "Remove Background", "Isolate a product or subject on a transparent background.", "image"),
            "start-background-relight": ("POST", "/v2beta/stable-image/edit/replace-background-and-relight", "Start Background and Lighting Edit", "Replace a background and adjust lighting as an asynchronous job.", "image-job"),
            "get-image-result": ("GET", "/v2beta/results/{id}", "Get Image Job Result", "Retrieve an asynchronous image or its pending status.", "image-result"),
            "control-sketch": ("POST", "/v2beta/stable-image/control/sketch", "Generate from Sketch", "Turn a sketch into a finished image guided by a prompt.", "image"),
            "control-structure": ("POST", "/v2beta/stable-image/control/structure", "Generate with Image Structure", "Keep an input image's structure while creating new content.", "image"),
            "control-style": ("POST", "/v2beta/stable-image/control/style", "Generate with Style Guide", "Create a new image using a reference image's visual style.", "image"),
            "transfer-style": ("POST", "/v2beta/stable-image/control/style-transfer", "Transfer Image Style", "Apply a reference style to an existing image composition.", "image"),
            "generate-fast-3d": ("POST", "/v2beta/3d/stable-fast-3d", "Generate Fast 3D Model", "Create a textured 3D asset from an image.", "model"),
            "generate-point-aware-3d": ("POST", "/v2beta/3d/stable-point-aware-3d", "Generate Point Aware 3D Model", "Create a 3D asset with inferred details on unseen surfaces.", "model"),
            "generate-audio-2": ("POST", "/v2beta/audio/stable-audio-2/text-to-audio", "Generate Audio 2 or 2.5", "Create music or sound effects from a text description.", "audio"),
            "transform-audio-2": ("POST", "/v2beta/audio/stable-audio-2/audio-to-audio", "Transform Audio 2 or 2.5", "Transform an audio recording using a text description.", "audio"),
            "inpaint-audio-2": ("POST", "/v2beta/audio/stable-audio-2/inpaint", "Inpaint Audio 2.5", "Replace a selected time range in an audio recording.", "audio"),
            "start-audio-3": ("POST", "/v2beta/audio/stable-audio/text-to-audio", "Start Audio 3 Generation", "Start generating a longer music track or sound effect.", "audio-job"),
            "start-audio-3-transform": ("POST", "/v2beta/audio/stable-audio/audio-to-audio", "Start Audio 3 Transformation", "Start transforming an audio recording with Stable Audio 3.", "audio-job"),
            "start-audio-3-inpaint": ("POST", "/v2beta/audio/stable-audio/inpaint", "Start Audio 3 Inpainting", "Start replacing a selected time range with Stable Audio 3.", "audio-job"),
            "get-audio-result": ("GET", "/v2beta/audio/results/{id}", "Get Audio Job Result", "Retrieve completed audio or its pending status.", "audio-result"),
            "get-account": ("GET", "/v1/user/account", "Get Account", "Read account details and available organizations.", "json"),
            "get-balance": ("GET", "/v1/user/balance", "Get Credit Balance", "Check the credit balance for the selected organization.", "json"),
            "list-engines": ("GET", "/v1/engines/list", "List Legacy Engines", "List legacy image engines available to this account.", "json"),
            "legacy-text-to-image": ("POST", "/v1/generation/{engine_id}/text-to-image", "Generate Legacy Image", "Create images with an available legacy engine.", "legacy-image"),
            "legacy-image-to-image": ("POST", "/v1/generation/{engine_id}/image-to-image", "Transform Legacy Image", "Transform an image with an available legacy engine.", "legacy-image"),
            "legacy-mask-image": ("POST", "/v1/generation/{engine_id}/image-to-image/masking", "Mask Legacy Image", "Replace a masked area with an available legacy engine.", "legacy-image"),
        };
    }

    private {
        static Mutex mutex();
        static *OpenApi3::OpenApi3Schema schema;
        static *RestSchemaActionSet actions;
    }

    #! Returns the offline contract shared by typed actions and direct callers
    /** @return parsed pinned OpenAPI contract
        @throw OPENAPI3-ERROR the installed contract is invalid
    */
    static OpenApi3::OpenApi3Schema getSchema() {
        AutoLock lock(mutex);
        return getSchemaIntern();
    }

    private static OpenApi3::OpenApi3Schema getSchemaIntern() {
        if (!schema) { schema = OpenApi3::OpenApi3Loader::fromString(Priv::StabilitySchemaText, True); }
        return schema;
    }

    #! Returns the typed operation set without performing network I/O
    /** @return operation set with stable action identities
        @throw REST-SCHEMA-ACTION-ERROR the pinned contract and manifest disagree
    */
    static RestSchemaActionSet getActionSet() {
        AutoLock lock(mutex);
        if (!actions) {
            list<hash<RestSchemaActionInfo>> manifest = ();
            hash<auto> document = parse_json(Priv::StabilitySchemaText);
            hash<auto> presentation = parse_json(Priv::StabilityPresentationText);
            foreach hash<auto> entry in (Operations.pairIterator()) {
                hash<RestSchemaActionInfo> item = <RestSchemaActionInfo>{
                    "action": entry.key, "method": entry.value[0], "path": entry.value[1],
                    "display_name": entry.value[2], "short_desc": entry.value[3], "desc": entry.value[3],
                    "response_envelope": entry.value[4] != "json", "metadata": {"kind": entry.value[4]},
                };
                # Result retrieval can also return a 202 pending envelope. The app publishes
                # the explicit media/job union; this selects the completed wire body contract.
                if (entry.value[4] =~ /-result$/) { item.response_code = "200"; }
                hash<auto> operation = document.paths{entry.value[1]}{entry.value[0].lwr()};
                hash<string, hash<RestSchemaFieldOverlayInfo>> fields =
                    presentation{entry.value[0] + " " + entry.value[1]} ?? {};
                foreach hash<auto> parameter in (operation.parameters) {
                    if (parameter."$ref") {
                        parameter = document.components.parameters{parameter."$ref".split("/")[-1]};
                    }
                    if (parameter.in == "header") {
                        string name = parameter.name;
                        if (name.lwr() == "accept") {
                            fields{"header." + name} = <RestSchemaFieldOverlayInfo>{
                                "name": "representation", "display_name": "Response Representation",
                                "short_desc": "Choose raw media or base64 JSON",
                                "desc": "Choose the service response representation. Media bytes and metadata are preserved.",
                                "default_value": parameter.schema.default,
                                "allowed_values": map <AllowedValueInfo>{"value": $1,
                                    "display_name": $1 == "application/json" ? "Base64 JSON" : "Raw Media"},
                                    parameter.schema."enum",
                            };
                        } else {
                            fields{"header." + name} = <RestSchemaFieldOverlayInfo>{"ignore": True};
                        }
                    }
                    if (parameter.in == "path" && parameter.name == "id") {
                        fields."path.id" = <RestSchemaFieldOverlayInfo>{
                            "name": "job_id", "display_name": "Job ID",
                            "short_desc": "Job ID returned by the start action",
                            "desc": "Use the job ID returned by the corresponding start action. URLs are not accepted.",
                        };
                    }
                }
                item.fields = fields;
                if (entry.key == "start-background-relight") {
                    item.fields."body.keep_original_background" += <RestSchemaFieldOverlayInfo>{
                        "codec": RestSchemaActionCodecs::BooleanString, "display_name": "Keep Original Background",
                        "short_desc": "Preserve the original background during lighting changes",
                        "desc": "Keep the existing background when adjusting its lighting.",
                    };
                }
                if (entry.value[4] == "legacy-image") {
                    item.fields."path.engine_id" = <RestSchemaFieldOverlayInfo>{
                        "ref_data": "engines", "display_name": "Legacy Engine",
                        "short_desc": "Available legacy image engine",
                        "desc": "Select an engine returned by the authenticated List Legacy Engines action.",
                        "supports_custom_values": True,
                    };
                }
                push manifest, item;
            }
            actions = new RestSchemaActionSet(getSchemaIntern(), manifest, {
                "engines": <RestSchemaReferenceDataInfo>{"name": "engines", "action": "list-engines",
                    "value_field": "id", "display_field": "name", "desc": "Available legacy image engine."},
            });
        }
        return actions;
    }

    #! Generates an image using the compatible endpoint-family selector
    /** @param client authenticated client
        @param options generation fields; model selects ultra, core, or sd3; sd3_model selects a concrete SD3.5 model
        @return verified image bytes, MIME type, seed, and completion metadata
        @throw STABILITYAI-INPUT-ERROR incompatible or invalid generation fields
    */
    static hash<auto> generateImage(AbstractRestClient client, hash<auto> options) {
        foreach hash<auto> field in (options.pairIterator()) {
            if (!exists field.value || field.value.typeCode() == NT_NULL) {
                throw "STABILITYAI-INPUT-ERROR", sprintf("option %y cannot be null", field.key);
            }
        }
        string model = options.model ?? DefaultModel;
        if (!ModelPathMap.hasKey(model)) { throw "STABILITYAI-INPUT-ERROR", "model must be ultra, sd3, or core"; }
        hash<auto> req = options - "model";
        if (req.hasKey("sd3_model")) {
            if (model != "sd3") { throw "STABILITYAI-INPUT-ERROR", "sd3_model requires the sd3 endpoint family"; }
            req.model = remove req.sd3_model;
        }
        StabilityAiOperation operation("generate-" + model, client);
        hash<auto> result = operation.doRequest(req);
        hash<auto> artifact = result.artifacts[0];
        return {"image": artifact.data, "content_type": artifact.content_type,
            "seed": exists artifact.seed ? artifact.seed.toString() : NOTHING,
            "finish_reason": artifact.finish_reason};
    }
}
}
