# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaDataProvider root class definition

/** JinaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
public class JinaDataProvider inherits JinaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "jina",
            "type": "JinaDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private:internal {
        #! the process-wide registry of child data providers; private:internal, so that only this class
        #! changes it (qore#6744)
        static hash<string, Reflection::Class> childMap = DefaultChildMap;
    }

    private {
        const DefaultChildMap = {
            "read-url": Class::forName("JinaDataProvider::JinaReaderDataProvider"),
            "search-web": Class::forName("JinaDataProvider::JinaSearchDataProvider"),
            "segment-text": Class::forName("JinaDataProvider::JinaSegmentDataProvider"),
            "chat": Class::forName("JinaDataProvider::JinaChatDataProvider"),
            "deep-search": Class::forName("JinaDataProvider::JinaDeepSearchDataProvider"),
            "download-batch-output": Class::forName("JinaDataProvider::JinaBatchOutputDataProvider"),
            "download-batch-errors": Class::forName("JinaDataProvider::JinaBatchErrorsDataProvider"),
            "create-embeddings":
                Class::forName("JinaDataProvider::JinaEmbeddingDataProvider"),
            "rerank":
                Class::forName("JinaDataProvider::JinaRerankDataProvider"),
        };
    }

    private:internal {
        static Mutex child_lock();
    }

    #! Creates the root provider from an authenticated transport
    /** @param rest the transport to copy */
    constructor(RestClientIo::RestClientIo rest) : JinaDataProviderCommon(rest) {
    }

    #! Creates the root provider from connection options
    /** @param options the ConstructorOptions values
        @throw CONSTRUCTOR-ERROR an option is invalid or authentication is missing
    */
    constructor(*hash<auto> options) : JinaDataProviderCommon(options) {
    }

    #! Returns the root provider name
    /** @return the stable factory name */
    string getName() {
        return ProviderInfo.name;
    }

    #! Registers an additional provider alongside the built-in actions
    /** @param pathname the new child name
        @param cls the provider class
        @throw JINA-CHILD-ERROR the name is already registered
        @throw SANDBOX-REGISTRATION-ERROR code that is sandboxed or acts for sandboxed code cannot change the
            registry, which is process-wide (see @ref sandbox_process_wide_registries)
    */
    static registerChild(string pathname, Reflection::Class cls) [dom=PROCESS_CONTROL] {
        # the registry is process-wide: sandboxed code cannot change it (qore#6744)
        SandboxManager::checkProcessWideRegistration("register a child data provider");
        AutoLock al(child_lock);
        if (childMap{pathname} || JinaSchema::getActionSet().getOperation(pathname)) {
            throw "JINA-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Describes all built-in and registered child providers
    /** @return the child summaries */
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        list<hash<DataProviderSummaryInfo>> rv = ();
        foreach string name in (getChildProviderNamesImpl()) {
            rv += cast<hash<DataProviderSummaryInfo>>(getChildProviderImpl(name).getInfo(){
                AbstractDataProvider::DataProviderSummaryInfoKeys});
        }
        return rv;
    }

    private *list<string> getChildProviderNamesImpl() {
        AutoLock al(child_lock);
        hash<string, bool> names = map {$1: True}, JinaSchema::getActionSet().getActionNames();
        map names{$1} = True, keys childMap;
        return keys names;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls;
        {
            AutoLock al(child_lock);
            cls = childMap{name};
        }
        if (!cls) {
            if (JinaSchema::getActionSet().getOperation(name)) {
                return getSchemaProvider(name);
            }
            return;
        }
        return cls.newObject(rest);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
