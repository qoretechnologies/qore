# -*- mode: qore; indent-tabs-mode: nil -*-
#! Typed BFL execution and pre-submission validation
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace BflRestClient {
#! Executes one pinned BFL operation through the shared REST schema transport
public class BflOperation inherits RestSchemaActionDataProvider {
    private {
        string action;
        BflRestClientIo client;
        *date started;
        *timeout budget;
    }

    #! Constructs an offline typed operation bound to the selected client
    /** @param action stable identity from BflApi::Operations
        @param client authenticated BFL client
        @param started optional overall start time; use only with a private per-call client
        @param budget optional overall submission budget, including validation
        @throw REST-SCHEMA-ACTION-ERROR unsupported action
    */
    constructor(string action, BflRestClientIo client, *date started, *timeout budget)
            : RestSchemaActionDataProvider(BflApi::getActionSet(), BflApi::getActionSet().getOperationEx(action), client) {
        self.action = action;
        self.client = client;
        self.started = started;
        self.budget = budget;
    }

    #! Validates model-specific options before sending one request
    /** @param req action options, preserving absence, null, false, and zero
        @param request_options shared transport controls
        @return typed service response with its original metadata
        @throw BFL-INPUT-ERROR invalid options or media
        @throw BFL-JOB-ERROR invalid submission identity or result
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        if (req.typeCode() != NT_HASH) { throw "BFL-INPUT-ERROR", "action options must be an object"; }
        hash<string, AbstractDataField> fields = getActionOperation().getRequestFields() ?? {};
        foreach hash<auto> entry in (req.pairIterator()) {
            if (!fields.hasKey(entry.key)) {
                throw "BFL-INPUT-ERROR", sprintf("option %y is not supported by this model", entry.key);
            }
            if (exists entry.value && entry.value.typeCode() != NT_NULL
                    && inlist(entry.key, ("seed", "width", "height", "steps", "safety_tolerance", "dilate_pixels",
                        "creativity", "top", "bottom", "left", "right", "reference_offset_x", "reference_offset_y", "number_of_generations"))) {
                auto value = entry.value;
                if (value.typeCode() != NT_INT) {
                    throw "BFL-INPUT-ERROR", sprintf("option %y must be an integer without coercion", entry.key);
                }
            }
        }
        if (getRequestType()) { req = getRequestType().acceptsValue(req); }
        validateOptions(action, req);
        if (started) {
            int remaining = int(budget) - get_duration_milliseconds(now_us() - started);
            if (remaining <= 0) {
                throw "BFL-JOB-TIMEOUT", "deadline expired during validation; no job was submitted";
            }
            client.setTimeout(min(client.getTimeout(), remaining));
            client.setConnectTimeout(min(client.getConnectTimeout(), remaining));
        }
        auto response = RestSchemaActionDataProvider::doRequest(req, request_options);
        string kind = BflApi::Operations{action}[4];
        if (inlist(kind, ("image", "video"))) {
            return BflJobs::submitted(client, response, exists req.webhook_url && req.webhook_url.typeCode() != NT_NULL);
        }
        if (kind == "result") { return BflJobs::checkedStatus(response, req.id); }
        if (action == "get-credits") {
            if (response.typeCode() != NT_HASH || response.hasKey("error")
                    || !inlist(response.credits.typeCode(), (NT_INT, NT_FLOAT, NT_NUMBER)) || response.credits < 0) {
                throw "BFL-RESPONSE-ERROR", "credit response must contain a nonnegative numeric balance";
            }
        }
        return response;
    }

    #! Checks dependencies and supplied media before a billable call
    /** @param action stable operation identity
        @param req normalized options
        @throw BFL-INPUT-ERROR invalid dimensions, masks, durations, or option combinations
    */
    static validateOptions(string action, hash<auto> req) {
        if (exists req.webhook_secret && req.webhook_secret.typeCode() != NT_NULL
                && (!exists req.webhook_url || req.webhook_url.typeCode() == NT_NULL)) {
            throw "BFL-INPUT-ERROR", "webhook_secret requires webhook_url";
        }
        if (exists req.aspect_ratio && req.aspect_ratio.typeCode() == NT_STRING
                && req.aspect_ratio != "auto" && action !~ /video/) {
            if (req.aspect_ratio !~ /^\d+(?:\.\d+)?:\d+(?:\.\d+)?$/) {
                throw "BFL-INPUT-ERROR", "aspect_ratio must contain two positive numbers separated by a colon";
            }
            list<string> parts = req.aspect_ratio.split(":");
            float ratio = float(parts[0]) / (float(parts[1]) ?: 1.0);
            float limit = action =~ /kontext/ ? 7.0 / 3.0 : 21.0 / 9.0;
            if (float(parts[0]) <= 0 || float(parts[1]) <= 0 || ratio > limit || ratio < 1.0 / limit) {
                throw "BFL-INPUT-ERROR", "aspect_ratio is outside this model's documented range";
            }
        }
        hash<string, hash<auto>> images = {};
        foreach string name in (BflApi::MediaFields) {
            if (!exists req{name} || req{name}.typeCode() == NT_NULL) { continue; }
            auto file = req{name};
            auto bytes = file.typeCode() == NT_HASH ? file.content : file;
            if (bytes.typeCode() == NT_STRING && bytes =~ /^https?:\/\//) {
                if (name == "image_prompt" || (inlist(name, ("image", "mask"))
                        && inlist(action, ("start-fill", "start-finetuned-fill", "start-expand")))) {
                    throw "BFL-INPUT-ERROR", "this endpoint requires base64 media or uploaded bytes, not a reference URL";
                }
                hash<auto> url = parse_url(bytes);
                if (!url.host.val() || url.username.val() || url.password.val() || bytes =~ /[\r\n\\#]/) {
                    throw "BFL-INPUT-ERROR", sprintf("invalid media URL in %s", name);
                }
                # BFL fetches remote references; their content is checked by the service.
                continue;
            }
            if (bytes.typeCode() == NT_STRING) {
                try { bytes = parse_base64_string(bytes); }
                catch () { throw "BFL-INPUT-ERROR", sprintf("%s must contain valid base64 media", name); }
            }
            if (bytes.typeCode() != NT_BINARY || !bytes.size() || bytes.size() > 52428800) {
                throw "BFL-INPUT-ERROR", sprintf("%s must contain nonempty media of at most 50 MiB", name);
            }
            if (name == "draft_cache") { continue; }
            *string declared = file.typeCode() == NT_HASH ? file.mime_type : NOTHING;
            string mime = QoreMediaUtils::validateMediaContentType(bytes, declared == "auto" ? NOTHING : declared);
            if (inlist(name, ("video", "input_video", "start_video"))) {
                if (mime != "video/mp4") { throw "BFL-INPUT-ERROR", "video inputs must be MP4"; }
                hash<QoreMediaUtils::Mp4VideoInfo> info = QoreMediaUtils::getMp4VideoInfo(bytes);
                if (action == "start-video-edit" && (info.duration_seconds > 15.0
                        || info.width < 160 || info.height < 160 || info.frame_count < 17)) {
                    throw "BFL-INPUT-ERROR", "video editing requires at least 160 pixels per side and 17 frames, at most 15 seconds";
                }
                if (action == "start-video-upscale" && (info.duration_seconds > 20.0 || info.width * info.height > 3686400)) {
                    throw "BFL-INPUT-ERROR", "video upscaling requires at most 20 seconds and 3.7 megapixels per frame";
                }
                continue;
            }
            if (!inlist(mime, ("image/jpeg", "image/png", "image/webp"))) {
                throw "BFL-INPUT-ERROR", sprintf("%s must contain a JPEG, PNG, or WebP image", name);
            }
            if (name == "mask" && mime != "image/png") {
                throw "BFL-INPUT-ERROR", "image masks must use PNG";
            }
            images{name} = QoreMediaUtils::getImageDimensions(bytes);
        }
        if (images.mask && images.image && images.mask != images.image) {
            throw "BFL-INPUT-ERROR", "mask dimensions must match the input image";
        }
        if (action == "start-outpaint") {
            if (number(req.width) * number(req.height) > 4194304n) {
                throw "BFL-INPUT-ERROR", "outpainting is limited to a canvas of 4194304 pixels";
            }
            if (images.input_image && !req.auto_crop) {
                auto size = images.input_image;
                int left = req.reference_offset_x ?? (req.width - size.width) / 2;
                int top = req.reference_offset_y ?? (req.height - size.height) / 2;
                if (left < 0 || top < 0 || left + size.width > req.width || top + size.height > req.height) {
                    throw "BFL-INPUT-ERROR", "the reference extends outside the canvas; enable auto_crop or adjust placement";
                }
            }
        }
        if (action == "start-video") {
            if (req.duration.typeCode() != NT_NOTHING && req.duration != "auto" && req.duration.typeCode() != NT_INT) {
                throw "BFL-INPUT-ERROR", "video duration must be a whole number of seconds or auto";
            }
            if (req.draft && req.resolution != "hd") {
                throw "BFL-INPUT-ERROR", "draft generation requires HD resolution";
            }
            if (req.mode == "i2v") { validateKeyframes(req.keyframes, req.duration); }
        }
    }

    private static validateKeyframes(auto frames, auto duration) {
        if (frames.typeCode() == NT_STRING) { frames = (frames,); }
        if (frames.typeCode() != NT_LIST || !frames.size()) {
            throw "BFL-INPUT-ERROR", "keyframes must contain one through ten images or timed image pairs";
        }
        if (frames.size() == 2 && inlist(frames[0].typeCode(), (NT_INT, NT_FLOAT, NT_NUMBER))) {
            frames = (frames,);
        }
        if (frames.size() > 10) { throw "BFL-INPUT-ERROR", "at most ten keyframes are supported"; }
        bool timed = frames[0].typeCode() == NT_LIST;
        float previous = -1.0;
        foreach auto frame in (frames) {
            if (timed) {
                if (frame.typeCode() != NT_LIST || frame.size() != 2
                        || !inlist(frame[0].typeCode(), (NT_INT, NT_FLOAT, NT_NUMBER))
                        || frame[0] < 0 || frame[0] <= previous || frame[0] > (duration == "auto" ? 20 : duration)) {
                    throw "BFL-INPUT-ERROR", "timed keyframes must be ordered [seconds, image] pairs within the video";
                }
                previous = frame[0];
                frame = frame[1];
            }
            if (frame.typeCode() != NT_STRING || !frame.val()) {
                throw "BFL-INPUT-ERROR", "each keyframe must be a nonempty media URL or base64 image";
            }
            validateOptions("keyframe", {"image": frame});
        }
        if (!timed && frames.size() >= 3 && duration == "auto") {
            throw "BFL-INPUT-ERROR", "three or more untimed keyframes require a fixed duration";
        }
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        if (type != "finetunes") { return RestSchemaActionDataProvider::getReferenceDataImpl(type, action_opts); }
        BflOperation lister("list-finetunes", client);
        auto response = lister.doRequest({});
        if (response.finetunes.typeCode() != NT_LIST) {
            throw "BFL-RESPONSE-ERROR", "fine-tune inventory must contain a list";
        }
        list<hash<AllowedValueInfo>> result = ();
        hash<string, bool> seen = {};
        foreach auto item in (response.finetunes) {
            auto id = item.typeCode() == NT_STRING ? item : item.finetune_id ?? item.name;
            auto label = item.typeCode() == NT_HASH ? item.name : id;
            if (id.typeCode() != NT_STRING || !id.val()) {
                throw "BFL-RESPONSE-ERROR", "fine-tune inventory contains an entry without a usable identity";
            }
            if (!seen{id}) {
                push result, <AllowedValueInfo>{"value": id, "display_name": label.val() ? string(label) : id};
                seen{id} = True;
            }
        }
        return result;
    }
}
}
