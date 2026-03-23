# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditAccountDataProvider.qc Reddit account data provider

/*  RedditAccountDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditDataProvider module
public namespace RedditDataProvider {

#! Reddit account data provider
public class RedditAccountDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "account",
            "desc": "Reddit account data provider for the authenticated user",
            "type": "RedditAccountDataProvider",
            "supports_read": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "update-prefs": Class::forName("RedditUpdatePrefsDataProvider"),
            "get-karma": Class::forName("RedditGetKarmaDataProvider"),
            "get-trophies": Class::forName("RedditGetTrophiesDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClientIo rest) : RedditDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "account";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reddit account data provider for the authenticated user";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new RedditAccountDataType();
    }

    #! Returns an iterator for the current user's account
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> result = doGet("api/v1/me");
        return new DefaultRecordIterator(new ListHashIterator((result,)),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
