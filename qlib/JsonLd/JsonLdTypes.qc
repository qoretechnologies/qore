# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdTypes.qc @brief JSON-LD type definitions, constants, enums, and hashdecls

/*  JsonLdTypes.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

# ============================================================================
# Enums
# ============================================================================

#! JSON-LD processing mode
/** @since json 1.12
*/
public enum JsonLdProcessingMode : string {
    #! JSON-LD 1.0 processing mode
    JsonLd10 = "json-ld-1.0",
    #! JSON-LD 1.1 processing mode
    JsonLd11 = "json-ld-1.1",
}

#! JSON-LD document form
/** @since json 1.12
*/
public enum JsonLdForm : string {
    #! Expanded form
    Expanded = "expanded",
    #! Compacted form
    Compacted = "compacted",
    #! Flattened form
    Flattened = "flattened",
    #! Framed form
    Framed = "framed",
}

#! RDF term type
/** @since json 1.12
*/
public enum RdfTermType : string {
    #! IRI reference
    Iri = "IRI",
    #! Blank node identifier
    BlankNode = "blank node",
    #! Literal value
    Literal = "literal",
}

# ============================================================================
# JSON-LD Keywords
# ============================================================================

#! \@base keyword
public const JSONLD_BASE = "@base";
#! \@container keyword
public const JSONLD_CONTAINER = "@container";
#! \@context keyword
public const JSONLD_CONTEXT = "@context";
#! \@direction keyword
public const JSONLD_DIRECTION = "@direction";
#! \@graph keyword
public const JSONLD_GRAPH = "@graph";
#! \@id keyword
public const JSONLD_ID = "@id";
#! \@import keyword
public const JSONLD_IMPORT = "@import";
#! \@included keyword
public const JSONLD_INCLUDED = "@included";
#! \@index keyword
public const JSONLD_INDEX = "@index";
#! \@json keyword
public const JSONLD_JSON = "@json";
#! \@language keyword
public const JSONLD_LANGUAGE = "@language";
#! \@list keyword
public const JSONLD_LIST = "@list";
#! \@nest keyword
public const JSONLD_NEST = "@nest";
#! \@none keyword
public const JSONLD_NONE = "@none";
#! \@prefix keyword
public const JSONLD_PREFIX = "@prefix";
#! \@propagate keyword
public const JSONLD_PROPAGATE = "@propagate";
#! \@protected keyword
public const JSONLD_PROTECTED = "@protected";
#! \@reverse keyword
public const JSONLD_REVERSE = "@reverse";
#! \@set keyword
public const JSONLD_SET = "@set";
#! \@type keyword
public const JSONLD_TYPE = "@type";
#! \@value keyword
public const JSONLD_VALUE = "@value";
#! \@version keyword
public const JSONLD_VERSION = "@version";
#! \@vocab keyword
public const JSONLD_VOCAB = "@vocab";

#! Set of all JSON-LD keywords for fast lookup
public const JSONLD_KEYWORDS = {
    "@base": True,
    "@container": True,
    "@context": True,
    "@direction": True,
    "@graph": True,
    "@id": True,
    "@import": True,
    "@included": True,
    "@index": True,
    "@json": True,
    "@language": True,
    "@list": True,
    "@nest": True,
    "@none": True,
    "@prefix": True,
    "@propagate": True,
    "@protected": True,
    "@reverse": True,
    "@set": True,
    "@type": True,
    "@value": True,
    "@version": True,
    "@vocab": True,
};

#! Keywords that are not allowed as term definitions
public const JSONLD_API_KEYWORDS = {
    "@context": True,
    "@preserve": True,
};

#! Valid container values
public const JSONLD_VALID_CONTAINERS = {
    "@graph": True,
    "@id": True,
    "@index": True,
    "@language": True,
    "@list": True,
    "@set": True,
    "@type": True,
};

# ============================================================================
# Content Types and Profiles
# ============================================================================

#! JSON-LD content type
public const MimeTypeJsonLd = "application/ld+json";

