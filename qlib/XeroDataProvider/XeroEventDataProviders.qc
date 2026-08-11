# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The default polling interval for a Xero event, in seconds
public const XeroDefaultPollInterval = 300;

#! The shortest polling interval a Xero event accepts, in seconds
/** Xero allows 60 calls a minute and 5000 a day per tenant, and a subscription that polled every few
    seconds would spend the day's budget before lunch.
*/
public const XeroMinPollInterval = 60;

#! How many pages one polling cycle reads before stopping
public const XeroMaxPollPages = 10;

#! Base class for Xero polling watch data providers
/** Xero's published webhook description declares only two payload types and no operations at all, and
    registering a webhook needs a subscription configured in the Xero developer portal rather than anything
    a connection can do.  So these poll, exactly as the application this module replaces did.

    They poll better, though, in two ways.  That application kept the identities it had already delivered in
    a bounded in-memory set, so a restart re-delivered whatever was still in the first page; and it read the
    whole collection every cycle.  These position the feed by \c UpdatedDateUTC through
    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" and send that
    cursor upstream as \c If-Modified-Since, so a cycle reads only what has changed since the last one.

    @note The watch base is inherited **first**, deliberately: both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method to
    the first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the operation as
    unsupported.

    @section xeroevent_dates Xero Writes Two Kinds of Timestamp

    Xero's accounting API answers JSON with .NET timestamps - \c "/Date(1573755038314+0000)/" - while its
    description declares them as RFC 3339 strings and its projects and payroll APIs actually send those.
    A cursor that could read only one of the two forms would either never advance or advance wrongly, so
    both are parsed here.
*/
public class XeroWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every Xero event provider
        const WatchConstructorOptions = {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll Xero, in seconds (minimum " + XeroMinPollInterval + ").\n\n"
                    "Xero allows 60 calls a minute and 5000 a day per organisation, which is what the "
                    "minimum protects.",
                "default_value": XeroDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records changed at or after this time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records changed at or after this time.\n\n"
                    "Without it the cursor starts at the moment the subscription starts, so nothing "
                    "already in the organisation is delivered",
            },
        };

        #! The record field Xero positions a change by
        const TimestampField = "UpdatedDateUTC";
    }

    private {
        #! The REST client used for API calls
        *XeroRestClient::XeroRestClientIo rest;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval ?? XeroDefaultPollInterval,
                XeroMinPollInterval), options.since) {
        self.rest = rest;
    }

    #! Stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels an in-flight REST request during shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.close();
        }
    }

    #! A Xero record is positioned by when it was last changed
    private string getTimestampField() {
        return TimestampField;
    }

    #! Returns the record's position in the feed, reading either form of Xero timestamp
    private *date getWatchRecordTimestamp(hash<auto> record) {
        auto val = record{TimestampField} ?? record{"updatedDateUTC"} ?? record{"UpdatedDateUTCString"};
        return XeroWatchDataProviderBase::parseXeroDate(val);
    }

    #! Parses a Xero timestamp in either the .NET or the RFC 3339 form
    /** @param val the value as it arrived

        @return the timestamp, or @ref nothing when there is no usable one

        @note This must not throw: an unusable timestamp skips one record, where an exception would abort
        the whole cycle and stall every other record with it.
    */
    static *date parseXeroDate(auto val) {
        switch (val.typeCode()) {
            case NT_DATE:
                return val.absolute() ? val : NOTHING;
            case NT_STRING: {
                if (!val.val()) {
                    return;
                }
                # "/Date(1573755038314)/" and "/Date(1573755038314+0000)/" are milliseconds since the epoch;
                # the offset, when present, describes the same instant and does not move it
                *string ms = (val =~ x/^\/Date\((-?[0-9]+)/)[0];
                if (ms.val()) {
                    int msi = ms.toInt();
                    int secs = msi / 1000;
                    int rem = msi % 1000;
                    if (rem < 0) {
                        --secs;
                        rem += 1000;
                    }
                    return gmtime(secs, rem * 1000);
                }
                date parsed;
                try {
                    parsed = date(val);
                } catch (hash<ExceptionInfo> ex) {
                    # ex: INVALID-DATE; the record simply has no position in the feed
                    return;
                }
                return parsed.absolute() ? parsed : NOTHING;
            }
        }
    }

    #! Reads a Xero accounting collection, newest first, from the given cursor
    /** @param path the path within the accounting API, ex: \c "/Contacts"
        @param envelope the name of the array in the response envelope, ex: \c "Contacts"
        @param since the cursor timestamp
        @param where_cond a filter expression, if any

        @return the records read
    */
    private list<hash<auto>> getAccountingRecords(string path, string envelope, date since, *string where_cond) {
        if (!rest) {
            throw "XERO-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        string prefix = XeroRestClient::XeroRestClientBase::AccountingPrefix;
        # the cursor is sent upstream, so a cycle reads what has changed rather than the whole collection
        TimeZone utc(0);
        hash<auto> hdr = {"If-Modified-Since": utc.date(since).format("YYYY-MM-DDTHH:mm:SS")};
        list<hash<auto>> rv = ();
        int page = 0;
        while (++page <= XeroMaxPollPages) {
            if (isStopping()) {
                break;
            }
            list<string> args = ("order=" + encode_url("UpdatedDateUTC DESC", True),
                "page=" + page);
            if (where_cond.val()) {
                args += "where=" + encode_url(where_cond, True);
            }
            hash<RestClient::RestResponseInfo> resp = rest.restGet(prefix + path + "?" + join("&", args),
                hdr);
            auto records = resp.body{envelope};
            if (records.typeCode() != NT_LIST || !records) {
                break;
            }
            foreach auto rec in (records) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }
        }
        return rv;
    }

    #! Returns the event this provider raises, and the shape of what it carries
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getDesc() ?? getEventName(),
                "type": getEventRecordType(),
            },
        };
    }

    #! Returns the record type an action's listing returns
    /** Taken from the action set the application ships, so that the event and the action a workflow reads
        it with cannot describe the same record differently.
    */
    private AbstractDataProviderType getEventRecordType() {
        *string action = getEventSourceAction();
        if (action.val()) {
            RestSchemaActionSet aset = XeroSchema::getAccountingActionSet();
            *RestSchemaActionOperation op = aset.getOperation(action);
            if (op) {
                *AbstractDataProviderType t = op.getResponseType();
                if (t) {
                    # a Xero accounting collection is an envelope with one array in it, named after the type
                    *hash<string, AbstractDataField> fields = t.getFields();
                    *AbstractDataField f = fields{getEventEnvelope()};
                    if (f) {
                        *AbstractDataProviderType vt = f.getType();
                        if (vt && vt.getBaseTypeCode() == NT_LIST) {
                            *AbstractDataProviderType et = vt.getElementType();
                            if (et) {
                                return et;
                            }
                        }
                    }
                    return t;
                }
            }
        }
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns the action whose records this provider reports, if it is a generated one
    private *string getEventSourceAction() {
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "";
    }
}

