# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider root class definition

/** AssemblyAiSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Installed module resource directory
const AssemblyAiModuleDir = get_script_dir();
}

public namespace AssemblyAiDataProvider {
#! Provenance of a reviewed upstream API description
public hashdecl AssemblyAiSchemaProvenanceInfo {
    #! Public upstream document URL
    string source_url;
    #! SHA-256 of the unmodified upstream bytes
    string source_sha256;
    #! Unmodified document size in bytes
    int source_size;
    #! Snapshot import date
    date imported;
    #! Number of upstream paths, before pruning
    int source_paths;
    #! Number of exported operations
    int exported_operations;
    #! Number of upstream component schemas
    int source_schemas;
}

#! Shared API schemas, parsed and validated during action registration
/** Only committed resources are loaded. The import specification records checked upstream repairs.
    Construction publishes the complete set under a lock, so a failed import cannot expose partial state.
*/
public class AssemblyAiSchema {
    public {
        #! Reviewed schema snapshot provenance, keyed by service document
        const hash<string, hash<AssemblyAiSchemaProvenanceInfo>> Provenance = {
            "transcripts": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/openapi.json",
                "source_sha256": "9c5f65722ea83c49c173608173ac8de5b79c13bbb65843b064b67fb33b6902bc",
                "source_size": 264603, "imported": 2026-09-18,
                "source_paths": 8, "exported_operations": 10,
                "source_schemas": 57,
            },
            "llm-gateway": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/llm-gateway.yaml",
                "source_sha256": "c4e63a8ade2d2635891051b9cd9928abaaab85b11f513a40a61632821fe774a0",
                "source_size": 49421, "imported": 2026-09-18,
                "source_paths": 3, "exported_operations": 3,
                "source_schemas": 38,
            },
            "streaming-token": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/streaming-token.yaml",
                "source_sha256": "c770ff4800eb3a154af97ccfc179a6e0b353c501e4aa87e80c9eeb61d54c37bd",
                "source_size": 6132, "imported": 2026-09-18,
                "source_paths": 1, "exported_operations": 1,
                "source_schemas": 1,
            },
            "voice-agent-token": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/voice-agent-token.yaml",
                "source_sha256": "6d86ce378f569e822561aa2158962f7585424522659789b42cb61cfe046918cd",
                "source_size": 4784, "imported": 2026-09-18,
                "source_paths": 1, "exported_operations": 1,
                "source_schemas": 1,
            },
            "agents": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/agents.yaml",
                "source_sha256": "f0fe8e45d3a8cb234f3acd7c3c491c20c04e0e2057ece3bc9074940d91a18b77",
                "source_size": 27670, "imported": 2026-09-18,
                "source_paths": 4, "exported_operations": 8,
                "source_schemas": 24,
            },
            "webhooks": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/webhooks.yaml",
                "source_sha256": "69e5832d880bf7263565fd57c7fb36345e813517dc93e44d655b9b6bef000cdf",
                "source_size": 7261, "imported": 2026-09-18,
                "source_paths": 2, "exported_operations": 5,
                "source_schemas": 6,
            },
            "sync-api": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/sync-api.yaml",
                "source_sha256": "d30eefd9efecd3e9d086c96abdbf5e25793a0ad6c836161a16c1d8cbb6d711e6",
                "source_size": 35198, "imported": 2026-09-18,
                "source_paths": 3, "exported_operations": 2,
                "source_schemas": 4,
            },
            "dictation": <AssemblyAiSchemaProvenanceInfo>{
                "source_url": "https://www.assemblyai.com/docs/api-reference/specs/dictation.yaml",
                "source_sha256": "393c348ada835b45d83da30695364a851d593ba9c4bd6391a1a20fd411886ca3",
                "source_size": 16884, "imported": 2026-09-18,
                "source_paths": 1, "exported_operations": 1,
                "source_schemas": 4,
            },
        };
        #! Action manifests in dependency order
        const hash<string, list<hash<RestSchemaActionInfo>>> Manifests = {
            "transcripts": AssemblyAiTranscriptsManifest::Manifest,
            "agents": AssemblyAiAgentsManifest::Manifest,
            "llm-gateway": AssemblyAiLlmGatewayManifest::Manifest,
            "streaming-token": AssemblyAiStreamingTokenManifest::Manifest,
            "voice-agent-token": AssemblyAiVoiceAgentTokenManifest::Manifest,
            "webhooks": AssemblyAiWebhooksManifest::Manifest,
            "sync-api": AssemblyAiSyncApiManifest::Manifest,
            "dictation": AssemblyAiDictationManifest::Manifest,
        };
        #! Companion service for each API document; transcripts use the connection client
        const Services = {
            "transcripts": "",
            "llm-gateway": "llm-gateway",
            "streaming-token": "streaming",
            "voice-agent-token": "agents",
            "agents": "agents",
            "webhooks": "agents",
            "sync-api": "sync",
            "dictation": "dictation",
        };
    }
    private {
        static hash<string, RestSchemaActionSet> action_sets;
        static Mutex lck();
    }

    #! Returns every validated action set
    /** @return sets keyed by API document name */
    static hash<string, RestSchemaActionSet> getActionSets() {
        AutoLock al(lck);
        if (!action_sets) {
            hash<string, RestSchemaActionSet> parsed = {};
            hash<string, hash<string, hash<RestSchemaReferenceDataInfo>>> refs = {
                "transcripts": AssemblyAiTranscriptsManifest::ReferenceData,
                "agents": AssemblyAiAgentsManifest::ReferenceData,
                "llm-gateway": AssemblyAiLlmGatewayManifest::ReferenceData,
                "streaming-token": AssemblyAiStreamingTokenManifest::ReferenceData,
                "voice-agent-token": AssemblyAiVoiceAgentTokenManifest::ReferenceData,
                "webhooks": AssemblyAiWebhooksManifest::ReferenceData,
                "sync-api": AssemblyAiSyncApiManifest::ReferenceData,
                "dictation": AssemblyAiDictationManifest::ReferenceData,
            };
            foreach string name in (keys Manifests) {
                OpenApi3::OpenApi3Schema schema = OpenApi3::OpenApi3Loader::fromFile(
                    Priv::AssemblyAiModuleDir + "/assemblyai-" + name + "-openapi.json");
                parsed{name} = new RestSchemaActionSet(schema, Manifests{name}, refs{name},
                    map $1, parsed.iterator());
            }
            action_sets = parsed;
        }
        return action_sets;
    }

    #! Finds the API document owning an action
    /** @param action stable action ID
        @return document name or @ref nothing
    */
    static *string findDocument(string action) {
        foreach auto entry in (Manifests.pairIterator()) {
            if (select entry.value, $1.action == action) {
                return entry.key;
            }
        }
        return;
    }

    #! Returns an action's schema contract
    /** @param action stable action ID
        @return resolved operation
        @throw ASSEMBLYAI-ACTION-ERROR an unknown action was requested
    */
    static RestSchemaActionOperation getOperationEx(string action) {
        *string doc = findDocument(action);
        if (!doc) {
            throw "ASSEMBLYAI-ACTION-ERROR", sprintf("unknown action %y", action);
        }
        return getActionSets(){doc}.getOperationEx(action);
    }
}
}
