#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# OpenApi3HubScans.q <components>: builds the type of a schema that every component of a document refers back to
# through an "anyOf" alternative, as the expandable fields of Stripe's schema do, and writes as JSON the number of
# graph nodes that the garbage collector's scans walked while the type was built; used by
# OpenApi3TypeCacheLifetime.qtest, which runs it with scan accounting enabled (QORE_SCAN_STATS)

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires json
%requires ../../../../qlib/OpenApi3.qm

int n = (shift ARGV ?? "40").toInt();

hash<auto!> schemas = {};
hash<auto!> hub_properties = {"id": {"type": "string"}};
for (int i = 0; i < n; ++i) {
    string name = "C" + i;
    schemas{name} = {
        "type": "object",
        "properties": {
            "id": {"type": "string", "description": "The identifier of the object."},
            "label": {"type": "string"},
            "hub": {"anyOf": ({"type": "string"}, {"\$ref": "#/components/schemas/Hub"})},
            "next": {"anyOf": ({"type": "string"}, {"\$ref": "#/components/schemas/C" + ((i + 1) % n)})},
            "items": {"type": "array", "items": {"\$ref": "#/components/schemas/C" + ((i + 7) % n)}},
        },
        "required": ("id",),
    };
    hub_properties{"c" + i} = {"anyOf": ({"type": "string"}, {"\$ref": "#/components/schemas/" + name})};
}
schemas.Hub = {"type": "object", "properties": hub_properties};

OpenApi3Schema doc = OpenApi3Loader::fromString(make_json({
    "openapi": "3.0.3",
    "info": {"title": "Hub", "version": "1"},
    "paths": {},
    "components": {"schemas": schemas},
}), True);

*hash<string, hash<auto>> sub scan_stats() {
    return map {$1.root: $1}, get_scan_stats();
}

*hash<string, hash<auto>> before = scan_stats();
AbstractDataField field = doc.getFieldFromSchema("body", "the body", doc.components.schemas.Hub);
*hash<string, hash<auto>> after = scan_stats();

int nodes = 0;
foreach hash<auto> i in (after.pairIterator()) {
    nodes += i.value.nodes - (before{i.key}.nodes ?? 0);
}
printf("%s\n", make_json({
    "components": n,
    "fields": field.getType().getFields().size(),
    "nodes": nodes,
}));
