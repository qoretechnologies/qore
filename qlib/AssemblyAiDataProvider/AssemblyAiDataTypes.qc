# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider request/response data type definitions

/** AssemblyAiDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Transcribe request: URL submission with model + analytics options
public class AssemblyAiTranscribeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "audio_url": {
                "display_name": "Audio URL",
                "short_desc": "Public URL of the audio file to transcribe",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Public URL of the audio file to transcribe.  AssemblyAI "
                    "fetches the file from this URL; it must be reachable from "
                    "the public internet.  Supports `mp3`, `mp4`, `wav`, `flac`, "
                    "`ogg`, `webm`, `m4a`, and more.",
                "required": True,
                "example_value": "https://example.com/audio.mp3",
            },
            "speech_model": {
                "display_name": "Speech Model",
                "short_desc": "AssemblyAI transcription model",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "AssemblyAI transcription model — `universal` (latest "
                    "general-purpose), `slam-1` (smaller / faster), or `nano` "
                    "(supports the broadest set of languages but lower accuracy).",
                "default_value": "universal",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "universal", "display_name": "Universal (latest)"},
                    <AllowedValueInfo>{"value": "slam-1",    "display_name": "Slam 1"},
                    <AllowedValueInfo>{"value": "nano",      "display_name": "Nano (broadest language support)"},
                ),
            },
            "language_code": {
                "display_name": "Language Code",
                "short_desc": "BCP-47 / ISO-639-1 language code (e.g., `en_us`, `de`, `fr`)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Language code (BCP-47 or ISO-639-1, depending on model).  "
                    "Omit to enable automatic detection (see `language_detection`).",
                "example_value": "en_us",
            },
            "language_detection": {
                "display_name": "Auto-Detect Language",
                "short_desc": "Auto-detect the spoken language",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, AssemblyAI auto-detects the spoken language "
                    "and ignores `language_code`.",
            },
            "punctuate": {
                "display_name": "Punctuate",
                "short_desc": "Add punctuation to the transcript",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "Add punctuation to the transcript.",
            },
            "format_text": {
                "display_name": "Format Text",
                "short_desc": "Apply text formatting (capitalization, numbers, etc.)",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "Apply text formatting — capitalization, written-form "
                    "numbers, dates, currency.",
            },
            "speaker_labels": {
                "display_name": "Speaker Labels (Diarization)",
                "short_desc": "Identify distinct speakers in the audio",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, attempts speaker diarization — labels each "
                    "utterance with `speaker: \"A\"`, `\"B\"`, etc.",
            },
            "summarization": {
                "display_name": "Summarization",
                "short_desc": "Generate a summary of the transcript",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, returns a summary of the transcript in "
                    "addition to the words.  Combine with `summary_model` and "
                    "`summary_type` for fine-grained control.",
            },
            "summary_model": {
                "display_name": "Summary Model",
                "short_desc": "Summarization model (used with `summarization`)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Summarization model name when `summarization` is `True`.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "informative", "display_name": "Informative"},
                    <AllowedValueInfo>{"value": "conversational", "display_name": "Conversational"},
                    <AllowedValueInfo>{"value": "catchy",      "display_name": "Catchy"},
                ),
            },
            "summary_type": {
                "display_name": "Summary Type",
                "short_desc": "Summary output style",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Summary output style — `bullets`, `bullets_verbose`, "
                    "`gist`, `headline`, or `paragraph`.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "bullets",         "display_name": "Bullets"},
                    <AllowedValueInfo>{"value": "bullets_verbose", "display_name": "Bullets (Verbose)"},
                    <AllowedValueInfo>{"value": "gist",            "display_name": "Gist"},
                    <AllowedValueInfo>{"value": "headline",        "display_name": "Headline"},
                    <AllowedValueInfo>{"value": "paragraph",       "display_name": "Paragraph"},
                ),
            },
            "sentiment_analysis": {
                "display_name": "Sentiment Analysis",
                "short_desc": "Analyze sentiment per sentence/utterance",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, AssemblyAI returns per-utterance sentiment "
                    "(positive / negative / neutral).",
            },
            "entity_detection": {
                "display_name": "Entity Detection",
                "short_desc": "Detect entities (people, places, organizations, etc.)",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, AssemblyAI returns detected entities with "
                    "type labels (PERSON_NAME, ORG, LOCATION, EVENT, etc.).",
            },
            "iab_categories": {
                "display_name": "IAB Categories (Topic Detection)",
                "short_desc": "Classify content into IAB topic categories",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, AssemblyAI classifies the transcript into "
                    "IAB Tech Lab topic categories.",
            },
            "content_safety": {
                "display_name": "Content Safety",
                "short_desc": "Flag sensitive content categories",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, AssemblyAI flags potentially sensitive "
                    "content (profanity, violence, hate-speech, etc.).",
            },
            "webhook_url": {
                "display_name": "Webhook URL",
                "short_desc": "Optional webhook for job-completion notification",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "When provided, AssemblyAI POSTs to this URL once the "
                    "transcript reaches a terminal state — useful for "
                    "fire-and-forget submission.",
            },
            "poll_interval": {
                "display_name": "Polling Interval",
                "short_desc": "Time between transcript status polls",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "Interval between job-status polls.  Defaults to `3s`.  "
                    "Lower values fetch results sooner at the cost of more API "
                    "calls.",
            },
            "transcription_timeout": {
                "display_name": "Overall Transcription Timeout",
                "short_desc": "Maximum total wait time for the polling loop",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "Maximum total wait time for the polling loop.  Defaults "
                    "to `30m`.  When exceeded, the action raises "
                    "`ASSEMBLYAI-ERROR`.",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Transcribe response: completed AssemblyAI transcript object
public class AssemblyAiTranscribeResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Transcript ID",
                "desc": "AssemblyAI-assigned transcript identifier.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "desc": "Final transcript status — always `completed` when this "
                    "response is returned (an `error` status raises `ASSEMBLYAI-ERROR`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "audio_url": {
                "display_name": "Audio URL",
                "desc": "The audio URL that was submitted for transcription.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "text": {
                "display_name": "Transcript Text",
                "desc": "The full transcribed text.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "language_code": {
                "display_name": "Detected Language",
                "desc": "Detected (or supplied) BCP-47 language code.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "audio_duration": {
                "display_name": "Audio Duration (seconds)",
                "desc": "Length of the audio in seconds.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "confidence": {
                "display_name": "Overall Confidence",
                "desc": "Overall transcript confidence (0.0 – 1.0).",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "words": {
                "display_name": "Words",
                "desc": "Per-word objects with `text`, `start`, `end`, "
                    "`confidence`, and `speaker` (when `speaker_labels` was set).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "utterances": {
                "display_name": "Utterances",
                "desc": "Speaker-grouped utterances with `text`, `start`, `end`, "
                    "and `speaker` (only when `speaker_labels` was set).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "summary": {
                "display_name": "Summary",
                "desc": "Generated summary (only when `summarization` was set).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sentiment_analysis_results": {
                "display_name": "Sentiment Analysis Results",
                "desc": "Per-utterance sentiment objects (only when "
                    "`sentiment_analysis` was set).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "entities": {
                "display_name": "Entities",
                "desc": "Detected entities with type + position offsets (only "
                    "when `entity_detection` was set).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "iab_categories_result": {
                "display_name": "IAB Categories Result",
                "desc": "Topic classification results (only when `iab_categories` "
                    "was set).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "content_safety_labels": {
                "display_name": "Content Safety Labels",
                "desc": "Content safety flags (only when `content_safety` was set).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}
