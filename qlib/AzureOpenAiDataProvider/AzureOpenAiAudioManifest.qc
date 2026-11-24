# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureOpenAiDataProvider root class

/** AzureOpenAiAudioManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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


public namespace AzureOpenAiDataProvider {
#! Audio uploads use Azure's deployment-based API contract
public class AzureOpenAiAudioManifest {
    public {
        #! Stable audio action IDs with documented deployment paths and input choices
        const Manifest = (
            <RestSchemaActionInfo>{
                "action": "create-transcription",
                "method": "POST",
                "mutates_state": False,
                "path": "/deployments/{deployment-id}/audio/transcriptions",
                "operation_id": "Transcriptions_Create",
                "display_name": "Create Transcription",
                "short_desc": "Transcribes audio into the input language",
                "desc": "Convert a recording into searchable text in its original language. Use a Whisper deployment for subtitles or word and segment timestamps. GPT transcription deployments support `json` output. Choose `verbose_json` when requesting timestamps.",
                "groups": ("Audio",),
                "fields": {
                    "deployment-id": <RestSchemaFieldOverlayInfo>{"name": "model","display_name": "Model Deployment", "short_desc": "Azure model deployment to use", "desc": "Name of a model deployment in this Azure resource. In the Azure Foundry portal, open Models and endpoints and copy the deployment name. It can differ from the model ID.", "example_value": "my-audio-deployment", "preselected": True, "validation_regex": "\\S"},
                    "response_format": <RestSchemaFieldOverlayInfo>{"display_name": "Response Format", "desc": "Choose `json` for text in an object or `verbose_json` for metadata. Whisper also supports `text`, `srt`, and `vtt`; GPT transcription deployments support `json`.", "default_value": "json", "preselected": True},
                    "temperature": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "file": <RestSchemaFieldOverlayInfo>{"display_name": "Audio File", "short_desc": "Named audio file to transcribe or translate", "desc": "Audio to transcribe or translate. Supply a named file, keeping an extension such as `.wav` or `.mp3`.", "preselected": True},
                    "api-version": <RestSchemaFieldOverlayInfo>{"display_name": "API Version", "short_desc": "Azure audio API version", "desc": "Version of the deployment-based Azure audio API. The default supports Whisper and GPT transcription deployments.", "default_value": "2025-04-01-preview"},
                    "timestamp_granularities[]": <RestSchemaFieldOverlayInfo>{"name": "timestamp_granularities", "display_name": "Timestamp Granularities"},
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "create-translation",
                "method": "POST",
                "mutates_state": False,
                "path": "/deployments/{deployment-id}/audio/translations",
                "operation_id": "Translations_Create",
                "display_name": "Create Translation",
                "short_desc": "Translate spoken audio into English",
                "desc": "Translate a spoken recording into English text or subtitles. Select a Whisper deployment and keep the filename extension when uploading the audio. Use `json` for text in an object, `text` for plain text, or `srt` and `vtt` for subtitles.",
                "groups": ("Audio",),
                "fields": {
                    "deployment-id": <RestSchemaFieldOverlayInfo>{"name": "model","display_name": "Model Deployment", "short_desc": "Azure model deployment to use", "desc": "Name of a model deployment in this Azure resource. In the Azure Foundry portal, open Models and endpoints and copy the deployment name. It can differ from the model ID.", "example_value": "my-audio-deployment", "preselected": True, "validation_regex": "\\S"},
                    "response_format": <RestSchemaFieldOverlayInfo>{"display_name": "Response Format", "desc": "Choose `json` for text in an object or `verbose_json` for metadata. Whisper also supports `text`, `srt`, and `vtt`; GPT transcription deployments support `json`.", "default_value": "json", "preselected": True},
                    "temperature": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "file": <RestSchemaFieldOverlayInfo>{"display_name": "Audio File", "short_desc": "Named audio file to transcribe or translate", "desc": "Audio to transcribe or translate. Supply a named file, keeping an extension such as `.wav` or `.mp3`.", "preselected": True},
                    "api-version": <RestSchemaFieldOverlayInfo>{"display_name": "API Version", "short_desc": "Azure audio API version", "desc": "Version of the deployment-based Azure audio API. The default supports Whisper and GPT transcription deployments.", "default_value": "2025-04-01-preview"},
                },
                "flatten_body": True,
            },
        );
    }
}
}
