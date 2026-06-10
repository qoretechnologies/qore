#!/bin/bash
# Regression: single-file qcc -L must defer static variable initialization
# when a sibling class constant is represented by a pending AOT shell.
#
# The shape mirrors QorusMapManager:
#   - Manager has static AsyncMetadata queues = new AsyncMetadata(omqmap)
#   - AsyncMetadata has a class constant that refers back to
#     Manager::CommonMetadata
#   - Manager has static SlaMetadata slas = new SlaMetadata(omqmap)
#   - SlaMetadata's constructor calls a static metadata method that refers
#     back to Manager constants
# Metadata-only sibling preload used to leave the base constructor call with
# an unusable pending constant argument and fail with RUNTIME-OVERLOAD-ERROR
# during Manager's parse commit.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-${QORE_QCC:-./build/qcc}}"
mkdir -p "${TMP}/qo" "${TMP}/single"

cat > "${TMP}/Base.qc" <<'QORE'
%modern

hashdecl MetaFieldInfo {
    string display_name;
    string type;
}

class OmqMap {
}

class AbstractMetadata {
    private {
        string type;
        hash<string, hash<MetaFieldInfo>> metadata;
        OmqMap omqmap;
    }

    constructor(string type, hash<string, hash<MetaFieldInfo>> metadata, OmqMap omqmap) {
        self.type = type;
        self.metadata = metadata;
        self.omqmap = omqmap;
    }

    string getType() {
        return type;
    }
}

class ManagerBase {
    private {
        static OmqMap omqmap();
    }
}
QORE

cat > "${TMP}/AsyncMetadata.qc" <<'QORE'
%modern

class AsyncMetadata inherits AbstractMetadata {
    public {
        const AsyncMetadata = Manager::CommonMetadata + {
            "groups": Manager::GroupsField,
        };
    }

    constructor(OmqMap omqmap) : AbstractMetadata("queue", AsyncMetadata, omqmap) {
    }
}
QORE

cat > "${TMP}/SlaMetadata.qc" <<'QORE'
%modern

class SlaMetadata inherits AbstractMetadata {
    public static hash<string, hash<MetaFieldInfo>> getSlaMetadata() {
        return Manager::NameDescFields + {
            "group_namespace": Manager::GroupNamespaceField,
            "groups": Manager::GroupsField,
        };
    }

    constructor(OmqMap omqmap) : AbstractMetadata("sla", SlaMetadata::getSlaMetadata(), omqmap) {
    }
}
QORE

cat > "${TMP}/Manager.qc" <<'QORE'
%modern

class Manager inherits ManagerBase {
    public {
        static AsyncMetadata queues = new AsyncMetadata(omqmap);
        static SlaMetadata slas = new SlaMetadata(omqmap);

        const GroupsField = <MetaFieldInfo>{
            "display_name": "Groups",
            "type": "list",
        };

        const GroupNamespaceField = <MetaFieldInfo>{
            "display_name": "Group Namespace",
            "type": "int",
        };

        const NameDescFields = {
            "name": <MetaFieldInfo>{
                "display_name": "Name",
                "type": "string",
            },
        };

        const CommonMetadata = {
            "name": <MetaFieldInfo>{
                "display_name": "Name",
                "type": "string",
            },
        };

    }

    static string getQueueType() {
        return queues.getType();
    }

    static string getSlaType() {
        return slas.getType();
    }
}
QORE

"${QCC}" -c --output-dir="${TMP}/qo" \
    "${TMP}/Base.qc" \
    "${TMP}/AsyncMetadata.qc" \
    "${TMP}/SlaMetadata.qc" \
    "${TMP}/Manager.qc" >/dev/null

if ! "${QCC}" -c -L"${TMP}/qo" -o "${TMP}/single/Manager.qo" \
        "${TMP}/Manager.qc" 2>"${TMP}/single.err"; then
    cat "${TMP}/single.err"
    exit 1
fi

if grep -q "RUNTIME-OVERLOAD-ERROR" "${TMP}/single.err"; then
    cat "${TMP}/single.err"
    exit 1
fi
if grep -q "block missing return statement" "${TMP}/single.err"; then
    cat "${TMP}/single.err"
    exit 1
fi

test -s "${TMP}/single/Manager.qo"
echo "OK: qcc -L deferred sibling static-var metadata dependency"
