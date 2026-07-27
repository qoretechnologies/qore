# FHIR R4 JSON Artifact Generation Gate

Reviewed: 2026-05-26

This note records the source, licensing, validation, and generation decisions for generated FHIR R4 JSON artifacts
in `module-json`.  It applies only to the FHIR R4 JSON client and data provider work under
`qlib/FhirRestDataProvider`; HL7 v2, MLLP, CDA, and application-layer integration are out of scope.

## Sources

- Target FHIR release: R4 `4.0.1`.
- Primary generation input: the HL7 R4 JSON definitions from
  `https://hl7.org/fhir/R4/definitions.json.zip`, or the equivalent `hl7.fhir.r4.core` package version `4.0.1`.
- Package metadata source: `https://packages.fhir.org/hl7.fhir.r4.core`; on 2026-05-26 it reported
  `hl7.fhir.r4.core` `4.0.1` as the latest R4 package with SHA1 `0e4b8d99f7918587557682c8b47df605424547a5`.
- Secondary validation input: `https://hl7.org/fhir/R4/fhir.schema.json.zip`.
- Specification references reviewed:
  - `https://hl7.org/fhir/R4/downloads.html`
  - `https://hl7.org/fhir/R4/license.html`
  - `https://hl7.org/fhir/R4/json.html`
  - `https://hl7.org/fhir/R4/validation.html`

The HL7 R4 downloads page identifies the JSON definitions as the complete source for implementation artifact
generation.  JSON Schema is useful as a structural validation source and cross-check, but it does not cover all FHIR
validity rules.

## Licensing and Trademark Constraints

- HL7 publishes the R4 FHIR specification materials under Creative Commons CC0, and permits redistribution and
  derivative implementation artifacts.
- FHIR names and the flame design are HL7 trademarks.  App metadata and documentation that use these marks must keep
  a legal notice and must not imply HL7 endorsement.
- The FHIR icon has separate trademark-use requirements.  Do not replace or regenerate `fhir-logo.svg` without
  preserving its source and permission rationale.
- Third-party terminology content referenced by FHIR can have separate licensing requirements.  Generated artifacts
  must not embed third-party expansions or proprietary terminology payloads unless their license is explicitly
  recorded and compatible with distribution.

## Generation Decision

Generated artifacts should be committed, but raw source ZIPs or NPM package tarballs should not be committed.
Generation must be deterministic and rerunnable from an explicit local source artifact or an explicit URL plus
checksum.  Normal builds and tests must not require network access.

The generator must produce a manifest beside the generated files, containing at least:

- FHIR release and package version.
- Source URL and source checksum.
- Generator script path and generator version.
- List of generated files.
- License summary and terminology-exclusion policy.

The generated layout should start under:

```text
qlib/FhirRestDataProvider/fhir/r4/
```

Expected generated outputs:

- A resource-type enum for R4 resource names.
- Resource and datatype metadata derived from `StructureDefinition.snapshot.element`.
- `HashDataType` definitions for data provider input/output typing and documentation.
- Search parameter metadata by resource type.
- Action catalog helpers that expose resource-specific actions without removing the existing generic advanced actions.

The generator must fail rather than silently degrade when it finds unsupported constructs, duplicate generated names,
checksum mismatches, invalid JSON source artifacts, or schema constructs that cannot be represented safely.

The first committed generated slice provides the complete R4 resource-type catalog and direct-field
`HashDataType` metadata for `Patient`, `Observation`, and `Bundle`.  The generated metadata is intentionally
permissive for unknown extension/profile fields by using a default `any` field type; full FHIR profile,
terminology, and invariant validation remains delegated as described below.

## Validation Model

Local validation will be layered and must be described accurately:

- JSON parsing validates JSON syntax and UTF-8 handling.
- JSON Schema validation can check important structural constraints.
- Generated `HashDataType` definitions improve data provider UX, examples, and typed option validation.
- Server-side FHIR `$validate`, `$validate-code`, and `$expand` remain the authoritative path for profile,
  terminology, invariant, and deployment-specific validation.

The module must not claim full local FHIR conformance validation unless it implements structure, cardinality, value
domains, terminology bindings, invariants, profiles, questionnaires, and external business rules for the applicable
context.

## Implementation Checklist

1. Add a deterministic generator script and tests for unsupported source constructs. **Implemented.**
2. Generate a small first slice for core metadata plus `Patient`, `Observation`, and `Bundle`. **Implemented.**
3. Add generated manifest verification tests. **Implemented for generator output.**
4. Add data provider action catalog tests for generated resource-specific actions. **Implemented.**
5. Add negative tests for invalid resource type, invalid field type, unsupported profile validation, and checksum
   mismatch. **Implemented for local validation boundaries: invalid resource types, typed generated field values, and
   checksum mismatch are covered. Profile validation remains delegated to server-side `$validate` by design.**
6. Document examples for generic actions, generated resource-specific actions, raw advanced search parameters, and
   server-side validation. **Implemented.**
