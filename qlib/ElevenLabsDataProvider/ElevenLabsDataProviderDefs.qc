# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElevenLabsDataProvider module
public namespace ElevenLabsDataProvider {
    #! ElevenLabs app name
    public const ElevenLabsAppName = "ElevenLabs";

    #! Allowed output format values for audio generation
    public const OutputFormatAllowedValues = (
        <AllowedValueInfo>{
            "display_name": "MP3 44.1kHz 128kbps",
            "value": "mp3_44100_128",
            "desc": "MP3 audio at 44.1kHz sample rate and 128kbps bitrate",
        },
        <AllowedValueInfo>{
            "display_name": "MP3 44.1kHz 64kbps",
            "value": "mp3_44100_64",
            "desc": "MP3 audio at 44.1kHz sample rate and 64kbps bitrate",
        },
        <AllowedValueInfo>{
            "display_name": "MP3 44.1kHz 32kbps",
            "value": "mp3_44100_32",
            "desc": "MP3 audio at 44.1kHz sample rate and 32kbps bitrate",
        },
        <AllowedValueInfo>{
            "display_name": "MP3 44.1kHz 192kbps",
            "value": "mp3_44100_192",
            "desc": "MP3 audio at 44.1kHz sample rate and 192kbps bitrate",
        },
        <AllowedValueInfo>{
            "display_name": "MP3 22.05kHz 32kbps",
            "value": "mp3_22050_32",
            "desc": "MP3 audio at 22.05kHz sample rate and 32kbps bitrate",
        },
        <AllowedValueInfo>{
            "display_name": "PCM 16kHz",
            "value": "pcm_16000",
            "desc": "PCM audio at 16kHz sample rate (16-bit signed little-endian)",
        },
        <AllowedValueInfo>{
            "display_name": "PCM 22.05kHz",
            "value": "pcm_22050",
            "desc": "PCM audio at 22.05kHz sample rate (16-bit signed little-endian)",
        },
        <AllowedValueInfo>{
            "display_name": "PCM 24kHz",
            "value": "pcm_24000",
            "desc": "PCM audio at 24kHz sample rate (16-bit signed little-endian)",
        },
        <AllowedValueInfo>{
            "display_name": "PCM 44.1kHz",
            "value": "pcm_44100",
            "desc": "PCM audio at 44.1kHz sample rate (16-bit signed little-endian)",
        },
        <AllowedValueInfo>{
            "display_name": "u-law 8kHz",
            "value": "ulaw_8000",
            "desc": "u-law audio at 8kHz sample rate",
        },
    );

    #! Allowed TTS model values
    public const TtsModelAllowedValues = (
        <AllowedValueInfo>{
            "display_name": "Eleven Multilingual v2",
            "value": "eleven_multilingual_v2",
            "desc": "Mature model supporting 29 languages with 10,000 character limit",
        },
        <AllowedValueInfo>{
            "display_name": "Eleven Flash v2.5",
            "value": "eleven_flash_v2_5",
            "desc": "Speed-optimized model with ~75ms latency, 32 languages, 40,000 character limit, 50% lower "
                "cost",
        },
        <AllowedValueInfo>{
            "display_name": "Eleven Turbo v2.5",
            "value": "eleven_turbo_v2_5",
            "desc": "High-quality low-latency model for 32 languages",
        },
        <AllowedValueInfo>{
            "display_name": "Eleven Monolingual v1",
            "value": "eleven_monolingual_v1",
            "desc": "English-only model optimized for English content",
        },
    );

    #! Allowed STS model values
    public const StsModelAllowedValues = (
        <AllowedValueInfo>{
            "display_name": "Eleven English STS v2",
            "value": "eleven_english_sts_v2",
            "desc": "Default speech-to-speech model for English voice conversion",
        },
        <AllowedValueInfo>{
            "display_name": "Eleven Multilingual STS v2",
            "value": "eleven_multilingual_sts_v2",
            "desc": "Multilingual speech-to-speech model for voice conversion",
        },
    );

