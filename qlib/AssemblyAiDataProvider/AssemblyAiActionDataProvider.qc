# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider root class definition

/** AssemblyAiActionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AssemblyAiDataProvider {
#! Routes action requests and resource dropdowns to the owning service
/** Cross-document reference data can use another hostname; retain the root client so regional routing
    is always derived from the connection, including when a language-model action lists transcripts.
*/
public class AssemblyAiActionDataProvider inherits RestSchemaActionDataProvider {
    private {
        AssemblyAiRestClientIo root_client;
    }
    #! Creates a schema action with the owning service client
    /** @param actions owning action set
        @param operation action contract
        @param root_client connection client
    */
    constructor(RestSchemaActionSet actions, RestSchemaActionOperation operation, AssemblyAiRestClientIo root_client)
            : RestSchemaActionDataProvider(actions, operation,
                AssemblyAiActionDataProvider::clientFor(root_client, operation.getAction())) {
        self.root_client = root_client;
    }

    #! Resolves resource selections using the listing action's service
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        *hash<RestSchemaReferenceDataInfo> rd = action_set.getReferenceDataDecl(type);
        if (!rd) {
            return;
        }
        RestSchemaActionDataProvider provider(action_set, op, clientFor(root_client, rd.action));
        return provider.getReferenceData(type, action_opts);
    }

    #! Represents HTTP 204 explicitly as the nothing type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return op.getResponseType() ?? AssemblyAiActions::EmptyResponseType;
    }

    #! Selects a service without accepting arbitrary hosts from request fields
    private static AssemblyAiRestClientIo clientFor(AssemblyAiRestClientIo root_client, string action) {
        string service = AssemblyAiSchema::Services{AssemblyAiSchema::findDocument(action)};
        return service.val() ? root_client.getServiceClient(service) : root_client;
    }
}
}
