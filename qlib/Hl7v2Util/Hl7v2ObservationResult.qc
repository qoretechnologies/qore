# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Hl7v2ObservationResult.qc HL7 v2 ORU^R01 observation result builders

/*  Hl7v2ObservationResult.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Hl7v2Util module namespace
public namespace Hl7v2Util {
    #! Default HL7 v2 version used by ORU^R01 builders
    public const Hl7v2DefaultVersionId = "2.5.1";

    #! Builds an HL7 v2 ORU^R01 observation result message
    /** @param opts message options:
        - \c message_control_id: required idempotency key for \c MSH-10
        - \c sending_application, \c sending_facility, \c receiving_application, \c receiving_facility:
          optional \c MSH routing fields
        - \c timestamp: optional \c MSH-7 timestamp as \c YYYYMMDDHHMMSS or a Qore date
        - \c patient: required patient hash with \c id and optional \c assigning_authority,
          \c identifier_type, \c family_name, \c given_name, \c middle_name, \c birth_date, and \c sex
        - \c order: optional order hash with \c placer_order_number, \c filler_order_number,
          \c universal_service_id, \c observation_time, \c ordering_provider, and \c result_status
        - \c observations: required non-empty list of observation hashes. Each observation requires
          \c value and either \c observation_id or \c code. Optional keys include \c value_type, \c text,
          \c coding_system, \c units, \c reference_range, \c abnormal_flags, \c result_status,
          \c observation_sub_id, and \c observation_time.

        @return serialized HL7 v2 ORU^R01 message with CR segment delimiters

        @throw HL7V2-ORU-ERROR if required inputs are missing or invalid

        @par Example
        Build a result message with one coded numeric observation.
        @code{.py}
string msg = Hl7v2Util::hl7v2_build_oru_r01({
    "message_control_id": "ORU-0001",
    "patient": {"id": "12345", "family_name": "Doe", "given_name": "Jane"},
    "observations": ({
        "code": "8867-4",
        "text": "Heart rate",
        "coding_system": "LN",
        "value": 72,
        "units": {"code": "/min", "text": "beats/minute", "coding_system": "UCUM"},
    },),
});
        @endcode
    */
    public string sub hl7v2_build_oru_r01(hash<auto> opts) {
        string message_control_id = hl7v2RequireString(opts, "message_control_id", "ORU^R01 option");
        hash<auto> patient = hl7v2RequireHash(opts, "patient", "ORU^R01 option");
        list<auto> observations = hl7v2RequireNonEmptyList(opts, "observations", "ORU^R01 option");
        hash<auto> order = exists opts.order ? hl7v2RequireHash(opts, "order", "ORU^R01 option") : {};

        string timestamp = exists opts.timestamp
            ? hl7v2FormatTimestamp(opts.timestamp, "ORU^R01 option \"timestamp\"")
            : format_date("YYYYMMDDHHmmSS", now());
        string version_id = exists opts.version_id
            ? hl7v2RequireString(opts, "version_id", "ORU^R01 option")
            : Hl7v2DefaultVersionId;

        Hl7v2Base base();
        list<string> segments = ();
        push segments, base.formatSegment({
            "tag": "MSH",
            "fields": (
                Hl7v2Delimiters::DefaultEncodingCharacters,
                hl7v2OptionalString(opts, "sending_application"),
                hl7v2OptionalString(opts, "sending_facility"),
                hl7v2OptionalString(opts, "receiving_application"),
                hl7v2OptionalString(opts, "receiving_facility"),
                timestamp,
                "",
                ("ORU", "R01", "ORU_R01"),
                message_control_id,
                hl7v2OptionalString(opts, "processing_id", "P"),
                version_id
            ),
        });
        push segments, base.formatSegment({
            "tag": "PID",
            "fields": (
                hl7v2OptionalString(patient, "set_id", "1"),
                "",
                hl7v2PatientIdentifier(patient),
                "",
                hl7v2PatientName(patient),
                "",
                hl7v2OptionalTimestamp(patient, "birth_date"),
                hl7v2OptionalString(patient, "sex")
            ),
        });
        push segments, base.formatSegment({
            "tag": "OBR",
            "fields": hl7v2ObrFields(order),
        });

        int set_id = 0;
        foreach auto observation_arg in (observations) {
            if (observation_arg.typeCode() != NT_HASH) {
                throw "HL7V2-ORU-ERROR", "ORU^R01 observations must be hashes";
            }
            hash<auto> observation = cast<hash<auto>>(observation_arg);
            push segments, base.formatSegment({
                "tag": "OBX",
                "fields": hl7v2ObxFields(++set_id, observation, order),
            });
        }

        return segments.join("\r") + "\r";
    }

    list<auto> sub hl7v2ObrFields(hash<auto> order) {
        return (
            hl7v2OptionalString(order, "set_id", "1"),
            hl7v2OptionalComposite(order, "placer_order_number"),
            hl7v2OptionalComposite(order, "filler_order_number"),
            exists order.universal_service_id
                ? hl7v2CodedElement(order.universal_service_id, "order.universal_service_id")
                : ("RESULTS", "Observation Results", "L"),
            "",
            "",
            hl7v2OptionalTimestamp(order, "observation_time"),
            "",
            "",
            "",
            "",
            "",
            hl7v2OptionalString(order, "relevant_clinical_info"),
            "",
            hl7v2OptionalComposite(order, "specimen_source"),
            hl7v2OptionalComposite(order, "ordering_provider"),
            "",
            hl7v2OptionalString(order, "placer_field_1"),
            hl7v2OptionalString(order, "placer_field_2"),
            hl7v2OptionalString(order, "filler_field_1"),
            hl7v2OptionalString(order, "filler_field_2"),
            hl7v2OptionalTimestamp(order, "result_time"),
            "",
            hl7v2OptionalString(order, "diagnostic_service_section_id"),
            hl7v2OptionalString(order, "result_status", "F")
        );
    }

    list<auto> sub hl7v2ObxFields(int set_id, hash<auto> observation, hash<auto> order) {
        if (!exists observation.value) {
            throw "HL7V2-ORU-ERROR", "ORU^R01 observation value is required";
        }
        return (
            string(set_id),
            exists observation.value_type
                ? hl7v2RequireString(observation, "value_type", "ORU^R01 observation")
                : hl7v2InferValueType(observation.value),
            exists observation.observation_id
                ? hl7v2CodedElement(observation.observation_id, "observation.observation_id")
                : hl7v2ObservationCode(observation),
            hl7v2OptionalString(observation, "observation_sub_id"),
            observation.value,
            exists observation.units ? hl7v2CodedElement(observation.units, "observation.units") : "",
            hl7v2OptionalString(observation, "reference_range"),
            hl7v2OptionalComposite(observation, "abnormal_flags"),
            "",
            "",
            hl7v2OptionalString(observation, "result_status", hl7v2OptionalString(order, "result_status", "F")),
            "",
            "",
            exists observation.observation_time
                ? hl7v2FormatTimestamp(observation.observation_time, "observation.observation_time")
                : hl7v2OptionalTimestamp(order, "observation_time")
        );
    }

    list<auto> sub hl7v2PatientIdentifier(hash<auto> patient) {
        if (exists patient.identifier) {
            return hl7v2Composite(patient.identifier, "patient.identifier");
        }
        return (
            hl7v2RequireString(patient, "id", "ORU^R01 patient"),
            "",
            "",
            hl7v2OptionalString(patient, "assigning_authority"),
            hl7v2OptionalString(patient, "identifier_type", "MR")
        );
    }

    list<auto> sub hl7v2PatientName(hash<auto> patient) {
        if (exists patient.name) {
            return hl7v2Composite(patient.name, "patient.name");
        }
        return (
            hl7v2OptionalString(patient, "family_name"),
            hl7v2OptionalString(patient, "given_name"),
            hl7v2OptionalString(patient, "middle_name"),
            hl7v2OptionalString(patient, "suffix"),
            hl7v2OptionalString(patient, "prefix")
        );
    }

    list<auto> sub hl7v2ObservationCode(hash<auto> observation) {
        return (
            hl7v2RequireString(observation, "code", "ORU^R01 observation"),
            hl7v2OptionalString(observation, "text"),
            hl7v2OptionalString(observation, "coding_system")
        );
    }

    string sub hl7v2InferValueType(auto value) {
        int tc = value.typeCode();
        return (tc == NT_INT || tc == NT_FLOAT || tc == NT_NUMBER) ? "NM" : "ST";
    }

    list<auto> sub hl7v2CodedElement(auto value, string ctx) {
        if (value.typeCode() == NT_HASH) {
            hash<auto> h = cast<hash<auto>>(value);
            return (
                hl7v2RequireString(h, "code", ctx),
                hl7v2OptionalString(h, "text", hl7v2OptionalString(h, "display")),
                hl7v2OptionalString(h, "coding_system", hl7v2OptionalString(h, "system"))
            );
        }
        return hl7v2Composite(value, ctx);
    }

    list<auto> sub hl7v2OptionalComposite(hash<auto> opts, string key) {
        return exists opts{key} ? hl7v2Composite(opts{key}, key) : ();
    }

    list<auto> sub hl7v2Composite(auto value, string ctx) {
        if (value.typeCode() == NT_LIST) {
            return value;
        }
        if (value.typeCode() == NT_STRING) {
            return (value,);
        }
        throw "HL7V2-ORU-ERROR", sprintf("%s must be a string, list, or coded hash", ctx);
    }

    hash<auto> sub hl7v2RequireHash(hash<auto> opts, string key, string ctx) {
        if (opts{key}.typeCode() != NT_HASH) {
            throw "HL7V2-ORU-ERROR", sprintf("%s %y must be a hash", ctx, key);
        }
        return cast<hash<auto>>(opts{key});
    }

    list<auto> sub hl7v2RequireNonEmptyList(hash<auto> opts, string key, string ctx) {
        if (opts{key}.typeCode() != NT_LIST || !opts{key}) {
            throw "HL7V2-ORU-ERROR", sprintf("%s %y must be a non-empty list", ctx, key);
        }
        return opts{key};
    }

    string sub hl7v2RequireString(hash<auto> opts, string key, string ctx) {
        if (opts{key}.typeCode() != NT_STRING || !opts{key}) {
            throw "HL7V2-ORU-ERROR", sprintf("%s %y must be a non-empty string", ctx, key);
        }
        return opts{key};
    }

    string sub hl7v2OptionalString(hash<auto> opts, string key, string default_value = "") {
        if (!exists opts{key}) {
            return default_value;
        }
        if (opts{key}.typeCode() != NT_STRING) {
            throw "HL7V2-ORU-ERROR", sprintf("optional ORU^R01 field %y must be a string", key);
        }
        return opts{key} ? opts{key} : default_value;
    }

    string sub hl7v2OptionalTimestamp(hash<auto> opts, string key) {
        return exists opts{key} ? hl7v2FormatTimestamp(opts{key}, key) : "";
    }

    string sub hl7v2FormatTimestamp(auto value, string ctx) {
        if (value.typeCode() == NT_DATE) {
            return format_date("YYYYMMDDHHmmSS", value);
        }
        if (value.typeCode() == NT_STRING && value) {
            return value;
        }
        throw "HL7V2-ORU-ERROR", sprintf("%s must be a non-empty timestamp string or date", ctx);
    }
}
