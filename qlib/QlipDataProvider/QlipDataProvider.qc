# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QlipDataProvider.qc Qlip root data provider

/*  QlipDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QlipDataProvider module
public namespace QlipDataProvider {

#! The main Qlip data provider class (root provider)
public class QlipDataProvider inherits QlipDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "qlip",
            "desc": "Qlip data provider for visual-regression review: builds (more resources "
                "to follow: snapshots, diffs, comments, story history)",
            "type": "QlipDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options (include watch options, forwarded to the events child)
        const ConstructorOptions = BaseConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "Polling interval in seconds for event triggers",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds for event triggers "
                    "(minimum " + MinPollInterval + ")",
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Start date for event triggers",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only detect builds created after this date/time",
            },
        };

        #! Child provider map
        const ChildMap = {
            "builds": Class::forName("QlipBuildsDataProvider"),
            "events": Class::forName("QlipEventsDataProvider"),
        };
    }

    private {
        #! Watch options (poll_interval, start_date) forwarded to the events child
        *hash<auto> watch_options;
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : QlipDataProviderBase(options) {
        hash<auto> wopts = {};
        if (options.poll_interval) {
            wopts.poll_interval = options.poll_interval;
        }
        if (options.start_date) {
            wopts.start_date = options.start_date;
        }
        if (wopts) {
            watch_options = wopts;
        }
    }

    #! Creates the object with a REST client
    constructor(QlipRestClient::QlipRestClient rest) : QlipDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(QlipRestClient::QlipRestConnection conn) : QlipDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "qlip";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Qlip data provider for visual-regression review";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            # The events container needs the watch options forwarded to its
            # child watch providers (poll_interval, start_date).
            if (name == "events") {
                return cls.newObject(rest, watch_options);
            }
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace QlipDataProvider
