# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingSensorDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
#! The EmpathicBuilding sensor data provider class
public class EmpathicBuildingSensorDataProvider inherits EmpathicBuildingDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sensors",
            "type": "EmpathicBuildingSensorsDataProvider",
            "supports_request": True,
            "supports_children": True,
            #"children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

       #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = EmpathicBuildingSensorType;
    }

    private {
        #! Org data
        hash<auto> org;

        #! Loc data
        hash<auto> loc;

        #! Sensor data
        hash<auto> sensor;

        const ChildMap = {
            "watch": Class::forName("EmpathicBuildingDataProvider::EmpathicBuildingWatchSensorDataProvider"),
        };
    }

    #! Create the object from the REST connection
    constructor(RestClient::AbstractRestClient rest, *Logger::LoggerInterface logger, hash<auto> org,
            hash<auto> loc, hash<auto> sensor) : EmpathicBuildingDataProviderBase(rest, logger) {
        self.org = org;
        self.loc = loc;
        self.sensor = sensor;
    }

    #! Returns the data provider name
    string getName() {
        return sensor.id;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("EmpathicBuilding data provider for org: `%s` (%s), location `%s` (%s), sensor  `%s` (%s)",
            org.id, org.name, loc.id, loc.name, sensor.id, getSensorDesc(sensor));
    }

    #! Returns a description of the sensor
    static string getSensorDesc(hash<auto> sensor) {
        string desc = sensor.name ?? sensor.uuid;
        if (sensor.type.val()) {
            desc += sprintf(", type: %y", sensor.type);
            if (sensor.subtype.val()) {
                desc += sprintf(" subtype: %y", sensor.subtype);
            }
        }
        if (sensor.vendor.val()) {
            desc += sprintf(", vendor: %y", sensor.vendor);
        }
        if (sensor.vendor_id.val()) {
            desc += sprintf(", vendor_id: %y", sensor.vendor_id);
        }
        return desc;
    }

    #! Converts ms epoch offsets to date/time values
    static hash<auto> getSensorData(hash<auto> sensor) {
        if (sensor.previous_measurement.time) {
            sensor.previous_measurement.time =
                EmpathicBuildingDataProviderBase::getTimestamp(sensor.previous_measurement.time);
        }
        if (sensor.previous_measurement.valid_to) {
            sensor.previous_measurement.valid_to =
                EmpathicBuildingDataProviderBase::getTimestamp(sensor.previous_measurement.valid_to);
        }
        if (sensor.last_measurement.time) {
            sensor.last_measurement.time =
                EmpathicBuildingDataProviderBase::getTimestamp(sensor.last_measurement.time);
        }
        if (sensor.last_measurement.valid_to) {
            sensor.last_measurement.valid_to =
                EmpathicBuildingDataProviderBase::getTimestamp(sensor.last_measurement.valid_to);
        }
        if (sensor.last_communication_time) {
            sensor.last_communication_time =
                EmpathicBuildingDataProviderBase::getTimestamp(sensor.last_communication_time);
        }
        if (sensor.last_confirmed_occupation) {
            sensor.last_confirmed_occupation =
                EmpathicBuildingDataProviderBase::getTimestamp(sensor.last_confirmed_occupation);
        }
        return sensor;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getSensorData(sensor);
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

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest, logger, org, loc, sensor);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Measurement type
public class EmpathicBuildingMeasurementType inherits HashDataType {
    public {
        const Fields = {
            "time": {
                "display_name": "Time",
                "short_desc": "The timestamp of the measurement",
                "desc": "The timestamp of the measurement",
                "type": AbstractDataProviderTypeMap."date",
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Sensor value",
                "desc": "Sensor value",
                "type": AbstractDataProviderTypeMap."float",
            },
            "used": {
                "display_name": "Used",
                "short_desc": "Used value",
                "desc": "Used value",
                "type": AbstractDataProviderTypeMap."int",
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Total value",
                "desc": "Total value",
                "type": AbstractDataProviderTypeMap."int",
            },
            "valid_to": {
                "display_name": "Valid Timestamp",
                "short_desc": "Valid timestamp",
                "desc": "Valid timestamp",
                "type": AbstractDataProviderTypeMap."date",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Measurement type constant
public const EmpathicBuildingMeasurementType = new EmpathicBuildingMeasurementType();

#! Sensor type
public class EmpathicBuildingSensorType inherits HashDataType {
    public {
        const Fields = {
            "legacy_id": {
                "display_name": "Legacy ID",
                "short_desc": "Legacy ID",
                "desc": "Legacy ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "ID",
                "short_desc": "ID",
                "desc": "ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "uuid": {
                "display_name": "UUID",
                "short_desc": "UUID",
                "desc": "UUID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "location_id": {
                "display_name": "Location ID",
                "short_desc": "Location ID",
                "desc": "Location ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "gateway_id": {
                "display_name": "Gateway ID",
                "short_desc": "Gateway ID",
                "desc": "Gateway ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "virtual_sensor_id": {
                "display_name": "Virtual Sensor ID",
                "short_desc": "Virtual sensor ID",
                "desc": "Virtual sensor ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Sensor type",
                "desc": "Sensor type",
                "type": AbstractDataProviderTypeMap."string",
            },
            "subtype": {
                "display_name": "Subtype",
                "short_desc": "Sensor subtype",
                "desc": "Sensor subtype",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Sensor name",
                "desc": "Sensor name",
                "type": AbstractDataProviderTypeMap."string",
            },
            "vendor": {
                "display_name": "Sensor Vendor",
                "short_desc": "Sensor vendor",
                "desc": "Sensor vendor",
                "type": AbstractDataProviderTypeMap."string",
            },
            "vendor_id": {
                "display_name": "Vendor ID",
                "short_desc": "Vendor ID",
                "desc": "Vendor ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "previous_measurement": {
                "display_name": "Previous Measurement",
                "short_desc": "Previous measurement",
                "desc": "Previous measurement",
                "type": EmpathicBuildingMeasurementType,
            },
            "last_measurement": {
                "display_name": "Last Measurement",
                "short_desc": "Last measurement",
                "desc": "Last measurement",
                "type": EmpathicBuildingMeasurementType,
            },
            "last_communication_time": {
                "display_name": "Last Communication Time",
                "short_desc": "Last communication time",
                "desc": "Last communication time",
                "type": AbstractDataProviderTypeMap."date",
            },
            "last_status": {
                "display_name": "Last Status",
                "short_desc": "Last status",
                "desc": "Last status",
                "type": AbstractDataProviderTypeMap."hash",
            },
            "active": {
                "display_name": "Active?",
                "short_desc": "Is the sensor currently active?",
                "desc": "Is the sensor currently active?",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "last_confirmed_occupation": {
                "display_name": "Last Confirmed Occupancy",
                "short_desc": "Last confirmed occupancy timestamp",
                "desc": "Last confirmed occupancy timestamp",
                "type": AbstractDataProviderTypeMap."date",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Sensor data type constant
public const EmpathicBuildingSensorType = new EmpathicBuildingSensorType();
}