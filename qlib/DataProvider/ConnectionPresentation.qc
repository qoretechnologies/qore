# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectionPresentation.qc source-owned connection presentation

/** ConnectionPresentation.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DataProvider {
#! A source-bound reference to one translated connection presentation field
/** References are copied with option metadata, independently of executable scheme names and defaults.
    @since %DataProvider 3.11
*/
public hashdecl ConnectionPresentationSource {
    string domain;
    string message_id;
    string source_locale;
    string source;
}

#! Pure preparation and extraction of shared connection presentation
/** Prepare producer constants before deriving other schemes from them. Unchanged fields retain their original
    references; changed fields receive the deriving producer's identity. No connection or dynamic callback runs.
    @since %DataProvider 3.11
*/
public class ConnectionPresentation {
    #! Encodes an exact technical identity for a message path
    /** @param identity nonempty technical identity
        @return reversible UTF-8 base64url segment
        @throw DATA-PROVIDER-PRESENTATION-ERROR if the identity is empty
    */
    static string segment(string identity) {
        if (!identity.val()) {
            throw "DATA-PROVIDER-PRESENTATION-ERROR", "connection presentation identity cannot be empty";
        }
        return convert_encoding(identity, "UTF-8").toBase64Url();
    }

    #! Returns the catalog domain owned by a connection module
    /** @param owner exact module name
        @return native provider catalog domain
        @throw DATA-PROVIDER-PRESENTATION-ERROR if the owner is empty
    */
    static string domain(string owner) {
        segment(owner);
        return DataProviderPresentation::defaultCatalogDomain("connection:" + owner);
    }

    #! Attaches source references to a scheme and its options before metadata is reused
    /** @param owner exact module name
        @param identity stable logical scheme identity shared by all aliases
        @param source_locale explicit producer BCP-47 locale
        @param info unprojected connection scheme metadata
        @return a copy carrying source-bound presentation references
        @throw I18N-LOCALE-ERROR for an invalid locale
        @throw DATA-PROVIDER-PRESENTATION-ERROR for missing identities, unknown source language, or duplicate messages
        @par Example
        An HTTP producer uses owner ConnectionProvider and identity HttpConnection for both http and https.
        @note Changing defaults preserves references; replacing text only replaces that field's reference.
    */
    static hash<auto> prepareScheme(string owner, string identity, string source_locale, hash<auto> info) {
        string locale = I18n::canonicalize_locale(source_locale);
        if (inlist(locale, ("root", "und"))) {
            throw "DATA-PROVIDER-PRESENTATION-ERROR", "connection producer locale must identify a language";
        }
        string catalog_domain = domain(owner);
        string prefix = "connection." + segment(owner) + ".scheme." + segment(identity);
        hash<auto> rv = openRecord(info);
        rv.presentation_owner = owner;
        rv.presentation_id = identity;
        rv.presentation_source_locale = locale;
        rv = prepareRecord(catalog_domain, prefix, locale, rv, True);
        rv.options = info.options.typeCode() == NT_HASH
            ? prepareOptions(catalog_domain, prefix, locale, info.options) : info.options;
        return rv;
    }

    #! Prepares an option map while preserving references inherited from other producers
    /** @param catalog_domain domain for newly defined fields
        @param prefix exact technical message prefix
        @param source_locale explicit producer locale
        @param options static option metadata
        @return a copy with presentation references
        @throw DATA-PROVIDER-PRESENTATION-ERROR for invalid technical identities
    */
    static hash<string, hash<ConnectionProvider::ConnectionOptionInfo>> prepareOptions(string catalog_domain,
            string prefix, string source_locale, *hash<auto> options) {
        hash<string, hash<ConnectionProvider::ConnectionOptionInfo>> rv = {};
        foreach hash<auto> pair in (options.pairIterator()) {
            rv{pair.key} = cast<hash<ConnectionProvider::ConnectionOptionInfo>>(
                prepareRecord(catalog_domain, prefix + ".option." + segment(pair.key), source_locale, pair.value));
        }
        return rv;
    }

