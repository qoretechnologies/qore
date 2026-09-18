# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider root class definition

/** AssemblyAiActions.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Complete action registration, shared by initial and deferred discovery
public class AssemblyAiActions {
    public {
        #! Stable response contract for HTTP operations returning no body
        const EmptyResponseType = AbstractDataProviderType::get(NothingType);
    }
    #! Returns explicit action metadata for the requested application
    /** @param app application identity
        @return all schema actions and the submit-and-wait helper
    */
    static list<hash<DataProviderActionInfo>> getActionInfoList(string app = AppName) {
        AssemblyAiTranscribeDataProvider::initializeTypes();
        list<hash<DataProviderActionInfo>> result = ();
        foreach RestSchemaActionSet set in (AssemblyAiSchema::getActionSets().iterator()) {
            foreach hash<DataProviderActionInfo> action in (set.getActionInfoList(app)) {
                if (!action.output_type) {
                    action.output_type = EmptyResponseType;
                }
                result += action;
            }
        }
        result += <DataProviderActionInfo>{
            "app": app, "action": "transcribe", "path": "/transcribe",
            "display_name": "Transcribe Audio",
            "short_desc": "Submit an audio URL and wait for its completed transcript",
            "desc": "Transcribe recorded audio and return the completed result. Configure model routing, speakers, redaction, and analysis as needed. The polling interval and timeout control how long this action waits.",
            "action_code": DPAT_API, "mutates_state": True,
            "groups": ("Transcription",),
            "options": DataProviderActionCatalog::getActionOptionFromFields(
                AssemblyAiTranscribeRequestDataType::Fields),
            "output_type": AssemblyAiTranscribeDataProvider::ResponseType,
        };
        return result;
    }

    #! Registers the complete explicit action inventory
    /** @param app application identity */
    static registerActions(string app = AppName) {
        map DataProviderActionCatalog::registerAction($1), getActionInfoList(app);
    }
}
}
