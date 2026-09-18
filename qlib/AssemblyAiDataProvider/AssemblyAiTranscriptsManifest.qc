# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider API action manifest

/** AssemblyAiTranscriptsManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Curated actions and resource selections for the transcripts API
public class AssemblyAiTranscriptsManifest {
    public {
        #! Official language codes with opaque presentation labels
        const Languages = (
            <AllowedValueInfo>{"value": "en", "display_name": "English (global)"},
            <AllowedValueInfo>{"value": "en_au", "display_name": "English (Australian)"},
            <AllowedValueInfo>{"value": "en_uk", "display_name": "English (British)"},
            <AllowedValueInfo>{"value": "en_us", "display_name": "English (US)"},
            <AllowedValueInfo>{"value": "es", "display_name": "Spanish"},
            <AllowedValueInfo>{"value": "fr", "display_name": "French"},
            <AllowedValueInfo>{"value": "de", "display_name": "German"},
            <AllowedValueInfo>{"value": "it", "display_name": "Italian"},
            <AllowedValueInfo>{"value": "pt", "display_name": "Portuguese"},
            <AllowedValueInfo>{"value": "nl", "display_name": "Dutch"},
            <AllowedValueInfo>{"value": "af", "display_name": "Afrikaans"},
            <AllowedValueInfo>{"value": "sq", "display_name": "Albanian"},
            <AllowedValueInfo>{"value": "am", "display_name": "Amharic"},
            <AllowedValueInfo>{"value": "ar", "display_name": "Arabic"},
            <AllowedValueInfo>{"value": "hy", "display_name": "Armenian"},
            <AllowedValueInfo>{"value": "as", "display_name": "Assamese"},
            <AllowedValueInfo>{"value": "az", "display_name": "Azerbaijani"},
            <AllowedValueInfo>{"value": "ba", "display_name": "Bashkir"},
            <AllowedValueInfo>{"value": "eu", "display_name": "Basque"},
            <AllowedValueInfo>{"value": "be", "display_name": "Belarusian"},
            <AllowedValueInfo>{"value": "bn", "display_name": "Bengali"},
            <AllowedValueInfo>{"value": "bs", "display_name": "Bosnian"},
            <AllowedValueInfo>{"value": "br", "display_name": "Breton"},
            <AllowedValueInfo>{"value": "bg", "display_name": "Bulgarian"},
            <AllowedValueInfo>{"value": "my", "display_name": "Burmese"},
            <AllowedValueInfo>{"value": "ca", "display_name": "Catalan"},
            <AllowedValueInfo>{"value": "zh", "display_name": "Chinese"},
            <AllowedValueInfo>{"value": "hr", "display_name": "Croatian"},
            <AllowedValueInfo>{"value": "cs", "display_name": "Czech"},
            <AllowedValueInfo>{"value": "da", "display_name": "Danish"},
            <AllowedValueInfo>{"value": "et", "display_name": "Estonian"},
            <AllowedValueInfo>{"value": "fo", "display_name": "Faroese"},
            <AllowedValueInfo>{"value": "fi", "display_name": "Finnish"},
            <AllowedValueInfo>{"value": "gl", "display_name": "Galician"},
            <AllowedValueInfo>{"value": "ka", "display_name": "Georgian"},
            <AllowedValueInfo>{"value": "el", "display_name": "Greek"},
            <AllowedValueInfo>{"value": "gu", "display_name": "Gujarati"},
            <AllowedValueInfo>{"value": "ht", "display_name": "Haitian"},
            <AllowedValueInfo>{"value": "ha", "display_name": "Hausa"},
            <AllowedValueInfo>{"value": "haw", "display_name": "Hawaiian"},
            <AllowedValueInfo>{"value": "he", "display_name": "Hebrew"},
            <AllowedValueInfo>{"value": "hi", "display_name": "Hindi"},
            <AllowedValueInfo>{"value": "hu", "display_name": "Hungarian"},
            <AllowedValueInfo>{"value": "is", "display_name": "Icelandic"},
            <AllowedValueInfo>{"value": "id", "display_name": "Indonesian"},
            <AllowedValueInfo>{"value": "ja", "display_name": "Japanese"},
            <AllowedValueInfo>{"value": "jw", "display_name": "Javanese"},
            <AllowedValueInfo>{"value": "kn", "display_name": "Kannada"},
            <AllowedValueInfo>{"value": "kk", "display_name": "Kazakh"},
            <AllowedValueInfo>{"value": "km", "display_name": "Khmer"},
            <AllowedValueInfo>{"value": "ko", "display_name": "Korean"},
            <AllowedValueInfo>{"value": "lo", "display_name": "Lao"},
            <AllowedValueInfo>{"value": "la", "display_name": "Latin"},
            <AllowedValueInfo>{"value": "lv", "display_name": "Latvian"},
            <AllowedValueInfo>{"value": "ln", "display_name": "Lingala"},
            <AllowedValueInfo>{"value": "lt", "display_name": "Lithuanian"},
            <AllowedValueInfo>{"value": "lb", "display_name": "Luxembourgish"},
            <AllowedValueInfo>{"value": "mk", "display_name": "Macedonian"},
            <AllowedValueInfo>{"value": "mg", "display_name": "Malagasy"},
            <AllowedValueInfo>{"value": "ms", "display_name": "Malay"},
            <AllowedValueInfo>{"value": "ml", "display_name": "Malayalam"},
            <AllowedValueInfo>{"value": "mt", "display_name": "Maltese"},
            <AllowedValueInfo>{"value": "mi", "display_name": "Maori"},
            <AllowedValueInfo>{"value": "mr", "display_name": "Marathi"},
            <AllowedValueInfo>{"value": "mn", "display_name": "Mongolian"},
            <AllowedValueInfo>{"value": "ne", "display_name": "Nepali"},
            <AllowedValueInfo>{"value": "no", "display_name": "Norwegian"},
            <AllowedValueInfo>{"value": "nn", "display_name": "Norwegian Nynorsk"},
            <AllowedValueInfo>{"value": "oc", "display_name": "Occitan"},
            <AllowedValueInfo>{"value": "pa", "display_name": "Panjabi"},
            <AllowedValueInfo>{"value": "ps", "display_name": "Pashto"},
            <AllowedValueInfo>{"value": "fa", "display_name": "Persian"},
            <AllowedValueInfo>{"value": "pl", "display_name": "Polish"},
            <AllowedValueInfo>{"value": "ro", "display_name": "Romanian"},
            <AllowedValueInfo>{"value": "ru", "display_name": "Russian"},
            <AllowedValueInfo>{"value": "sa", "display_name": "Sanskrit"},
            <AllowedValueInfo>{"value": "sr", "display_name": "Serbian"},
            <AllowedValueInfo>{"value": "sn", "display_name": "Shona"},
            <AllowedValueInfo>{"value": "sd", "display_name": "Sindhi"},
            <AllowedValueInfo>{"value": "si", "display_name": "Sinhala"},
            <AllowedValueInfo>{"value": "sk", "display_name": "Slovak"},
            <AllowedValueInfo>{"value": "sl", "display_name": "Slovenian"},
            <AllowedValueInfo>{"value": "so", "display_name": "Somali"},
            <AllowedValueInfo>{"value": "su", "display_name": "Sundanese"},
            <AllowedValueInfo>{"value": "sw", "display_name": "Swahili"},
            <AllowedValueInfo>{"value": "sv", "display_name": "Swedish"},
            <AllowedValueInfo>{"value": "tl", "display_name": "Tagalog"},
            <AllowedValueInfo>{"value": "tg", "display_name": "Tajik"},
            <AllowedValueInfo>{"value": "ta", "display_name": "Tamil"},
            <AllowedValueInfo>{"value": "tt", "display_name": "Tatar"},
            <AllowedValueInfo>{"value": "te", "display_name": "Telugu"},
            <AllowedValueInfo>{"value": "th", "display_name": "Thai"},
            <AllowedValueInfo>{"value": "bo", "display_name": "Tibetan"},
            <AllowedValueInfo>{"value": "tr", "display_name": "Turkish"},
            <AllowedValueInfo>{"value": "tk", "display_name": "Turkmen"},
            <AllowedValueInfo>{"value": "uk", "display_name": "Ukrainian"},
            <AllowedValueInfo>{"value": "ur", "display_name": "Urdu"},
            <AllowedValueInfo>{"value": "uz", "display_name": "Uzbek"},
            <AllowedValueInfo>{"value": "vi", "display_name": "Vietnamese"},
            <AllowedValueInfo>{"value": "cy", "display_name": "Welsh"},
            <AllowedValueInfo>{"value": "yi", "display_name": "Yiddish"},
            <AllowedValueInfo>{"value": "yo", "display_name": "Yoruba"},
        );

        #! Resource listings used by action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "transcripts": <RestSchemaReferenceDataInfo>{"name": "transcripts", "action": "list-transcripts", "records_path": ("transcripts",), "value_field": "id", "desc": "Recent transcripts; values from earlier workflow steps are also accepted."},
        };

        #! Stable action contracts resolved against the vendored API description
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "upload-audio",
                "method": "POST",
                "path": "/v2/upload",
                "operation_id": "uploadFile",
                "display_name": "Upload Audio",
                "short_desc": "Upload media bytes for subsequent transcription",
                "desc": "Upload an audio or video file and use the returned `upload_url` with Create Transcript or Transcribe Audio. Use the same project API key for upload and transcription.",
                "groups": ("Transcription",),
            },
            <RestSchemaActionInfo>{
                "action": "create-transcript",
                "method": "POST",
                "path": "/v2/transcript",
                "operation_id": "createTranscript",
                "display_name": "Create Transcript",
                "short_desc": "Submit audio for asynchronous transcription",
                "desc": "Submit an audio URL and return its transcript ID immediately. Use Get Transcript to check progress, or Transcribe Audio to wait for a completed result. Optional analysis, redaction, and speech understanding settings are included in the request.",
                "groups": ("Transcription",),
                "fields": {
                    "audio_url": <RestSchemaFieldOverlayInfo>{"display_name": "Audio URL", "preselected": True, "example_value": "https://example.com/meeting.wav"},
                    "speech_models": <RestSchemaFieldOverlayInfo>{"preselected": True, "element_allowed_values": (<AllowedValueInfo>{"value": "universal-3-5-pro", "display_name": "Universal-3.5 Pro"}, <AllowedValueInfo>{"value": "universal-2", "display_name": "Universal-2"})},
                    "language_code": <RestSchemaFieldOverlayInfo>{"preselected": True, "exclusive_with": ("language_detection",), "allowed_values": Languages, "allowed_values_creatable": True},
                    "language_codes": <RestSchemaFieldOverlayInfo>{"element_allowed_values": Languages},
                    "speaker_labels": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "webhook_auth_header_value": <RestSchemaFieldOverlayInfo>{"sensitive": True},
                    "auto_chapters": <RestSchemaFieldOverlayInfo>{"desc": "Generate chapter summaries with Universal-2. Requires `punctuate: true` and cannot be combined with `summarization`. **Deprecated:** use [LLM Gateway](https://www.assemblyai.com/docs/llm-gateway/quickstart) for flexible chapter summaries."},
                    "summarization": <RestSchemaFieldOverlayInfo>{"desc": "Summarize a Universal-2 transcript. Requires `punctuate: true` and `format_text: true`; cannot be combined with `auto_chapters`. **Deprecated:** use [LLM Gateway](https://www.assemblyai.com/docs/llm-gateway/quickstart) for flexible summaries."},
                    "redact_pii_return_unredacted": <RestSchemaFieldOverlayInfo>{"desc": "Return `unredacted_text`, `unredacted_words`, and `unredacted_utterances` alongside the redacted fields. Requires `redact_pii: true`. **Sensitive output:** enabling this retains original personal information. Disabled by default. See [PII redaction](https://www.assemblyai.com/docs/pii-redaction)."},
                    "redact_static_entities": <RestSchemaFieldOverlayInfo>{"desc": "**Protect proprietary terms**\n- Map redaction labels to exact terms, for example `{\"INTERNAL_TOOL\": [\"Bearclaw\"]}`. Requires `redact_pii: true` and uses `redact_pii_sub`. Matching tolerates casing, punctuation, and minor spacing differences. It does not generalize beyond supplied terms.\n- With `redact_pii_audio`, matches are also removed from audio.\n\n**Limits**\n- Up to 100 labels, 200 terms per label, and 200 characters per term.\n- Labels allow letters, numbers, spaces, underscores, and hyphens, up to 80 characters.\n\nSee [Static Entity Redaction](https://www.assemblyai.com/docs/guardrails/redact-pii-from-transcripts#static-entity-redaction)."},
                    "language_detection_options": <RestSchemaFieldOverlayInfo>{"fields": {
                        "localization": <RestSchemaFieldOverlayInfo>{"desc": "Use `en_au` or `en_uk` spelling when English is detected; only one variant per base language is allowed. Base/default codes such as `en` or `en_us` are rejected. The response's `language_code` reports the chosen regional code. Other detected languages ignore this setting. See [Language Detection](https://www.assemblyai.com/docs/pre-recorded-audio/language-detection)."},
                        "on_no_speech_detected": <RestSchemaFieldOverlayInfo>{"desc": "With `error` (default), audio without speech fails with a reason in `error`. With `fallback`, it completes with empty `text`, the configured `fallback_language`, and a warning in `metadata.warnings`. **Fallback requires a specific language:** `auto` is rejected. See [Language Detection](https://www.assemblyai.com/docs/pre-recorded-audio/language-detection)."},
                    }},
                    "speaker_options": <RestSchemaFieldOverlayInfo>{"fields": {
                        "max_speakers_expected": <RestSchemaFieldOverlayInfo>{"desc": "Cap the number of speaker labels; additional speakers merge into existing labels. **Too high a limit can reduce accuracy by splitting one speaker into several.** Defaults: no limit for 0–2 minutes, 10 for 2–10 minutes, 30 above 10 minutes. See [Speaker Ranges](https://www.assemblyai.com/docs/pre-recorded-audio/label-speakers#set-a-range-of-possible-speakers)."},
                    }},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-transcripts",
                "method": "GET",
                "path": "/v2/transcript",
                "operation_id": "listTranscripts",
                "display_name": "List Transcripts",
                "short_desc": "Browse recent transcripts with status and date filters",
                "desc": "List transcripts from the last 90 days, newest first. Use `before_id` or `after_id` to page through results.",
                "groups": ("Transcription",),
            },
            <RestSchemaActionInfo>{
                "action": "get-transcript",
                "method": "GET",
                "path": "/v2/transcript/{transcript_id}",
                "operation_id": "getTranscript",
                "display_name": "Get Transcript",
                "short_desc": "Retrieve transcript status, text, and requested analysis",
                "desc": "Retrieve a transcript by ID. Read `status` before using its text: queued and processing jobs do not yet have their final result.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-transcript",
                "method": "DELETE",
                "path": "/v2/transcript/{transcript_id}",
                "operation_id": "deleteTranscript",
                "display_name": "Delete Transcript",
                "short_desc": "Delete a transcript and its associated uploaded media",
                "desc": "Delete transcript data and associated uploaded media. The API returns the deleted transcript record. This operation cannot be undone.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-subtitles",
                "method": "GET",
                "path": "/v2/transcript/{transcript_id}/{subtitle_format}",
                "operation_id": "getSubtitles",
                "display_name": "Get Subtitles",
                "short_desc": "Export a completed transcript as SRT or WebVTT captions",
                "desc": "Export captions from a completed transcript. Choose SRT or VTT and optionally limit characters per caption. The result is subtitle text.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                    "subtitle_format": <RestSchemaFieldOverlayInfo>{"display_name": "Subtitle Format", "default_value": "srt", "allowed_values": (<AllowedValueInfo>{"value": "srt", "display_name": "SubRip (SRT)"}, <AllowedValueInfo>{"value": "vtt", "display_name": "WebVTT"})},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-sentences",
                "method": "GET",
                "path": "/v2/transcript/{transcript_id}/sentences",
                "operation_id": "getTranscriptSentences",
                "display_name": "Get Transcript Sentences",
                "short_desc": "Split a completed transcript into timed sentences",
                "desc": "Retrieve sentences with text, timestamps, confidence, and word details from a completed transcript.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-paragraphs",
                "method": "GET",
                "path": "/v2/transcript/{transcript_id}/paragraphs",
                "operation_id": "getTranscriptParagraphs",
                "display_name": "Get Transcript Paragraphs",
                "short_desc": "Split a completed transcript into timed paragraphs",
                "desc": "Retrieve paragraphs with text, timestamps, confidence, and word details from a completed transcript.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-transcript-words",
                "method": "GET",
                "path": "/v2/transcript/{transcript_id}/word-search",
                "operation_id": "wordSearch",
                "display_name": "Search Transcript Words",
                "short_desc": "Find words or short phrases and their audio timestamps",
                "desc": "Search a completed transcript for words, numbers, or phrases of up to five words. Returns matches and their timestamps.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-redacted-audio",
                "method": "GET",
                "path": "/v2/transcript/{transcript_id}/redacted-audio",
                "operation_id": "getRedactedAudio",
                "display_name": "Get Redacted Audio",
                "short_desc": "Retrieve the status and download URL of redacted audio",
                "desc": "Retrieve redacted audio generated with `redact_pii_audio` during transcription. Download URLs expire after 24 hours.",
                "groups": ("Transcription",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"display_name": "Transcript", "ref_data": "transcripts", "supports_custom_values": True},
                },
            },
        );
    }
}
}
