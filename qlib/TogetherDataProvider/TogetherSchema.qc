# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TogetherSchema class definition

/** TogetherSchema.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

namespace Priv {
#! Directory containing the pinned schema resources
const TogetherModuleDir = get_script_dir();
}

public namespace TogetherDataProvider {
public hashdecl TogetherSchemaProvenanceInfo {
    #! The URL the schema was imported from
    string source_url;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The date the schema was imported
    date imported;

    #! The number of paths the upstream document declares
    int source_paths;

    #! The number of operations the upstream document declares
    int source_operations;

    #! The number of component schemas the upstream document declares
    int source_schemas;

    #! The number of webhooks the upstream document declares
    int source_webhooks;

    #! The API version the upstream document reports for itself
    /** Together uses v1 and v2 endpoint paths within this one description. */
    string api_version;
}

#! Loads Together AI's reviewed v1 and v2 action contracts
/** Each partition uses its documented server prefix. Parsing and registration are protected by a
    mutex, and action sets are published only when both partitions have been validated.
*/
public class TogetherSchema {
    public {
        #! Provenance of the unmodified official schema used for both partitions
        const Provenance = <TogetherSchemaProvenanceInfo>{
            "source_url": "https://docs.together.ai/openapi.yaml",
            "source_sha256": "b74f4746f29ad6c18d210c53c321c30d2d9aa934fbe5d3c8334dd1947e3a7c16",
            "source_size": 816443,
            "imported": 2026-09-08,
            "source_paths": 142,
            "source_operations": 199,
            "source_schemas": 469,
            "source_webhooks": 0,
            "api_version": "2.0.0",
        };
    }
    private {
        static list<OpenApi3Schema> schemas;
        static list<RestSchemaActionSet> action_sets;
        static Mutex lock();
    }

    #! Returns both action sets, sharing reference data from the primary API
    /** @return the v1 and v2 action sets, in order
        @throw REST-SCHEMA-ACTION-ERROR the manifest and schema disagree
    */
    static list<RestSchemaActionSet> getActionSets() {
        AutoLock al(lock);
        initIntern();
        return action_sets;
    }

    #! Returns the primary v1 action set
    /** @return the v1 actions */
    static RestSchemaActionSet getActionSet() {
        return getActionSets()[0];
    }

    #! Finds the action set owning an action
    /** @param action stable action identifier
        @return its action set, or nothing when it is unknown
    */
    static *RestSchemaActionSet findActionSet(string action) {
        foreach RestSchemaActionSet set in (getActionSets()) {
            if (set.getOperation(action)) {
                return set;
            }
        }
        return;
    }

    #! Returns an action contract from either API version
    /** @param action stable action identifier
        @return its operation
        @throw TOGETHER-ACTION-ERROR the action is unknown
    */
    static RestSchemaActionOperation getOperationEx(string action) {
        *RestSchemaActionSet set = findActionSet(action);
        if (!set) {
            throw "TOGETHER-ACTION-ERROR", sprintf("unknown Together AI action %y", action);
        }
        return set.getOperationEx(action);
    }

    #! Returns a parsed schema for a documented API version
    /** @param version API version, 1 or 2
        @return the pinned schema
        @throw TOGETHER-SCHEMA-ERROR the version is unsupported
    */
    static OpenApi3Schema getSchema(int version = 1) {
        if (version != 1 && version != 2) {
            throw "TOGETHER-SCHEMA-ERROR", "supported API versions are 1 and 2";
        }
        AutoLock al(lock);
        initIntern();
        return schemas[version - 1];
    }

    #! Builds the schema objects while holding the lock
    private static initIntern() {
        if (action_sets) {
            return;
        }
        list<OpenApi3Schema> parsed = ();
        foreach int version in (1, 2) {
            push parsed, OpenApi3Loader::fromFile(Priv::TogetherModuleDir
                + sprintf("/together-v%d-openapi.json", version));
        }
        RestSchemaActionSet primary(parsed[0], TogetherManifest::Manifest, TogetherManifest::ReferenceData,
            NOTHING, NOTHING, RestSchemaActionSet::LazyResponseTypes);
        RestSchemaActionSet dedicated(parsed[1], TogetherDedicatedManifest::Manifest,
            TogetherDedicatedManifest::ReferenceData, (primary,),
            NOTHING, RestSchemaActionSet::LazyResponseTypes);
        schemas = parsed;
        action_sets = (primary, dedicated);
    }
}
}
