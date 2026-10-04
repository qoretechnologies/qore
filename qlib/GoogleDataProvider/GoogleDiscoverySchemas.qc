# -*- mode: qore; indent-tabs-mode: nil -*-
#! Complete, versioned Google Discovery documents for offline metadata resolution
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

namespace Priv {
    # Read immutable module resources during initialization, including under a sandbox.
    # A missing or mismatched resource fails module loading; no failure is cached, so repairing
    # the installed package permits a subsequent load to succeed.
    const ShippedGoogleSchemas = loadShippedGoogleSchemas();

    hash<string, hash<auto>> sub loadShippedGoogleSchemas() {
        hash<string, hash<auto>> schemas;
        foreach hash<string, string> pin in (
            {"api": "calendar", "version": "v3", "revision": "20260925",
                "sha256": "7028e89f8dbd0696230cbf8e60646e33ab6afa495a2c59c0f259f9deba4bb85e"},
            {"api": "gmail", "version": "v1", "revision": "20260727",
                "sha256": "7591d33ec87e4ef83551f71630213a89ac349becebe9bd9b3e4ce3851dede0fe"}) {
            string file = sprintf("%s/%s.%s.json", get_script_dir(), pin.api, pin.version);
            on_error rethrow "GOOGLE-SCHEMA-UNAVAILABLE", sprintf("cannot load pinned Google %s %s schema: %s",
                pin.api, pin.version, $1.desc);
            binary data = ReadOnlyFile::readBinaryFile(file);
            if (digest("sha256", data).toHex() != pin.sha256) {
                throw "GOOGLE-SCHEMA-UNAVAILABLE", sprintf("%y does not match the shipped revision %s (SHA-256 %s)",
                    file, pin.revision, pin.sha256);
            }
            hash<auto> schema = parse_json(data.toString());
            if (schema.name != pin.api || schema.version != pin.version || schema.revision != pin.revision
                    || schema.kind != "discovery#restDescription" || !schema.schemas || !schema.resources) {
                throw "GOOGLE-SCHEMA-UNAVAILABLE", sprintf("%y is not the pinned complete Discovery document", file);
            }
            schemas{pin.api} = schema;
        }
        return schemas;
    }
}
