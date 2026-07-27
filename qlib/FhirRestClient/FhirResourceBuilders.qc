# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FhirResourceBuilders.qc FHIR JSON resource builder helpers

/*  FhirResourceBuilders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %FhirRestClient namespace holds all public definitions in the %FhirRestClient module
public namespace FhirRestClient {
    #! UCUM code system URI
    public const FhirSystemUcum = "http://unitsofmeasure.org";

    #! LOINC code system URI
    public const FhirSystemLoinc = "http://loinc.org";

    #! SNOMED CT code system URI
    public const FhirSystemSnomedCt = "http://snomed.info/sct";

    #! Common UCUM aliases accepted by the helper functions
    const FhirUcumUnitAliases = {
        "%": {"code": "%", "unit": "%"},
        "/min": {"code": "/min", "unit": "/min"},
        "1/min": {"code": "/min", "unit": "/min"},
        "bpm": {"code": "/min", "unit": "/min"},
        "cel": {"code": "Cel", "unit": "Cel"},
        "c": {"code": "Cel", "unit": "Cel"},
        "celsius": {"code": "Cel", "unit": "Cel"},
        "cm": {"code": "cm", "unit": "cm"},
        "d": {"code": "d", "unit": "d"},
        "g": {"code": "g", "unit": "g"},
        "h": {"code": "h", "unit": "h"},
        "kg": {"code": "kg", "unit": "kg"},
        "kpa": {"code": "kPa", "unit": "kPa"},
        "l": {"code": "L", "unit": "L"},
        "m": {"code": "m", "unit": "m"},
        "mg": {"code": "mg", "unit": "mg"},
        "min": {"code": "min", "unit": "min"},
        "ml": {"code": "mL", "unit": "mL"},
        "mm": {"code": "mm", "unit": "mm"},
        "mm[Hg]": {"code": "mm[Hg]", "unit": "mmHg"},
        "mmhg": {"code": "mm[Hg]", "unit": "mmHg"},
        "pa": {"code": "Pa", "unit": "Pa"},
        "s": {"code": "s", "unit": "s"},
    };

    #! Builds a FHIR Coding hash
    /** @param system code system URI
        @param code code value
        @param display optional display text
        @param version optional code system version
        @return FHIR Coding element
        @throw FHIR-RESOURCE-BUILDER-ERROR if required fields are empty
    */
    public hash<auto> sub fhir_make_coding(string system, string code, *string display = NOTHING,
            *string version = NOTHING) {
        fhir_assert_nonempty("Coding.system", system);
        fhir_assert_nonempty("Coding.code", code);

        hash<auto> rv = ({
            "system": system,
            "code": code,
        });
        if (display.val()) {
            rv.display = display;
        }
        if (version.val()) {
            rv.version = version;
        }
        return rv;
    }

    #! Builds a FHIR CodeableConcept hash
    /** @param coding list of FHIR Coding elements
        @param text optional text
        @return FHIR CodeableConcept element
        @throw FHIR-RESOURCE-BUILDER-ERROR if no coding or text is supplied
    */
    public hash<auto> sub fhir_make_codeable_concept(list<hash<auto>> coding,
            *string text = NOTHING) {
        if (!coding && !text.val()) {
            throw "FHIR-RESOURCE-BUILDER-ERROR", "CodeableConcept requires `coding` or `text`";
        }
        hash<auto!> rv = ({});
        if (coding) {
            rv.coding = coding;
        }
        if (text.val()) {
            rv.text = text;
        }
        return rv;
    }

    #! Builds a FHIR Reference hash
    /** @param reference FHIR reference string, for example \c "Patient/example"
        @param display optional display text
        @return FHIR Reference element
        @throw FHIR-RESOURCE-BUILDER-ERROR if \a reference is empty
    */
    public hash<auto> sub fhir_make_reference(string reference, *string display = NOTHING) {
        fhir_assert_nonempty("Reference.reference", reference);
        hash<auto> rv = ({"reference": reference});
        if (display.val()) {
            rv.display = display;
        }
        return rv;
    }

    #! Normalizes and validates a UCUM unit code for FHIR Quantity values
    /** @param unit_code UCUM unit code or supported common alias
        @return hash with \c system, \c code, and \c unit keys
        @throw FHIR-UCUM-ERROR if the code is empty or syntactically unsafe

        @note This helper validates a safe UCUM code shape and normalizes common
        aliases. It does not embed a licensed terminology corpus.
    */
    public hash<auto> sub fhir_normalize_ucum_unit(string unit_code) {
        string code = trim(unit_code);
        if (!code) {
            throw "FHIR-UCUM-ERROR", "UCUM unit code must not be empty";
        }

        *hash<auto> alias = FhirUcumUnitAliases{code};
        if (!alias) {
            alias = FhirUcumUnitAliases{code.lwr()};
        }
        if (alias) {
            return ({
                "system": FhirSystemUcum,
                "code": alias.code,
                "unit": alias.unit,
            });
        }

        if (code !~ /^[A-Za-z0-9%][A-Za-z0-9%_'.{}\[\]\/\*\+\-\(\)]*$/) {
            throw "FHIR-UCUM-ERROR", sprintf("invalid UCUM unit code: %y", code);
        }
        return ({
            "system": FhirSystemUcum,
            "code": code,
            "unit": code,
        });
    }

    #! Builds a FHIR Quantity hash with UCUM system metadata
    /** @param value numeric quantity value
        @param unit_code UCUM unit code or supported common alias
        @param comparator optional FHIR Quantity comparator
        @return FHIR Quantity element
        @throw FHIR-RESOURCE-BUILDER-ERROR if \a value is not numeric
        @throw FHIR-UCUM-ERROR if \a unit_code is invalid
    */
    public hash<auto> sub fhir_make_ucum_quantity(auto value, string unit_code,
            *string comparator = NOTHING) {
        int tc = value.typeCode();
        if (tc != NT_INT && tc != NT_FLOAT && tc != NT_NUMBER) {
            throw "FHIR-RESOURCE-BUILDER-ERROR", "Quantity.value must be numeric";
        }
        hash<auto> unit = fhir_normalize_ucum_unit(unit_code);
        hash<auto!> rv = ({
            "value": value,
            "unit": unit.unit,
            "system": unit.system,
            "code": unit.code,
        });
        if (comparator.val()) {
            rv.comparator = comparator;
        }
        return rv;
    }

    #! Builds a FHIR Observation resource hash
    /** @param opts Observation options. Required keys are \c code or \c code_system plus \c code_code.
        Provide a value with \c value_quantity or with \c value plus \c unit_code.
        Common optional keys include \c id, \c identifier, \c subject, \c device,
        \c effectiveDateTime, \c issued, \c performer, \c category, \c bodySite,
        \c method, \c interpretation, \c component, and \c dataAbsentReason.
        @return FHIR Observation resource
        @throw FHIR-RESOURCE-BUILDER-ERROR if required clinical fields are missing
        @throw FHIR-UCUM-ERROR if a UCUM quantity unit is invalid
    */
    public hash<auto> sub fhir_make_observation(hash<auto> opts) {
        string status = opts.status ?? "final";
        fhir_assert_nonempty("Observation.status", status);

        hash<auto!> observation = ({
            "resourceType": "Observation",
            "status": status,
            "code": fhir_get_codeable_concept(opts, "Observation.code", "code"),
        });

        fhir_add_optional_value(\observation, "id", opts.id);
        fhir_add_optional_list(\observation, "identifier", opts.identifier);
        fhir_add_optional_reference(\observation, "subject", opts.subject);
        fhir_add_optional_reference(\observation, "device", opts.device);
        fhir_add_optional_reference_list(\observation, "performer", opts.performer);
        fhir_add_optional_list(\observation, "category", opts.category);
        fhir_add_optional_value(\observation, "effectiveDateTime", opts.effectiveDateTime);
        fhir_add_optional_value(\observation, "issued", opts.issued);
        fhir_add_optional_value(\observation, "bodySite", opts.bodySite);
        fhir_add_optional_value(\observation, "method", opts.method);
        fhir_add_optional_list(\observation, "interpretation", opts.interpretation);

        if (exists opts.value_quantity) {
            observation.valueQuantity = opts.value_quantity;
        } else if (exists opts.value) {
            *string unit_code = opts.unit_code ?? opts.unit;
            if (!unit_code.val()) {
                throw "FHIR-RESOURCE-BUILDER-ERROR",
                    "Observation quantity value requires `unit_code` or `unit`";
            }
            observation.valueQuantity = fhir_make_ucum_quantity(opts.value, string(unit_code), opts.comparator);
        }
        fhir_add_optional_list(\observation, "component", opts.component);
        fhir_add_optional_value(\observation, "dataAbsentReason", opts.dataAbsentReason);

        if (!exists observation.valueQuantity && !exists observation.component && !exists observation.dataAbsentReason) {
            throw "FHIR-RESOURCE-BUILDER-ERROR",
                "Observation requires `value_quantity`, `value`, `component`, or `dataAbsentReason`";
        }
        return observation;
    }

    #! Builds a FHIR AuditEvent resource hash
    /** @param opts AuditEvent options. Required keys are \c type, \c action,
        \c recorded, \c agent, and \c source. Optional keys include \c id,
        \c outcome, \c outcomeDesc, \c entity, and \c subtype.
        @return FHIR AuditEvent resource
        @throw FHIR-RESOURCE-BUILDER-ERROR if required audit fields are missing
    */
    public hash<auto> sub fhir_make_audit_event(hash<auto> opts) {
        fhir_require_key(opts, "type", "AuditEvent.type");
        fhir_require_key(opts, "action", "AuditEvent.action");
        fhir_require_key(opts, "recorded", "AuditEvent.recorded");
        fhir_require_key(opts, "agent", "AuditEvent.agent");
        fhir_require_key(opts, "source", "AuditEvent.source");

        hash<auto!> rv = ({
            "resourceType": "AuditEvent",
            "type": opts.type,
            "action": opts.action,
            "recorded": opts.recorded,
            "agent": fhir_as_list(opts.agent),
            "source": opts.source,
        });
        fhir_add_optional_value(\rv, "id", opts.id);
        fhir_add_optional_value(\rv, "outcome", opts.outcome);
        fhir_add_optional_value(\rv, "outcomeDesc", opts.outcomeDesc);
        fhir_add_optional_list(\rv, "entity", opts.entity);
        fhir_add_optional_list(\rv, "subtype", opts.subtype);
        return rv;
    }

    #! Builds a FHIR Provenance resource hash
    /** @param opts Provenance options. Required keys are \c target, \c recorded,
        and \c agent. Optional keys include \c id, \c activity, \c entity,
        \c signature, and \c reason.
        @return FHIR Provenance resource
        @throw FHIR-RESOURCE-BUILDER-ERROR if required provenance fields are missing
    */
    public hash<auto> sub fhir_make_provenance(hash<auto> opts) {
        fhir_require_key(opts, "target", "Provenance.target");
        fhir_require_key(opts, "recorded", "Provenance.recorded");
        fhir_require_key(opts, "agent", "Provenance.agent");

        hash<auto!> rv = ({
            "resourceType": "Provenance",
            "target": fhir_as_reference_list(opts.target),
            "recorded": opts.recorded,
            "agent": fhir_as_list(opts.agent),
        });
        fhir_add_optional_value(\rv, "id", opts.id);
        fhir_add_optional_value(\rv, "activity", opts.activity);
        fhir_add_optional_list(\rv, "entity", opts.entity);
        fhir_add_optional_list(\rv, "signature", opts.signature);
        fhir_add_optional_list(\rv, "reason", opts.reason);
        return rv;
    }

    sub fhir_assert_nonempty(string field, string value) {
        if (!value) {
            throw "FHIR-RESOURCE-BUILDER-ERROR", sprintf("%s must not be empty", field);
        }
    }

    sub fhir_require_key(hash<auto> h, string key, string field) {
        if (!exists h{key}) {
            throw "FHIR-RESOURCE-BUILDER-ERROR", sprintf("%s is required", field);
        }
    }

    hash<auto> sub fhir_get_codeable_concept(hash<auto> opts, string field, string prefix) {
        if (exists opts{prefix}) {
            if (opts{prefix}.typeCode() != NT_HASH) {
                throw "FHIR-RESOURCE-BUILDER-ERROR", sprintf("%s must be a CodeableConcept hash", field);
            }
            return opts{prefix};
        }

        string system_key = prefix + "_system";
        string code_key = prefix + "_code";
        if (!exists opts{system_key} || !exists opts{code_key}) {
            throw "FHIR-RESOURCE-BUILDER-ERROR",
                sprintf("%s requires `%s` and `%s` when `%s` is not supplied", field, system_key, code_key, prefix);
        }
        return fhir_make_codeable_concept((
            fhir_make_coding(string(opts{system_key}), string(opts{code_key}), opts{prefix + "_display"}),
        ));
    }

    list<auto> sub fhir_as_list(auto value) {
        if (value.typeCode() == NT_LIST) {
            return value;
        }
        return (value,);
    }

    list<auto> sub fhir_as_reference_list(auto value) {
        list<auto> rv = ();
        foreach auto item in (fhir_as_list(value)) {
            if (item.typeCode() == NT_STRING) {
                push rv, fhir_make_reference(item);
            } else if (item.typeCode() == NT_HASH) {
                push rv, item;
            } else {
                throw "FHIR-RESOURCE-BUILDER-ERROR", "FHIR reference list entries must be strings or hashes";
            }
        }
        return rv;
    }

    sub fhir_add_optional_value(reference<hash<auto>> h, string key, auto value) {
        if (exists value) {
            h{key} = value;
        }
    }

    sub fhir_add_optional_list(reference<hash<auto>> h, string key, auto value) {
        if (exists value) {
            h{key} = fhir_as_list(value);
        }
    }

    sub fhir_add_optional_reference(reference<hash<auto>> h, string key, auto value) {
        if (!exists value) {
            return;
        }
        if (value.typeCode() == NT_STRING) {
            h{key} = fhir_make_reference(value);
        } else if (value.typeCode() == NT_HASH) {
            h{key} = value;
        } else {
            throw "FHIR-RESOURCE-BUILDER-ERROR",
                sprintf("%s must be a FHIR Reference string or hash", key);
        }
    }

    sub fhir_add_optional_reference_list(reference<hash<auto>> h, string key, auto value) {
        if (exists value) {
            h{key} = fhir_as_reference_list(value);
        }
    }
}
