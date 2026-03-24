# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The ElevenLabs data provider class
public class ElevenLabsDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "elevenlabs",
            "type": "ElevenLabsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        #! Application name
        const AppName = ElevenLabsAppName;
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Child data providers
        const DefaultChildMap = {
            "add-pronunciation-rules":
                Class::forName("ElevenLabsDataProvider::ElevenLabsAddPronunciationRulesDataProvider"),
            "audio-isolation":
                Class::forName("ElevenLabsDataProvider::ElevenLabsAudioIsolationDataProvider"),
            "create-dubbing":
                Class::forName("ElevenLabsDataProvider::ElevenLabsCreateDubbingDataProvider"),
            "create-pronunciation-dictionary":
                Class::forName("ElevenLabsDataProvider::ElevenLabsCreatePronunciationDictionaryDataProvider"),
            "delete-dubbing":
                Class::forName("ElevenLabsDataProvider::ElevenLabsDeleteDubbingDataProvider"),
            "create-voice":
                Class::forName("ElevenLabsDataProvider::ElevenLabsCreateVoiceDataProvider"),
            "delete-history-item":
                Class::forName("ElevenLabsDataProvider::ElevenLabsDeleteHistoryItemDataProvider"),
            "delete-voice":
                Class::forName("ElevenLabsDataProvider::ElevenLabsDeleteVoiceDataProvider"),
            "edit-voice":
                Class::forName("ElevenLabsDataProvider::ElevenLabsEditVoiceDataProvider"),
            "get-dubbing":
                Class::forName("ElevenLabsDataProvider::ElevenLabsGetDubbingDataProvider"),
            "get-dubbing-audio":
                Class::forName("ElevenLabsDataProvider::ElevenLabsGetDubbingAudioDataProvider"),
            "get-pronunciation-dictionary":
                Class::forName("ElevenLabsDataProvider::ElevenLabsGetPronunciationDictionaryDataProvider"),
            "get-history-audio":
                Class::forName("ElevenLabsDataProvider::ElevenLabsGetHistoryAudioDataProvider"),
            "get-history-item":
                Class::forName("ElevenLabsDataProvider::ElevenLabsGetHistoryItemDataProvider"),
            "get-voice":
                Class::forName("ElevenLabsDataProvider::ElevenLabsGetVoiceDataProvider"),
            "history":
                Class::forName("ElevenLabsDataProvider::ElevenLabsHistoryDataProvider"),
            "list-dubbing":
                Class::forName("ElevenLabsDataProvider::ElevenLabsListDubbingDataProvider"),
            "list-pronunciation-dictionaries":
                Class::forName("ElevenLabsDataProvider::ElevenLabsListPronunciationDictionariesDataProvider"),
            "models":
                Class::forName("ElevenLabsDataProvider::ElevenLabsModelsDataProvider"),
            "speech-to-speech":
                Class::forName("ElevenLabsDataProvider::ElevenLabsSpeechToSpeechDataProvider"),
            "speech-to-text":
                Class::forName("ElevenLabsDataProvider::ElevenLabsSpeechToTextDataProvider"),
            "text-to-sound-effects":
                Class::forName("ElevenLabsDataProvider::ElevenLabsTextToSoundEffectsDataProvider"),
            "text-to-speech":
                Class::forName("ElevenLabsDataProvider::ElevenLabsTextToSpeechDataProvider"),
            "voice-design":
                Class::forName("ElevenLabsDataProvider::ElevenLabsVoiceDesignDataProvider"),
            "voices":
                Class::forName("ElevenLabsDataProvider::ElevenLabsVoicesDataProvider"),
        };
    }

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) : ElevenLabsDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : ElevenLabsDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Register a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "ELEVENLABS-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
