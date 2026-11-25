# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockDataProvider action registration

/** BedrockActions.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace BedrockDataProvider {
#! Owns Bedrock action metadata and idempotent catalog registration
/** Keeping registration in a callable class lets qualification exercise the same producer path used at
    module initialization, including repeated and deferred registration.
*/
public class BedrockActions {
    #! Returns all explicit action definitions for an application name
    /** @param app application name
        @return the 116 schema actions followed by the curated embedding action
    */
    static list<hash<DataProviderActionInfo>> getActionInfoList(string app) {
        list<hash<DataProviderActionInfo>> result = ();
        foreach RestSchemaActionSet actions in (BedrockSchema::getActionSets()) {
            # the POST operations that only query or infer are marked read-only in the manifests (qore#7454)
            foreach hash<DataProviderActionInfo> action in (actions.getActionInfoList(app)) {
                if (inlist(action.action, ("get-foundation-model", "list-foundation-models"))) {
                    action.billing = <DataProviderBillingInfo>{"model": "included"};
                }
                push result, action;
            }
        }
        result += getEmbedActionInfo(app);
        return result;
    }

    #! Registers all explicit Bedrock actions
    /** @param app application name */
    static registerActions(string app = AppName) [dom=PROCESS_CONTROL] {
        map DataProviderActionCatalog::registerAction($1), getActionInfoList(app);
    }

    #! Returns the curated embedding action definition
    private static hash<DataProviderActionInfo> getEmbedActionInfo(string app) {
        return <DataProviderActionInfo>{
            "app": app,
            "path": "/embed",
            "action": "embed",
            "billing": <DataProviderBillingInfo>{"unit": "token"},
            "display_name": "Embed Text",
            "short_desc": "Generate text embeddings via a Bedrock embedding model",
            "desc": "Generate dense vector embeddings for text using a Bedrock "
                "embedding model (Amazon Titan Text Embeddings v2, or Cohere "
                "`embed-english-v3` / `embed-multilingual-v3`). The action "
                "auto-routes to the correct request body shape based on the "
                "model ID prefix.",
            "action_code": DPAT_API,
            "mutates_state": False,
            "groups": ("Embeddings",),
            "options": DataProviderActionCatalog::getActionOptionFromFields(
                BedrockEmbedRequestDataType::Fields{"modelId",}, {
                    "preselected": True,
                    "required": True,
                }
            ) + DataProviderActionCatalog::getActionOptionFromFields(
                BedrockEmbedRequestDataType::Fields - ("modelId",)
            ),
            "output_type": BedrockEmbedDataProvider::ResponseType,
        };
    }
}
}
