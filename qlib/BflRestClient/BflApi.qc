# -*- mode: qore; indent-tabs-mode: nil -*-
#! Pinned BFL operation contracts shared by app actions and media backends
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

namespace Priv {
    const BflSchemaText = File::readTextFile(get_script_dir() + "/bfl-openapi.json");
}

public namespace BflRestClient {
#! Offline action contract and lightweight operation inventory
public class BflApi {
    public {
        # Identities are available before schema parsing or action construction.
        const Operations = {
            "get-finetune": ("GET", "/v1/finetune_details", "Get Fine-Tune Details", "Read the settings of an existing fine-tune.", "json"),
            "list-finetunes": ("GET", "/v1/my_finetunes", "List Fine-Tunes", "List fine-tunes owned by the selected account.", "json"),
            "delete-finetune": ("POST", "/v1/delete_finetune", "Delete Fine-Tune", "Permanently delete a selected fine-tune.", "json"),
            "get-credits": ("GET", "/v1/credits", "Get Credit Balance", "Check the available balance before generating media.", "json"),
            "get-result": ("GET", "/v1/get_result", "Get Result on This Region", "Read a job known to belong to the connection's region.", "result"),
            "start-flux-pro-1-1": ("POST", "/v1/flux-pro-1.1", "Start FLUX Pro 1.1 Image", "Create an image with FLUX Pro 1.1.", "image"),
            "start-flux-dev": ("POST", "/v1/flux-dev", "Start FLUX Dev Image", "Create a concept image with FLUX Dev.", "image"),
            "start-flux-pro-1-1-ultra": ("POST", "/v1/flux-pro-1.1-ultra", "Start FLUX Pro 1.1 Ultra Image", "Create a high-resolution image with FLUX Ultra.", "image"),
            "start-fill": ("POST", "/v1/flux-pro-1.0-fill", "Start Image Fill", "Replace a masked or transparent region using a prompt.", "image"),
            "start-expand": ("POST", "/v1/flux-pro-1.0-expand", "Start Image Expansion", "Extend an image by a specified number of pixels.", "image"),
            "start-finetuned-fill": ("POST", "/v1/flux-pro-1.0-fill-finetuned", "Start Legacy Fine-Tuned Fill", "Fill an image with an existing legacy fine-tune.", "image"),
            "start-finetuned-ultra": ("POST", "/v1/flux-pro-1.1-ultra-finetuned", "Start Legacy Fine-Tuned Ultra", "Create an image with a legacy Ultra fine-tune.", "image"),
            "start-flux-2-flex": ("POST", "/v1/flux-2-flex", "Start FLUX.2 Flex Image", "Control sampling steps and guidance for detailed image work.", "image"),
            "start-flux-2-klein-9b": ("POST", "/v1/flux-2-klein-9b", "Start FLUX.2 Klein 9B Image", "Generate or edit images with up to four references.", "image"),
            "start-flux-2-klein-9b-preview": ("POST", "/v1/flux-2-klein-9b-preview", "Start FLUX.2 Klein 9B Preview", "Try the preview Klein model for image generation or editing.", "image"),
            "start-flux-2-klein-4b": ("POST", "/v1/flux-2-klein-4b", "Start FLUX.2 Klein 4B Image", "Generate or edit images with the smaller Klein model.", "image"),
            "start-flux-2-max": ("POST", "/v1/flux-2-max", "Start FLUX.2 Max Image", "Create detailed images with up to eight references.", "image"),
            "start-flux-2-pro-preview": ("POST", "/v1/flux-2-pro-preview", "Start FLUX.2 Pro Preview", "Try the preview Pro model for image generation or editing.", "image"),
            "start-flux-2-pro": ("POST", "/v1/flux-2-pro", "Start FLUX.2 Pro Image", "Generate or edit images with the production Pro model.", "image"),
            "start-video": ("POST", "/v1/flux-3-video", "Start FLUX 3 Video", "Create, continue, or enhance a video with optional audio.", "video"),
            "start-video-upscale": ("POST", "/v1/flux-tools/video-upscale-v1", "Start Video Upscale", "Enlarge a short video while preserving its composition.", "video"),
            "start-video-edit": ("POST", "/v1/flux-tools/video-edit-v1", "Start Video Edit", "Transform a short video using an editing instruction.", "video"),
            "start-outpaint": ("POST", "/v1/flux-tools/outpainting-v1", "Start Image Outpainting", "Fit an image to a new canvas with generated borders.", "image"),
            "start-try-on-v1": ("POST", "/v1/flux-tools/vto-v1", "Start Virtual Try-On v1", "Show how a garment looks on a person.", "image"),
            "start-try-on-v2": ("POST", "/v1/flux-tools/vto-v2", "Start Virtual Try-On v2", "Create a higher-resolution garment try-on image.", "image"),
            "start-erase": ("POST", "/v1/flux-tools/erase-v1", "Start Object Erasure", "Remove unwanted objects selected by an image mask.", "image"),
            "start-deblur": ("POST", "/v1/flux-tools/deblur-v1", "Start Image Deblurring", "Restore details in a blurred image.", "image"),
            "start-flux-kontext-max": ("POST", "/v1/flux-kontext-max", "Start FLUX Kontext Max Image", "Generate or edit an image with Kontext Max.", "image"),
            "start-flux-kontext-pro": ("POST", "/v1/flux-kontext-pro", "Start FLUX Kontext Pro Image", "Generate or edit an image with Kontext Pro.", "image"),
            "report-license-usage": ("POST", "/v1/licenses/models/{model_slug}/usage", "Report Licensed Model Usage", "Submit a billing-affecting report for a licensed model.", "json"),
            "start-flux-pro": ("POST", "/v1/flux-pro", "Start FLUX Pro Image", "Use the documented original FLUX Pro compatibility endpoint.", "image"),
            "start-lora-4b": ("POST", "/v1/flux-2-klein-4b-finetuned", "Start Klein 4B LoRA Image", "Generate with a matching Klein 4B fine-tune in public beta.", "image"),
            "start-lora-9b": ("POST", "/v1/flux-2-klein-9b-finetuned", "Start Klein 9B LoRA Image", "Generate with a matching Klein 9B fine-tune in public beta.", "image"),
            "start-lora-9b-kv": ("POST", "/v1/flux-2-klein-9b-kv-finetuned", "Start Klein 9B KV LoRA Image", "Generate with a matching cached Klein fine-tune in public beta.", "image"),
            "start-lora-9b-kv-bf16": ("POST", "/v1/flux-2-klein-9b-kv-bf16-finetuned", "Start Klein 9B KV BF16 LoRA", "Generate with a matching BF16 Klein fine-tune in public beta.", "image"),
            "start-lora-base-4b": ("POST", "/v1/flux-2-klein-base-4b-finetuned", "Start Klein Base 4B LoRA Image", "Use the documented common fields for a Base 4B fine-tune.", "image"),
            "start-lora-base-9b": ("POST", "/v1/flux-2-klein-base-9b-finetuned", "Start Klein Base 9B LoRA Image", "Use the documented common fields for a Base 9B fine-tune.", "image"),
        };
        const MediaFields = ("image", "mask", "image_prompt", "input_image", "input_image_2", "input_image_3",
            "input_image_4", "input_image_5", "input_image_6", "input_image_7", "input_image_8", "person", "garment",
            "input_video", "video", "start_video", "draft_cache");
    }
    private {
        static Mutex mutex();
        static *OpenApi3::OpenApi3Schema schema;
        static *RestSchemaActionSet actions;
    }