#! Base class for the Xero events that poll one accounting collection
public class XeroAccountingWatchDataProviderBase inherits XeroWatchDataProviderBase {
    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroWatchDataProviderBase(rest, options) {
    }

    #! Returns the path within the accounting API this event polls
    abstract private string getEventPath();

    #! Returns the filter this event applies, if any
    private *string getEventWhere() {
    }

    #! Fetches the records changed since the cursor
    private list<hash<auto>> fetchRecords(date since) {
        return getAccountingRecords(getEventPath(), getEventEnvelope(), since, getEventWhere());
    }
}

#! Raised for each new contact
public class XeroNewContactDataProvider inherits XeroAccountingWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_contact";

        #! The event display name
        const DisplayName = "New Contact";

        #! The event short description
        const ShortDesc = "Fires when a contact is created or changed";

        #! The event description
        const Desc = "Fires for each contact created or changed in the organisation.\n\n"
            "Xero positions a contact by `UpdatedDateUTC` and offers no \"created since\" filter, so a "
            "contact edited later fires this again. The event carries the contact record Xero returns.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewContactDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroAccountingWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the path within the accounting API this event polls
    private string getEventPath() {
        return "/Contacts";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "Contacts";
    }

    #! Returns the action whose records this provider reports
    private *string getEventSourceAction() {
        return "getContacts";
    }

    #! Returns the record field holding the record's stable identity
    /** Xero names it after the type rather than calling it "id", and the cursor needs it to break a tie
        between records sharing a timestamp.
    */
    private string getIdField() {
        return "ContactID";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new bill
public class XeroNewBillDataProvider inherits XeroAccountingWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_bill";

        #! The event display name
        const DisplayName = "New Bill";

        #! The event short description
        const ShortDesc = "Fires when a bill is created or changed";

        #! The event description
        const Desc = "Fires for each bill created or changed.\n\n"
            "A bill is an invoice of type `ACCPAY`, which is the filter this event applies; a sales "
            "invoice (`ACCREC`) does not fire it. The event carries the invoice record Xero returns.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewBillDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroAccountingWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the path within the accounting API this event polls
    private string getEventPath() {
        return "/Invoices";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "Invoices";
    }

    #! Returns the filter this event applies
    private *string getEventWhere() {
        return "Type==\"ACCPAY\"";
    }

    #! Returns the action whose records this provider reports
    private *string getEventSourceAction() {
        return "getInvoices";
    }

    #! Returns the record field holding the record's stable identity
    /** Xero names it after the type rather than calling it "id", and the cursor needs it to break a tie
        between records sharing a timestamp.
    */
    private string getIdField() {
        return "InvoiceID";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new credit note
public class XeroNewCreditNoteDataProvider inherits XeroAccountingWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_credit_note";

        #! The event display name
        const DisplayName = "New Credit Note";

        #! The event short description
        const ShortDesc = "Fires when a credit note is created or changed";

        #! The event description
        const Desc = "Fires for each credit note created or changed.\n\n"
            "The event carries the credit note record Xero returns. The application this module replaces "
            "polled credit notes without exporting any way to list or read one; **Find Credit Notes** "
            "exists so that this event has a counterpart.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewCreditNoteDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroAccountingWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the path within the accounting API this event polls
    private string getEventPath() {
        return "/CreditNotes";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "CreditNotes";
    }

    #! Returns the action whose records this provider reports
    private *string getEventSourceAction() {
        return "getCreditNotes";
    }

    #! Returns the record field holding the record's stable identity
    /** Xero names it after the type rather than calling it "id", and the cursor needs it to break a tie
        between records sharing a timestamp.
    */
    private string getIdField() {
        return "CreditNoteID";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new bank transaction
public class XeroNewBankTransactionDataProvider inherits XeroAccountingWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_bank_transaction";

        #! The event display name
        const DisplayName = "New Bank Transaction";

        #! The event short description
        const ShortDesc = "Fires when a bank transaction is created or changed";

        #! The event description
        const Desc = "Fires for each bank transaction created or changed - money spent or received "
            "against a bank account.\n\n"
            "The event carries the bank transaction record Xero returns.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewBankTransactionDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroAccountingWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the path within the accounting API this event polls
    private string getEventPath() {
        return "/BankTransactions";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "BankTransactions";
    }

    #! Returns the action whose records this provider reports
    private *string getEventSourceAction() {
        return "getBankTransactions";
    }

    #! Returns the record field holding the record's stable identity
    /** Xero names it after the type rather than calling it "id", and the cursor needs it to break a tie
        between records sharing a timestamp.
    */
    private string getIdField() {
        return "BankTransactionID";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new payment
public class XeroNewPaymentDataProvider inherits XeroAccountingWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_payment";

        #! The event display name
        const DisplayName = "New Payment";

        #! The event short description
        const ShortDesc = "Fires when a payment is created or changed";

        #! The event description
        const Desc = "Fires for each payment created or changed against an invoice or a credit note.\n\n"
            "The event carries the payment record Xero returns.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewPaymentDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroAccountingWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the path within the accounting API this event polls
    private string getEventPath() {
        return "/Payments";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "Payments";
    }

    #! Returns the action whose records this provider reports
    private *string getEventSourceAction() {
        return "getPayments";
    }

    #! Returns the record field holding the record's stable identity
    /** Xero names it after the type rather than calling it "id", and the cursor needs it to break a tie
        between records sharing a timestamp.
    */
    private string getIdField() {
        return "PaymentID";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new purchase order
public class XeroNewPurchaseOrderDataProvider inherits XeroAccountingWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_purchase_order";

        #! The event display name
        const DisplayName = "New Purchase Order";

        #! The event short description
        const ShortDesc = "Fires when a purchase order is created or changed";

        #! The event description
        const Desc = "Fires for each purchase order created or changed.\n\n"
            "The event carries the purchase order record Xero returns.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewPurchaseOrderDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroAccountingWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the path within the accounting API this event polls
    private string getEventPath() {
        return "/PurchaseOrders";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "PurchaseOrders";
    }

    #! Returns the action whose records this provider reports
    private *string getEventSourceAction() {
        return "getPurchaseOrders";
    }

    #! Returns the record field holding the record's stable identity
    /** Xero names it after the type rather than calling it "id", and the cursor needs it to break a tie
        between records sharing a timestamp.
    */
    private string getIdField() {
        return "PurchaseOrderID";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new employee
/** This is the event the \c /Employees defect broke: it polled the accounting API, where employees do not
    exist, so it never fired.  It reads the payroll API of the connection's region instead; see
    @ref XeroDataProvider::XeroEmployeesDataProvider "XeroEmployeesDataProvider".
*/
public class XeroNewEmployeeDataProvider inherits XeroWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_employee";

        #! The event display name
        const DisplayName = "New Employee";

        #! The event short description
        const ShortDesc = "Fires when an employee is added to payroll or changed";

        #! The event description
        const Desc = "Fires for each employee added to the organisation's payroll or changed.\n\n"
            "**Which payroll API is polled comes from the connection's `payroll_region`**: Australia is "
            "served at `payroll.xro/1.0` and New Zealand and the United Kingdom at `payroll.xro/2.0`. A "
            "connection with no region set reports that rather than guessing one.\n\n"
            "The application this module replaces polled employees through the **accounting** API, where "
            "they do not exist, so this event never fired at all.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "XeroNewEmployeeDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest, *hash<auto> options)
            : XeroWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Fetches the employees changed since the cursor
    private list<hash<auto>> fetchRecords(date since) {
        if (!rest) {
            throw "XERO-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        string prefix = rest.getPayrollPrefix();
        TimeZone utc(0);
        hash<auto> hdr = {"If-Modified-Since": utc.date(since).format("YYYY-MM-DDTHH:mm:SS")};
        list<hash<auto>> rv = ();
        int page = 0;
        while (++page <= XeroMaxPollPages) {
            if (isStopping()) {
                break;
            }
            hash<RestClient::RestResponseInfo> resp = rest.restGet(sprintf("%s/Employees?page=%d", prefix,
                page), hdr);
            list<auto> records = XeroEmployeesDataProvider::getEmployeeList(resp.body);
            if (!records) {
                break;
            }
            foreach auto rec in (records) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }
        }
        return rv;
    }

    #! Returns the record field holding the record's stable identity
    private string getIdField() {
        return "EmployeeID";
    }

    #! Returns the record's position in the feed
    /** The 1.0 API writes \c UpdatedDateUTC and the 2.0 APIs \c updatedDateUTC, so both are read.
    */
    private *date getWatchRecordTimestamp(hash<auto> record) {
        return XeroWatchDataProviderBase::parseXeroDate(record.UpdatedDateUTC ?? record.updatedDateUTC);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the Xero event providers
public class XeroEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": XeroDataProvider::EventsChildName,
            "type": "XeroEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            XeroNewContactDataProvider::EventName: Class::forName("XeroNewContactDataProvider"),
            XeroNewBillDataProvider::EventName: Class::forName("XeroNewBillDataProvider"),
            XeroNewCreditNoteDataProvider::EventName: Class::forName("XeroNewCreditNoteDataProvider"),
            XeroNewBankTransactionDataProvider::EventName:
                Class::forName("XeroNewBankTransactionDataProvider"),
            XeroNewPaymentDataProvider::EventName: Class::forName("XeroNewPaymentDataProvider"),
            XeroNewPurchaseOrderDataProvider::EventName:
                Class::forName("XeroNewPurchaseOrderDataProvider"),
            XeroNewEmployeeDataProvider::EventName: Class::forName("XeroNewEmployeeDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *XeroRestClient::XeroRestClientIo rest;
    }

    #! Creates the object
    constructor(*XeroRestClient::XeroRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return XeroDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Xero polling event providers";
    }

    #! Returns the event names
    private *list<string> getChildProviderNamesImpl() {
        return keys EventMap;
    }

    #! Returns the given event provider or @ref nothing if the event is unknown
    /** The child returned here is a **template**: it carries the event's option declarations, and the
        values are supplied when a subscription is created.
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = EventMap{name};
        if (!cls) {
            return;
        }
        return cast<AbstractDataProvider>(cls.newObject(rest));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
