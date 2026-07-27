# -*- mode: qore; indent-tabs-mode: nil -*-
#! Generated FHIR R4 resource type catalog; do not edit manually

/*  Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FhirRestDataProvider module
public namespace FhirRestDataProvider {
#! FHIR R4 resource type enum generated from official HL7 R4 definitions
public enum FhirR4ResourceType : string {
    #! Account resource type
    Account = "Account",
    #! ActivityDefinition resource type
    ActivityDefinition = "ActivityDefinition",
    #! AdverseEvent resource type
    AdverseEvent = "AdverseEvent",
    #! AllergyIntolerance resource type
    AllergyIntolerance = "AllergyIntolerance",
    #! Appointment resource type
    Appointment = "Appointment",
    #! AppointmentResponse resource type
    AppointmentResponse = "AppointmentResponse",
    #! AuditEvent resource type
    AuditEvent = "AuditEvent",
    #! Basic resource type
    Basic = "Basic",
    #! Binary resource type
    Binary = "Binary",
    #! BiologicallyDerivedProduct resource type
    BiologicallyDerivedProduct = "BiologicallyDerivedProduct",
    #! BodyStructure resource type
    BodyStructure = "BodyStructure",
    #! Bundle resource type
    Bundle = "Bundle",
    #! CapabilityStatement resource type
    CapabilityStatement = "CapabilityStatement",
    #! CarePlan resource type
    CarePlan = "CarePlan",
    #! CareTeam resource type
    CareTeam = "CareTeam",
    #! CatalogEntry resource type
    CatalogEntry = "CatalogEntry",
    #! ChargeItem resource type
    ChargeItem = "ChargeItem",
    #! ChargeItemDefinition resource type
    ChargeItemDefinition = "ChargeItemDefinition",
    #! Claim resource type
    Claim = "Claim",
    #! ClaimResponse resource type
    ClaimResponse = "ClaimResponse",
    #! ClinicalImpression resource type
    ClinicalImpression = "ClinicalImpression",
    #! CodeSystem resource type
    CodeSystem = "CodeSystem",
    #! Communication resource type
    Communication = "Communication",
    #! CommunicationRequest resource type
    CommunicationRequest = "CommunicationRequest",
    #! CompartmentDefinition resource type
    CompartmentDefinition = "CompartmentDefinition",
    #! Composition resource type
    Composition = "Composition",
    #! ConceptMap resource type
    ConceptMap = "ConceptMap",
    #! Condition resource type
    Condition = "Condition",
    #! Consent resource type
    Consent = "Consent",
    #! Contract resource type
    Contract = "Contract",
    #! Coverage resource type
    Coverage = "Coverage",
    #! CoverageEligibilityRequest resource type
    CoverageEligibilityRequest = "CoverageEligibilityRequest",
    #! CoverageEligibilityResponse resource type
    CoverageEligibilityResponse = "CoverageEligibilityResponse",
    #! DetectedIssue resource type
    DetectedIssue = "DetectedIssue",
    #! Device resource type
    Device = "Device",
    #! DeviceDefinition resource type
    DeviceDefinition = "DeviceDefinition",
    #! DeviceMetric resource type
    DeviceMetric = "DeviceMetric",
    #! DeviceRequest resource type
    DeviceRequest = "DeviceRequest",
    #! DeviceUseStatement resource type
    DeviceUseStatement = "DeviceUseStatement",
    #! DiagnosticReport resource type
    DiagnosticReport = "DiagnosticReport",
    #! DocumentManifest resource type
    DocumentManifest = "DocumentManifest",
    #! DocumentReference resource type
    DocumentReference = "DocumentReference",
    #! EffectEvidenceSynthesis resource type
    EffectEvidenceSynthesis = "EffectEvidenceSynthesis",
    #! Encounter resource type
    Encounter = "Encounter",
    #! Endpoint resource type
    Endpoint = "Endpoint",
    #! EnrollmentRequest resource type
    EnrollmentRequest = "EnrollmentRequest",
    #! EnrollmentResponse resource type
    EnrollmentResponse = "EnrollmentResponse",
    #! EpisodeOfCare resource type
    EpisodeOfCare = "EpisodeOfCare",
    #! EventDefinition resource type
    EventDefinition = "EventDefinition",
    #! Evidence resource type
    Evidence = "Evidence",
    #! EvidenceVariable resource type
    EvidenceVariable = "EvidenceVariable",
    #! ExampleScenario resource type
    ExampleScenario = "ExampleScenario",
    #! ExplanationOfBenefit resource type
    ExplanationOfBenefit = "ExplanationOfBenefit",
    #! FamilyMemberHistory resource type
    FamilyMemberHistory = "FamilyMemberHistory",
    #! Flag resource type
    Flag = "Flag",
    #! Goal resource type
    Goal = "Goal",
    #! GraphDefinition resource type
    GraphDefinition = "GraphDefinition",
    #! Group resource type
    Group = "Group",
    #! GuidanceResponse resource type
    GuidanceResponse = "GuidanceResponse",
    #! HealthcareService resource type
    HealthcareService = "HealthcareService",
    #! ImagingStudy resource type
    ImagingStudy = "ImagingStudy",
    #! Immunization resource type
    Immunization = "Immunization",
    #! ImmunizationEvaluation resource type
    ImmunizationEvaluation = "ImmunizationEvaluation",
    #! ImmunizationRecommendation resource type
    ImmunizationRecommendation = "ImmunizationRecommendation",
    #! ImplementationGuide resource type
    ImplementationGuide = "ImplementationGuide",
    #! InsurancePlan resource type
    InsurancePlan = "InsurancePlan",
    #! Invoice resource type
    Invoice = "Invoice",
    #! Library resource type
    Library = "Library",
    #! Linkage resource type
    Linkage = "Linkage",
    #! List resource type
    List = "List",
    #! Location resource type
    Location = "Location",
    #! Measure resource type
    Measure = "Measure",
    #! MeasureReport resource type
    MeasureReport = "MeasureReport",
    #! Media resource type
    Media = "Media",
    #! Medication resource type
    Medication = "Medication",
    #! MedicationAdministration resource type
    MedicationAdministration = "MedicationAdministration",
    #! MedicationDispense resource type
    MedicationDispense = "MedicationDispense",
    #! MedicationKnowledge resource type
    MedicationKnowledge = "MedicationKnowledge",
    #! MedicationRequest resource type
    MedicationRequest = "MedicationRequest",
    #! MedicationStatement resource type
    MedicationStatement = "MedicationStatement",
    #! MedicinalProduct resource type
    MedicinalProduct = "MedicinalProduct",
    #! MedicinalProductAuthorization resource type
    MedicinalProductAuthorization = "MedicinalProductAuthorization",
    #! MedicinalProductContraindication resource type
    MedicinalProductContraindication = "MedicinalProductContraindication",
    #! MedicinalProductIndication resource type
    MedicinalProductIndication = "MedicinalProductIndication",
    #! MedicinalProductIngredient resource type
    MedicinalProductIngredient = "MedicinalProductIngredient",
    #! MedicinalProductInteraction resource type
    MedicinalProductInteraction = "MedicinalProductInteraction",
    #! MedicinalProductManufactured resource type
    MedicinalProductManufactured = "MedicinalProductManufactured",
    #! MedicinalProductPackaged resource type
    MedicinalProductPackaged = "MedicinalProductPackaged",
    #! MedicinalProductPharmaceutical resource type
    MedicinalProductPharmaceutical = "MedicinalProductPharmaceutical",
    #! MedicinalProductUndesirableEffect resource type
    MedicinalProductUndesirableEffect = "MedicinalProductUndesirableEffect",
    #! MessageDefinition resource type
    MessageDefinition = "MessageDefinition",
    #! MessageHeader resource type
    MessageHeader = "MessageHeader",
    #! MolecularSequence resource type
    MolecularSequence = "MolecularSequence",
    #! NamingSystem resource type
    NamingSystem = "NamingSystem",
    #! NutritionOrder resource type
    NutritionOrder = "NutritionOrder",
    #! Observation resource type
    Observation = "Observation",
    #! ObservationDefinition resource type
    ObservationDefinition = "ObservationDefinition",
    #! OperationDefinition resource type
    OperationDefinition = "OperationDefinition",
    #! OperationOutcome resource type
    OperationOutcome = "OperationOutcome",
    #! Organization resource type
    Organization = "Organization",
    #! OrganizationAffiliation resource type
    OrganizationAffiliation = "OrganizationAffiliation",
    #! Parameters resource type
    Parameters = "Parameters",
    #! Patient resource type
    Patient = "Patient",
    #! PaymentNotice resource type
    PaymentNotice = "PaymentNotice",
    #! PaymentReconciliation resource type
    PaymentReconciliation = "PaymentReconciliation",
    #! Person resource type
    Person = "Person",
    #! PlanDefinition resource type
    PlanDefinition = "PlanDefinition",
    #! Practitioner resource type
    Practitioner = "Practitioner",
    #! PractitionerRole resource type
    PractitionerRole = "PractitionerRole",
    #! Procedure resource type
    Procedure = "Procedure",
    #! Provenance resource type
    Provenance = "Provenance",
    #! Questionnaire resource type
    Questionnaire = "Questionnaire",
    #! QuestionnaireResponse resource type
    QuestionnaireResponse = "QuestionnaireResponse",
    #! RelatedPerson resource type
    RelatedPerson = "RelatedPerson",
    #! RequestGroup resource type
    RequestGroup = "RequestGroup",
    #! ResearchDefinition resource type
    ResearchDefinition = "ResearchDefinition",
    #! ResearchElementDefinition resource type
    ResearchElementDefinition = "ResearchElementDefinition",
    #! ResearchStudy resource type
    ResearchStudy = "ResearchStudy",
    #! ResearchSubject resource type
    ResearchSubject = "ResearchSubject",
    #! RiskAssessment resource type
    RiskAssessment = "RiskAssessment",
    #! RiskEvidenceSynthesis resource type
    RiskEvidenceSynthesis = "RiskEvidenceSynthesis",
    #! Schedule resource type
    Schedule = "Schedule",
    #! SearchParameter resource type
    SearchParameter = "SearchParameter",
    #! ServiceRequest resource type
    ServiceRequest = "ServiceRequest",
    #! Slot resource type
    Slot = "Slot",
    #! Specimen resource type
    Specimen = "Specimen",
    #! SpecimenDefinition resource type
    SpecimenDefinition = "SpecimenDefinition",
    #! StructureDefinition resource type
    StructureDefinition = "StructureDefinition",
    #! StructureMap resource type
    StructureMap = "StructureMap",
    #! Subscription resource type
    Subscription = "Subscription",
    #! Substance resource type
    Substance = "Substance",
    #! SubstanceNucleicAcid resource type
    SubstanceNucleicAcid = "SubstanceNucleicAcid",
    #! SubstancePolymer resource type
    SubstancePolymer = "SubstancePolymer",
    #! SubstanceProtein resource type
    SubstanceProtein = "SubstanceProtein",
    #! SubstanceReferenceInformation resource type
    SubstanceReferenceInformation = "SubstanceReferenceInformation",
    #! SubstanceSourceMaterial resource type
    SubstanceSourceMaterial = "SubstanceSourceMaterial",
    #! SubstanceSpecification resource type
    SubstanceSpecification = "SubstanceSpecification",
    #! SupplyDelivery resource type
    SupplyDelivery = "SupplyDelivery",
    #! SupplyRequest resource type
    SupplyRequest = "SupplyRequest",
    #! Task resource type
    Task = "Task",
    #! TerminologyCapabilities resource type
    TerminologyCapabilities = "TerminologyCapabilities",
    #! TestReport resource type
    TestReport = "TestReport",
    #! TestScript resource type
    TestScript = "TestScript",
    #! ValueSet resource type
    ValueSet = "ValueSet",
    #! VerificationResult resource type
    VerificationResult = "VerificationResult",
    #! VisionPrescription resource type
    VisionPrescription = "VisionPrescription",
}

#! Sorted FHIR R4 resource type names
public const FhirR4ResourceTypeList = (
    "Account",
    "ActivityDefinition",
    "AdverseEvent",
    "AllergyIntolerance",
    "Appointment",
    "AppointmentResponse",
    "AuditEvent",
    "Basic",
    "Binary",
    "BiologicallyDerivedProduct",
    "BodyStructure",
    "Bundle",
    "CapabilityStatement",
    "CarePlan",
    "CareTeam",
    "CatalogEntry",
    "ChargeItem",
    "ChargeItemDefinition",
    "Claim",
    "ClaimResponse",
    "ClinicalImpression",
    "CodeSystem",
    "Communication",
    "CommunicationRequest",
    "CompartmentDefinition",
    "Composition",
    "ConceptMap",
    "Condition",
    "Consent",
    "Contract",
    "Coverage",
    "CoverageEligibilityRequest",
    "CoverageEligibilityResponse",
    "DetectedIssue",
    "Device",
    "DeviceDefinition",
    "DeviceMetric",
    "DeviceRequest",
    "DeviceUseStatement",
    "DiagnosticReport",
    "DocumentManifest",
    "DocumentReference",
    "EffectEvidenceSynthesis",
    "Encounter",
    "Endpoint",
    "EnrollmentRequest",
    "EnrollmentResponse",
    "EpisodeOfCare",
    "EventDefinition",
    "Evidence",
    "EvidenceVariable",
    "ExampleScenario",
    "ExplanationOfBenefit",
    "FamilyMemberHistory",
    "Flag",
    "Goal",
    "GraphDefinition",
    "Group",
    "GuidanceResponse",
    "HealthcareService",
    "ImagingStudy",
    "Immunization",
    "ImmunizationEvaluation",
    "ImmunizationRecommendation",
    "ImplementationGuide",
    "InsurancePlan",
    "Invoice",
    "Library",
    "Linkage",
    "List",
    "Location",
    "Measure",
    "MeasureReport",
    "Media",
    "Medication",
    "MedicationAdministration",
    "MedicationDispense",
    "MedicationKnowledge",
    "MedicationRequest",
    "MedicationStatement",
    "MedicinalProduct",
    "MedicinalProductAuthorization",
    "MedicinalProductContraindication",
    "MedicinalProductIndication",
    "MedicinalProductIngredient",
    "MedicinalProductInteraction",
    "MedicinalProductManufactured",
    "MedicinalProductPackaged",
    "MedicinalProductPharmaceutical",
    "MedicinalProductUndesirableEffect",
    "MessageDefinition",
    "MessageHeader",
    "MolecularSequence",
    "NamingSystem",
    "NutritionOrder",
    "Observation",
    "ObservationDefinition",
    "OperationDefinition",
    "OperationOutcome",
    "Organization",
    "OrganizationAffiliation",
    "Parameters",
    "Patient",
    "PaymentNotice",
    "PaymentReconciliation",
    "Person",
    "PlanDefinition",
    "Practitioner",
    "PractitionerRole",
    "Procedure",
    "Provenance",
    "Questionnaire",
    "QuestionnaireResponse",
    "RelatedPerson",
    "RequestGroup",
    "ResearchDefinition",
    "ResearchElementDefinition",
    "ResearchStudy",
    "ResearchSubject",
    "RiskAssessment",
    "RiskEvidenceSynthesis",
    "Schedule",
    "SearchParameter",
    "ServiceRequest",
    "Slot",
    "Specimen",
    "SpecimenDefinition",
    "StructureDefinition",
    "StructureMap",
    "Subscription",
    "Substance",
    "SubstanceNucleicAcid",
    "SubstancePolymer",
    "SubstanceProtein",
    "SubstanceReferenceInformation",
    "SubstanceSourceMaterial",
    "SubstanceSpecification",
    "SupplyDelivery",
    "SupplyRequest",
    "Task",
    "TerminologyCapabilities",
    "TestReport",
    "TestScript",
    "ValueSet",
    "VerificationResult",
    "VisionPrescription",
);

#! FHIR R4 resource type membership map
public const FhirR4ResourceTypeMap = {
    "Account": True,
    "ActivityDefinition": True,
    "AdverseEvent": True,
    "AllergyIntolerance": True,
    "Appointment": True,
    "AppointmentResponse": True,
    "AuditEvent": True,
    "Basic": True,
    "Binary": True,
    "BiologicallyDerivedProduct": True,
    "BodyStructure": True,
    "Bundle": True,
    "CapabilityStatement": True,
    "CarePlan": True,
    "CareTeam": True,
    "CatalogEntry": True,
    "ChargeItem": True,
    "ChargeItemDefinition": True,
    "Claim": True,
    "ClaimResponse": True,
    "ClinicalImpression": True,
    "CodeSystem": True,
    "Communication": True,
    "CommunicationRequest": True,
    "CompartmentDefinition": True,
    "Composition": True,
    "ConceptMap": True,
    "Condition": True,
    "Consent": True,
    "Contract": True,
    "Coverage": True,
    "CoverageEligibilityRequest": True,
    "CoverageEligibilityResponse": True,
    "DetectedIssue": True,
    "Device": True,
    "DeviceDefinition": True,
    "DeviceMetric": True,
    "DeviceRequest": True,
    "DeviceUseStatement": True,
    "DiagnosticReport": True,
    "DocumentManifest": True,
    "DocumentReference": True,
    "EffectEvidenceSynthesis": True,
    "Encounter": True,
    "Endpoint": True,
    "EnrollmentRequest": True,
    "EnrollmentResponse": True,
    "EpisodeOfCare": True,
    "EventDefinition": True,
    "Evidence": True,
    "EvidenceVariable": True,
    "ExampleScenario": True,
    "ExplanationOfBenefit": True,
    "FamilyMemberHistory": True,
    "Flag": True,
    "Goal": True,
    "GraphDefinition": True,
    "Group": True,
    "GuidanceResponse": True,
    "HealthcareService": True,
    "ImagingStudy": True,
    "Immunization": True,
    "ImmunizationEvaluation": True,
    "ImmunizationRecommendation": True,
    "ImplementationGuide": True,
    "InsurancePlan": True,
    "Invoice": True,
    "Library": True,
    "Linkage": True,
    "List": True,
    "Location": True,
    "Measure": True,
    "MeasureReport": True,
    "Media": True,
    "Medication": True,
    "MedicationAdministration": True,
    "MedicationDispense": True,
    "MedicationKnowledge": True,
    "MedicationRequest": True,
    "MedicationStatement": True,
    "MedicinalProduct": True,
    "MedicinalProductAuthorization": True,
    "MedicinalProductContraindication": True,
    "MedicinalProductIndication": True,
    "MedicinalProductIngredient": True,
    "MedicinalProductInteraction": True,
    "MedicinalProductManufactured": True,
    "MedicinalProductPackaged": True,
    "MedicinalProductPharmaceutical": True,
    "MedicinalProductUndesirableEffect": True,
    "MessageDefinition": True,
    "MessageHeader": True,
    "MolecularSequence": True,
    "NamingSystem": True,
    "NutritionOrder": True,
    "Observation": True,
    "ObservationDefinition": True,
    "OperationDefinition": True,
    "OperationOutcome": True,
    "Organization": True,
    "OrganizationAffiliation": True,
    "Parameters": True,
    "Patient": True,
    "PaymentNotice": True,
    "PaymentReconciliation": True,
    "Person": True,
    "PlanDefinition": True,
    "Practitioner": True,
    "PractitionerRole": True,
    "Procedure": True,
    "Provenance": True,
    "Questionnaire": True,
    "QuestionnaireResponse": True,
    "RelatedPerson": True,
    "RequestGroup": True,
    "ResearchDefinition": True,
    "ResearchElementDefinition": True,
    "ResearchStudy": True,
    "ResearchSubject": True,
    "RiskAssessment": True,
    "RiskEvidenceSynthesis": True,
    "Schedule": True,
    "SearchParameter": True,
    "ServiceRequest": True,
    "Slot": True,
    "Specimen": True,
    "SpecimenDefinition": True,
    "StructureDefinition": True,
    "StructureMap": True,
    "Subscription": True,
    "Substance": True,
    "SubstanceNucleicAcid": True,
    "SubstancePolymer": True,
    "SubstanceProtein": True,
    "SubstanceReferenceInformation": True,
    "SubstanceSourceMaterial": True,
    "SubstanceSpecification": True,
    "SupplyDelivery": True,
    "SupplyRequest": True,
    "Task": True,
    "TerminologyCapabilities": True,
    "TestReport": True,
    "TestScript": True,
    "ValueSet": True,
    "VerificationResult": True,
    "VisionPrescription": True,
};

#! Manifest for generated FHIR R4 resource catalog artifacts
public const FhirR4ArtifactManifest = {
    "artifact": "fhir-r4-resource-catalog",
    "fhir_version": "4.0.1",
    "generated_files": (
        "qlib/FhirRestDataProvider/FhirR4ResourceTypes.qc",
        "qlib/FhirRestDataProvider/FhirR4ResourceDataTypes.qc",
        "qlib/FhirRestDataProvider/fhir/r4/resource-metadata.json",
        "qlib/FhirRestDataProvider/fhir/r4/manifest.json",
    ),
    "generated_resource_count": 3,
    "generated_resources": (
        "Bundle",
        "Observation",
        "Patient",
    ),
    "generator": "tools/FhirR4ArtifactGenerator.qm",
    "generator_version": "0.1.0",
    "license": "HL7 FHIR specification materials are licensed under Creative Commons CC0",
    "package_name": "hl7.fhir.r4.core",
    "package_version": "4.0.1",
    "resource_count": 146,
    "resources": (
        "Account",
        "ActivityDefinition",
        "AdverseEvent",
        "AllergyIntolerance",
        "Appointment",
        "AppointmentResponse",
        "AuditEvent",
        "Basic",
        "Binary",
        "BiologicallyDerivedProduct",
        "BodyStructure",
        "Bundle",
        "CapabilityStatement",
        "CarePlan",
        "CareTeam",
        "CatalogEntry",
        "ChargeItem",
        "ChargeItemDefinition",
        "Claim",
        "ClaimResponse",
        "ClinicalImpression",
        "CodeSystem",
        "Communication",
        "CommunicationRequest",
        "CompartmentDefinition",
        "Composition",
        "ConceptMap",
        "Condition",
        "Consent",
        "Contract",
        "Coverage",
        "CoverageEligibilityRequest",
        "CoverageEligibilityResponse",
        "DetectedIssue",
        "Device",
        "DeviceDefinition",
        "DeviceMetric",
        "DeviceRequest",
        "DeviceUseStatement",
        "DiagnosticReport",
        "DocumentManifest",
        "DocumentReference",
        "EffectEvidenceSynthesis",
        "Encounter",
        "Endpoint",
        "EnrollmentRequest",
        "EnrollmentResponse",
        "EpisodeOfCare",
        "EventDefinition",
        "Evidence",
        "EvidenceVariable",
        "ExampleScenario",
        "ExplanationOfBenefit",
        "FamilyMemberHistory",
        "Flag",
        "Goal",
        "GraphDefinition",
        "Group",
        "GuidanceResponse",
        "HealthcareService",
        "ImagingStudy",
        "Immunization",
        "ImmunizationEvaluation",
        "ImmunizationRecommendation",
        "ImplementationGuide",
        "InsurancePlan",
        "Invoice",
        "Library",
        "Linkage",
        "List",
        "Location",
        "Measure",
        "MeasureReport",
        "Media",
        "Medication",
        "MedicationAdministration",
        "MedicationDispense",
        "MedicationKnowledge",
        "MedicationRequest",
        "MedicationStatement",
        "MedicinalProduct",
        "MedicinalProductAuthorization",
        "MedicinalProductContraindication",
        "MedicinalProductIndication",
        "MedicinalProductIngredient",
        "MedicinalProductInteraction",
        "MedicinalProductManufactured",
        "MedicinalProductPackaged",
        "MedicinalProductPharmaceutical",
        "MedicinalProductUndesirableEffect",
        "MessageDefinition",
        "MessageHeader",
        "MolecularSequence",
        "NamingSystem",
        "NutritionOrder",
        "Observation",
        "ObservationDefinition",
        "OperationDefinition",
        "OperationOutcome",
        "Organization",
        "OrganizationAffiliation",
        "Parameters",
        "Patient",
        "PaymentNotice",
        "PaymentReconciliation",
        "Person",
        "PlanDefinition",
        "Practitioner",
        "PractitionerRole",
        "Procedure",
        "Provenance",
        "Questionnaire",
        "QuestionnaireResponse",
        "RelatedPerson",
        "RequestGroup",
        "ResearchDefinition",
        "ResearchElementDefinition",
        "ResearchStudy",
        "ResearchSubject",
        "RiskAssessment",
        "RiskEvidenceSynthesis",
        "Schedule",
        "SearchParameter",
        "ServiceRequest",
        "Slot",
        "Specimen",
        "SpecimenDefinition",
        "StructureDefinition",
        "StructureMap",
        "Subscription",
        "Substance",
        "SubstanceNucleicAcid",
        "SubstancePolymer",
        "SubstanceProtein",
        "SubstanceReferenceInformation",
        "SubstanceSourceMaterial",
        "SubstanceSpecification",
        "SupplyDelivery",
        "SupplyRequest",
        "Task",
        "TerminologyCapabilities",
        "TestReport",
        "TestScript",
        "ValueSet",
        "VerificationResult",
        "VisionPrescription",
    ),
    "source_sha1": "970b7a5ca5f317bc7e59d22b01f0d512240d120b",
    "source_url": "https://hl7.org/fhir/R4/definitions.json.zip",
    "terminology_policy": "Third-party terminology expansions are not embedded by this generated catalog.",
    "trademark_notice": "HL7, FHIR, and the FHIR flame design are registered trademarks of Health Level Seven International; use does not constitute endorsement by HL7.",
};

#! Returns True if the argument is a known FHIR R4 resource type
public bool sub isFhirR4ResourceType(string resource_type) {
    return exists FhirR4ResourceTypeMap{resource_type};
}

#! Validates and returns a FHIR R4 resource type
public string sub assertFhirR4ResourceType(string resource_type) {
    if (!isFhirR4ResourceType(resource_type)) {
        throw "FHIR-RESOURCE-TYPE-ERROR", sprintf("unsupported FHIR R4 resource type: %y", resource_type);
    }
    return resource_type;
}

#! Validates and returns the resource type from a FHIR R4 resource body
public string sub assertFhirR4ResourceBodyType(hash<auto> resource) {
    if (!exists resource.resourceType) {
        throw "FHIR-RESOURCE-TYPE-ERROR", "FHIR R4 resource body is missing resourceType";
    }
    if (resource.resourceType.typeCode() != NT_STRING) {
        throw "FHIR-RESOURCE-TYPE-ERROR", "FHIR R4 resource body resourceType must be a string";
    }
    return assertFhirR4ResourceType(resource.resourceType);
}

#! Validates request and body resource types for FHIR R4 resource requests
public string sub assertFhirR4ResourceRequestType(string resource_type, *hash<auto> resource = NOTHING) {
    assertFhirR4ResourceType(resource_type);
    if (resource) {
        string body_resource_type = assertFhirR4ResourceBodyType(resource);
        if (body_resource_type != resource_type) {
            throw "FHIR-RESOURCE-TYPE-ERROR", sprintf("request resource_type %y does not match body resourceType %y", resource_type, body_resource_type);
        }
    }
    return resource_type;
}
}
