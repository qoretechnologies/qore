# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroEmployeeDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the XeroDataProvider module
public namespace XeroDataProvider {
#! The hand-written employee actions
/** @section xeroemployees_defect The Defect This Fixes

    The application this module replaces exported \c "/Employees" and read it through the **accounting**
    API — \c https://api.xero.com/api.xro/2.0/Employees.  That path no longer exists: Xero's published
    accounting description declares 138 paths and \c /Employees is not among them.  So its employee
    dropdown was always empty and its "new employee" trigger never fired, while the application requested
    the \c payroll.employees scope, which is where the intent shows: only the routing was wrong.

    Employees live in the **payroll** API, and there is no single payroll API to move them to.  Xero
    publishes one description per region, and they are not the same API:

    |!Region|!Base path|!Listing envelope|!Create takes
    |Australia|\c /payroll.xro/1.0|\c {Employees: [...]}|an **array** of employees
    |New Zealand|\c /payroll.xro/2.0|\c {employees: [...], pagination}|**one** employee
    |United Kingdom|\c /payroll.xro/2.0|\c {employees: [...], pagination}|**one** employee

    @section xeroemployees_handwritten Why These Are Not Generated

    An action registered in the application catalog has exactly one option shape, and these three
    descriptions do not agree on one.  Generating from any single region would publish that region's
    contract to everybody, so a New Zealand organisation would be shown Australia's envelope or the other
    way round — a request contract that looks generated but is wrong for two thirds of users.

    That is the case the schema-driven path documents as hand-written: when a schema cannot describe the
    operation the application means, the action is written by hand and keeps its action ID.  These two do
    exactly that.  They take the region from the connection, send what that region's API accepts, and
    return the employees as a plain list whichever envelope carried them.

    The action IDs \c "getEmployees" and \c "updateOrCreateEmployees" are the ones the superseded
    application registered, and existing workflows call them.
*/
public class XeroEmployeesDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The action ID that lists employees
        const ActionGetEmployees = "getEmployees";

        #! The action ID that creates employees
        const ActionCreateEmployees = "updateOrCreateEmployees";

        #! The action IDs this class implements
        const ActionNames = (ActionGetEmployees, ActionCreateEmployees);

        #! The display name of each action
        const DisplayNames = {
            ActionGetEmployees: "Find Employees",
            ActionCreateEmployees: "Create Employees",
        };

        #! The short description of each action
        const ShortDescs = {
            ActionGetEmployees: "List the employees in the payroll organisation",
            ActionCreateEmployees: "Create employees in the payroll organisation",
        };

        #! The description of each action
        const Descs = {
            ActionGetEmployees: "Lists the employees in the organisation's payroll.\n\n"
                "**Which payroll API is used comes from the connection's `payroll_region`**, because Xero "
                "has no single payroll API: Australia is served at `payroll.xro/1.0` and New Zealand and "
                "the United Kingdom at `payroll.xro/2.0`. A connection with no region set reports that "
                "rather than guessing one.\n\n"
                "The regions wrap the result differently — `{Employees: [...]}` in Australia and "
                "`{employees: [...], pagination}` in the other two — and this action returns the employees "
                "as a plain list either way.\n\n"
                "The application this module replaces read employees through the accounting API, where "
                "they do not exist, so this never returned anything at all.",
            ActionCreateEmployees: "Creates employees in the organisation's payroll.\n\n"
                "`Employees` is a list even for one employee. Australia's payroll API takes a list "
                "directly; New Zealand's and the United Kingdom's take one employee per call, so a list of "
                "several is sent one at a time and the results collected.\n\n"
                "**Which payroll API is used comes from the connection's `payroll_region`.** The fields an "
                "employee needs differ by region — a United Kingdom employee has a National Insurance "
                "number where a New Zealand one has an IRD number — so this passes the employee through "
                "as given rather than declaring one region's shape for all of them.",
        };

        #! The payroll path holding employees
        const EmployeesPath = "/Employees";

