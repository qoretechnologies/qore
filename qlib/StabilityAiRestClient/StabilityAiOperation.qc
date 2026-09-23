# -*- mode: qore; indent-tabs-mode: nil -*-
#! Shared typed execution and media decoding for Stability AI
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace StabilityAiRestClient {
#! Typed media artifact returned by Stability AI
public class StabilityAiArtifactType inherits HashDataType {
    private {
        const Fields = {
            "data": {"display_name": "Media Bytes", "desc": "Decoded image, audio, or GLB bytes.",
                "type": AbstractDataProviderTypeMap."binary", "required": True},
            "content_type": {"display_name": "Content Type", "desc": "Verified MIME type of the media bytes.",
                "type": AbstractDataProviderTypeMap."string", "required": True},
            "seed": {"display_name": "Seed", "desc": "Seed reported by the service, including zero.",
                "type": AbstractDataProviderTypeMap."*int"},
            "finish_reason": {"display_name": "Finish Reason", "desc": "Service completion or safety result.",
                "type": AbstractDataProviderTypeMap."*string"},
        };
    }
    #! Creates the immutable artifact field definitions
    constructor() : HashDataType("StabilityAiArtifact") { addQoreFields(Fields); }
}
#! Shared artifact type
public const StabilityAiArtifact = new StabilityAiArtifactType();
#! Optional list of completed artifacts
public const StabilityAiArtifacts = new ListDataType("StabilityAiArtifacts", StabilityAiArtifact, True);

#! Media result with explicit asynchronous state and typed artifacts
public class StabilityAiMediaType inherits HashDataType {
    private {
        const Fields = {
            "status": {"display_name": "Status", "desc": "Whether the job is pending or has completed.",
                "type": AbstractDataProviderTypeMap."string", "required": True,
                "allowed_values": (<AllowedValueInfo>{"value": "pending", "display_name": "Pending"},
                    <AllowedValueInfo>{"value": "succeeded", "display_name": "Succeeded"})},
            "job_id": {"display_name": "Job ID", "desc": "Job ID used to retrieve an asynchronous result.",
                "type": AbstractDataProviderTypeMap."*string"},
            "artifacts": {"display_name": "Artifacts", "desc": "Completed media artifacts in service order.",
                "type": StabilityAiArtifacts},
            "retry_after": {"display_name": "Retry After", "desc": "Minimum polling delay in seconds, when provided.",
                "type": AbstractDataProviderTypeMap."*int"},
        };
    }
    #! Creates the immutable media/job result field definitions
    constructor() : HashDataType("StabilityAiMediaResult") { addQoreFields(Fields); }
}
#! Shared media/job result type
public const StabilityAiMediaResult = new StabilityAiMediaType();

#! Executes the pinned multipart contracts and checks operation-specific constraints
/** Shared by the app and QoreMediaUtils. Transport, named file inputs, choice conversion, and schemas
    belong to RestSchemaActions; this class owns Stability's conditional rules and response metadata.
*/
public class StabilityAiOperation inherits RestSchemaActionDataProvider {
    public {
        const MediaType = StabilityAiMediaResult;
    }
    private {
        string action;
        string kind;
    }

    #! Creates an operation bound to an authenticated client
    /** @param action stable operation identity from StabilityAiApi::Operations
        @param client configured client; construction performs no requests
        @throw REST-SCHEMA-ACTION-ERROR the action is not in the pinned contract
    */
    constructor(string action, AbstractRestClient client)
            : RestSchemaActionDataProvider(StabilityAiApi::getActionSet(),
                StabilityAiApi::getActionSet().getOperationEx(action), client) {
        self.action = action;
        kind = StabilityAiApi::Operations{action}[4];
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return kind == "json" ? RestSchemaActionDataProvider::getResponseTypeImpl() : MediaType;
    }

    #! Validates dependent fields before any billable request
    /** @param req action options; explicit null and unknown fields are rejected
        @param request_options transport options
        @return structured account data or a typed media/job result
        @throw STABILITYAI-INPUT-ERROR invalid conditional parameters
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() != NT_HASH) {
            throw "STABILITYAI-INPUT-ERROR", "action options must be an object";
        }
        *hash<string, AbstractDataField> fields = getActionOperation().getRequestFields();
        foreach hash<auto> item in (req.pairIterator()) {
            if (!fields.hasKey(item.key) || !exists item.value || item.value.typeCode() == NT_NULL) {
                throw "STABILITYAI-INPUT-ERROR", sprintf("unknown or null option %y", item.key);
            }
        }
        if (req.hasKey("seed") && (req.seed.typeCode() == NT_BOOLEAN
                || (req.seed.typeCode() == NT_STRING && req.seed !~ /^\d+$/)
                || (inlist(req.seed.typeCode(), (NT_FLOAT, NT_NUMBER)) && req.seed != int(req.seed)))) {
            throw "STABILITYAI-INPUT-ERROR", "seed must be an integer";
        }
        # Normalize choices through the published type, so labels never affect dependency rules.
        if (getRequestType()) { req = getRequestType().acceptsValue(req); }
        validateOptions(action, req);
        auto result = RestSchemaActionDataProvider::doRequest(req, request_options);
        return kind == "json" ? result : decode(result, kind, req.job_id);
    }

    #! Checks conditional fields that OpenAPI cannot express
    /** @param action stable action identity
        @param req normalized action options
        @throw STABILITYAI-INPUT-ERROR unsupported parameter combination
    */
    static validateOptions(string action, hash<auto> req) {
        if (exists req.job_id && (req.job_id.typeCode() != NT_STRING
                || req.job_id !~ /^[a-zA-Z0-9]{64}$/)) {
            throw "STABILITYAI-INPUT-ERROR", "job_id must be a service job ID, not a URL";
        }
        if (exists req.engine_id && req.engine_id !~ /^[a-zA-Z0-9_-]{1,128}$/) {
            throw "STABILITYAI-INPUT-ERROR", "engine_id must be an engine ID from List Legacy Engines";
        }
        int seed_max = action =~ /^legacy-/ ? 4294967295 : 4294967294;
        if (exists req.seed && (req.seed < 0 || req.seed > seed_max || req.seed != int(req.seed))) {
            throw "STABILITYAI-INPUT-ERROR", sprintf("seed must be an integer from 0 through %d", seed_max);
        }
        if (action == "generate-sd3") {
            string mode = req.mode ?? "text-to-image";
            if (mode == "image-to-image") {
                if (!exists req.image || !exists req.strength) {
                    throw "STABILITYAI-INPUT-ERROR", "image-to-image requires image and strength";
                }
                if (exists req.aspect_ratio) {
                    throw "STABILITYAI-INPUT-ERROR", "aspect_ratio is only valid for text-to-image";
                }
            } else if (exists req.image || exists req.strength) {
                throw "STABILITYAI-INPUT-ERROR", "image and strength require image-to-image mode";
            }
        }
        if (action == "generate-ultra" && exists req.strength && !exists req.image) {
            throw "STABILITYAI-INPUT-ERROR", "strength requires an image";
        }
        if (action == "generate-ultra" && exists req.image && !exists req.strength) {
            throw "STABILITYAI-INPUT-ERROR", "image requires strength";
        }
        if (action == "outpaint-image" && !(req.left + req.right + req.up + req.down > 0)) {
            throw "STABILITYAI-INPUT-ERROR", "at least one image border must be extended";
        }
        if (action == "start-background-relight") {
            if (!exists req.background_reference && !exists req.background_prompt) {
                throw "STABILITYAI-INPUT-ERROR", "a background_reference or background_prompt is required";
            }
            if (exists req.background_reference && exists req.background_prompt) {
                throw "STABILITYAI-INPUT-ERROR", "choose background_reference or background_prompt";
            }
            if (exists req.light_source_direction && exists req.light_reference) {
                throw "STABILITYAI-INPUT-ERROR", "choose light_source_direction or light_reference";
            }
        }
        if (action =~ /audio/ && exists req.mask_start && exists req.mask_end
                && (req.mask_end <= req.mask_start || (exists req.duration && req.mask_end > req.duration))) {
            throw "STABILITYAI-INPUT-ERROR", "mask_end must follow mask_start and fit within duration";
        }
        if (inlist(action, ("generate-audio-2", "transform-audio-2"))) {
            bool v25 = req.model == "stable-audio-2.5";
            if (exists req.steps && (v25 ? (req.steps < 4 || req.steps > 8) : (req.steps < 30 || req.steps > 100))) {
                throw "STABILITYAI-INPUT-ERROR", "steps must be 4–8 for Audio 2.5 or 30–100 for Audio 2";
            }
        }
        if (action == "legacy-mask-image" && inlist(req.mask_source, ("MASK_IMAGE_WHITE", "MASK_IMAGE_BLACK"))
                && !exists req.mask_image) {
            throw "STABILITYAI-INPUT-ERROR", "mask_image is required for an explicit mask source";
        }
        if (action == "legacy-mask-image" && req.mask_source == "INIT_IMAGE_ALPHA" && exists req.mask_image) {
            throw "STABILITYAI-INPUT-ERROR", "mask_image cannot be used with INIT_IMAGE_ALPHA";
        }
        if (action == "generate-fast-3d" && exists req.vertex_count && req.vertex_count != int(req.vertex_count)) {
            throw "STABILITYAI-INPUT-ERROR", "vertex_count must be an integer";
        }
        foreach string name in (("image", "mask", "subject_image", "background_reference", "light_reference",
                "init_image", "style_image", "mask_image", "audio")) {
            if (!exists req{name}) { continue; }
            auto file = req{name};
            auto bytes = file.typeCode() == NT_HASH ? file.content : file;
            if (bytes.typeCode() == NT_STRING) { bytes = parse_base64_string(bytes); }
            if (bytes.typeCode() != NT_BINARY || !bytes.size()) {
                throw "STABILITYAI-INPUT-ERROR", sprintf("%s must contain nonempty media bytes", name);
            }
            *string declared = file.typeCode() == NT_HASH ? file.mime_type : NOTHING;
            string mime = QoreMediaUtils::validateMediaContentType(bytes, declared == "auto" ? NOTHING : declared);
            if (name == "audio") {
                if (!inlist(mime, ("audio/mpeg", "audio/wav"))) {
                    throw "STABILITYAI-INPUT-ERROR", "audio must contain MP3 or WAV data";
                }
                float duration = QoreMediaUtils::getAudioDuration(bytes);
                int maximum = action =~ /^start-audio-3/ ? 380 : 190;
                if (duration < 6.0 || duration > maximum) {
                    throw "STABILITYAI-INPUT-ERROR", sprintf("input audio duration must be 6 through %d seconds", maximum);
                }
                continue;
            }
            hash<string, int> size = QoreMediaUtils::getImageDimensions(bytes);
            int minimum = action == "upscale-fast" ? 32 : 64;
            int maximum = action == "upscale-fast" ? 1536 : action == "generate-ultra" ? 16384 : 16383;
            int pixels = size.width * size.height;
            int pixel_max = inlist(action, ("remove-background", "generate-fast-3d", "generate-point-aware-3d"))
                ? 4194304 : inlist(action, ("upscale-fast", "start-creative-upscale")) ? 1048576 : 9437184;
            if (action == "generate-ultra" || action == "generate-sd3") { pixel_max = 268435456; }
            bool aspect = inlist(action, ("upscale-conservative", "outpaint-image", "search-and-replace",
                "search-and-recolor", "control-sketch", "control-structure", "control-style", "transfer-style",
                "start-background-relight")) && !inlist(name, ("style_image", "light_reference", "background_reference"));
            if (action == "legacy-mask-image" && name == "mask_image") {
                auto source = req.init_image.typeCode() == NT_HASH ? req.init_image.content : req.init_image;
                if (source.typeCode() == NT_STRING) { source = parse_base64_string(source); }
                if (size != QoreMediaUtils::getImageDimensions(source)) {
                    throw "STABILITYAI-INPUT-ERROR", "legacy mask dimensions must match init_image";
                }
            }
            # The v2 API resizes masks; legacy masks must match the source image.
            if (!inlist(name, ("mask", "mask_image")) && (size.width < minimum || size.height < minimum
                    || size.width > maximum || size.height > maximum || pixels > pixel_max
                    || (aspect && (size.width > size.height * 2.5 || size.height > size.width * 2.5)))) {
                throw "STABILITYAI-INPUT-ERROR", sprintf("%s dimensions exceed this operation's supported bounds", name);
            }
        }
    }

    #! Decodes raw or JSON/base64 media while preserving service metadata
    /** @param response HTTP response envelope
        @param kind operation's media family
        @param job_id ID used for a result lookup
        @return typed media/job result
        @throw STABILITYAI-RESPONSE-ERROR invalid response or filtered media
    */
    static hash<auto> decode(hash<auto> response, string kind, *string job_id) {
        hash<auto> headers = map {$1.key.lwr(): $1.value}, response.headers.pairIterator();
        auto body = response.body;
        if (response.status_code == 202 || kind =~ /-job$/) {
            if (body.typeCode() != NT_HASH || body.id.typeCode() != NT_STRING
                    || body.id !~ /^[a-zA-Z0-9]{64}$/ || (job_id && body.id != job_id)) {
                throw "STABILITYAI-RESPONSE-ERROR", "pending response has a missing or mismatched job ID";
            }
            if (body.hasKey("error") || body.hasKey("errors")
                    || (body.hasKey("status") && body.status != "in-progress")) {
                throw "STABILITYAI-RESPONSE-ERROR", "pending response contains an error or an invalid status";
            }
            hash<auto!> pending = {"status": "pending", "job_id": body.id};
            if (headers."retry-after" =~ /^\d+$/) { pending.retry_after = int(headers."retry-after"); }
            return pending;
        }
        list<hash<auto>> artifacts = ();
        if (body.typeCode() == NT_HASH) {
            if (exists body.errors || exists body.error) {
                throw "STABILITYAI-RESPONSE-ERROR", "service returned an error in a successful HTTP response";
            }
            list<auto> records = body.artifacts ?? (body,);
            foreach hash<auto> record in (records) {
                string finish = record.finish_reason ?? record.finishReason ?? headers."finish-reason" ?? "SUCCESS";
                checkFinish(finish);
                auto encoded = record.image ?? record.audio ?? record.base64;
                if (encoded.typeCode() != NT_STRING || !encoded.val()) {
                    throw "STABILITYAI-RESPONSE-ERROR", "JSON media response is missing base64 data";
                }
                binary bytes = parse_base64_string(encoded);
                push artifacts, artifact(bytes, NOTHING, record.seed ?? headers.seed, finish, kind);
            }
        } else {
            if (body.typeCode() != NT_BINARY) {
                throw "STABILITYAI-RESPONSE-ERROR", "expected raw media bytes or a JSON media object";
            }
            string finish = headers."finish-reason" ?? "SUCCESS";
            checkFinish(finish);
            push artifacts, artifact(body, headers."content-type", headers.seed, finish, kind);
        }
        if (!artifacts) { throw "STABILITYAI-RESPONSE-ERROR", "response contains no media artifacts"; }
        hash<auto> result = {"status": "succeeded", "artifacts": artifacts};
        if (job_id) { result.job_id = job_id; }
        return result;
    }

    private static checkFinish(string finish) {
        if (finish != "SUCCESS") {
            throw "STABILITYAI-CONTENT-ERROR", "service did not produce a successful media artifact",
                {"finish_reason": finish};
        }
    }

    private static hash<auto> artifact(binary bytes, *string declared, auto seed, string finish, string kind) {
        string mime = QoreMediaUtils::validateMediaContentType(bytes, declared);
        if ((kind =~ /image/ && mime !~ /^image\//) || (kind =~ /audio/ && mime !~ /^audio\//)
                || (kind == "model" && mime != "model/gltf-binary")) {
            throw "STABILITYAI-RESPONSE-ERROR", "media does not match this operation's output family";
        }
        hash<auto> result = {"data": bytes, "content_type": mime, "finish_reason": finish};
        if (exists seed) {
            if (seed.typeCode() != NT_INT && !(seed.typeCode() == NT_STRING && seed =~ /^\d+$/)) {
                throw "STABILITYAI-RESPONSE-ERROR", "invalid response seed";
            }
            result.seed = int(seed);
        }
        return result;
    }
}
}