    #! Prepares app-specific connection options using the app's presentation identity
    /** @param app static application metadata
        @return prepared options; inherited scheme references remain unchanged
        @throw DATA-PROVIDER-PRESENTATION-ERROR for an invalid app identity
    */
    static hash<string, hash<ConnectionProvider::ConnectionOptionInfo>> appOptions(hash<auto> app) {
        return prepareOptions(DataProviderPresentation::appCatalogDomain(app),
            "app." + segment(app.name) + ".connection", DataProviderPresentation::appSourceLocale(app),
            app.connection_options);
    }

    #! Extracts one owner's scheme records into native source catalogs
    /** @param schemes registered scheme metadata; aliases may repeat records
        @param owner optional exact owner filter
        @return native source catalogs keyed by domain
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR for conflicting references
        @note Dependency-owned option references are emitted only by their defining module.
    */
    static hash<string, hash<auto>> buildSchemeCatalogs(hash<auto> schemes, *string owner = NOTHING) {
        hash<string, hash<auto>> messages;
        foreach hash<auto> scheme in (schemes.values()) {
            hash<auto> info = openRecord(scheme);
            if (!info.presentation_owner.val()) {
                if (owner.val() && info.cls && info.cls.getModuleName() == owner) {
                    throw "DATA-PROVIDER-PRESENTATION-CATALOG-ERROR",
                        sprintf("connection scheme of module %y has no source presentation identity", owner);
                }
                continue;
            }
            info = prepareScheme(info.presentation_owner, info.presentation_id,
                info.presentation_source_locale, info);
            list<hash<auto>> records = (info,) + (info.options.values() ?? ());
            foreach hash<auto> record in (records) {
                hash<auto> open = openRecord(record);
                foreach hash<ConnectionPresentationSource> ref in (open.presentation_sources.values()) {
                    if (owner.val() && ref.domain != domain(owner)) {
                        continue;
                    }
                    addMessage(\messages{ref.domain}, ref);
                }
            }
        }
        hash<string, hash<auto>> rv = {};
        foreach hash<auto> pair in (messages.pairIterator()) {
            rv{pair.key} = {"schema_version": I18n::get_catalog_schema_version(),
                "locales": {"root": {"messages": pair.value}}};
        }
        return rv;
    }

    #! Adds app-owned connection option messages to an existing source catalog
    /** @param messages destination message map
        @param app static application metadata
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR for conflicting references
    */
    static addAppMessages(reference<hash<auto>> messages, hash<auto> app) {
        string catalog_domain = DataProviderPresentation::appCatalogDomain(app);
        foreach hash<auto> option in (appOptions(app).values()) {
            addRecordMessages(\messages, option, catalog_domain);
        }
    }

    #! Copies typed metadata into an open hash without mutating the producer
    /** @param record producer metadata
        @return open shallow copy
    */
    static hash<auto> openRecord(hash<auto> record) {
        hash<auto!> rv = {};
        foreach hash<auto> pair in (record.pairIterator()) {
            rv{pair.key} = pair.value;
        }
        return rv;
    }

    #! Returns the stable suffix for an allowed value in an option
    /** @param kind allowed_values or element_allowed_values
        @param value exact scalar technical value
        @return suffix, or NOTHING for a non-scalar value
    */
    static *string valueSuffix(string kind, auto value) {
        *string identity = DataProviderPresentation::scalarValueIdentity(value);
        return identity.val() ? kind + "." + segment(identity) + "." : NOTHING;
    }

    private static hash<auto> prepareRecord(string catalog_domain, string prefix, string source_locale,
            hash<auto> record, bool scheme = False) {
        hash<auto> rv = openRecord(record);
        hash<string, hash<ConnectionPresentationSource>> old = rv.presentation_sources ?? {};
        hash<string, hash<ConnectionPresentationSource>> refs = {};
        addFields(\refs, old, catalog_domain, prefix, source_locale, rv, "",
            DataProviderPresentation::PresentationFields);
        foreach string kind in (("allowed_values", "element_allowed_values")) {
            foreach hash<auto> value in (rv{kind}) {
                *string suffix = valueSuffix(kind, value.value);
                if (suffix.val()) {
                    addFields(\refs, old, catalog_domain, prefix, source_locale, openRecord(value), suffix,
                        DataProviderPresentation::PresentationFields);
                }
            }
        }
        if (scheme) {
            hash<string, bool> seen;
            foreach hash<auto> message in (rv.messages) {
                hash<auto> msg = openRecord(message);
                if (!msg.presentation_id.val() || seen{msg.presentation_id}) {
                    throw "DATA-PROVIDER-PRESENTATION-ERROR",
                        sprintf("connection scheme %y messages require distinct, nonempty presentation_id values; got %y",
                            prefix, msg.presentation_id);
                }
                seen{msg.presentation_id} = True;
                addFields(\refs, old, catalog_domain, prefix, source_locale, msg,
                    "message." + segment(msg.presentation_id) + ".", ("title", "content"));
            }
        }
        rv.presentation_sources = refs;
        return rv;
    }

