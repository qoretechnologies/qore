# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingMeasurementsCreateDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

%try-module uuid
%define NoUuid
%endtry

%ifdef NoUuid
# Stub class when uuid module is not available
class UUID {
    static string get() { throw "UUID-ERROR", "the uuid module is required for this operation"; }
}
%endif

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
#! The Empathic Building measurements create API data provider
/** This API allows the caller to create Empathic Building measurements
*/
public class EmpathicBuildingMeasurementsCreateDataProvider inherits EmpathicBuildingDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "EmpathicBuilding measurements create API data provider",
            "type": "EmpathicBuildingMeasurementsCreateDataProvider",
            "constructor_options": EmpathicBuildingDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new SoftListDataType("EmpathicBuildingMeasurementEventType",
            new EmpathicBuildingMeasurementsCreateRequestDataType());

        #! Response type
        const ResponseType = AbstractDataProviderTypeMap."hash";
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", EmpathicBuildingDataProvider::ConstructorOptions, options);
        rest = EmpathicBuildingDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest, *Logger::LoggerInterface logger)
            : EmpathicBuildingDataProviderBase(rest, logger) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Empathic Building measurements %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # convert date/time values to milliseconds after the epoch
        # strip types
        softlist req0 = req;
        hash<string, int> emap;
        int events;
        foreach hash event in (\req0) {
            if (!event.time) {
                event.time = clock_getmillis();
            } else {
                # convert to milliseconds since the epoch
                event.time = event.time.getEpochSeconds() * 1000 + event.time.milliseconds();
            }
            foreach hash<auto> e in (\event.events) {
                e.start = e.start.getEpochSeconds() * 1000 + e.start.milliseconds();
                e.end = e.end.getEpochSeconds() * 1000 + e.end.milliseconds();
                if (!e.booking_ref.val()) {
                    e.booking_ref = UUID::get();
                }
                ++events;
            }
            emap{event.name} = event.events.size();
        }
        hash<auto> info;
        try {
            LoggerWrapper::debug("posting measurements: %y", req0);
            # this API returns no data
            rest.post("v2/measurements", req0, \info);
            return emap;
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            rethrow ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Measurement event data type
public class EmpathicBuildingMeasurementEventType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # query parameters
            "start": {
                "type": DateType,
                "desc": "The start time of reservation (resolution of milliseconds)",
            },
            "end": {
                "type": DateType,
                "desc": "The end time of reservation (resolution of milliseconds)",
            },
            "subject": {
                "type": StringOrNothingType,
                "desc": "The optional subject of the reservation",
            },
            "booking_ref": {
                "type": StringOrNothingType,
                "desc": "The booking reference for the reservation",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Create measurements request data type
public class EmpathicBuildingMeasurementsCreateRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # query parameters
            "type": {
                "type": StringType,
                "desc": "The name of measurements to create",
            },
            "time": {
                "type": DateOrNothingType,
                "desc": "The current date/time of the request; if omitted, it will be added automatically to the "
                    "request (resultion of milliseconds)",
            },
            "vendor_id": {
                "type": StringType,
                "desc": "An ID value from the remote / vendor system",
            },
            /*
            "vendor": {
                "type": StringOrNothingType,
                "desc": "The source of the data (or the 'integrator')",
            },
            */
            "name": {
                "type": StringType,
                "desc": "Human readable name for this specific data provider",
            },
            "events": {
                "type": new SoftListDataType("EmpathicBuildingMeasurementEventType", new EmpathicBuildingMeasurementEventType()),
                "desc": "List of measurement events to create",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}
}