    #! Returns a stable submission action for an existing public endpoint path
    /** @param path relative service path
        @return action identity
        @throw BFL-INPUT-ERROR unsupported path or non-generation operation
    */
    static string actionForPath(string path) {
        foreach hash<auto> entry in (Operations.pairIterator()) {
            if (entry.value[1] == path && inlist(entry.value[4], ("image", "video"))) { return entry.key; }
        }
        throw "BFL-INPUT-ERROR", "unsupported BFL generation endpoint; select a published start action";
    }

    #! Generates, waits for, and downloads verified media within one overall deadline
    /** @param client selected BFL client
        @param model published image or video model
        @param req model-specific fields plus optional job_timeout, poll_interval, and max_bytes
        @return completed job with original metadata and a verified media object
        @throw BFL-INPUT-ERROR unsupported model or request
        @throw BFL-JOB-TIMEOUT the overall deadline expired; retain the existing job identity
        @throw BFL-DOWNLOAD-ERROR download failed validation
    */
    static hash<auto> generateMedia(BflRestClientIo client, string model, hash<auto> req) {
        date start = now_us();
        BflJobs::validateControls(client, req);
        *string path = ImageModelPathMap{model} ?? VideoModelPathMap{model};
        if (!path) {
            throw "BFL-INPUT-ERROR", "unsupported model; the retired flux-1.1-pro-video alias must be replaced explicitly with flux-3-video";
        }
        timeout budget = req.job_timeout ?? DefaultJobTimeout;
        int remaining = int(budget) - get_duration_milliseconds(now_us() - start);
        if (remaining <= 0) { throw "BFL-JOB-TIMEOUT", "deadline expired before submission; no job was submitted"; }
        hash<auto> done = client.submitAndWait(path, req - ("job_timeout", "poll_interval", "max_bytes"),
            req + {"job_timeout": remaining});
        remaining = int(budget) - get_duration_milliseconds(now_us() - start);
        if (remaining <= 0) { BflJobs::timedOut(done); }
        try {
            done.media = downloadMedia(done.result.sample, remaining, req.max_bytes ?? 104857600,
                client.getUrl() =~ /^http:/ ? client.getUrl() : NOTHING);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err =~ /TIMEOUT/) { BflJobs::timedOut(done, done); }
            throw "BFL-DOWNLOAD-ERROR", "the completed job's media could not be downloaded", BflJobs::safeIdentity(done, done);
        }
        bool video = VideoModelPathMap.hasKey(model);
        if (video ? done.media.content_type != "video/mp4" : done.media.content_type !~ /^image\//) {
            throw "BFL-DOWNLOAD-ERROR", "downloaded media does not match the selected model", BflJobs::safeIdentity(done, done);
        }
        return done;
    }