    private static addFields(reference<hash<string, hash<ConnectionPresentationSource>>> refs,
            hash<string, hash<ConnectionPresentationSource>> old, string catalog_domain, string prefix,
            string source_locale, hash<auto> record, string suffix, list<string> fields) {
        foreach string field in (fields) {
            if (record{field}.typeCode() != NT_STRING || !record{field}.val()) {
                continue;
            }
            string key = suffix + field;
            string source = record{field};
            if (old{key} && old{key}.source == source) {
                refs{key} = old{key};
            } else {
                refs{key} = <ConnectionPresentationSource>{
                    "domain": catalog_domain, "message_id": prefix + "." + key,
                    "source_locale": source_locale, "source": source,
                };
            }
        }
    }

    private static addRecordMessages(reference<hash<auto>> messages, hash<auto> record, string catalog_domain) {
        hash<auto> info = openRecord(record);
        foreach hash<ConnectionPresentationSource> ref in (info.presentation_sources.values()) {
            if (ref.domain != catalog_domain) {
                continue;
            }
            addMessage(\messages, ref);
        }
    }
    private static addMessage(reference<hash<auto>> messages, hash<ConnectionPresentationSource> ref) {
        if (exists messages{ref.message_id} && messages{ref.message_id}.source != ref.source) {
            throw "DATA-PROVIDER-PRESENTATION-CATALOG-ERROR",
                sprintf("connection message %y has conflicting sources", ref.message_id);
        }
        messages{ref.message_id} = {"source": ref.source, "message": ref.source};
    }

}

#! An operation-scoped projection of connection presentation across its defining catalogs
/** Catalog instances are cached only within this object. Use one object per request; do not share it across threads.
    @since %DataProvider 3.11
*/
public class ConnectionPresentationCatalog {
    private {
        string locale;
        *hash<auto> discovery_opts;
        I18n::CatalogDiscoveryIndex discovery_index();
        hash<string, DataProviderPresentationCatalog> catalogs;
    }

    #! Creates a connection-form projector
    /** @param requested_locale target BCP-47 locale, or root for producer text
        @param discovery_opts native catalog search options
        @throw I18N-LOCALE-ERROR for an invalid locale
        @par Example
        ConnectionPresentationCatalog view("de"); view.projectScheme(http_scheme);
    */
    constructor(string requested_locale, *hash<auto> discovery_opts = NOTHING) {
        locale = requested_locale == "root" ? "root" : I18n::canonicalize_locale(requested_locale);
        self.discovery_opts = discovery_opts;
    }

    #! Projects a scheme, its options, and its static messages
    /** @param scheme unprojected scheme metadata
        @return independent projected metadata with incomplete/source provenance
        @throw I18N-CATALOG-ERROR for malformed catalogs
        @note Executable schemes, defaults, option types, and allowed values are unchanged.
    */
    hash<auto> projectScheme(hash<auto> scheme) {
        hash<auto> source = ConnectionPresentation::openRecord(scheme);
        hash<auto> rv = projectRecord(source, "", source.presentation_sources,
            DataProviderPresentation::PresentationFields, source.presentation_source_locale ?? "und");
        rv.options = projectOptions(source.options);
        if (source.messages) {
            list<hash<auto>> messages;
            foreach hash<auto> message in (source.messages) {
                hash<auto> msg = ConnectionPresentation::openRecord(message);
                string suffix = msg.presentation_id.val()
                    ? "message." + ConnectionPresentation::segment(msg.presentation_id) + "." : "unidentified.";
                messages += projectRecord(msg, suffix, source.presentation_sources, ("title", "content"),
                    source.presentation_source_locale ?? "und");
            }
            rv.messages = messages;
        }
        return rv;
    }

