# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Fireworks AI action catalog

/** FireworksSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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
const FireworksModuleDir = get_script_dir();
const FireworksLogo = File::readTextFile(FireworksModuleDir + "/fireworks-ai-logo.svg");
}

public namespace FireworksDataProvider {
#! Provenance of an unmodified upstream document before pruning
public hashdecl FireworksSchemaProvenanceInfo {
    #! Unmodified upstream document URL
    string source_url;
    #! SHA-256 of the upstream bytes before pruning
    string source_sha256;
    #! Upstream byte count
    int source_size;
    #! Import date
    date imported;
    #! Upstream path count
    int source_paths;
    #! Upstream operation count
    int source_operations;
    #! Upstream component schema count
    int source_schemas;
    #! Upstream webhook count
    int source_webhooks;
    #! Upstream API version
    string api_version;
}

#! Loads pinned schemas and shares their immutable action sets
/** Control-plane, upload, text, responses, and messages schemas are separate because their base paths
    differ. Inference sets refer to the control set for account/model dropdowns. No schema is downloaded
    at runtime. Re-import with <tt>tools/rest-schema-import.qr --spec fireworks-import.yaml</tt>.
*/
public class FireworksSchema {
    public {
        const ControlProvenance = <FireworksSchemaProvenanceInfo>{
            "source_url": "https://docs.fireworks.ai/gateway.openapi.yaml",
            "source_sha256": "53dd9744f09db2b79834b8fe6e5d673ee28c65f4fc57d3e24cbe494f41b58427",
            "source_size": 647549,
            "imported": 2026-09-08,
            "source_paths": 139,
            "source_operations": 213,
            "source_schemas": 359,
            "source_webhooks": 0,
            "api_version": "5.10.0",
        };
        const UploadProvenance = <FireworksSchemaProvenanceInfo>{
            "source_url": "https://docs.fireworks.ai/gateway-extra.openapi.yaml",
            "source_sha256": "f25afc96c7fb5aee9cea53c7b3673962faa2f38796f4a77b3a90fe419d7ba502",
            "source_size": 2525,
            "imported": 2026-09-08,
            "source_paths": 1,
            "source_operations": 1,
            "source_schemas": 1,
            "source_webhooks": 0,
            "api_version": "3.0.359",
        };
        const TextProvenance = <FireworksSchemaProvenanceInfo>{
            "source_url": "https://docs.fireworks.ai/text-completion.openapi.yaml",
            "source_sha256": "0badee79d7ea2e883c71ae9b6114a5948efd885e822c14c16c2a74e14ad940d2",
            "source_size": 124217,
            "imported": 2026-09-08,
            "source_paths": 2,
            "source_operations": 2,
            "source_schemas": 43,
            "source_webhooks": 0,
            "api_version": "0.1.0",
        };
        const ResponsesProvenance = <FireworksSchemaProvenanceInfo>{
            "source_url": "https://docs.fireworks.ai/responses.openapi.yaml",
            "source_sha256": "b055e33b117adaa274b7af3897e4de38af1af075b5fce0de737e85caf590dd6d",
            "source_size": 26919,
            "imported": 2026-09-08,
            "source_paths": 2,
            "source_operations": 4,
            "source_schemas": 10,
            "source_webhooks": 0,
            "api_version": "0.6.1",
        };
        const MessagesProvenance = <FireworksSchemaProvenanceInfo>{
            "source_url": "https://docs.fireworks.ai/anthropic-messages.openapi.json",
            "source_sha256": "485f7a8351e771757eeb4cb5f82fabaa9807bfcea37ed3e3424db2c8b7b5a09b",
            "source_size": 85644,
            "imported": 2026-09-08,
            "source_paths": 1,
            "source_operations": 1,
            "source_schemas": 70,
            "source_webhooks": 0,
            "api_version": "1.0.0",
        };
    }
    private {
        static Mutex mutex();
        static hash<string, OpenApi3::OpenApi3Schema> schemas;
        static hash<string, RestSchemaActionSet> sets;
    }

    #! Returns every action set in dependency order
    /** @return cached action sets
        @throw REST-SCHEMA-ACTION-ERROR a schema or manifest is invalid
    */
    static list<RestSchemaActionSet> getActionSets() {
        AutoLock al(mutex);
        if (!sets) {
            hash<string, RestSchemaActionSet> pending;
            pending.Control = new RestSchemaActionSet(getSchemaIntern("control"),
                FireworksManifest::ControlManifest, FireworksManifest::ReferenceData);
            pending.Upload = new RestSchemaActionSet(getSchemaIntern("upload"),
                FireworksManifest::UploadManifest, NOTHING, pending.Control);
            pending.Text = new RestSchemaActionSet(getSchemaIntern("text"),
                FireworksManifest::TextManifest, NOTHING, pending.Control);
            pending.Responses = new RestSchemaActionSet(getSchemaIntern("responses"),
                FireworksManifest::ResponsesManifest, FireworksManifest::ResponseReferenceData, pending.Control);
            pending.Messages = new RestSchemaActionSet(getSchemaIntern("messages"),
                FireworksManifest::MessagesManifest, NOTHING, pending.Control);
            sets = pending;
        }
        return (sets.Control, sets.Upload, sets.Text, sets.Responses, sets.Messages);
    }

    #! Returns the action set owning an action
    /** @param action stable action ID
        @return its owning action set
        @throw FIREWORKS-ACTION-ERROR the action is unknown
    */
    static RestSchemaActionSet getActionSet(string action) {
        foreach RestSchemaActionSet aset in (getActionSets()) {
            if (aset.getOperation(action)) {
                return aset;
            }
        }
        throw "FIREWORKS-ACTION-ERROR", sprintf("unknown Fireworks action %y", action);
    }

    private static OpenApi3::OpenApi3Schema getSchemaIntern(string name) {
        if (!schemas{name}) {
            schemas{name} = OpenApi3::OpenApi3Loader::fromFile(Priv::FireworksModuleDir
                + "/fireworks-" + name + "-openapi.yaml");
        }
        return schemas{name};
    }
}
}
