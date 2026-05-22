# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepgramDataProvider request/response data type definitions

/** DeepgramDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DeepgramDataProvider {
#! Transcribe-file request: URL submission with model + ASR options
public class DeepgramTranscribeFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Audio URL",
                "short_desc": "Public URL of the audio file to transcribe",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Public URL of the audio file to transcribe.  Deepgram fetches "
                    "the file from this URL; it must be reachable from the public "
                    "internet.",
                "required": True,
                "example_value": "https://example.com/audio.wav",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Deepgram ASR model name",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The Deepgram ASR model — one of `nova-3` (latest), `nova-2`, "
                    "`nova`, `enhanced`, or `base`.  `nova-3` offers the best accuracy "
                    "for general-purpose transcription.",
                "default_value": "nova-3",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "nova-3", "display_name": "Nova 3 (latest)"},
                    <AllowedValueInfo>{"value": "nova-2", "display_name": "Nova 2"},
                    <AllowedValueInfo>{"value": "nova",   "display_name": "Nova"},
                    <AllowedValueInfo>{"value": "enhanced", "display_name": "Enhanced"},
                    <AllowedValueInfo>{"value": "base",   "display_name": "Base"},
                ),
            },
            "language": {
                "display_name": "Language",
                "short_desc": "BCP-47 language code (e.g., `en-US`, `de`, `fr`)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "BCP-47 language code.  Pass `multi` to enable Nova-3's "
                    "multilingual mode.  Default is `en` (English).",
                "default_value": "en",
                "example_value": "en-US",
            },
            "smart_format": {
                "display_name": "Smart Format",
                "short_desc": "Apply punctuation, capitalization, and number formatting",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, applies smart formatting (punctuation, "
                    "capitalization, dates, currency, etc.) to the transcript.",
                "default_value": True,
            },
            "punctuate": {
                "display_name": "Punctuate",
                "short_desc": "Add punctuation to the transcript",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "Add punctuation to the transcript.  Implied by `smart_format`.",
            },
            "diarize": {
                "display_name": "Diarize",
                "short_desc": "Identify distinct speakers in the audio",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, attempts speaker diarization — labels segments "
                    "with `speaker: 0`, `speaker: 1`, etc.",
            },
            "summarize": {
                "display_name": "Summarize",
                "short_desc": "Return a summary of the transcript",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "When set to `v2`, returns an abstractive summary of the "
                    "transcript in addition to the words.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "v2", "display_name": "v2 (abstractive)"},
                ),
            },
            "detect_language": {
                "display_name": "Detect Language",
                "short_desc": "Auto-detect the spoken language",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, ignores the `language` parameter and detects "
                    "the language automatically.",
            },
            "utterances": {
                "display_name": "Utterances",
                "short_desc": "Group transcript words into utterances",
                "type": AbstractDataProviderTypeMap."*softbool",
                "desc": "When `True`, groups words into utterances based on natural "
                    "speech pauses.",
            },
            "keywords": {
                "display_name": "Keywords",
                "short_desc": "Keywords to boost during recognition (list of strings)",
                "type": AbstractDataProviderTypeMap."*softlist",
                "desc": "List of keywords to boost during recognition.  Each entry "
                    "may also include a `:<intensity>` suffix (e.g., `Qore:2`).",
            },
            "callback": {
                "display_name": "Callback URL",
                "short_desc": "Webhook URL for async transcript delivery",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "When provided, Deepgram returns immediately and POSTs the "
                    "final transcript to this URL when ready.  Use for long files.",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Transcribe-file response: Deepgram listen result
public class DeepgramTranscribeFileResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "metadata": {
                "display_name": "Metadata",
                "desc": "Deepgram metadata: request ID, model info, audio duration, etc.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "results": {
                "display_name": "Results",
                "desc": "Top-level results object containing `channels[]` with "
                    "`alternatives[]` (transcript + words + confidence) — and "
                    "`utterances[]` when `utterances` was requested.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Synthesize-speech request: text to audio bytes
public class DeepgramSynthesizeSpeechRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "Text to synthesize",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The text to synthesize into speech.  Maximum length depends "
                    "on the model — typically up to ~2000 characters per request.",
                "required": True,
                "example_value": "Hello, world!",
            },
            "model": {
                "display_name": "Voice Model",
                "short_desc": "Deepgram TTS voice model",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The Deepgram TTS voice model — e.g. `aura-asteria-en` (female, "
                    "English), `aura-luna-en`, `aura-stella-en`, `aura-athena-en`, "
                    "`aura-hera-en`, `aura-orion-en` (male), `aura-arcas-en`, "
                    "`aura-perseus-en`, `aura-angus-en` (Irish English), "
                    "`aura-orpheus-en`, `aura-helios-en`, `aura-zeus-en`.",
                "default_value": "aura-asteria-en",
                "example_value": "aura-asteria-en",
            },
            "encoding": {
                "display_name": "Audio Encoding",
                "short_desc": "Output audio encoding",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Output audio encoding.  `mp3` (default), `linear16` (16-bit "
                    "PCM), `flac`, `mulaw`, `alaw`, `opus`, or `aac`.",
                "default_value": "mp3",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "mp3",      "display_name": "MP3"},
                    <AllowedValueInfo>{"value": "linear16", "display_name": "Linear 16 (PCM)"},
                    <AllowedValueInfo>{"value": "flac",     "display_name": "FLAC"},
                    <AllowedValueInfo>{"value": "mulaw",    "display_name": "Mu-law"},
                    <AllowedValueInfo>{"value": "alaw",     "display_name": "A-law"},
                    <AllowedValueInfo>{"value": "opus",     "display_name": "Opus"},
                    <AllowedValueInfo>{"value": "aac",      "display_name": "AAC"},
                ),
            },
            "sample_rate": {
                "display_name": "Sample Rate",
                "short_desc": "Output sample rate in Hz (encoding-specific)",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "Output sample rate in Hz.  Valid values depend on the "
                    "selected encoding — e.g. `8000`, `16000`, `24000`, `32000`, "
                    "`48000` for linear16.",
                "example_value": 24000,
            },
            "bit_rate": {
                "display_name": "Bit Rate",
                "short_desc": "Output bit rate (for compressed encodings)",
                "type": AbstractDataProviderTypeMap."*softint",
                "desc": "Output bit rate in bits/second.  Applies to compressed "
                    "encodings (`mp3`, `opus`, `aac`).  Typical values: `32000`, "
                    "`48000`, `64000`, `128000`.",
            },
            "container": {
                "display_name": "Container",
                "short_desc": "Container format wrapping the audio",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Container format wrapping the audio (e.g., `wav`, `ogg`, "
                    "`none`).  Defaults to a sensible value for each encoding.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "wav",  "display_name": "WAV"},
                    <AllowedValueInfo>{"value": "ogg",  "display_name": "OGG"},
                    <AllowedValueInfo>{"value": "none", "display_name": "None (raw)"},
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Synthesize-speech response: raw audio bytes + content type
public class DeepgramSynthesizeSpeechResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "audio": {
                "display_name": "Audio Bytes",
                "desc": "Raw audio bytes in the requested encoding.",
                "type": AbstractDataProviderTypeMap."binary",
            },
            "content_type": {
                "display_name": "Content Type",
                "desc": "MIME type of the returned audio (e.g., `audio/mpeg`, "
                    "`audio/wav`, `audio/flac`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "request_id": {
                "display_name": "Request ID",
                "desc": "Deepgram request ID from the `dg-request-id` response header.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}