    #! Projects the options selected by an app override or its connection scheme
    /** @param app application metadata
        @param scheme connection scheme metadata
        @return projected options honoring app-specific overrides
        @throw I18N-CATALOG-ERROR for malformed catalogs
    */
    hash<string, hash<auto>> projectAppOptions(hash<auto> app, hash<auto> scheme) {
        return projectOptions(app.connection_options.typeCode() == NT_HASH
            ? ConnectionPresentation::appOptions(app) : scheme.options);
    }

    #! Projects an option map, including allowed-value presentation
    /** @param options unprojected option metadata
        @return projected option map with unchanged technical keys
        @throw I18N-CATALOG-ERROR for malformed catalogs
    */
    hash<string, hash<auto>> projectOptions(*hash<auto> options) {
        hash<string, hash<auto>> rv = {};
        foreach hash<auto> pair in (options.pairIterator()) {
            hash<auto> source = ConnectionPresentation::openRecord(pair.value);
            hash<auto> projected = projectRecord(source, "", source.presentation_sources,
                DataProviderPresentation::PresentationFields, "und");
            foreach string kind in (("allowed_values", "element_allowed_values")) {
                if (source{kind}) {
                    list<hash<auto>> values;
                    foreach hash<auto> value in (source{kind}) {
                        *string suffix = ConnectionPresentation::valueSuffix(kind, value.value);
                        values += projectRecord(ConnectionPresentation::openRecord(value),
                            suffix ?? "unidentified.", source.presentation_sources,
                            DataProviderPresentation::PresentationFields, "und");
                    }
                    projected{kind} = values;
                }
            }
            rv{pair.key} = projected;
        }
        return rv;
    }

    private hash<auto> projectRecord(hash<auto> source, string suffix,
            *hash<string, hash<ConnectionPresentationSource>> refs, list<string> fields, string source_locale) {
        hash<auto> rv = ConnectionPresentation::openRecord(source);
        hash<auto!> producer;
        hash<string, string> field_languages;
        bool incomplete;
        foreach string field in (fields) {
            if (source{field}.typeCode() != NT_STRING) {
                continue;
            }
            string text = source{field};
            producer{field} = text;
            *hash<ConnectionPresentationSource> ref = refs{suffix + field};
            string language = ref ? ref.source_locale : source_locale;
            field_languages{field} = language;
            if (!text.val() || DataProviderPresentation::sourceLocaleMatches(locale, language)) {
                rv{field} = text;
                continue;
            }
            *string translated;
            if (ref && ref.source == text) {
                if (!catalogs{ref.domain}) {
                    catalogs{ref.domain} = new DataProviderPresentationCatalog(ref.domain, locale,
                        discovery_opts, discovery_index);
                }
                translated = catalogs{ref.domain}.lookupSourceMessage(ref.message_id, text);
            }
            if (translated.val()) {
                rv{field} = translated;
            } else {
                rv{field} = "";
                incomplete = True;
            }
        }
        if (producer) {
            list<string> languages = uniq(field_languages.values());
            producer.language = languages.size() == 1 ? languages.first() : source_locale;
            producer.field_languages = field_languages;
            rv.producer_presentation = producer;
        }
        # Option/choice messages have no stable identity. Preserve the source separately, as for missing fields.
        if (suffix !~ /^message\./ && source.messages && source_locale == "und") {
            list<hash<auto>> messages;
            foreach hash<auto> message in (source.messages) {
                hash<auto> projected = projectRecord(ConnectionPresentation::openRecord(message), "message.unidentified.",
                    NOTHING, ("title", "content"), source_locale);
                messages += projected;
                incomplete = incomplete || projected.presentation_incomplete;
            }
            rv.messages = messages;
        }
        rv.presentation_language = locale;
        if (incomplete) {
            rv.presentation_incomplete = True;
        } else {
            remove rv.presentation_incomplete;
        }
        remove rv.presentation_sources;
        return rv;
    }
}
}
