#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Read-only qualification of existing live artifacts; never makes a service call.
# No source prepend: select the intended compiled artifacts explicitly.
%modern
%requires StabilityAiDataProvider
%requires QoreMediaUtils
%requires json

if (!ARGV[0].val()) { throw "ARTIFACT-CHECK-ERROR", "pass the live artifact directory"; }
AbstractDataProviderType media = Serializable::deserialize(StabilityAiOperation::MediaType.serialize());
foreach string action in (("generate-image", "media-facade", "remove-background", "upscale-fast", "wait-for-image")) {
    hash<auto> meta = parse_json(File::readTextFile(ARGV[0] + "/" + action + "-result.json"));
    binary data = File::readBinaryFile(ARGV[0] + "/" + action + ".png");
    QoreMediaUtils::validateMediaContentType(data, meta.content_type);
    if (data.size() != meta.bytes || QoreMediaUtils::getImageDimensions(data) != meta.dimensions
            || meta.status != "succeeded" || meta.finish_reason != "SUCCESS") {
        throw "ARTIFACT-CHECK-ERROR", "live artifact metadata differs: " + action;
    }
    hash<auto> result = {"status": "succeeded", "artifacts": ({"data": data, "content_type": meta.content_type,
        "seed": int(meta.seed), "finish_reason": meta.finish_reason},)};
    if (meta.job_id) { result.job_id = meta.job_id; }
    auto restored = media.acceptsValue(result);
    if (restored.artifacts[0].data !== data || restored.job_id != meta.job_id) {
        throw "ARTIFACT-CHECK-ERROR", "serialized output type lost live bytes or job identity";
    }
    printf("%s: bytes=%d dimensions=%s sha256=%s restored-type=accepted\n", action, data.size(),
        make_json(meta.dimensions), digest("sha256", data).toHex());
}