        #! Options for the listing action
        const GetOptions = {
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "A filter expression, for New Zealand and the United Kingdom",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "A filter expression. The 2.0 payroll APIs (New Zealand, United Kingdom) call this "
                    "`filter`; Australia's 1.0 API calls it `where`, and whichever this connection's "
                    "region uses is what it is sent as.",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Which page to return, counting from 1",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Which page to return, counting from 1",
            },
        };

        #! Options for the create action
        const CreateOptions = {
            "Employees": <DataProviderOptionInfo>{
                "display_name": "Employees",
                "short_desc": "The employees to create; a list even for one",
                "type": AbstractDataProviderTypeMap."list",
                "desc": "The employees to create, as a list even for one.\n\n"
                    "The properties an employee needs differ by region, so this is passed through as "
                    "given; see Xero's payroll documentation for the region in use.",
                "required": True,
                "preselected": True,
            },
        };
    }

    private {
        #! The REST client used for API calls
        *XeroRestClient::XeroRestClientIo rest;

        #! Which of the two actions this provider is
        string action;
    }

    #! Creates the object
    /** @param rest the Xero REST client to use for API calls
        @param action the action ID this provider implements
    */
    constructor(*XeroRestClient::XeroRestClientIo rest, string action) {
        if (!inlist(action, ActionNames)) {
            throw "XERO-EMPLOYEE-ERROR", sprintf("%y is not an employee action; known actions: %y", action,
                ActionNames);
        }
        self.rest = rest;
        self.action = action;
    }

    #! Returns the data provider name
    string getName() {
        return action;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Descs{action};
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        HashDataType rv(action + "-request");
        hash<string, hash<DataProviderOptionInfo>> opts = action == ActionGetEmployees
            ? GetOptions
            : CreateOptions;
        foreach hash<auto> i in (opts.pairIterator()) {
            # an option declares a list of accepted types; the first is the one presented
            softlist<AbstractDataProviderType> types = i.value."type";
            rv.addField(new QoreDataField(i.key, i.value.short_desc, types[0]));
        }
        return rv;
    }

    #! Returns the response type
    /** Every region returns a different employee shape, so the type says what is certain — a list of
        employees — rather than declaring one region's properties for all of them.
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."list";
    }

    #! Executes the action
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return action == ActionGetEmployees ? listEmployees(req) : createEmployees(req);
    }

    #! Lists the employees in the connection's payroll region
    private list<auto> listEmployees(*hash<auto> req) {
        string prefix = getPayrollPrefix();
        # the 1.0 API filters with "where" and the 2.0 ones with "filter"; the option is one either way
        string filter_arg = prefix =~ /1\.0$/ ? "where" : "filter";
        list<string> args = ();
        if (req.filter.val()) {
            args += sprintf("%s=%s", filter_arg, encode_url(req.filter.toString(), True));
        }
        if (exists req.page) {
            args += sprintf("page=%s", encode_url(req.page.toString(), True));
        }
        string path = prefix + EmployeesPath + (args ? "?" + join("&", args) : "");
        hash<RestClient::RestResponseInfo> resp = getRest().restGet(path);
        return XeroEmployeesDataProvider::getEmployeeList(resp.body);
    }

    #! Creates the employees given, one call per employee where the region takes one at a time
    private list<auto> createEmployees(*hash<auto> req) {
        softlist<auto> employees = req.Employees ?? ();
        if (!employees) {
            throw "XERO-EMPLOYEE-ERROR", "no employees were given to create";
        }
        string prefix = getPayrollPrefix();
        string path = prefix + EmployeesPath;
        # Australia's 1.0 API takes the whole list in one call; the 2.0 APIs take one employee per call
        if (prefix =~ /1\.0$/) {
            hash<RestClient::RestResponseInfo> resp = getRest().restPost(path, employees);
            return XeroEmployeesDataProvider::getEmployeeList(resp.body);
        }
        list<auto> rv = ();
        foreach auto employee in (employees) {
            hash<RestClient::RestResponseInfo> resp = getRest().restPost(path, employee);
            rv += XeroEmployeesDataProvider::getEmployeeList(resp.body);
        }
        return rv;
    }

    #! Returns the employees in a payroll response, whichever envelope carried them
    /** The 1.0 API answers \c {Employees: [...]}, the 2.0 APIs \c {employees: [...], pagination} for a
        listing and \c {employee: {...}} for a create.  A caller should not have to know which.

        @param body the response body

        @return the employees, as a list
    */
    static list<auto> getEmployeeList(auto body) {
        if (body.typeCode() == NT_LIST) {
            return body;
        }
        if (body.typeCode() != NT_HASH) {
            return ();
        }
        foreach string key in (("Employees", "employees")) {
            if (body{key}.typeCode() == NT_LIST) {
                return body{key};
            }
        }
        foreach string key in (("Employee", "employee")) {
            if (body{key}.typeCode() == NT_HASH) {
                return (body{key},);
            }
        }
        return ();
    }

    #! Returns the payroll path prefix for this connection's region
    private string getPayrollPrefix() {
        return getRest().getPayrollPrefix();
    }

    #! Returns the REST client, or reports that there is none
    private XeroRestClient::XeroRestClientIo getRest() {
        if (!rest) {
            throw "XERO-EMPLOYEE-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        return rest;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        # the options are the request itself, published through getRequestTypeImpl(); declaring them as
        # "request_options" as well would make the framework demand them a second time, separately
        return <DataProviderInfo>{
            "name": action,
            "type": "XeroEmployeesDataProvider",
            "supports_request": True,
        };
    }
}
}
