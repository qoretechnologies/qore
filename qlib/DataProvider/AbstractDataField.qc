# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataField class definition

/** AbstractDataField.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

%requires reflection

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! describes a data type
public hashdecl DataFieldInfo {
    #! the name of the field
    string name;

    #! The display name of the field
    string display_name;

    #! The short description of the field in plain text
    *string short_desc;

    #! the description of the field, if available, with markdown formatting
    *string desc;

    #! the default value for the field
    auto default_value;

    #! Any example value for the field
    auto example_value;

    #! for enum fields; allowed values
    *list<hash<AllowedValueInfo>> allowed_values;

    #! Does the field accept values not defined in allowed_values too?
    *bool allowed_values_creatable;

    #! for enum fields; allowed values for elements for list types
    *list<hash<AllowedValueInfo>> element_allowed_values;

    #! Do elements of the field for list types accept values not defined in element_allowed_values too?
    *bool element_allowed_values_creatable;

    #! Type info
    hash<DataTypeInfo> type;

    #! Unique field identifier
    string unique_key;

    #! User-defined attributes set on the field
    *hash<auto> attr;
}

#! describes a data type based on a hashdecl
public class AbstractDataField inherits Qore::Serializable {
    public {
        #! The width a derived short description is capped at, in display columns
        /** A short description is a **one-line summary** shown beside a field, so it has to fit on one line.
            @ref getShortDescription() takes the first sentence of a long description, and a first sentence
            is very often longer than a line: an API's own parameter documentation regularly opens with a
            sentence of 150 characters or more, and every one of those became a short description that no
            user interface could show.

            The cap therefore applies by default rather than on request.  Nothing is lost by it - the whole
            text remains available as the long description - and a caller that really wants an uncapped
            summary passes \c 0.

            @since DataProvider 3.6
        */
        const ShortDescriptionMaxLength = 80;

        #! Marks a short description that was cut short, so that a reader knows to look at the long one
        /** @since DataProvider 3.6
        */
        const ShortDescriptionEllipsis = "…";

        #! Abbreviations whose full stop does not end a sentence
        /** @ref getShortDescription() takes the first sentence of a long description, and a full stop that
            ends one of these does not end a sentence.  Xero documents an argument as
            <tt>"e.g. includeArchived=true - Contacts with a status of ARCHIVED will be included"</tt>,
            whose first "sentence" is otherwise the single letter \c "e".

            Compared case-insensitively.

            @since DataProvider 3.6
        */
        const SentenceAbbreviations = (
            "e.g.", "i.e.", "etc.", "vs.", "cf.", "approx.", "resp.", "no.", "fig.", "ca.", "al.",
            "inc.", "ltd.", "co.", "st.", "dr.", "mr.", "mrs.", "ms.", "prof.",
        );

        #! Maximum length in characters of an all upper-case word assumed to be an acronym
        /** All upper-case words no longer than this are not case-folded when converting a technical name to a
            display name with @ref getDisplayName(), so that short acronyms are preserved
            (ex: \c "CUSTOMER_ID" -> \c "Customer ID"); this rule is script-independent, so short acronyms in
            other scripts are preserved as well (ex: Ukrainian \c "ПІБ", Greek \c "ΑΦΜ")

            @see Acronyms

            @since DataProvider 3.6
        */
        const AcronymMaxLength = 3;

        #! Common short words and abbreviations that are case-folded despite being no longer than @ref AcronymMaxLength
        /** Used by @ref getDisplayName() to case-fold upper-case words that are ordinary words or abbreviations
            rather than acronyms (ex: \c "MAX_RETRIES" -> \c "Max Retries"); keys must be in upper case

            @note This list is necessarily language-specific; use the \c extra_acronyms argument to
            @ref getDisplayName() to override the classification of any word in any language

            @since DataProvider 3.6
        */
        const NonAcronyms = map {$1: True}, (
            "ADD", "AGE", "ALL", "AMT", "ANY", "ARE", "AVG", "BAD", "BIG", "BOX", "BUY", "CAN", "CNT", "COL",
            "CUT", "DAY", "DID", "DIR", "DOC", "DUE", "END", "ERR", "FEE", "FEW", "FIT", "FIX", "GET", "GOT",
            "HAD", "HAS", "HER", "HIM", "HIS", "HOT", "HOW", "IDX", "ITS", "JOB", "KEY", "LEN", "LET", "LOG",
            "LOT", "LOW", "MAP", "MAX", "MAY", "MEN", "MIN", "NET", "NEW", "NOT", "NOW", "NUM", "OLD", "ONE",
            "OUR", "OUT", "OWN", "PAY", "PCT", "PUT", "QTY", "RAW", "REF", "ROW", "RUN", "SEE", "SEQ", "SET",
            "SHE", "SRC", "SUB", "SUM", "TAG", "TAX", "TOP", "TRY", "TWO", "USE", "VAL", "WAS", "WAY", "WEB",
            "WHO", "WHY", "WIN", "YES", "YET", "YOU", "ZIP",
        );

        #! Well-known acronyms longer than @ref AcronymMaxLength that are never case-folded
        /** Used by @ref getDisplayName() to preserve acronyms that would otherwise be case-folded due to their
            length (ex: \c "HTTP_SERVER" -> \c "HTTP Server"); keys must be in upper case

            Acronyms specific to a particular domain or language can be added by passing the \c extra_acronyms
            argument to @ref getDisplayName()

            @since DataProvider 3.6
        */
        const Acronyms = map {$1: True}, (
            # data formats and encodings
            "ASCII", "EBCDIC", "HTML", "JSON", "MIME", "TOML", "UTF8", "XHTML", "XSLT", "YAML",
            # protocols and standards
            "AMQP", "CIDR", "FTPS", "GRPC", "HTTP", "HTTPS", "IMAP", "IPV4", "IPV6", "LDAP", "LDAPS", "MQTT",
            "OAUTH", "OAUTH2", "OIDC", "POP3", "REST", "SAML", "SFTP", "SMTP", "SOAP", "WSDL",
            # identifiers and database terms
            "GUID", "JDBC", "ODBC", "RDBMS", "UUID",
            # business and finance
            "EDIFACT", "EORI", "IBAN", "IDOC", "ISBN", "ISIN", "SEPA", "SWIFT",
        );

        #! Words that are never acronyms and are not capitalized in the middle of a display name
        /** English function words are short enough to be mistaken for acronyms by the
            @ref AcronymMaxLength rule (ex: \c "CREATED_AT" would otherwise yield \c "Created AT"), and
            conventional English title case leaves them in lower case unless they begin or end the name
            (ex: \c "wait_for_active_shards" -> \c "Wait for Active Shards", but \c "created_at" ->
            \c "Created At", where \c "at" ends the name)

            Compared in lower case.

            @note This list is English-specific; use the \c extra_acronyms argument to
            @ref getDisplayName() to override the classification of any word

            @since DataProvider 3.8
        */
        const FunctionWords = map {$1: True}, (
            "a", "an", "and", "as", "at", "but", "by", "for", "from", "if", "in", "into", "is", "nor",
            "of", "off", "on", "or", "per", "the", "to", "via", "vs", "with",
        );

        #! Canonical renderings for acronyms, initialisms, and irregularly capitalized proper nouns
        /** Used by @ref getDisplayName() to render a word in its conventional form regardless of the case
            it is written in in the technical name, so that lower-case and upper-case technical names produce
            the same display name (ex: \c "api_key" and \c "API_KEY" both yield \c "API Key")

            Keys must be in lower case; values give the canonical rendering, which may be mixed case
            (ex: \c "saas" -> \c "SaaS", \c "ipv4" -> \c "IPv4", \c "paypal" -> \c "PayPal")

            This table supersedes @ref Acronyms, which is still honored for words with no entry here

            @note Renderings must not contain word separators (space, \c ".", \c "-", or \c "/"), because
            @ref getNameFromDisplayName() would otherwise not recover the original technical name; use
            \c "UTF8" rather than \c "UTF-8" and \c "IO" rather than \c "I/O"

            @note This list covers terms in general technical and business use; domain-, vendor-, or
            language-specific terms can be supplied with the \c extra_acronyms argument to
            @ref getDisplayName()

            @since DataProvider 3.8
        */
        const WordRenderings = {
            # network, transport, and protocols
            "api": "API", "url": "URL", "uri": "URI", "urn": "URN", "http": "HTTP", "https": "HTTPS",
            "ftp": "FTP", "ftps": "FTPS", "sftp": "SFTP", "smtp": "SMTP", "imap": "IMAP", "pop3": "POP3",
            "ssh": "SSH", "ssl": "SSL", "tls": "TLS", "mtls": "mTLS", "tcp": "TCP", "udp": "UDP",
            "ip": "IP", "ipv4": "IPv4", "ipv6": "IPv6", "dns": "DNS", "dhcp": "DHCP", "cidr": "CIDR",
            "nat": "NAT", "vpn": "VPN", "cdn": "CDN", "mtu": "MTU", "qos": "QoS", "ws": "WS",
            "wss": "WSS", "sse": "SSE", "rpc": "RPC", "grpc": "gRPC", "rest": "REST", "soap": "SOAP",
            "wsdl": "WSDL", "graphql": "GraphQL", "mqtt": "MQTT", "amqp": "AMQP", "smpp": "SMPP",
            "sip": "SIP", "rtp": "RTP", "srt": "SRT", "quic": "QUIC", "h2": "H2", "h3": "H3",
            # data formats, encodings, and markup
            "json": "JSON", "jsonl": "JSONL", "ndjson": "NDJSON", "xml": "XML", "xhtml": "XHTML",
            "html": "HTML", "yaml": "YAML", "toml": "TOML", "csv": "CSV", "tsv": "TSV", "pdf": "PDF",
            "sql": "SQL", "utf8": "UTF8", "utf16": "UTF16", "ascii": "ASCII", "ebcdic": "EBCDIC",
            "mime": "MIME", "xslt": "XSLT", "xsd": "XSD", "xpath": "XPath", "css": "CSS", "svg": "SVG",
            "png": "PNG", "jpeg": "JPEG", "gif": "GIF", "webp": "WebP", "mp3": "MP3", "mp4": "MP4",
            "zip": "ZIP", "gzip": "GZip", "bmp": "BMP", "tiff": "TIFF", "eml": "EML", "ics": "ICS",
            # security, identity, and authorization
            "oauth": "OAuth", "oauth2": "OAuth2", "oidc": "OIDC", "saml": "SAML", "jwt": "JWT",
            "jwk": "JWK", "sso": "SSO", "mfa": "MFA", "2fa": "2FA", "otp": "OTP", "totp": "TOTP",
            "acl": "ACL", "rbac": "RBAC", "abac": "ABAC", "ldap": "LDAP", "ldaps": "LDAPS",
            "sasl": "SASL", "gssapi": "GSSAPI", "hmac": "HMAC", "aes": "AES", "rsa": "RSA",
            "cn": "CN", "dn": "DN", "ou": "OU",
            "ecdsa": "ECDSA", "sha": "SHA", "md5": "MD5", "pki": "PKI", "csr": "CSR", "crl": "CRL",
            "cors": "CORS", "csrf": "CSRF", "xss": "XSS", "waf": "WAF", "iam": "IAM",
            # identifiers and reference numbers
            "id": "ID", "uuid": "UUID", "guid": "GUID", "urid": "URID", "sku": "SKU", "upc": "UPC",
            "ean": "EAN", "isbn": "ISBN", "issn": "ISSN", "isin": "ISIN", "iban": "IBAN", "bic": "BIC",
            "swift": "SWIFT", "eori": "EORI", "vat": "VAT", "ein": "EIN", "ssn": "SSN", "tin": "TIN",
            "duns": "DUNS", "lei": "LEI", "imei": "IMEI", "vin": "VIN", "msisdn": "MSISDN",
            # platform, infrastructure, and computing
            "aws": "AWS", "gcp": "GCP", "cpu": "CPU", "gpu": "GPU", "tpu": "TPU", "npu": "NPU",
            "ram": "RAM", "rom": "ROM", "ssd": "SSD", "hdd": "HDD", "os": "OS", "io": "IO",
            "sdk": "SDK", "cli": "CLI", "gui": "GUI", "tui": "TUI", "ui": "UI",
            "ux": "UX", "db": "DB", "orm": "ORM", "crud": "CRUD", "jdbc": "JDBC", "odbc": "ODBC",
            "rdbms": "RDBMS", "oltp": "OLTP", "olap": "OLAP", "etl": "ETL", "elt": "ELT",
            "vm": "VM", "k8s": "K8s", "ci": "CI", "cd": "CD", "iac": "IaC", "dr": "DR",
            "saas": "SaaS", "paas": "PaaS", "iaas": "IaaS", "faas": "FaaS", "utc": "UTC",
            "gmt": "GMT", "tz": "TZ", "dst": "DST", "i18n": "i18n", "l10n": "l10n", "a11y": "a11y",
            "ttl": "TTL", "fifo": "FIFO", "lifo": "LIFO", "lru": "LRU", "sla": "SLA",
            "slo": "SLO", "sli": "SLI", "rto": "RTO", "rpo": "RPO", "eof": "EOF", "eol": "EOL",
            # artificial intelligence and data science
            "ai": "AI", "ml": "ML", "llm": "LLM", "nlp": "NLP", "nlu": "NLU", "ocr": "OCR",
            "asr": "ASR", "tts": "TTS", "stt": "STT", "rag": "RAG", "cnn": "CNN", "rnn": "RNN",
            "gan": "GAN", "svm": "SVM", "knn": "KNN", "pca": "PCA", "mcp": "MCP", "onnx": "ONNX",
            # media and presentation
            "qr": "QR", "rgb": "RGB", "rgba": "RGBA", "cmyk": "CMYK", "dpi": "DPI", "ppi": "PPI",
            "fps": "FPS", "hdr": "HDR", "sdr": "SDR", "hd": "HD", "uhd": "UHD",
            # business, finance, and accounting
            "erp": "ERP", "crm": "CRM", "scm": "SCM", "plm": "PLM", "cms": "CMS", "hcm": "HCM",
            "hr": "HR", "kpi": "KPI", "okr": "OKR", "roi": "ROI", "tco": "TCO", "npv": "NPV",
            "irr": "IRR", "capex": "CapEx", "opex": "OpEx", "cogs": "COGS", "ebit": "EBIT",
            "ebitda": "EBITDA", "gaap": "GAAP", "ifrs": "IFRS", "gl": "GL", "ap": "AP", "ar": "AR",
            "po": "PO", "so": "SO", "pr": "PR", "rma": "RMA", "asn": "ASN", "bom": "BOM", "moq": "MOQ",
            "eta": "ETA", "etd": "ETD", "fob": "FOB", "cif": "CIF", "ddp": "DDP", "lcl": "LCL",
            "fcl": "FCL", "b2b": "B2B", "b2c": "B2C", "d2c": "D2C", "poc": "PoC", "mvp": "MVP",
            "sow": "SOW", "rfp": "RFP", "rfq": "RFQ", "rfi": "RFI", "nda": "NDA", "eula": "EULA",
            "faq": "FAQ", "toc": "ToC", "yoy": "YoY", "qoq": "QoQ", "mom": "MoM", "ytd": "YTD",
            "mtd": "MTD", "qtd": "QTD", "arr": "ARR", "mrr": "MRR", "cac": "CAC", "ltv": "LTV",
            "arpu": "ARPU", "cpc": "CPC", "cpm": "CPM", "ctr": "CTR", "seo": "SEO", "sem": "SEM",
            "ach": "ACH", "sepa": "SEPA", "edi": "EDI", "edifact": "EDIFACT", "idoc": "IDoc",
            "pos": "POS", "atm": "ATM", "kyc": "KYC", "aml": "AML", "pci": "PCI", "dso": "DSO",
            # regulatory, legal, and standards bodies
            "gdpr": "GDPR", "ccpa": "CCPA", "hipaa": "HIPAA", "ferpa": "FERPA", "sox": "SOX",
            "pii": "PII", "phi": "PHI", "iso": "ISO", "ansi": "ANSI", "ieee": "IEEE", "ietf": "IETF",
            "rfc": "RFC", "w3c": "W3C", "nist": "NIST", "osha": "OSHA", "epa": "EPA", "fda": "FDA",
            "fcc": "FCC", "sec": "SEC", "irs": "IRS", "doe": "DOE", "dod": "DOD", "dot": "DOT",
            "eu": "EU", "un": "UN", "wto": "WTO", "elf": "ELF",
            # healthcare and life sciences
            "ehr": "EHR", "emr": "EMR", "hl7": "HL7", "fhir": "FHIR", "icd": "ICD", "cpt": "CPT",
            "npi": "NPI", "ndc": "NDC", "lims": "LIMS", "gxp": "GxP", "gmp": "GMP",
            # brands and products with irregular capitalization
            "paypal": "PayPal", "ebay": "eBay", "ios": "iOS", "macos": "macOS", "iphone": "iPhone",
            "ipad": "iPad", "javascript": "JavaScript", "typescript": "TypeScript", "nodejs": "NodeJS",
            "mysql": "MySQL", "postgresql": "PostgreSQL", "sqlite": "SQLite", "mongodb": "MongoDB",
            "mariadb": "MariaDB", "dynamodb": "DynamoDB", "github": "GitHub", "gitlab": "GitLab",
            "bitbucket": "Bitbucket", "openai": "OpenAI", "youtube": "YouTube", "linkedin": "LinkedIn",
            "whatsapp": "WhatsApp", "docusign": "DocuSign", "quickbooks": "QuickBooks",
            "salesforce": "Salesforce", "servicenow": "ServiceNow", "hubspot": "HubSpot",
            "woocommerce": "WooCommerce", "bigcommerce": "BigCommerce", "shopify": "Shopify",
            "tiktok": "TikTok", "npm": "npm", "php": "PHP", "dotnet": "dotNET", "jquery": "jQuery",
            "graphviz": "GraphViz", "powershell": "PowerShell", "sharepoint": "SharePoint",
            "onedrive": "OneDrive", "dropbox": "Dropbox", "mailchimp": "Mailchimp", "sendgrid": "SendGrid",
            "netsuite": "NetSuite", "workday": "Workday", "sapui5": "SAPUI5", "sap": "SAP",
            "aftership": "AfterShip", "baselinker": "BaseLinker", "clickfunnels": "ClickFunnels",
        };
    }

    private {
        #! the default value for the field, if any
        auto default_value;

        #! for enum fields; raw allowed values
        list<hash<AllowedValueInfo>> allowed_values;

        #! Does the field accept values not defined in allowed_values too?
        *bool allowed_values_creatable;

        #! for enum fields; allowed values for elements for list types
        *list<hash<AllowedValueInfo>> element_allowed_values;

        #! Do elements of the field for list types accept values not defined in element_allowed_values too?
        *bool element_allowed_values_creatable;

        #! for enum fields; allowed values
        /** only supported for types that convert to a string
        */
        hash<string, bool> allowed_values_map;

        #! for enum fields; allowed values for elements for list types
        /** only supported for types that convert to a string
        */
        hash<string, bool> element_allowed_values_map;

        #! User-defined custom attributes set on the field
        hash<auto> attr;
    }

    #! returns True if this field's type can be assigned from values of the argument type
    bool isAssignableFrom(AbstractDataProviderType t) {
        return getType().isAssignableFrom(t);
    }

    #! returns True if this field's type can be assigned from values of the argument type
    bool isAssignableFrom(Type t) {
        return getType().isAssignableFrom(t);
    }

    #! returns True if this field's type is a list
    bool isList() {
        return getType().isList();
    }

    #! returns True if the field's type must have a value
    bool isMandatory() {
        return getType().isMandatory();
    }

    #! returns the value of the given option on the field's type
    auto getOptionValue(string opt) {
        return getType().getOptionValue(opt);
    }

    #! returns options set on the field's type
    *hash<auto> getOptions() {
        return getType().getOptions();
    }

    #! returns supported options on the field's type
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return getType().getSupportedOptions();
    }

    #! sets the given option on the field's type
    /** @param opt the option to set
        @param value the value to set

        @throw TYPE-OPTION-ERROR invalid option or invalid option type
    */
    AbstractDataField setOption(string opt, auto value) {
        getType().setOption(opt, value);
        return self;
    }

    #! sets the given options on the field's type
    /** @param options a hash of options, if any options match supported options for this type, they are set

        @throw TYPE-OPTION-ERROR option value has an invalid type
    */
    AbstractDataField setOptions(hash<auto> options) {
        getType().setOptions(options);
        return self;
    }

    #! returns the type name
    string getTypeName() {
        return getType().getName();
    }

    #! Sets allowed values for enum types
    AbstractDataField setAllowedValues(list<auto> allowed_values, *bool allowed_values_creatable) {
        AbstractDataProviderType type = getType();
        if (allowed_values[0] instanceof hash<AllowedValueInfo>) {
            self.allowed_values = allowed_values;
        } else if (allowed_values[0].typeCode() == NT_HASH && allowed_values[0].hasKey("value")
            && type.getBaseTypeCode() != NT_HASH) {
            self.allowed_values = map cast<hash<AllowedValueInfo>>($1), allowed_values;
        } else {
            self.allowed_values = map <AllowedValueInfo>{
                "display_name": $1.toString(),
                "value": $1,
            }, allowed_values;
        }
        on_error rethrow $1.err, sprintf("%s (allowed_values: %y)", $1.desc, allowed_values);
        # check that the value can be assigned to the type
        map self.allowed_values[$#].value = type.acceptsValue($1.value), self.allowed_values.iterator();
        if (allowed_values_creatable) {
            self.allowed_values_creatable = True;
        } else {
            # NOTE: requires that values can be converted to a string hash key to simulate a set
            allowed_values_map = map {$1.value: True}, self.allowed_values;
        }
        return self;
    }

    #! Sets allowed values for enum types
    AbstractDataField setElementAllowedValues(list<auto> element_allowed_values,
            *bool element_allowed_values_creatable) {
        AbstractDataProviderType type = getType();
        if (element_allowed_values[0] instanceof hash<AllowedValueInfo>) {
            self.element_allowed_values = element_allowed_values;
        } else if (element_allowed_values[0].typeCode() == NT_HASH && element_allowed_values[0].hasKey("value")
            && type.getBaseTypeCode() != NT_HASH) {
            self.element_allowed_values = map cast<hash<AllowedValueInfo>>($1), element_allowed_values;
        } else {
            self.element_allowed_values = map <AllowedValueInfo>{
                "display_name": $1.toString(),
                "value": $1,
            }, element_allowed_values;
        }
        on_error rethrow $1.err, sprintf("%s (element_allowed_values: %y)", $1.desc, element_allowed_values);
        if (*AbstractDataProviderType et = type.getElementType()) {
            # check that the value can be assigned to the type
            map self.element_allowed_values[$#].value = et.acceptsValue($1.value),
                self.element_allowed_values.iterator();
        }
        if (element_allowed_values_creatable) {
            self.element_allowed_values_creatable = True;
        } else if (element_allowed_values[0].value.strp()) {
            # NOTE: requires that values can be converted to a string hash key to simulate a set
            element_allowed_values_map = map {$1.value: True}, self.element_allowed_values;
        }
        return self;
    }

    #! sets the default value for the field
    AbstractDataField setDefaultValue(auto default_value) {
        # check that the value can be assigned to the type
        self.default_value = getType().acceptsValue(default_value);
        return self;
    }

    #! get default value, if any
    auto getDefaultValue() {
        return default_value ?? getType().getDefaultValue();
    }

    #! Get allowed values
    *list<hash<AllowedValueInfo>> getAllowedValues() {
        return allowed_values;
    }

    #! Get allowed values
    *hash<string, bool> getAllowedValueMap() {
        return allowed_values_map;
    }

    #! Get the "allowed values creatable" flag
    bool getAllowedValuesCreatable() {
        return allowed_values_creatable ?? False;
    }

    #! Get element allowed values
    *list<hash<AllowedValueInfo>> getElementAllowedValues() {
        return element_allowed_values;
    }

    #! Get allowed values
    *hash<string, bool> getElementAllowedValueMap() {
        return element_allowed_values_map;
    }

    #! Get the "element allowed values creatable" flag
    bool getElementAllowedValuesCreatable() {
        return element_allowed_values_creatable ?? False;
    }

    #! returns True if the field's type is not a wildcard type
    bool hasType() {
        return getType().hasType();
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type
    */
    auto acceptsValue(auto value) {
        # do not reference default_value directly here in case it's provided with code
        if (!exists value && exists (auto dv = getDefaultValue())) {
            return dv;
        }
        try {
            # first do any value / type filtering / conversions
            value = getType().acceptsValue(value);
            if (allowed_values && !allowed_values_creatable) {
                if (!allowed_values_map{value}) {
                    throw "FIELD-VALUE-ERROR", sprintf("field %y does not accept value %y; allowed values: %y",
                        getName(), value, (map $1.value, allowed_values));
                }
            }
            if (element_allowed_values_map) {
                foreach auto v in (value) {
                    if (!element_allowed_values_map{v}) {
                        throw "FIELD-VALUE-ERROR", sprintf("field %y does not accept value %y; allowed "
                            "values for elements: %y", getName(), v, (map $1.value, element_allowed_values));
                    }
                }
            } else if (element_allowed_values && !element_allowed_values_creatable) {
                foreach auto v in (value) {
                    if (!inlist(v, element_allowed_values)) {
                        throw "FIELD-VALUE-ERROR", sprintf("field %y does not accept value %y; allowed "
                            "values for elements: %y", getName(), v, (map $1.value, element_allowed_values));
                    }
                }
            }
            return value;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, sprintf("%s (while setting field %y)", ex.desc, getName()), ex.arg;
        }
    }

    #! returns information about the field as an input field
    /** the \c default_value and \c example_value keys are not returned, and the \c type key returns only input
        information
    */
    hash<DataFieldInfo> getInputInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return <DataFieldInfo>{
            "name": getName(),
            "display_name": getDisplayName(),
            "short_desc": getShortDescription(),
            "desc": getDescription(),
            "type": getType().getInfo(),
            "unique_key": self.uniqueHash(),
            "attr": attr,
        };
    }

    #! returns information about the field
    hash<DataFieldInfo> getInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return getLocalInfo() + <DataFieldInfo>{
            "type": getType().getInfo(),
        };
    }

    #! Returns information about the field without calling AbstractDataProviderType::getInfo() on the type
    hash<DataFieldInfo> getLocalInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return <DataFieldInfo>{
            "name": getName(),
            "display_name": getDisplayName(),
            "short_desc": getShortDescription(),
            "desc": getDescription(),
            "default_value": getDefaultValue(),
            "example_value": getExampleValue(),
            "allowed_values": allowed_values,
            "allowed_values_creatable": allowed_values_creatable ?? False,
            "element_allowed_values": element_allowed_values,
            "element_allowed_values_creatable": element_allowed_values_creatable ?? False,
            "unique_key": self.uniqueHash(),
            "attr": attr,
        };
    }

    #! returns information about the field
    hash<DataFieldInfo> getConfigInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return getLocalInfo() + <DataFieldInfo>{
            "type": getType().getInfo(){"name", "path", "base_type", "options", "default_value", "mandatory", "tags",
                "default_field_type_info"},
        };
    }

    #! returns a field with a "soft" type equivalent to the current type
    /** The base class method returns the same field; this method must be overridden in child classes to return a
        field with a usable "soft" type

        @return a field with a "soft" type equivalent to the current type
    */
    AbstractDataField getSoftType() {
        return self;
    }

    #! returns a field with an "or nothing" type equivalent to the current type
    /** The base class method returns the same field; this method must be overridden in child classes to return a
        field with a usable "or nothing" type

        @return a field with an "or nothing" type equivalent to the current type
    */
    AbstractDataField getOrNothingType() {
        return self;
    }

    #! Returns a mandatory (i.e. not "or nothing") field equivalent to the current field
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory field

        @return a mandatory (i.e. not "or nothing") field equivalent to the current field

        @since DataProvider 3.0
    */
    AbstractDataField getMandatoryType() {
        return self;
    }

    #! Returns a copy of this field published under a different name
    /** The base class method throws an exception; it must be overridden in child classes that can carry a
        name independent of their source.

        A field is often shared — schema-derived fields in particular are cached and handed out to every
        caller — so renaming returns a copy and never modifies the field it was called on.

        @param new_name the name for the returned field
        @param new_display_name the display name for the returned field; if not present, the display name is
        re-derived from \a new_name when the current display name was itself derived from the current name,
        and is otherwise preserved

        @return a copy of this field with the new name

        @throw FIELD-RENAME-ERROR this field type does not support renaming

        @since DataProvider 3.8
    */
    AbstractDataField getRenamedField(string new_name, *string new_display_name) {
        throw "FIELD-RENAME-ERROR", sprintf("field %y of class %y cannot be renamed to %y; class %y does not "
            "implement getRenamedField()", getName(), self.className(), new_name, self.className());
    }

    #! Returns the display name of the field
    string getDisplayName() {
        return AbstractDataField::getDisplayName(getName());
    }

    #! Returns the short description, if any
    *string getShortDescription() {
        return AbstractDataField::getShortDescription(getDescription());
    }

    #! Returns custom attributes set on the field
    *hash<auto> getAttributes() {
        return attr;
    }

    #! Sets custom attributes
    AbstractDataField setAttributes(hash<auto> attr) {
        self.attr += attr;
        return self;
    }

    #! Replaces all custom attributes
    replaceAttributes(*hash<auto> attr) {
        remove self.attr;
        if (attr) {
            self.attr += attr;
        }
    }

    #! Returns any example value for the field, otherwise return example data from the type
    /** @param emap a hash to ensure that examples are only produced once
        @param fname any field context for the value

        @return if a default value exists, it is returned first, then the example value for the type, if any

        @since DataProvider 3.0
    */
    auto getExampleValue(*hash<string, bool> emap, *string fname) {
        return getDefaultValue() ?? getType().getExampleValue(emap, getDisplayName());
    }

    #! Converts a long description into a short description
    /** Takes the first sentence of the long description, renders any markdown code spans as plain text, and
        caps the result at @ref ShortDescriptionMaxLength characters, cutting on a word boundary and marking
        the elision with @ref ShortDescriptionEllipsis.

        @param desc the long description, which may carry markdown
        @param maxlen the length to cap the result at; defaults to @ref ShortDescriptionMaxLength, and
        \c 0 or a negative value means no cap at all

        @return the short description in plain text, or @ref nothing if \a desc is @ref nothing

        @note The cap applies by default.  A short description is a one-line summary, and a long
        description's first sentence is very often longer than a line - most of an API's own parameter
        documentation is - so leaving it uncapped produced summaries no user interface could show.
    */
    static *string getShortDescription(*string desc, *int maxlen = ShortDescriptionMaxLength) {
        if (!exists desc) {
            return;
        }
        # convert backquotes to single quotes
        desc =~ s/`/'/g;

        # a short description is plain text, so markdown emphasis is rendered away rather than shown:
        # GitLab marks a deprecated argument as "_(Deprecated)_ Disable email notifications".  Only
        # emphasis is matched - an underscore inside a word belongs to an identifier such as
        # "shipping_option_code" and is left exactly as it is
        desc =~ s/\*\*([^*]+)\*\*/$1/g;
        desc =~ s/(^|[[:space:]])_([^_]+)_(?=[[:space:][:punct:]]|$)/$1$2/g;

        # get single-quoted strings; an apostrophe inside a word is not an
        # opening quote (for example, the apostrophe in "connection's")
        *list<string> sq = (desc =~ x/(?<![[:alnum:]])'([^\'\n]*)'/g);
        # replace them with markers
        if (sq) {
            map desc = replace(desc, $1, "{{" + $# + "}}"), sq;
        }
        # get double-quoted strings
        *list<string> dq = (desc =~ x/"([^\"\n]*)"/g);
        # replace them with markers
        if (dq) {
            map desc = replace(desc, $1, "{{" + $# + "}}"), dq;
        }

        # end after the first sentence or newline, if any
        int end = AbstractDataField::findSentenceEnd(desc);
        if (end >= 0) {
            splice desc, end;
        }
        trim desc;

        # re-add quotes to what's left
        map desc = replace(desc, "{{" + $# + "}}", $1), sq;
        map desc = replace(desc, "{{" + $# + "}}", $1), dq;

        if (maxlen > 0 && char_width(desc) > maxlen) {
            desc = AbstractDataField::truncateShortDescription(desc,
                maxlen);
        }

        return desc;
    }

    #! Truncates plain presentation text on a safe display boundary
    /** The result is measured in terminal display columns rather than code
        points, so wide scripts obey the same visual limit as Latin text.
        Balanced brace, bracket, parenthesis, and quoted groups are atomic:
        a placeholder such as <tt>{name}</tt> is either present in full or
        omitted in full.

        @param desc plain presentation text
        @param max_width maximum display width including the ellipsis

        @return the safely truncated presentation text
    */
    private static string truncateShortDescription(string desc,
            int max_width) {
        int content_width = max_width
            - char_width(ShortDescriptionEllipsis);
        if (content_width <= 0) {
            return ShortDescriptionEllipsis;
        }

        int pos = 0;
        int width = 0;
        int safe_pos = 0;
        int word_boundary = -1;
        list<string> closers = ();
        while (pos < desc.length()) {
            string ch = desc.substr(pos, 1);
            int ch_width = char_width(ch);
            if (width + ch_width > content_width) {
                break;
            }

            string closer = closers ? closers[-1] : "";
            if (closer.val() && ch == closer) {
                splice closers, closers.size() - 1;
            } else if (closer != "'" && closer != "\"") {
                switch (ch) {
                    case "{":
                        closers += "}";
                        break;
                    case "[":
                        closers += "]";
                        break;
                    case "(":
                        closers += ")";
                        break;
                    case "'":
                    case "\"": {
                        string previous = pos
                            ? desc.substr(pos - 1, 1) : "";
                        if (!previous.val()
                                || previous =~ /[[:space:][:punct:]]/) {
                            closers += ch;
                        }
                        break;
                    }
                }
            }

            width += ch_width;
            ++pos;
            if (!closers) {
                safe_pos = pos;
                if (ch =~ /[[:space:]]/) {
                    word_boundary = pos - 1;
                }
            }
        }

        int cut_pos = word_boundary > 0 ? word_boundary : safe_pos;
        string cut = desc.substr(0, cut_pos);
        trim cut;
        return cut.val() ? cut + ShortDescriptionEllipsis
            : ShortDescriptionEllipsis;
    }

    #! Returns the offset of the end of the first sentence, or -1 where the text is a single sentence
    /** A newline or a semicolon always ends one.  A full stop ends one only where it is really sentence
        punctuation: one that is not followed by whitespace is a decimal point, a domain name or a file
        extension, and one that ends a @ref SentenceAbbreviations "known abbreviation" is not a sentence end
        either.

        @param desc the description to scan

        @return the offset of the terminator, or \c -1 where there is none

        @since DataProvider 3.6
    */
    private static int findSentenceEnd(string desc) {
        int rv = -1;
        foreach string t in (("\n", ";")) {
            int p = desc.find(t);
            if (p >= 0 && (rv < 0 || p < rv)) {
                rv = p;
            }
        }
        int pos = 0;
        while (True) {
            int dot = desc.find(".", pos);
            if (dot < 0 || (rv >= 0 && dot > rv)) {
                break;
            }
            string next = desc.substr(dot + 1, 1);
            if ((next.val() && next !~ /^[[:space:]]/) || AbstractDataField::endsAbbreviation(desc, dot)) {
                pos = dot + 1;
                continue;
            }
            if (rv < 0 || dot < rv) {
                rv = dot;
            }
            break;
        }
        return rv;
    }

    #! Returns @ref True if the full stop at the given offset ends a known abbreviation
    /** @param desc the description
        @param dot the offset of the full stop

        @see @ref SentenceAbbreviations

        @since DataProvider 3.6
    */
    private static bool endsAbbreviation(string desc, int dot) {
        foreach string a in (SentenceAbbreviations) {
            int start = dot + 1 - a.length();
            if (start < 0 || desc.substr(start, a.length()).lwr() != a.lwr()) {
                continue;
            }
            # the abbreviation has to be a word of its own: without this, "st." matches the end of
            # "request." and every sentence ending in one of those words stops being a sentence
            if (start > 0 && desc.substr(start - 1, 1) =~ /[[:alnum:]]/) {
                continue;
            }
            return True;
        }
        return False;
    }

    #! Converts a technical name into a display name
    /** Word breaks are taken from dashes, underscores, dots, spaces, and case transitions, and each word is
        capitalized; acronyms are preserved (ex: \c "getUserURL" -> \c "Get User URL", \c "createTPSReports" ->
        \c "Create TPS Reports")

        Words in all upper case are case-folded to an initial capital followed by lower case, so that technical
        names in upper case produce readable display names (ex: \c "PAYMENT-DISPUTE-28" ->
        \c "Payment Dispute 28"); acronyms are excluded from case folding and are recognized as follows, in
        order of precedence:
        - words classified explicitly with the \c extra_acronyms argument
        - words listed in @ref Acronyms (ex: \c "HTTP_SERVER" -> \c "HTTP Server")
        - words no longer than @ref AcronymMaxLength characters that are neither listed in @ref NonAcronyms
          nor in @ref FunctionWords (ex: \c "CUSTOMER_ID" -> \c "Customer ID", but \c "MAX_RETRIES" ->
          \c "Max Retries" and \c "CREATED_AT" -> \c "Created At")

        Each resulting word is then rendered as follows, in order of precedence:
        - a word classified explicitly with the \c extra_acronyms argument keeps the case it was given by
          the rules above
        - a word listed in @ref FunctionWords is written in lower case unless it begins or ends the display
          name (ex: \c "wait_for_active_shards" -> \c "Wait for Active Shards", but \c "created_at" ->
          \c "Created At")
        - a word listed in @ref WordRenderings is written in its canonical form, whatever case it is written
          in in the technical name, so that lower-case, upper-case, and camel-case technical names all
          produce the same display name (ex: \c "api_key", \c "API_KEY", and \c "apiKey" all yield
          \c "API Key", and \c "saas_provider" yields \c "SaaS Provider")
        - any other word has its initial letter capitalized and the rest of the word left unchanged, so that
          acronyms preserved by the case-folding rules above keep their case (ex: \c "TPS", \c "URLs")

        Words in caseless scripts (ex: Chinese, Japanese, Korean, Arabic, Hebrew, Thai, Devanagari) are
        always left unchanged

        Consecutive word separators produce a single word break, and a name consisting only of separators
        has no words and therefore no display name

        @param name the technical name to convert
        @param extra_acronyms optional explicit acronym classification for domain- or language-specific words;
        keys must be in upper case, a @ref True "True" value marks the word as an acronym to be preserved, and
        a @ref False "False" value marks it as an ordinary word to be case-folded

        @return the display name corresponding to the technical name

        @note Case conversions use locale-independent Unicode case mappings, with the exception that the final
        form of the Greek sigma is used at the end of a word; language-specific mappings (ex: the Turkish
        dotless \c i) are not applied

        @note The @ref FunctionWords rule and the English title-case convention it implements are
        English-specific; use the \c extra_acronyms argument to override the classification of any word

        @since DataProvider 3.6 words in upper case are case-folded, and case handling is Unicode-aware in all
        scripts

        @since DataProvider 3.8 words listed in @ref WordRenderings are written in their canonical form
        regardless of the case they are written in in the technical name, function words listed in
        @ref FunctionWords are no longer treated as acronyms and are written in lower case in the middle of a
        display name, and consecutive word separators produce a single word break
    */
    static string getDisplayName(string name, *hash<string, bool> extra_acronyms) {
        # convert dashes and underscores to spaces
        name =~ s/[-_]/ /g;
        # case-fold all upper-case words that are not acronyms; this must be done before word breaks are
        # detected below, as case transitions carry no information in an all upper-case word
        if (name =~ /\p{Lu}/) {
            name = AbstractDataField::foldUpperCaseWords(name, extra_acronyms);
        }
        list<string> words = AbstractDataField::splitWords(name);
        list<string> rv = ();
        for (int i = 0, int e = words.size(); i < e; ++i) {
            rv += AbstractDataField::renderWord(words[i], !i, i == (e - 1), extra_acronyms);
        }
        return rv.join(" ");
    }

    #! Splits a technical name into words on separators and case transitions
    /** @param name the technical name with dashes and underscores already converted to spaces and
        upper-case non-acronym words already case-folded

        @return the list of words in the name; empty words arising from consecutive separators are omitted

        @since DataProvider 3.8
    */
    private static list<string> splitWords(string name) {
        list<string> words = ();
        string word = "";
        for (int i = 0, int e = name.length(); i < e; ++i) {
            string c = name[i];
            # word separators must match those handled in foldUpperCaseWords()
            if (c == "." || c == " ") {
                if (word.val()) {
                    words += word;
                    word = "";
                }
                continue;
            }
            if (word.val() && i > 1 && c =~ /\p{Lu}/ && word =~ /\p{Ll}$/) {
                # an upper-case letter after a lower-case letter starts a new word
                # (ex: "getUser" -> "get" + "User")
                words += word;
                word = c;
                continue;
            }
            if (i > 1 && c =~ /\p{Ll}/ && word =~ /^\p{Lu}{2,}\p{Ll}$/) {
                # a run of upper-case letters followed by two lower-case letters breaks before the last
                # upper-case letter, which begins the following word (ex: "XMLHttp" -> "XML" + "Http");
                # a single trailing lower-case letter is a plural suffix instead (ex: "URLs")
                words += word.substr(0, word.length() - 2);
                word = word.substr(word.length() - 2);
            }
            word += c;
        }
        if (word.val()) {
            words += word;
        }
        return words;
    }

    #! Renders a single word of a display name in its conventional form
    /** @param word the word to render
        @param first @ref True "True" if the word begins the display name
        @param last @ref True "True" if the word ends the display name
        @param extra_acronyms optional explicit acronym classification as documented in @ref getDisplayName()

        @return the word in its canonical rendering if it has one, in lower case if it is a function word in
        the middle of the display name, and otherwise with its initial letter capitalized

        @since DataProvider 3.8
    */
    private static string renderWord(string word, bool first, bool last, *hash<string, bool> extra_acronyms) {
        string key = word.lwr();
        # an explicit classification by the caller takes precedence over all other rules; the word was
        # already preserved or case-folded accordingly when upper-case words were folded
        if (exists extra_acronyms{word.upr()}) {
            return AbstractDataField::capitalizeWord(word);
        }
        # function words are never acronyms, and English title case leaves them in lower case unless they
        # begin or end the display name
        if (FunctionWords{key}) {
            return (first || last) ? AbstractDataField::capitalizeWord(word) : key;
        }
        # a canonical rendering applies regardless of the case the word is written in, so that lower-case
        # and upper-case technical names produce the same display name
        *string canonical = WordRenderings{key};
        if (exists canonical) {
            return canonical;
        }
        return AbstractDataField::capitalizeWord(word);
    }

    #! Capitalizes the initial letter of a word and leaves the rest of the word unchanged
    /** The remainder of the word is left as it is, so that acronyms preserved when upper-case words were
        case-folded keep their case (ex: \c "TPS" and \c "URLs")

        @param word the word to capitalize

        @return the word with its initial letter capitalized

        @since DataProvider 3.8
    */
    private static string capitalizeWord(string word) {
        if (!word.val()) {
            return word;
        }
        return word[0].upr() + word.substr(1);
    }

    #! Case-folds all upper-case words in a technical name that are not acronyms
    /** @param name the technical name with dashes and underscores already converted to spaces
        @param extra_acronyms optional explicit acronym classification as documented in @ref getDisplayName()

        @return the name with all upper-case non-acronym words converted to an initial capital followed by
        lower case; all other characters are returned unchanged

        @since DataProvider 3.6
    */
    private static string foldUpperCaseWords(string name, *hash<string, bool> extra_acronyms) {
        string rv = "";
        string word = "";
        for (int i = 0, int e = name.length(); i < e; ++i) {
            string c = name[i];
            # word separators must match those handled in getDisplayName()
            if (c == " " || c == ".") {
                rv += AbstractDataField::foldUpperCaseWord(word, extra_acronyms) + c;
                word = "";
                continue;
            }
            word += c;
        }
        return rv + AbstractDataField::foldUpperCaseWord(word, extra_acronyms);
    }

    #! Case-folds a single word if it is in all upper case and is not an acronym
    /** @param word the word to case-fold
        @param extra_acronyms optional explicit acronym classification as documented in @ref getDisplayName()

        @return the word converted to an initial capital followed by lower case, if it is an all upper-case
        non-acronym word, otherwise the word unchanged

        @since DataProvider 3.6
    */
    private static string foldUpperCaseWord(string word, *hash<string, bool> extra_acronyms) {
        # words with no upper-case letter (ex: camelCase words, digits, and words in caseless scripts such as
        # Chinese, Japanese, Arabic, and Hebrew) and words with a lower-case letter that carries case
        # information are left unchanged
        if (word !~ /\p{Lu}/ || AbstractDataField::hasCasedLowerCase(word)) {
            return word;
        }
        # an explicit classification by the caller takes precedence over all other rules
        *bool acronym = extra_acronyms{word};
        if (!exists acronym) {
            # short words and well-known acronyms are assumed to be acronyms and are left unchanged
            acronym = (word.length() <= AcronymMaxLength && !NonAcronyms{word}
                && !FunctionWords{word.lwr()}) || Acronyms{word};
        }
        if (acronym) {
            return word;
        }
        # the first upper-case letter keeps its case; all other letters are converted to lower case
        string rv = "";
        bool first = True;
        for (int i = 0, int e = word.length(); i < e; ++i) {
            string c = word[i];
            if (first && c =~ /\p{Lu}/) {
                first = False;
                rv += c;
                continue;
            }
            rv += c.lwr();
        }
        # Unicode case mappings always produce the medial form of the Greek sigma; use the final form at the
        # end of the word
        rv =~ s/σ$/ς/;
        return rv;
    }

    #! Returns @ref True "True" if a word contains a lower-case letter that carries case information
    /** A lower-case letter says something about the case a word was written in only if it has a
        single-character upper-case form.  Letters that upper-case to a sequence (ex: the German sharp s,
        \c "ß" -> \c "SS", and the typographic ligatures) or that have no upper-case form at all are the only
        form they have, so a word in upper case that contains one (ex: \c "STRAßE", produced by upper-casing
        \c "Straße") is still a word in upper case and must be case-folded like any other

        @param word the word to inspect

        @return @ref True "True" if the word contains a lower-case letter with a single-character upper-case
        form, @ref False "False" otherwise

        @since DataProvider 3.8
    */
    private static bool hasCasedLowerCase(string word) {
        # almost all words in upper case have no lower-case letter at all; check that first
        if (word !~ /\p{Ll}/) {
            return False;
        }
        for (int i = 0, int e = word.length(); i < e; ++i) {
            string c = word[i];
            if (c !~ /\p{Ll}/) {
                continue;
            }
            string upper = c.upr();
            if (upper.length() == c.length() && upper != c) {
                return True;
            }
        }
        return False;
    }

    #! Converts a display name into a technical name
    /** @param display_name the display name to convert

        @return the technical name corresponding to the display name

        @since DataProvider 3.6 display names in all scripts are converted to lower case; previously only
        Latin-script display names were converted
    */
    static string getNameFromDisplayName(string display_name) {
        # convert the name to all lower case
        string name = display_name.lwr();
        # convert spaces to dashes
        name =~ s/\s+/-/g;
        # remove all punctuation and special characters
        name =~ s/[!@#\$%\^&*\(\)_=\+\{\}\[\]|\\;':",\.\/<>\?`~]//g;
        return name;
    }

    #! returns the name of the field
    abstract string getName();

    #! returns the description, if any
    abstract *string getDescription();

    #! returns the type of the field
    abstract AbstractDataProviderType getType();
}
}