    #! Allowed STT model values
    public const SttModelAllowedValues = (
        <AllowedValueInfo>{
            "display_name": "Scribe v2",
            "value": "scribe_v2",
            "desc": "Speech-to-text model supporting 90+ languages with word-level timestamps and speaker "
                "diarization",
        },
    );

    #! Common voice settings type
    public class ElevenLabsVoiceSettingsType inherits HashDataType {
        public {
            const Fields = {
                "stability": {
                    "display_name": "Stability",
                    "short_desc": "Voice consistency between regenerations",
                    "desc": "Voice consistency between regenerations. Lower values produce more emotional and "
                        "expressive speech, higher values produce more consistent output. Range: `0.0` to `1.0`",
                    "type": AbstractDataProviderTypeMap."*float",
                },
                "similarity_boost": {
                    "display_name": "Similarity Boost",
                    "short_desc": "Adherence to the original voice",
                    "desc": "How closely the AI should adhere to the original voice. Higher values increase "
                        "similarity but may introduce artifacts. Range: `0.0` to `1.0`",
                    "type": AbstractDataProviderTypeMap."*float",
                },
                "style": {
                    "display_name": "Style",
                    "short_desc": "Emotional exaggeration of the voice",
                    "desc": "Emotional exaggeration of the voice. Higher values increase expressiveness but may "
                        "reduce stability. Range: `0.0` to `1.0`. Only available in `eleven_multilingual_v2`",
                    "type": AbstractDataProviderTypeMap."*float",
                },
                "use_speaker_boost": {
                    "display_name": "Speaker Boost",
                    "short_desc": "Enhanced speaker similarity",
                    "desc": "If `True`, boosts similarity to the original speaker. Increases latency slightly",
                    "type": AbstractDataProviderTypeMap."*bool",
                },
                "speed": {
                    "display_name": "Speed",
                    "short_desc": "Speed adjustment for the voice",
                    "desc": "Speed adjustment for the voice. `1.0` is the default speed",
                    "type": AbstractDataProviderTypeMap."*float",
                },
            };
        }

        #! Creates the type with the default base type
        constructor() : HashDataType("ElevenLabsVoiceSettings") {
            addQoreFields(Fields);
        }

        #! Creates the type with a custom base type (e.g., AutoHashOrNothingType for optional fields)
        constructor(Type base_type) : HashDataType(base_type, "ElevenLabsVoiceSettings") {
            addQoreFields(Fields);
        }
    }

    #! Singleton instance of voice settings type (for optional fields — accepts NOTHING)
    public const VoiceSettingsType = new ElevenLabsVoiceSettingsType(AutoHashOrNothingType);

