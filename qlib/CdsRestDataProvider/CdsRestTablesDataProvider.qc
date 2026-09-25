# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CdsRestDataProvider module definition

/** CdsRestDataProviderTables.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! The CdsRest data provider class
public class CdsRestTablesDataProvider inherits CdsRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tables",
            "type": "CdsRestDataProviderTables",
            "supports_children": True,
            "constructor_options": BaseConstructorOptions,
            "children_can_support_apis": False,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Metadata keyed by entity type
        *hash<auto> meta;

        #! Metadata mutex
        Qore::Thread::Mutex lck();
    }

    #! Creates the object from the arguments
    /** @param rest the REST client for API calls

        @since CdsRestDataProvider 2.2
    */
    constructor(CdsRestClient::CdsRestClientIo rest) : CdsRestDataProviderBase(rest) {
    }

    #! Creates the object from the arguments
    /** @param rest a synchronous REST client; API calls are made with a
        @ref CdsRestClient::CdsRestClientIo "CdsRestClientIo" created with its options and connection path by
        @ref CdsRestDataProviderBase::getAsyncClient()
    */
    constructor(CdsRestClient::CdsRestClient rest) : CdsRestDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        bool auto_config = DataProvider::getAutoConfig();
        if (auto_config) {
            map options{$1.key} = ENV{$1.value}, EnvOptions.pairIterator(),
                exists ENV{$1.value} && !exists options{$1.key};
        }
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.cdsrestclient) {
            rest = CdsRestDataProviderBase::getClientFromOption(copts.cdsrestclient);
        } else {
            rest = new CdsRestClientIo(getClientOptions(copts));
        }
    }

    #! Returns the data provider name
    string getName() {
        return "tables";
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getMetadata();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *hash<auto> obj_meta = getMetadata(){name};
        if (obj_meta) {
            return new ODataEntityDataProvider(rest, name, obj_meta);
        }
    }

    #! Returns metadata
    private *hash<auto> getMetadata() {
        if (meta) {
            return meta;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (meta) {
            return meta;
        }

        # the exception argument of an error response includes the response
        hash<auto> resp = rest.restGet("$metadata?annotations=true").body."edmx:Edmx"."edmx:DataServices".Schema;
        #printf("%Y\n", resp.EntityType);

        meta = map {$1."^attributes^".Name: $1}, resp.EntityType, ($1.Property || ($1.Abstract == "true"));
        return meta;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

}
}
