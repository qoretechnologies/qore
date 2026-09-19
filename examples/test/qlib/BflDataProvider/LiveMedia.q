#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Compiled-artifact helper: invoke only through the explicitly enabled ledger runner.
%modern
%requires BflDataProvider
%requires QoreMediaUtils
%requires json

if (ENV.QORE_BFL_LIVE_MEDIA != "1" || !ENV.BFL_APIKEY.val()) {
    throw "LIVE-TEST-DISABLED", "use the opt-in ledger runner with a privately loaded credential";
}
BflRestConnection connection({"name": "blackforestlabs", "url": ENV.QORE_BFL_LIVE_URL,
    "opts": {"apikey": ENV.BFL_APIKEY, "timeout": 120s, "connect_timeout": 10s}});
BflRestClientIo client = connection.getAsync(False);
BflDataProvider::BflDataProvider root(client);
string action = ARGV[0];
if (action == "balance") {
    printf("%s\n", make_json(root.getChildProvider("get-credits").doRequest({})));
    exit(0);
}
if (action == "inventory") {
    auto response = root.getChildProvider("list-finetunes").doRequest({});
    printf("%s\n", make_json({"fine_tune_count": response.finetunes.size()}));
    exit(0);
}
if (!ENV.QORE_BFL_RESERVATION.val() || !ARGV[1].val()) {
    throw "LIVE-TEST-DISABLED", "a reservation and private output directory are required";
}
string directory = ARGV[1];
hash<auto> done;
string prompt = "Studio product photograph of one matte blue ceramic coffee mug with a single curved handle, "
    "centered on a plain white background, soft even lighting, full object visible, no text or logos.";
try {
    if (action == "generate-image") {
        done = root.getChildProvider("generate-image").doRequest({"model": "flux-pro-1.1", "prompt": prompt,
            "width": 512, "height": 512, "seed": 0, "output_format": "png", "job_timeout": 10m});
    } else if (action == "image-facade") {
        auto result = QoreMediaUtils::MediaProviderRegistry::createImageGenerator(<QoreMediaUtils::ImageGeneratorConfig>{
            "backend": "bfl", "api_key": ENV.BFL_APIKEY, "url": client.getUrl(), "model": "flux-2-klein-4b",
            "provider_options": {"width": 512, "height": 512, "job_timeout": 10m}, "output_format": "png"})
            .generateImage(prompt, {"seed": 0});
        done = result.metadata + {"media": result.metadata{"width", "height"}
            + {"data": result.image, "content_type": result.content_type}};
    } else if (action == "edit-image") {
        binary source = File::readBinaryFile(directory + "/generate-image.png");
        auto job = root.getChildProvider("start-flux-2-klein-4b").doRequest({
            "prompt": "Change only the mug color from blue to bright red. Keep the same mug shape, handle, camera, "
                "lighting, and plain white background. No text or logos.",
            "input_image": {"name": "original-blue-mug.png", "mime_type": "image/png", "content": source},
            "width": 512, "height": 512, "seed": 0, "output_format": "png"});
        File identity();
        identity.open(directory + "/edit-image-job-private.json", O_CREAT | O_WRONLY | O_TRUNC, 0600);
        identity.write(make_json(job)); identity.close();
        done = root.getChildProvider("wait-for-result").doRequest(job{"id", "polling_url"} + {"job_timeout": 10m});
        done.media = root.getChildProvider("download-result").doRequest(done{"id", "polling_url"});
    } else if (action == "video-facade") {
        auto result = QoreMediaUtils::MediaProviderRegistry::createVideoGenerator(<QoreMediaUtils::VideoGeneratorConfig>{
            "backend": "bfl", "api_key": ENV.BFL_APIKEY, "url": client.getUrl(), "model": "flux-3-video",
            "duration_seconds": 5, "aspect_ratio": "16:9",
            "provider_options": {"mode": "t2v", "resolution": "hd", "draft": True,
                "generate_audio": False, "job_timeout": 15m}}).generateVideo(
                "A matte blue ceramic coffee mug slowly rotates on a white studio turntable. "
                "Locked camera, even lighting, simple white background, no text or logos.", {});
        done = result.metadata + {"media": result.metadata{"width", "height", "duration_seconds", "frame_count",
            "frame_rate", "has_audio"} + {"data": result.video, "content_type": result.content_type}};
    } else { throw "LIVE-TEST-DISABLED", "action is outside the reviewed plan"; }
    string extension = action == "video-facade" ? ".mp4" : ".png";
    File media();
    media.open(directory + "/" + action + extension, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    media.write(done.media.data); media.close();
    # Retain routed and delivery identities privately before secondary assertions.
    File metadata();
    metadata.open(directory + "/" + action + "-private.json", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    metadata.write(make_json(done - "media")); metadata.close();
    hash<auto> safe = {"action": action, "id": done.id, "status": done.status, "cost": done.cost,
        "seed": done.result.seed, "bytes": done.media.data.size(), "media": done.media - "data",
        "sha256": digest("sha256", done.media.data).toHex(),
        "client_module": get_module_hash().BflRestClient.filename,
        "provider_module": get_module_hash().BflDataProvider.filename};
    File evidence();
    evidence.open(directory + "/" + action + "-result.json", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    evidence.write(make_json(safe)); evidence.close();
    AbstractDataProviderType restored = BflDataProvider::BflCompletedJob;
    for (int pass = 0; pass < 3; ++pass) { restored = Serializable::deserialize(restored.serialize()); }
    restored.acceptsValue(done);
    printf("%s\n", make_json(safe + {"restored_type": "accepted"}));
} catch (hash<ExceptionInfo> ex) {
    File failure();
    failure.open(directory + "/" + action + "-failure-private.json", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    failure.write(make_json({"error": ex.err, "job_id": ex.arg.job_id, "polling_url": ex.arg.polling_url,
        "last_status": ex.arg.last_status, "status_code": ex.arg.status_code})); failure.close();
    printf("%s\n", make_json({"error": ex.err, "job_id": ex.arg.job_id, "status_code": ex.arg.status_code,
        "reservation_retained": True}));
    exit(1);
}