#! JSON content type
public const MimeTypeJson = "application/json";

#! JSON-LD profile: expanded
public const JSONLD_PROFILE_EXPANDED = "http://www.w3.org/ns/json-ld#expanded";
#! JSON-LD profile: compacted
public const JSONLD_PROFILE_COMPACTED = "http://www.w3.org/ns/json-ld#compacted";
#! JSON-LD profile: flattened
public const JSONLD_PROFILE_FLATTENED = "http://www.w3.org/ns/json-ld#flattened";
#! JSON-LD profile: framed
public const JSONLD_PROFILE_FRAMED = "http://www.w3.org/ns/json-ld#framed";
#! JSON-LD profile: context
public const JSONLD_PROFILE_CONTEXT = "http://www.w3.org/ns/json-ld#context";

# ============================================================================
# XSD Namespace and Types
# ============================================================================

#! XSD namespace
public const XSD_NS = "http://www.w3.org/2001/XMLSchema#";
#! XSD boolean type
public const XSD_BOOLEAN = "http://www.w3.org/2001/XMLSchema#boolean";
#! XSD double type
public const XSD_DOUBLE = "http://www.w3.org/2001/XMLSchema#double";
#! XSD integer type
public const XSD_INTEGER = "http://www.w3.org/2001/XMLSchema#integer";
#! XSD string type
public const XSD_STRING = "http://www.w3.org/2001/XMLSchema#string";

# ============================================================================
# RDF Namespace and Terms
# ============================================================================

#! RDF namespace
public const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
#! RDF type predicate
public const RDF_TYPE = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
#! RDF first predicate (for lists)
public const RDF_FIRST = "http://www.w3.org/1999/02/22-rdf-syntax-ns#first";
#! RDF rest predicate (for lists)
public const RDF_REST = "http://www.w3.org/1999/02/22-rdf-syntax-ns#rest";
#! RDF nil (list terminator)
public const RDF_NIL = "http://www.w3.org/1999/02/22-rdf-syntax-ns#nil";
#! RDF List type
public const RDF_LIST = "http://www.w3.org/1999/02/22-rdf-syntax-ns#List";
#! RDF langString type
public const RDF_LANGSTRING = "http://www.w3.org/1999/02/22-rdf-syntax-ns#langString";
#! RDF JSON type
public const RDF_JSON = "http://www.w3.org/1999/02/22-rdf-syntax-ns#JSON";
#! RDF direction type (i18n namespace)
public const RDF_DIRECTION = "https://www.w3.org/ns/i18n#";

# ============================================================================
# Hashdecls
# ============================================================================

#! JSON-LD processor options (W3C JsonLdOptions)
/** @since json 1.12
*/
public hashdecl JsonLdOptions {
    #! Base IRI for the document
    *string base;

    #! If true, arrays with one element are compacted to scalar values
    bool compactArrays = True;

    #! If true, IRIs are compacted relative to base
    bool compactToRelative = True;

    #! Custom document loader callback: code<hash<JsonLdRemoteDocument>(string, *hash<JsonLdLoadDocumentOptions>)>
    *code<hash<auto>(string, *hash<auto>)> documentLoader;

    #! Context to expand with
    *auto expandContext;

    #! Enable frame expansion mode
    bool frameExpansion = False;

    #! Process elements in lexicographic order
    bool ordered = False;

    #! Processing mode
    string processingMode = "json-ld-1.1";

    #! Produce generalized RDF (blank node predicates)
    bool produceGeneralizedRdf = False;

    #! RDF direction handling: "i18n-datatype" or NOTHING
    *string rdfDirection;

    #! Convert RDF types to native JSON types
    bool useNativeTypes = False;

    #! Use rdf:type instead of \@type
    bool useRdfType = False;

    #! Framing: embed mode ("\@always", "\@once", "\@never")
    *string embed;

    #! Framing: require all properties to match
    *bool explicit_;

    #! Framing: include \@default for missing properties
    *bool omitDefault;

    #! Framing: omit \@graph for single graphs
    *bool omitGraph;

    #! Framing: require all frame properties
    *bool requireAll;
}

