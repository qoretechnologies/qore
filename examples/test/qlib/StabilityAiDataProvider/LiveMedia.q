#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Called only by live-media.py after a persistent credit reservation. No source prepend: qualify compiled artifacts.
%modern
%requires StabilityAiDataProvider
%requires QoreMediaUtils
%requires json

if (ENV.QORE_STABILITYAI_LIVE_MEDIA != "1" || !ENV.STABILITYAI_APIKEY.val()) {
    throw "LIVE-TEST-DISABLED", "use the explicitly opt-in ledger runner with a privately loaded credential";
}
hash<auto> options = {"apikey": ENV.STABILITYAI_APIKEY, "timeout": 120s, "connect_timeout": 10s};
if (ENV.QORE_STABILITYAI_LIVE_ORGANIZATION.val()) { options.organization = ENV.QORE_STABILITYAI_LIVE_ORGANIZATION; }
StabilityAiRestClientIo client(options);
StabilityAiDataProvider::StabilityAiDataProvider root(client);
string action = ARGV[0];
if (action == "balance") {
    auto balance = root.getChildProvider("get-balance").doRequest({});
    printf("%s\n", make_json({"credits": balance.credits}));
    exit(0);
}
if (!ENV.QORE_STABILITYAI_RESERVATION.val() || !ARGV[1].val()) {
    throw "LIVE-TEST-DISABLED", "a ledger reservation and output directory are required";
}
string directory = ARGV[1];
hash<auto> request;
hash<auto> result;
*AbstractDataProviderType output_type;
string prompt = "Studio product photograph of one matte blue ceramic coffee mug with a single curved handle, "
    "centered on a plain white background, soft even lighting, full object visible, no text or logos.";
if (action == "generate-image") {
    request = {"model": "core", "prompt": prompt, "seed": 5422, "output_format": "png", "aspect_ratio": "1:1", "negative_prompt": ""};
    AbstractDataProvider provider = root.getChildProvider(action);
    result = provider.doRequest(request);
    output_type = provider.getResponseType();
} else if (action == "media-facade") {
    request = {"model": "core", "prompt": "A single orange wooden sphere on a plain pale blue studio background, no text.",
        "seed": 5423, "output_format": "png"};
    hash<auto!> config = {"backend": "stability-ai", "api_key": ENV.STABILITYAI_APIKEY, "model": "core"};
    if (exists options.organization) { config.provider_options = {"organization": options.organization}; }
    auto generator = QoreMediaUtils::MediaProviderRegistry::createImageGenerator(cast<hash<QoreMediaUtils::ImageGeneratorConfig>>(config));
    result = map {$1.key: $1.value}, generator.generateImage(request.prompt, {"seed": 5423}).pairIterator();
} else if (action == "wait-for-image") {
    request = {"job_id": ARGV[2], "operation_timeout": 5m, "poll_interval": 10s};
    AbstractDataProvider provider = root.getChildProvider(action);
    result = provider.doRequest(request);
    output_type = provider.getResponseType();
} else {
    binary input = File::readBinaryFile(directory + (action == "upscale-fast" ? "/upscale-input.png" : "/generate-image.png"));
    hash<auto> file = {"name": "qore-original-mug.png", "mime_type": "image/png", "content": input};
    if (action == "start-background-relight") {
        request = {"subject_image": file, "background_prompt": "a simple pale green studio background",
            "foreground_prompt": "a matte blue ceramic coffee mug", "seed": 5422, "output_format": "png",
            "keep_original_background": False};
    } else if (inlist(action, ("remove-background", "upscale-fast"))) {
        request = {"image": file, "output_format": "png"};
    } else { throw "LIVE-TEST-DISABLED", "action is outside the reviewed live plan"; }
    AbstractDataProvider provider = root.getChildProvider(action);
    result = provider.doRequest(request);
    output_type = provider.getResponseType();
}
hash<auto!> safe = {"action": action, "status": result.status ?? "succeeded", "job_id": result.job_id,
    "client_module": get_module_hash().StabilityAiRestClient.filename,
    "provider_module": get_module_hash().StabilityAiDataProvider.filename};
if (result.status != "pending") {
    binary content = result.image ?? result.artifacts[0].data;
    File output();
    output.open(directory + "/" + action + ".png", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    output.write(content);
    output.close();
    safe.bytes = content.size();
    safe.dimensions = QoreMediaUtils::getImageDimensions(content);
    safe.content_type = result.content_type ?? result.artifacts[0].content_type;
    safe.seed = result.seed ?? result.artifacts[0].seed;
    safe.finish_reason = result.finish_reason ?? result.metadata.finish_reason ?? result.artifacts[0].finish_reason;
}
File evidence();
evidence.open(directory + "/" + action + "-result.json", O_CREAT | O_WRONLY | O_TRUNC, 0600);
evidence.write(make_json(safe));
evidence.close();
if (output_type) {
    AbstractDataProviderType restored = Serializable::deserialize(output_type.serialize());
    restored.acceptsValue(result);
}
printf("%s\n", make_json(safe));
