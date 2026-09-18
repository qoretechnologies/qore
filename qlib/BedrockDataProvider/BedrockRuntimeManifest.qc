# -*- mode: qore; indent-tabs-mode: nil -*-
#! Generated AWS Bedrock action manifest; regenerate with tools/botocore-rest-json-to-openapi.py

/** Copyright 2026 Qore Technologies, s.r.o.
    SPDX-License-Identifier: MIT
*/

public namespace BedrockDataProvider {
#! Pinned action inventory and presentation metadata for BedrockRuntimeManifest
public class BedrockRuntimeManifest {
    public {
        #! Exact reviewed operation inventory in upstream declaration order
        const Manifest = (
            <RestSchemaActionInfo>{
                "action": "apply-guardrail",
                "method": "POST",
                "path": "/guardrail/{guardrailIdentifier}/version/{guardrailVersion}/apply",
                "operation_id": "ApplyGuardrail",
                "display_name": "Apply Guardrail",
                "short_desc": "The action to apply a guardrail.",
                "desc": "The action to apply a guardrail.\n\nFor troubleshooting some of the common errors you might encounter when using the `ApplyGuardrail` API, see [Troubleshooting Amazon Bedrock API Error Codes](https://docs.aws.amazon.com/bedrock/latest/userguide/troubleshooting-api-error-codes.html) in the Amazon Bedrock User Guide",
                "groups": ("Guardrails",),
                "fields": {"guardrailIdentifier": <RestSchemaFieldOverlayInfo>{"preselected": True, "ref_data": "guardrails", "supports_custom_values": True}, "guardrailVersion": <RestSchemaFieldOverlayInfo>{"preselected": True}, "source": <RestSchemaFieldOverlayInfo>{"preselected": True}, "content": <RestSchemaFieldOverlayInfo>{"preselected": True}},
                "flatten_body": True,
                "metadata": {"aws_operation": "ApplyGuardrail"},
            },
            <RestSchemaActionInfo>{
                "action": "converse",
                "method": "POST",
                "path": "/model/{modelId}/converse",
                "operation_id": "Converse",
                "display_name": "Converse",
                "short_desc": "Sends messages to the specified Amazon Bedrock model.",
                "desc": "Sends messages to the specified Amazon Bedrock model. `Converse` provides a consistent interface that works with all models that support messages. This allows you to write code once and use it with different models. If a model has unique inference parameters, you can also pass those unique parameters to the model.\n\nAmazon Bedrock doesn't store any text, images, or documents that you provide as content. The data is only used to generate the response.\n\nYou can submit a prompt by including it in the `messages` field, specifying the `modelId` of a foundation model or inference profile to run inference on it, and including any other fields that are relevant to your use case.\n\nYou can also submit a prompt from Prompt management by specifying the ARN of the prompt version and including a map of variables to values in the `promptVariables` field. You can append more messages to the prompt by using the `messages` field. If you use a prompt from Prompt management, you can't include the following fields in the request: `additionalModelRequestFields`, `inferenceConfig`, `system`, or `toolConfig`. Instead, these fields must be defined through Prompt management. For more information, see [Use a prompt from Prompt management](https://docs.aws.amazon.com/bedrock/latest/userguide/prompt-management-use.html).\n\nFor information about the Converse API, see *Use the Converse API* in the *Amazon Bedrock User Guide*. To use a guardrail, see *Use a guardrail with the Converse API* in the *Amazon Bedrock User Guide*. To use a tool with a model, see *Tool use (Function calling)* in the *Amazon Bedrock User Guide*\n\nFor example code, see *Converse API examples* in the *Amazon Bedrock User Guide*.\n\nThis operation requires permission for the `bedrock:InvokeModel` action.\n\nTo deny all inference access to resources that you specify in the modelId field, you need to deny access to the `bedrock:InvokeModel` and `bedrock:InvokeModelWithResponseStream` actions. Doing this also denies access to the resource through the base inference actions ([InvokeModel](https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_InvokeModel.html) and [InvokeModelWithResponseStream](https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_InvokeModelWithResponseStream.html)). For more information see [Deny access for inference on specific models](https://docs.aws.amazon.com/bedrock/latest/userguide/security_iam_id-based-policy-examples.html#security_iam_id-based-policy-examples-deny-inference).\n\nFor troubleshooting some of the common errors you might encounter when using the `Converse` API, see [Troubleshooting Amazon Bedrock API Error Codes](https://docs.aws.amazon.com/bedrock/latest/userguide/troubleshooting-api-error-codes.html) in the Amazon Bedrock User Guide",
                "groups": ("Inference",),
                "fields": {"modelId": <RestSchemaFieldOverlayInfo>{"preselected": True, "ref_data": "foundation-models", "supports_custom_values": True}},
                "flatten_body": True,
                "metadata": {"aws_operation": "Converse"},
            },
            <RestSchemaActionInfo>{
                "action": "count-tokens",
                "method": "POST",
                "path": "/model/{modelId}/count-tokens",
                "operation_id": "CountTokens",
                "display_name": "Count Tokens",
                "short_desc": "Returns the token count for a given inference request.",
                "desc": "Returns the token count for a given inference request. This operation helps you estimate token usage before sending requests to foundation models by returning the token count that would be used if the same input were sent to the model in an inference request.\n\nToken counting is model-specific because different models use different tokenization strategies. The token count returned by this operation will match the token count that would be charged if the same input were sent to the model in an `InvokeModel` or `Converse` request.\n\nYou can use this operation to:\n\n- Estimate costs before sending inference requests.\n\n- Optimize prompts to fit within token limits.\n\n- Plan for token usage in your applications.\n\nThis operation accepts the same input formats as `InvokeModel` and `Converse`, allowing you to count tokens for both raw text inputs and structured conversation formats.\n\nThe following operations are related to `CountTokens`:\n\n- [InvokeModel](https://docs.aws.amazon.com/bedrock/latest/API/API_runtime_InvokeModel.html) - Sends inference requests to foundation models\n\n- [Converse](https://docs.aws.amazon.com/bedrock/latest/API/API_runtime_Converse.html) - Sends conversation-based inference requests to foundation models",
                "groups": ("Inference",),
                "fields": {"modelId": <RestSchemaFieldOverlayInfo>{"preselected": True, "ref_data": "foundation-models", "supports_custom_values": True}, "input": <RestSchemaFieldOverlayInfo>{"preselected": True}},
                "flatten_body": True,
                "metadata": {"aws_operation": "CountTokens"},
            },
            <RestSchemaActionInfo>{
                "action": "get-async-invoke",
                "method": "GET",
                "path": "/async-invoke/{invocationArn}",
                "operation_id": "GetAsyncInvoke",
                "display_name": "Get Async Invoke",
                "short_desc": "Retrieve information about an asynchronous invocation.",
                "desc": "Retrieve information about an asynchronous invocation.",
                "groups": ("Asynchronous Inference",),
                "fields": {"invocationArn": <RestSchemaFieldOverlayInfo>{"preselected": True}},
                "flatten_body": True,
                "metadata": {"aws_operation": "GetAsyncInvoke"},
            },
            <RestSchemaActionInfo>{
                "action": "invoke-guardrail-checks",
                "method": "POST",
                "path": "/guardrail-checks/invoke",
                "operation_id": "InvokeGuardrailChecks",
                "display_name": "Invoke Guardrail Checks",
                "short_desc": "Evaluates messages against inline guardrail checks.",
                "desc": "Evaluates messages against inline guardrail checks. You specify the check configurations directly in the request, and Amazon Bedrock returns per-check results with severity or confidence scores.",
                "groups": ("Guardrails",),
                "fields": {"messages": <RestSchemaFieldOverlayInfo>{"preselected": True}, "checks": <RestSchemaFieldOverlayInfo>{"preselected": True}},
                "flatten_body": True,
                "metadata": {"aws_operation": "InvokeGuardrailChecks"},
            },
            <RestSchemaActionInfo>{
                "action": "invoke-model",
                "method": "POST",
                "path": "/model/{modelId}/invoke",
                "operation_id": "InvokeModel",
                "display_name": "Invoke Model",
                "short_desc": "Run the Invoke Model operation.",
                "desc": "Invokes the specified Amazon Bedrock model to run inference using the prompt and inference parameters provided in the request body. You use model inference to generate text, images, and embeddings.\n\nFor example code, see *Invoke model code examples* in the *Amazon Bedrock User Guide*.\n\nThis operation requires permission for the `bedrock:InvokeModel` action.\n\nTo deny all inference access to resources that you specify in the modelId field, you need to deny access to the `bedrock:InvokeModel` and `bedrock:InvokeModelWithResponseStream` actions. Doing this also denies access to the resource through the Converse API actions ([Converse](https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_Converse.html) and [ConverseStream](https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_ConverseStream.html)). For more information see [Deny access for inference on specific models](https://docs.aws.amazon.com/bedrock/latest/userguide/security_iam_id-based-policy-examples.html#security_iam_id-based-policy-examples-deny-inference).\n\nFor troubleshooting some of the common errors you might encounter when using the `InvokeModel` API, see [Troubleshooting Amazon Bedrock API Error Codes](https://docs.aws.amazon.com/bedrock/latest/userguide/troubleshooting-api-error-codes.html) in the Amazon Bedrock User Guide",
                "groups": ("Inference",),
                "fields": {"body": <RestSchemaFieldOverlayInfo>{"display_name": "Body", "short_desc": "Invoke Model request body", "desc": "The prompt and inference parameters in the format specified in the `contentType` in the header. You must provide the body in JSON format. To see the format and content of the request and response bodies for different models, refer to [Inference parameters](https://docs.aws.amazon.com/bedrock/latest/userguide/model-parameters.html). For more information, see [Run inference](https://docs.aws.amazon.com/bedrock/latest/userguide/api-methods-run.html) in the Bedrock User Guide.", "preselected": True}, "Content-Type": <RestSchemaFieldOverlayInfo>{"name": "contentType", "default_value": "application/json", "allowed_values": ({"value": "application/json", "display_name": "JSON"},)}, "Accept": <RestSchemaFieldOverlayInfo>{"name": "accept", "default_value": "application/json", "allowed_values": ({"value": "application/json", "display_name": "JSON"},)}, "modelId": <RestSchemaFieldOverlayInfo>{"preselected": True, "ref_data": "foundation-models", "supports_custom_values": True}, "X-Amzn-Bedrock-Trace": <RestSchemaFieldOverlayInfo>{"name": "trace"}, "X-Amzn-Bedrock-GuardrailIdentifier": <RestSchemaFieldOverlayInfo>{"name": "guardrailIdentifier", "ref_data": "guardrails", "supports_custom_values": True}, "X-Amzn-Bedrock-GuardrailVersion": <RestSchemaFieldOverlayInfo>{"name": "guardrailVersion"}, "X-Amzn-Bedrock-PerformanceConfig-Latency": <RestSchemaFieldOverlayInfo>{"name": "performanceConfigLatency"}, "X-Amzn-Bedrock-Service-Tier": <RestSchemaFieldOverlayInfo>{"name": "serviceTier"}, "X-Amzn-Bedrock-Request-Metadata": <RestSchemaFieldOverlayInfo>{"name": "requestMetadata"}},
                "flatten_body": False,
                "metadata": {"aws_operation": "InvokeModel"},
            },
            <RestSchemaActionInfo>{
                "action": "list-async-invokes",
                "method": "GET",
                "path": "/async-invoke",
                "operation_id": "ListAsyncInvokes",
                "display_name": "List Async Invokes",
                "short_desc": "Lists asynchronous invocations.",
                "desc": "Lists asynchronous invocations.",
                "groups": ("Asynchronous Inference",),
                "flatten_body": True,
                "metadata": {"aws_operation": "ListAsyncInvokes"},
            },
            <RestSchemaActionInfo>{
                "action": "start-async-invoke",
                "method": "POST",
                "path": "/async-invoke",
                "operation_id": "StartAsyncInvoke",
                "display_name": "Start Async Invoke",
                "short_desc": "Starts an asynchronous invocation.",
                "desc": "Starts an asynchronous invocation.\n\nThis operation requires permission for the `bedrock:InvokeModel` action.\n\nTo deny all inference access to resources that you specify in the modelId field, you need to deny access to the `bedrock:InvokeModel` and `bedrock:InvokeModelWithResponseStream` actions. Doing this also denies access to the resource through the Converse API actions ([Converse](https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_Converse.html) and [ConverseStream](https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_ConverseStream.html)). For more information see [Deny access for inference on specific models](https://docs.aws.amazon.com/bedrock/latest/userguide/security_iam_id-based-policy-examples.html#security_iam_id-based-policy-examples-deny-inference).",
                "groups": ("Asynchronous Inference",),
                "fields": {"modelId": <RestSchemaFieldOverlayInfo>{"preselected": True, "ref_data": "foundation-models", "supports_custom_values": True}, "modelInput": <RestSchemaFieldOverlayInfo>{"preselected": True}, "outputDataConfig": <RestSchemaFieldOverlayInfo>{"preselected": True}},
                "flatten_body": True,
                "metadata": {"aws_operation": "StartAsyncInvoke"},
            },
        );
    }
}
}