#! Document loader options
/** @since json 1.12
*/
public hashdecl JsonLdLoadDocumentOptions {
    #! Expected document profile
    *string profile;

    #! Requested content types
    *list<string> requestProfile;
}

#! Loaded remote document
/** @since json 1.12
*/
public hashdecl JsonLdRemoteDocument {
    #! Final URL after redirects
    string documentUrl;

    #! Parsed document content
    auto document;

    #! Context URL from Link header
    *string contextUrl;

    #! Content type of the document
    *string contentType;

    #! Profile of the document
    *string profile;
}

#! Active context term definition
/** @since json 1.12
*/
public hashdecl JsonLdTermDefinition {
    #! IRI mapping for this term
    *string iriMapping;

    #! Whether this term can be used as a prefix
    bool prefix = False;

    #! Whether this term definition is protected
    bool protected_ = False;

    #! Whether this is a reverse property
    bool reverse = False;

    #! Base URL for relative IRI resolution
    *string baseUrl;

    #! Container mapping (\@set, \@list, \@language, \@index, \@id, \@graph, \@type)
    *list<string> containerMapping;

    #! Local context for scoped contexts
    *auto localContext;

    #! Whether a local context has been explicitly set (even to null)
    bool hasLocalContext = False;

    #! Language mapping for this term
    *string languageMapping;

    #! Whether a language mapping has been explicitly set
    bool hasLanguageMapping = False;

    #! Direction mapping for this term
    *string directionMapping;

    #! Whether a direction mapping has been explicitly set
    bool hasDirectionMapping = False;

    #! Nest value
    *string nestValue;

    #! Index property for property-based indexing
    *string indexProperty;

    #! Type mapping (\@id, \@vocab, \@json, \@none, or IRI)
    *string typeMapping;
}

#! Active context state
/** @since json 1.12
*/
public hashdecl JsonLdActiveContext {
    #! Term definitions
    hash<string, hash<JsonLdTermDefinition>> termDefinitions = {};

    #! Base IRI
    *string baseIri;

    #! Original base IRI (before resolution)
    *string originalBaseIri;

    #! Inverse context for compaction
    *hash<auto> inverseContext;

    #! Vocabulary mapping
    *string vocabularyMapping;

    #! Default language
    *string defaultLanguage;

    #! Default base direction
    *string defaultBaseDirection;

    #! Previous context (for property-scoped contexts with \@propagate = false)
    *hash<JsonLdActiveContext> previousContext;
}

#! RDF term (IRI, blank node, or literal)
/** @since json 1.12
*/
public hashdecl RdfTerm {
    #! Term type: "IRI", "blank node", or "literal"
    string termType;

    #! Term value
    string value;

    #! Datatype IRI for literals
    *string datatype;

    #! Language tag for language-tagged literals
    *string language;
}

#! RDF quad (subject, predicate, object, optional graph)
/** @since json 1.12
*/
public hashdecl RdfQuad {
    #! Subject (IRI or blank node)
    hash<RdfTerm> subject;

    #! Predicate (IRI)
    hash<RdfTerm> predicate;

    #! Object (IRI, blank node, or literal)
    hash<RdfTerm> object;

    #! Graph name (IRI or blank node), NOTHING for default graph
    *hash<RdfTerm> graphName;
}

#! RDF dataset (default graph + named graphs)
/** @since json 1.12
*/
public hashdecl RdfDataset {
    #! Quads in the default graph
    list<hash<RdfQuad>> defaultGraph = ();

    #! Named graphs mapping graph name to list of quads
    hash<string, list<hash<RdfQuad>>> namedGraphs = {};
}

#! Context cache entry (internal)
/** @since json 1.12
*/
public hashdecl JsonLdCacheEntry {
    #! Cached context value
    auto contextValue;

    #! When this entry was cached (epoch seconds)
    int cachedAt;

    #! ETag for conditional requests
    *string etag;
}

} # namespace JsonLd
