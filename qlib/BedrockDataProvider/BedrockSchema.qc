# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockDataProvider module: pinned AWS Bedrock API descriptions

/** BedrockSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Directory containing the installed Bedrock schema resources
const BedrockDataProviderModuleDir = get_script_dir();
}

public namespace BedrockDataProvider {
#! Provenance for one converted botocore service model
public hashdecl BedrockSchemaProvenanceInfo {
    #! Immutable upstream source URL
    string source_url;

    #! SHA-256 of the unmodified upstream model
    string source_sha256;

    #! Size of the unmodified upstream model in bytes
    int source_size;

    #! Date the upstream model was imported
    date imported;

    #! Botocore service API version
    string api_version;

    #! Number of upstream operations
    int source_operations;

    #! Number of upstream shapes
    int source_shapes;

    #! Operations deliberately excluded because they return an event stream
    list<string> excluded_streaming_operations;
}

#! Lazily loads the pinned Bedrock control-plane and runtime API descriptions
/** The upstream botocore models are AWS's machine-readable service contracts.  They are converted at
    development time by `tools/botocore-rest-json-to-openapi.py`; runtime code reads only the committed
    OpenAPI 3 artifacts.  This keeps action identity and types stable until a reviewed import updates the
    named botocore commit.

    The completed-response surface exports every control-plane operation and every non-streaming runtime
    operation. `ConverseStream`, `InvokeModelWithBidirectionalStream`, and
    `InvokeModelWithResponseStream` are excluded because a completed-value data-provider action cannot
    represent their event-stream lifetime. Use `BedrockRestClientIo` for these native streaming operations.
*/
public class BedrockSchema {
    public {
        #! Botocore revision used by both source models
        const BotocoreRevision = "04d4c189b371a8973af54dc1d2ff2446f93efee8";

        #! Control-plane model provenance
        const ControlProvenance = <BedrockSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/boto/botocore/04d4c189b371a8973af54dc1d2ff2446f93efee8/botocore/data/bedrock/2023-04-20/service-2.json",
            "source_sha256": "91f94306b7fbb600ac6d46d64954564e2815a2c7c6da191e35af36db1946f5f0",
            "source_size": 650374,
            "imported": 2026-09-17,
            "api_version": "2023-04-20",
            "source_operations": 108,
            "source_shapes": 907,
            "excluded_streaming_operations": (),
        };

        #! Runtime model provenance
        const RuntimeProvenance = <BedrockSchemaProvenanceInfo>{
            "source_url": "https://raw.githubusercontent.com/boto/botocore/04d4c189b371a8973af54dc1d2ff2446f93efee8/botocore/data/bedrock-runtime/2023-09-30/service-2.json",
            "source_sha256": "618bf3a6f4cb6e4987a01bd8ea2ce1f070e854991c750c67babf03520315dead",
            "source_size": 220823,
            "imported": 2026-09-17,
            "api_version": "2023-09-30",
            "source_operations": 11,
            "source_shapes": 393,
            "excluded_streaming_operations": (
                "ConverseStream",
                "InvokeModelWithBidirectionalStream",
                "InvokeModelWithResponseStream",
            ),
        };
    }

    private {
        static list<OpenApi3::OpenApi3Schema> schemas;
        static list<RestSchemaActions::RestSchemaActionSet> action_sets;
        static Mutex lck();
    }

    #! Returns the shared control-plane schema
    /** @return the parsed control-plane schema */
    static OpenApi3::OpenApi3Schema getControlSchema() {
        AutoLock al(lck);
        initIntern();
        return schemas[0];
    }

    #! Returns the shared runtime schema
    /** @return the parsed runtime schema */
    static OpenApi3::OpenApi3Schema getRuntimeSchema() {
        AutoLock al(lck);
        initIntern();
        return schemas[1];
    }

    #! Returns both shared, validated action sets
    /** @return control-plane then runtime action sets
        @throw REST-SCHEMA-ACTION-ERROR if a schema and manifest disagree
    */
    static list<RestSchemaActions::RestSchemaActionSet> getActionSets() {
        AutoLock al(lck);
        initIntern();
        return action_sets;
    }

    #! Finds the action set that owns an action
    /** @param action stable action ID
        @return the owning action set or @ref nothing
    */
    static *RestSchemaActions::RestSchemaActionSet findActionSet(string action) {
        foreach RestSchemaActions::RestSchemaActionSet set in (getActionSets()) {
            if (set.getOperation(action)) {
                return set;
            }
        }
        return;
    }

    #! Returns an action contract from either service model
    /** @param action stable action ID
        @return its operation
        @throw BEDROCK-ACTION-ERROR if the action is unknown
    */
    static RestSchemaActions::RestSchemaActionOperation getOperationEx(string action) {
        *RestSchemaActions::RestSchemaActionSet set = findActionSet(action);
        if (!set) {
            throw "BEDROCK-ACTION-ERROR", sprintf("unknown AWS Bedrock action %y", action);
        }
        return set.getOperationEx(action);
    }

    #! Loads both schemas and publishes the action sets atomically under the class lock
    private static initIntern() {
        if (action_sets) {
            return;
        }
        list<OpenApi3::OpenApi3Schema> parsed = (
            OpenApi3::OpenApi3Loader::fromFile(
                Priv::BedrockDataProviderModuleDir + "/bedrock-control-openapi.json"),
            OpenApi3::OpenApi3Loader::fromFile(
                Priv::BedrockDataProviderModuleDir + "/bedrock-runtime-openapi.json"),
        );
        RestSchemaActions::RestSchemaActionSet control(parsed[0],
            BedrockControlManifest::Manifest, BedrockControlManifest::ReferenceData);
        RestSchemaActions::RestSchemaActionSet runtime(parsed[1],
            BedrockRuntimeManifest::Manifest, NOTHING, (control,));
        schemas = parsed;
        action_sets = (control, runtime);
    }
}
}