    #! Common binary audio response type
    public class ElevenLabsAudioResponseType inherits HashDataType {
        public {
            const Fields = {
                "audio": {
                    "display_name": "Audio Data",
                    "short_desc": "The generated audio data",
                    "desc": "The generated audio data as a binary blob",
                    "type": AbstractDataProviderTypeMap."binary",
                },
                "content_type": {
                    "display_name": "Content Type",
                    "short_desc": "The MIME type of the audio data",
                    "desc": "The MIME type of the audio data (e.g., `audio/mpeg`)",
                    "type": AbstractDataProviderTypeMap."string",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsAudioResponse") {
            addQoreFields(Fields);
        }
    }

    #! Singleton instance of audio response type
    public const AudioResponseType = new ElevenLabsAudioResponseType();

    #! Voice summary data type for list responses
    public class ElevenLabsVoiceSummaryDataType inherits HashDataType {
        public {
            const Fields = {
                "voice_id": {
                    "display_name": "Voice ID",
                    "short_desc": "The unique identifier of the voice",
                    "desc": "The unique identifier of the voice",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "name": {
                    "display_name": "Name",
                    "short_desc": "The name of the voice",
                    "desc": "The name of the voice",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "category": {
                    "display_name": "Category",
                    "short_desc": "The category of the voice",
                    "desc": "The category of the voice (e.g., `premade`, `cloned`, `generated`)",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "description": {
                    "display_name": "Description",
                    "short_desc": "The description of the voice",
                    "desc": "The description of the voice",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "labels": {
                    "display_name": "Labels",
                    "short_desc": "Labels associated with the voice",
                    "desc": "Key-value labels associated with the voice",
                    "type": AbstractDataProviderTypeMap."*hash",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsVoiceSummary") {
            addQoreFields(Fields);
        }
    }

    #! Model data type for list responses
    public class ElevenLabsModelDataType inherits HashDataType {
        public {
            const Fields = {
                "model_id": {
                    "display_name": "Model ID",
                    "short_desc": "The unique identifier of the model",
                    "desc": "The unique identifier of the model",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "name": {
                    "display_name": "Name",
                    "short_desc": "The name of the model",
                    "desc": "The name of the model",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "description": {
                    "display_name": "Description",
                    "short_desc": "A description of the model",
                    "desc": "A description of the model and its capabilities",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "can_be_finetuned": {
                    "display_name": "Can Be Finetuned",
                    "short_desc": "Whether the model can be finetuned",
                    "desc": "Whether the model can be finetuned with custom voice data",
                    "type": AbstractDataProviderTypeMap."*bool",
                },
                "can_do_text_to_speech": {
                    "display_name": "Can Do Text to Speech",
                    "short_desc": "Whether the model supports TTS",
                    "desc": "Whether the model supports text-to-speech synthesis",
                    "type": AbstractDataProviderTypeMap."*bool",
                },
                "can_do_voice_conversion": {
                    "display_name": "Can Do Voice Conversion",
                    "short_desc": "Whether the model supports voice conversion",
                    "desc": "Whether the model supports speech-to-speech voice conversion",
                    "type": AbstractDataProviderTypeMap."*bool",
                },
                "languages": {
                    "display_name": "Languages",
                    "short_desc": "Supported languages",
                    "desc": "The list of languages supported by this model",
                    "type": new SoftListDataType("Languages", new ElevenLabsLanguageDataType(), False),
                },
            };
        }

        constructor() : HashDataType("ElevenLabsModel") {
            addQoreFields(Fields);
        }
    }

    #! History item data type for list responses
    public class ElevenLabsHistoryItemDataType inherits HashDataType {
        public {
            const Fields = {
                "history_item_id": {
                    "display_name": "History Item ID",
                    "short_desc": "The unique identifier of the history item",
                    "desc": "The unique identifier of the history item",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "voice_id": {
                    "display_name": "Voice ID",
                    "short_desc": "The ID of the voice used",
                    "desc": "The unique identifier of the voice used for generation",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "voice_name": {
                    "display_name": "Voice Name",
                    "short_desc": "The name of the voice used",
                    "desc": "The name of the voice used for generation",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "text": {
                    "display_name": "Text",
                    "short_desc": "The input text",
                    "desc": "The text that was used for generation",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "date_unix": {
                    "display_name": "Date (Unix)",
                    "short_desc": "Unix timestamp of the generation",
                    "desc": "The Unix timestamp of when the generation was created",
                    "type": AbstractDataProviderTypeMap."*int",
                },
                "state": {
                    "display_name": "State",
                    "short_desc": "The state of the history item",
                    "desc": "The current state of the history item",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "content_type": {
                    "display_name": "Content Type",
                    "short_desc": "The MIME type of the generated audio",
                    "desc": "The MIME type of the generated audio content",
                    "type": AbstractDataProviderTypeMap."*string",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsHistoryItem") {
            addQoreFields(Fields);
        }
    }

    #! Pronunciation dictionary summary data type for list responses
    public class ElevenLabsPronunciationDictionarySummaryDataType inherits HashDataType {
        public {
            const Fields = {
                "id": {
                    "display_name": "ID",
                    "short_desc": "The unique identifier of the dictionary",
                    "desc": "The unique identifier of the pronunciation dictionary",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "name": {
                    "display_name": "Name",
                    "short_desc": "The name of the dictionary",
                    "desc": "The name of the pronunciation dictionary",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "description": {
                    "display_name": "Description",
                    "short_desc": "The description of the dictionary",
                    "desc": "The description of the pronunciation dictionary",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "created_by": {
                    "display_name": "Created By",
                    "short_desc": "The user who created the dictionary",
                    "desc": "The user ID of the person who created the dictionary",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "creation_time_unix": {
                    "display_name": "Creation Time",
                    "short_desc": "The creation timestamp",
                    "desc": "The Unix timestamp when the dictionary was created",
                    "type": AbstractDataProviderTypeMap."*int",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsPronunciationDictionarySummary") {
            addQoreFields(Fields);
        }
    }

    #! Language data type for model responses
    public class ElevenLabsLanguageDataType inherits HashDataType {
        public {
            const Fields = {
                "language_id": {
                    "display_name": "Language ID",
                    "short_desc": "The language identifier",
                    "desc": "The language identifier code",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "name": {
                    "display_name": "Name",
                    "short_desc": "The language name",
                    "desc": "The display name of the language",
                    "type": AbstractDataProviderTypeMap."string",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsLanguage") {
            addQoreFields(Fields);
        }
    }

    #! Voice sample data type for get-voice responses
    public class ElevenLabsVoiceSampleDataType inherits HashDataType {
        public {
            const Fields = {
                "sample_id": {
                    "display_name": "Sample ID",
                    "short_desc": "The unique identifier of the sample",
                    "desc": "The unique identifier of the voice sample",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "file_name": {
                    "display_name": "File Name",
                    "short_desc": "The file name of the sample",
                    "desc": "The file name of the voice sample",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "mime_type": {
                    "display_name": "MIME Type",
                    "short_desc": "The MIME type of the sample",
                    "desc": "The MIME type of the voice sample audio file",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "size_bytes": {
                    "display_name": "Size (Bytes)",
                    "short_desc": "The size of the sample in bytes",
                    "desc": "The size of the voice sample file in bytes",
                    "type": AbstractDataProviderTypeMap."*int",
                },
                "hash": {
                    "display_name": "Hash",
                    "short_desc": "The hash of the sample file",
                    "desc": "The hash of the voice sample file content",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "duration_secs": {
                    "display_name": "Duration (Seconds)",
                    "short_desc": "Duration of the sample in seconds",
                    "desc": "The duration of the voice sample in seconds",
                    "type": AbstractDataProviderTypeMap."*float",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsVoiceSample") {
            addQoreFields(Fields);
        }
    }

    #! Voice preview data type for voice design responses
    public class ElevenLabsVoicePreviewDataType inherits HashDataType {
        public {
            const Fields = {
                "generated_voice_id": {
                    "display_name": "Generated Voice ID",
                    "short_desc": "The ID of the generated voice preview",
                    "desc": "The unique identifier for the generated voice preview",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "audio_base_64": {
                    "display_name": "Audio (Base64)",
                    "short_desc": "Base64-encoded audio data",
                    "desc": "The generated audio data encoded as a base64 string",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "media_type": {
                    "display_name": "Media Type",
                    "short_desc": "The media type of the audio",
                    "desc": "The MIME media type of the generated audio (e.g., `audio/mpeg`)",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "duration_secs": {
                    "display_name": "Duration (Seconds)",
                    "short_desc": "Duration of the preview in seconds",
                    "desc": "The duration of the generated audio preview in seconds",
                    "type": AbstractDataProviderTypeMap."*float",
                },
                "language": {
                    "display_name": "Language",
                    "short_desc": "The detected language",
                    "desc": "The detected language of the generated voice preview",
                    "type": AbstractDataProviderTypeMap."*string",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsVoicePreview") {
            addQoreFields(Fields);
        }
    }

    #! Pronunciation dictionary locator type for TTS requests
    public class ElevenLabsPronunciationDictionaryLocatorType inherits HashDataType {
        public {
            const Fields = {
                "pronunciation_dictionary_id": {
                    "display_name": "Dictionary ID",
                    "short_desc": "The ID of the pronunciation dictionary",
                    "desc": "The unique identifier of the pronunciation dictionary to use",
                    "type": AbstractDataProviderTypeMap."string",
                    "required": True,
                    "ref_data": "pronunciation_dictionaries",
                },
                "version_id": {
                    "display_name": "Version ID",
                    "short_desc": "The version ID of the dictionary",
                    "desc": "The version ID of the pronunciation dictionary to use",
                    "type": AbstractDataProviderTypeMap."string",
                    "required": True,
                },
            };
        }

        constructor() : HashDataType("ElevenLabsPronunciationDictionaryLocator") {
            addQoreFields(Fields);
        }
    }

    #! Pronunciation rule data type for add-rules requests
    public class ElevenLabsPronunciationRuleType inherits HashDataType {
        public {
            const Fields = {
                "type": {
                    "display_name": "Rule Type",
                    "short_desc": "The type of pronunciation rule",
                    "desc": "The type of pronunciation rule",
                    "type": AbstractDataProviderTypeMap."string",
                    "required": True,
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "display_name": "Alias",
                            "value": "alias",
                            "desc": "Replace a string with an alias pronunciation",
                        },
                        <AllowedValueInfo>{
                            "display_name": "Phoneme",
                            "value": "phoneme",
                            "desc": "Replace a string with a phoneme representation",
                        },
                    ),
                },
                "string_to_replace": {
                    "display_name": "String to Replace",
                    "short_desc": "The text string to match for replacement",
                    "desc": "The text string to match for pronunciation replacement",
                    "type": AbstractDataProviderTypeMap."string",
                    "required": True,
                    "example_value": "tomato",
                },
                "alias": {
                    "display_name": "Alias",
                    "short_desc": "The alias text to use for pronunciation",
                    "desc": "The alias text to use for pronunciation. Required when `type` is `alias`",
                    "type": AbstractDataProviderTypeMap."*string",
                    "example_value": "toh-mah-toh",
                },
                "phoneme": {
                    "display_name": "Phoneme",
                    "short_desc": "The phoneme representation",
                    "desc": "The phoneme representation for pronunciation. Required when `type` is `phoneme`",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "alphabet": {
                    "display_name": "Alphabet",
                    "short_desc": "The phoneme alphabet to use",
                    "desc": "The phoneme alphabet to use (e.g., `ipa`). Required when `type` is `phoneme`",
                    "type": AbstractDataProviderTypeMap."*string",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsPronunciationRule") {
            addQoreFields(Fields);
        }
    }

    #! Word timing data type for speech-to-text responses
    public class ElevenLabsWordTimingDataType inherits HashDataType {
        public {
            const Fields = {
                "text": {
                    "display_name": "Text",
                    "short_desc": "The word text",
                    "desc": "The transcribed word text",
                    "type": AbstractDataProviderTypeMap."string",
                },
                "start": {
                    "display_name": "Start Time",
                    "short_desc": "Start time in seconds",
                    "desc": "The start time of the word in seconds from the beginning of the audio",
                    "type": AbstractDataProviderTypeMap."*float",
                },
                "end": {
                    "display_name": "End Time",
                    "short_desc": "End time in seconds",
                    "desc": "The end time of the word in seconds from the beginning of the audio",
                    "type": AbstractDataProviderTypeMap."*float",
                },
                "type": {
                    "display_name": "Type",
                    "short_desc": "The type of the word element",
                    "desc": "The type of the element (e.g., `word`, `spacing`, `audio_event`)",
                    "type": AbstractDataProviderTypeMap."*string",
                },
                "speaker_id": {
                    "display_name": "Speaker ID",
                    "short_desc": "Speaker identifier for diarization",
                    "desc": "The speaker identifier when diarization is enabled",
                    "type": AbstractDataProviderTypeMap."*string",
                },
            };
        }

        constructor() : HashDataType("ElevenLabsWordTiming") {
            addQoreFields(Fields);
        }
    }
}
