# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditModerationDataProvider.qc Reddit moderation container and action providers

/*  RedditModerationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace RedditDataProvider {

#! Reddit moderation container data provider
public class RedditModerationDataProvider inherits RedditDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "moderation",
            "desc": "Reddit moderation actions",
            "type": "RedditModerationDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "approve": Class::forName("RedditApproveDataProvider"),
            "remove": Class::forName("RedditRemoveDataProvider"),
            "distinguish": Class::forName("RedditDistinguishDataProvider"),
            "lock": Class::forName("RedditLockDataProvider"),
            "unlock": Class::forName("RedditUnlockDataProvider"),
            "sticky": Class::forName("RedditStickyDataProvider"),
            "mark-nsfw": Class::forName("RedditMarkNsfwDataProvider"),
            "unmark-nsfw": Class::forName("RedditUnmarkNsfwDataProvider"),
            "mark-spoiler": Class::forName("RedditSpoilerDataProvider"),
            "unmark-spoiler": Class::forName("RedditUnspoilerDataProvider"),
        };
    }

    constructor(RedditRestClient::RedditRestClient rest) : RedditDataProviderBase(rest) {
    }

    string getName() {
        return "moderation";
    }

    *string getDesc() {
        return "Reddit moderation actions";
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditDataProvider
