#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Read-only checks of saved live media; no source prepend and no service calls.
%modern
%requires BflDataProvider
%requires QoreMediaUtils
%requires json

if (!ARGV[0].val()) { throw "ARTIFACT-CHECK-ERROR", "pass the private live artifact directory"; }
foreach string action in (("generate-image", "image-facade", "edit-image", "video-facade")) {
    hash<auto> meta = parse_json(File::readTextFile(ARGV[0] + "/" + action + "-result.json"));
    string extension = action == "video-facade" ? ".mp4" : ".png";
    binary data = File::readBinaryFile(ARGV[0] + "/" + action + extension);
    QoreMediaUtils::validateMediaContentType(data, meta.media.content_type);
    hash<auto> dimensions = action == "video-facade" ? QoreMediaUtils::getMp4VideoInfo(data)
        : QoreMediaUtils::getImageDimensions(data);
    if (data.size() != meta.bytes || dimensions.width != meta.media.width || dimensions.height != meta.media.height
            || digest("sha256", data).toHex() != meta.sha256 || meta.status != "Ready") {
        throw "ARTIFACT-CHECK-ERROR", "saved media differs from verified result: " + action;
    }
    hash<auto> done = parse_json(File::readTextFile(ARGV[0] + "/" + action + "-private.json"));
    done.media = meta.media + {"data": data};
    AbstractDataProviderType restored = BflDataProvider::BflCompletedJob;
    for (int pass = 0; pass < 3; ++pass) { restored = Serializable::deserialize(restored.serialize()); }
    auto checked = restored.acceptsValue(done);
    if (checked.media.data !== data || checked.id != meta.id || checked.result.seed != meta.seed) {
        throw "ARTIFACT-CHECK-ERROR", "serialized type lost real media, identity, or reported seed";
    }
    printf("%s: bytes=%d dimensions=%s restored-type=accepted\n", action, data.size(), make_json(dimensions));
}
