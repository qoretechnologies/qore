# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepSeekActions class definition

/** DeepSeekActions.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DeepSeekDataProvider {

#! Immutable routing and presentation for one DeepSeek action
public hashdecl DeepSeekActionInfo {
    #! Action name shown to users
    string display_name;
    #! Plain summary
    string short_desc;
    #! Markdown description
    string desc;
    #! Action group
    string group;
    #! HTTP method
    string method;
    #! Absolute API path
    string path;
    #! Request contract
    *AbstractDataProviderType request_type;
    #! Response contract
    AbstractDataProviderType response_type;
    #! Fields used for action options
    hash<string, hash<auto>> fields;
}

#! Documented DeepSeek API operations
/** Contracts follow the DeepSeek API reference, checked September 2026.
    See the module documentation for coverage and examples.
*/
public class DeepSeekActions {
    public {
        #! Immutable operation definitions
        const hash<string, hash<DeepSeekActionInfo>> Operations = {
            "list-models": <DeepSeekActionInfo>{
                "display_name": "List Models",
                "short_desc": "Find available models for your workflows",
                "desc": "Find available models for your workflows.",
                "group": "Models",
                "method": "GET",
                "path": "/v1/models",
                "request_type": NOTHING,
                "response_type": new DeepSeekListModelsResponseDataType(),
                "fields": {
                },
            },
            "get-balance": <DeepSeekActionInfo>{
                "display_name": "Get Balance",
                "short_desc": "Check available credit before running AI workflows",
                "desc": "Check available credit before running AI workflows.\n\nReturns credit availability and balances without generating tokens.",
                "group": "Account",
                "method": "GET",
                "path": "/v1/user/balance",
                "request_type": NOTHING,
                "response_type": new DeepSeekBalanceResponseDataType(),
                "fields": {
                },
            },
            "fim-completion": <DeepSeekActionInfo>{
                "display_name": "Fill in the Middle",
                "short_desc": "Complete text or code between a prompt and suffix",
                "desc": "Complete text or code between a prompt and suffix.\n\nUses `/beta/completions`. Select a model that supports Fill-in-the-Middle, currently `deepseek-v4-pro`.",
                "group": "Chat",
                "method": "POST",
                "path": "/beta/completions",
                "request_type": new DeepSeekFimRequestDataType(),
                "response_type": new DeepSeekFimResponseDataType(),
                "fields": DeepSeekFimRequestDataType::Fields,
            },
            "create-response": <DeepSeekActionInfo>{
                "display_name": "Create Response",
                "short_desc": "Generate a response with tools, images, or structured output",
                "desc": "Generate a response with tools, images, or structured output.\n\nSupports function calls, server-side web search, vision, and JSON Schema output. **Stateless:** send the full conversation history in `input` for each turn.",
                "group": "Chat",
                "method": "POST",
                "path": "/v1/responses",
                "request_type": new DeepSeekCreateResponseRequestDataType(),
                "response_type": new DeepSeekCreateResponseDataType(),
                "fields": DeepSeekCreateResponseRequestDataType::Fields,
            },
            "upload-file": <DeepSeekActionInfo>{
                "display_name": "Upload Image File",
                "short_desc": "Upload an image for reuse in model requests",
                "desc": "Upload an image for reuse in model requests.",
                "group": "Files",
                "method": "POST",
                "path": "/v1/files",
                "request_type": new DeepSeekUploadFileRequestDataType(),
                "response_type": new DeepSeekFileDataType(),
                "fields": DeepSeekUploadFileRequestDataType::Fields,
            },
            "list-files": <DeepSeekActionInfo>{
                "display_name": "List Image Files",
                "short_desc": "Browse uploaded images with pagination",
                "desc": "Browse uploaded images with pagination.\n\nUse `last_id` as the next `after` cursor while `has_more` is `True`.",
                "group": "Files",
                "method": "GET",
                "path": "/v1/files",
                "request_type": new DeepSeekListFilesRequestDataType(),
                "response_type": new DeepSeekListFilesResponseDataType(),
                "fields": DeepSeekListFilesRequestDataType::Fields,
            },
            "get-file": <DeepSeekActionInfo>{
                "display_name": "Get Image File",
                "short_desc": "Inspect an uploaded image and its expiration",
                "desc": "Inspect an uploaded image and its expiration.",
                "group": "Files",
                "method": "GET",
                "path": "/v1/files/{file_id}",
                "request_type": new DeepSeekFileRequestDataType(),
                "response_type": new DeepSeekFileDataType(),
                "fields": DeepSeekFileRequestDataType::Fields,
            },
            "delete-file": <DeepSeekActionInfo>{
                "display_name": "Delete Image File",
                "short_desc": "Remove an uploaded image from your account",
                "desc": "Remove an uploaded image from your account.\n\n**Permanent:** subsequent model requests cannot use this file.",
                "group": "Files",
                "method": "DELETE",
                "path": "/v1/files/{file_id}",
                "request_type": new DeepSeekFileRequestDataType(),
                "response_type": new DeepSeekDeleteFileResponseDataType(),
                "fields": DeepSeekFileRequestDataType::Fields,
            },
        };
    }

    #! Registers documented REST actions
    /** @throw DATA-PROVIDER-ACTION-ERROR invalid action metadata */
    static registerActions() [dom=PROCESS_CONTROL] {
        foreach hash<auto> entry in (Operations.pairIterator()) {
            hash<DeepSeekActionInfo> op = entry.value;
            string action = entry.key;
            hash<string, hash<ActionOptionInfo>> options =
                DataProviderActionCatalog::getActionOptionFromFields(op.fields);
            remove options.stream;
            if (action == "list-files") {
                options.limit.preselected = True;
                options.order.preselected = True;
            }
            DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
                "app": AppName, "action": action, "path": "/" + action,
                "display_name": op.display_name, "short_desc": op.short_desc, "desc": op.desc,
                "groups": (op.group,), "action_code": DPAT_API,
                "mutates_state": action == "upload-file" || action == "delete-file",
                "options": options, "output_type": op.response_type,
            });
        }
    }
}
}