    #! Returns the immutable pinned schema without accessing the service
    /** @return parsed offline contract
        @throw OPENAPI3-ERROR invalid installed contract
    */
    static OpenApi3::OpenApi3Schema getSchema() {
        AutoLock lock(mutex);
        return getSchemaIntern();
    }
    private static OpenApi3::OpenApi3Schema getSchemaIntern() {
        if (!schema) { schema = OpenApi3::OpenApi3Loader::fromString(Priv::BflSchemaText, True); }
        return schema;
    }

    #! Builds typed action forms from the pinned contract and reviewed metadata
    /** @return reusable action set
        @throw REST-SCHEMA-ACTION-ERROR contract and inventory disagree
    */
    static RestSchemaActionSet getActionSet() {
        AutoLock lock(mutex);
        if (actions) { return actions; }
        hash<auto> document = parse_json(Priv::BflSchemaText);
        list<hash<RestSchemaActionInfo>> manifest = ();
        foreach hash<auto> entry in (Operations.pairIterator()) {
            hash<RestSchemaActionInfo> item = <RestSchemaActionInfo>{"action": entry.key,
                "method": entry.value[0], "path": entry.value[1], "display_name": entry.value[2],
                "short_desc": entry.value[3], "desc": entry.value[3], "metadata": {"kind": entry.value[4]}};
            if (inlist(entry.value[4], ("image", "video"))) {
                item.desc += "\n\nThis action consumes credits and returns a job identity. Use **Wait for Result** "
                    "with both `id` and `polling_url`, then download the result before its temporary URL expires.";
            }
            if (entry.key =~ /^(start-finetuned-|get-finetune|list-finetunes|delete-finetune)/) {
                item.desc += "\n\n**Availability:** the current schema retains this operation, but older fine-tune "
                    "workflows were deprecated. Account support must be verified.";
            }
            if (entry.key == "delete-finetune") {
                item.desc += "\n\n**Permanent deletion:** this removes the selected model. Check its ID before running.";
            }
            if (entry.key == "report-license-usage") {
                item.desc += "\n\n**Billing:** report only actual licensed model usage. This is separate from inference "
                    "and can affect charges; it is not a connection test.";
            }
            if (entry.key =~ /^start-lora-/) {
                item.desc += "\n\n**Public beta:** the LoRA must match the base model and precision. Base variants expose "
                    "only the common fields currently documented. Uploads and sharing are managed in the BFL Dashboard.";
            }
            hash<auto> operation = document.paths{entry.value[1]}{entry.value[0].lwr()};
            hash<string, hash<RestSchemaFieldOverlayInfo>> fields = {};
            if (operation.requestBody) {
                hash<auto> body = resolve(document, operation.requestBody.content."application/json".schema);
                fields = overlays(document, body, "body.");
                if (entry.key == "start-video") {
                    fields."body.mode".allowed_values = map <AllowedValueInfo>{"value": $1,
                        "display_name": choiceLabel($1)}, ("t2v", "i2v", "v2v", "draft_enhance");
                }
            }
            foreach hash<auto> parameter in (operation.parameters) {
                hash<RestSchemaFieldOverlayInfo> field = fieldInfo(document, parameter.name,
                    resolve(document, parameter.schema));
                fields{parameter.in + "." + parameter.name} = field;
            }
            item.fields = fields;
            push manifest, item;
        }
        actions = new RestSchemaActionSet(getSchemaIntern(), manifest, {
            "finetunes": <RestSchemaReferenceDataInfo>{"name": "finetunes", "action": "list-finetunes",
                "records_path": "finetunes", "value_field": "finetune_id", "display_field": "name",
                "desc": "Fine-tune available to the selected account."},
        });
        return actions;
    }

    private static hash<auto> resolve(hash<auto> document, hash<auto> node) {
        if (node."$ref") {
            return resolve(document, document.components.schemas{node."$ref".split("/")[-1]}) + (node - "$ref");
        }
        return node;
    }

    private static hash<string, hash<RestSchemaFieldOverlayInfo>> overlays(hash<auto> document,
            hash<auto> node, string prefix) {
        hash<string, hash<RestSchemaFieldOverlayInfo>> result = {};
        foreach hash<auto> branch in ((node.oneOf ?? ()) + (node.anyOf ?? ())) {
            result += overlays(document, resolve(document, branch), prefix);
        }
        foreach hash<auto> field in (node.properties.pairIterator()) {
            hash<auto> property = resolve(document, field.value);
            hash<RestSchemaFieldOverlayInfo> overlay = fieldInfo(document, field.key, property);
            if (inlist(field.key, MediaFields)) { overlay.codec = RestSchemaActionCodecs::Base64OrUrl; }
            if (field.key == "webhook_secret") { overlay.sensitive = True; }
            if (field.key == "prompt") { overlay.example_value = "A blue ceramic mug on a plain studio background."; }
            if (field.key == "finetune_id") {
                overlay.allowed_values_creatable = True;
                overlay.ref_data = "finetunes";
                overlay.example_value = "my-portrait-lora";
            }
            overlay.preselected = inlist(field.key, node.required ?? ()) || inlist(field.key, ("output_format", "mode"));
            result{prefix + field.key} = overlay;
        }
        return result;
    }

    private static hash<RestSchemaFieldOverlayInfo> fieldInfo(hash<auto> document, string name, hash<auto> schema) {
        *list<string> help = Priv::BflFieldText{name};
        string title = help[0] ?? schema.title ?? replace(name, "_", " ");
        title = title[0].upr() + title.substr(1);
        hash<RestSchemaFieldOverlayInfo> result = <RestSchemaFieldOverlayInfo>{"display_name": title,
            "short_desc": title, "desc": help[1] ?? schema.description ?? (title + ".")};
        list<auto> values = schema."enum" ?? (name == "version" && exists schema.const ? (schema.const,) : ());
        foreach hash<auto> branch in (schema.anyOf ?? ()) {
            branch = resolve(document, branch);
            if (!branch."enum" && !exists branch.const && branch.type != "null") {
                values = ();
                break;
            }
            values += branch."enum" ?? (exists branch.const ? (branch.const,) : ());
        }
        if (values) {
            result.allowed_values = map <AllowedValueInfo>{"value": $1, "display_name": choiceLabel($1)}, values;
            if (name == "creativity") {
                result.allowed_values = (<AllowedValueInfo>{"value": 0, "display_name": "Preserve Source"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Creative Detail"});
            }
        }
        return result;
    }

    private static string choiceLabel(auto value) {
        const hash<string, string> Labels = {"jpeg": "JPEG", "png": "PNG", "webp": "WebP", "hd": "HD", "fhd": "Full HD",
            "qhd": "Quad HD", "uhd": "Ultra HD", "auto": "Automatic", "t2v": "Text to Video",
            "i2v": "Image to Video", "v2v": "Video Continuation", "draft_enhance": "Enhance Draft",
            "latest": "Latest Release", "fast": "Fast", "high": "High Quality"};
        return Labels{string(value)} ?? replace(string(value), "_", " ");
    }
}
}
