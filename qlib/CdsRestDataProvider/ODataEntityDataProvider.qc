# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ODataEntityDataProvider class definition

/** ODataEntityDataProvider.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! Unified OData entity data provider class
/** Provides full CRUD operations for both Dynamics 365 CDS and Business Central entities.
    Behavior is driven by the API profile and optional company context.
*/
public class ODataEntityDataProvider inherits AbstractODataEntityDataProvider {
    public {
        #! Company ID (only set for Business Central)
        *string companyId;

        #! Metadata description from $metadata (only for CDS)
        *hash<auto> meta;

        #! Static provider info for BC (no groupby support)
        const BcProviderInfo = <DataProviderInfo>{
            "type": "ODataEntityDataProvider",
            "supports_children": False,
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_update": True,
            "supports_upsert": True,
            "supports_delete": True,
            "supports_create": True,
            "search_options": ODataSearchOptions,
        };

        #! Static provider info for CDS (with groupby support)
        const CdsProviderInfo = CdsRestDataProvider::ProviderInfo - "constructor_options" + <DataProviderInfo>{
            "search_options": ODataSearchOptions + {
                "groupby": <DataProviderOptionInfo>{
                    "display_name": "Group By",
                    "short_desc": "Field(s) to group results by",
                    "type": new QoreListDataType(new Type("softlist<auto>")),
                    "desc": "Field(s) to group results by in case there are aggregate expressions in the query; "
                        "`groupby` argument for the query expression",
                },
            },
        };
    }

    #! Creates the object for Dynamics 365 CDS (no company context)
    /** @param rest the REST client connection
        @param name the entity name
        @param meta metadata from $metadata endpoint
    */
    constructor(CdsRestClient::CdsRestClient rest, string name, hash<auto> meta)
            : AbstractODataEntityDataProvider(rest, name) {
        self.meta = meta;
    }

    #! Creates the object for Business Central (with company context)
    /** @param rest the REST client connection
        @param companyId the company ID
        @param name the entity name
    */
    constructor(CdsRestClient::CdsRestClient rest, string companyId, string name)
            : AbstractODataEntityDataProvider(rest, name) {
        self.companyId = companyId;
    }

    #! Returns the URI for an entity
    private string getEntityUri(*string id) {
        if (companyId) {
            # Business Central: company-scoped URI
            if (id) {
                return sprintf("companies(%s)/%s(%s)", companyId, name, id);
            }
            return sprintf("companies(%s)/%s", companyId, name);
        } else {
            # Dynamics CDS: direct entity URI
            if (id) {
                return sprintf("%s(%s)", record_info.entity_name, id);
            }
            return record_info.entity_name;
        }
    }

    #! Creates a record iterator
    private AbstractDataProviderRecordIterator createRecordIterator(*hash<auto> where_cond, *hash<auto> search_options) {
        string entity_path = companyId
            ? sprintf("companies(%s)/%s", companyId, name)
            : record_info.entity_name;
        return new ODataRecordIterator(rest, entity_path, record_info, where_cond, search_options);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv;
        if (companyId) {
            # Business Central
            rv = BcProviderInfo;
        } else {
            # Dynamics CDS
            rv = CdsProviderInfo;
            if (meta.Key.PropertyRef."^attributes^".Name) {
                rv += {
                    "has_record": True,
                    "supports_read": True,
                    "supports_native_search": True,
                    "supports_update": True,
                    "supports_upsert": True,
                    "supports_delete": True,
                    "supports_create": True,
                };
            }
        }
        # Add expression support for both platforms
        rv += {
            "supports_search_expressions": True,
            "expressions": (map {$1.key: $1.value.exp}, ODataExpressions.pairIterator()),
        };
        return rv;
    }

    #! Check if error is a duplicate record error
    private bool isDuplicateError(hash<ExceptionInfo> ex, hash<auto> info) {
        if (companyId) {
            # Business Central
            return ex.err == "BadRequest" || ex.err =~ /duplicate/i;
        }
        # Dynamics CDS
        return ex.arg.decoded_body.error."code" == "0x80040237";
    }

    #! Process create response to extract new record ID (CDS only)
    private *hash<auto> processCreateResponse(hash<auto> rec, hash<auto> resp, hash<auto> info) {
        if (companyId) {
            # Business Central - ID is in response body
            return resp.body;
        }
        # Dynamics CDS - extract ID from OData-EntityId header
        *string new_key = (info."response-headers"."odata-entityid" =~ x/\(([-0-9a-f]+)\)$/)[0];
        if (!new_key) {
            throw "CDS_ENTITY-CREATE-ERROR", sprintf("failed to find new entity ID in OData-EntityId header: %y",
                info."response-headers"."odata-entityid");
        }
        return rec + {record_info.key: new_key};
    }

    #! Get headers for upsert operation
    private hash<auto> getUpsertHeaders(string id) {
        if (companyId) {
            # Business Central - needs ETag
            *string etag = getRecordEtag(id);
            if (etag) {
                return {"If-Match": etag};
            }
        }
        return {};
    }

    #! Get headers for update operation
    private hash<auto> getUpdateHeaders(string id) {
        if (companyId) {
            # Business Central - needs ETag
            *string etag = getRecordEtag(id);
            if (etag) {
                return {"If-Match": etag};
            }
        }
        return {};
    }

    #! Check if upsert resulted in insert or update
    private string checkUpsertResult(hash<auto> resp, date now) {
        if (companyId) {
            # Business Central - check systemCreatedAt field
            if (resp.body.systemCreatedAt) {
                date created = date(resp.body.systemCreatedAt);
                if (created < now) {
                    return UpsertResultUpdated;
                }
            }
            return UpsertResultUpdated;
        }
        # Dynamics CDS - check createdon field
        if ((*string createdon = resp.body.createdon) && date(createdon) < now) {
            return UpsertResultUpdated;
        }
        return UpsertResultInserted;
    }

    #! Gets the ETag for a record (BC only)
    private *string getRecordEtag(string id) {
        string url = getEntityUri(id);
        hash<auto> info;
        try {
            hash<auto> resp = rest.get(url, NOTHING, \info);
            return resp.body."@odata.etag";
        } catch (hash<ExceptionInfo> ex) {
            # If record doesn't exist, return nothing
            if (info."response-code" == 404) {
                return NOTHING;
            }
            rethrow;
        }
    }

    #! Initializes record info
    private initRecordInfo() {
        if (companyId) {
            initRecordInfoFromSample();
        } else {
            initRecordInfoFromMetadata();
        }
    }

    #! Initializes record info from CDS $metadata
    private initRecordInfoFromMetadata() {
        *string key = meta.Key.PropertyRef."^attributes^".Name;
        if (!key) {
            throw "CDS-ENTITY-ERROR", sprintf("entity %y has no key property (%y)", name, meta.Key);
        }
        record_info.key = key;
        # get entity set name
        setEntitySetName();
        foreach hash<auto> field in (meta.Property) {
            AbstractDataProviderType type;
            hash<auto> opts += {
                "qore.no_null": True,
            };
            hash<auto> attr = field."^attributes^";
            string cdstype;
            switch (attr.Type) {
                case "Edm.String":
                    type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                    break;

                case "Edm.Guid":
                    type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                    cdstype = "guid";
                    break;

                case "Edm.Int32":
                case "Edm.Int64":
                    type = AbstractDataProviderType::get(SoftIntOrNothingType, opts);
                    break;

                case "Edm.Double":
                case "Edm.Decimal":
                    type = AbstractDataProviderType::get(SoftFloatOrNothingType, opts);
                    break;

                case "Edm.Boolean":
                    type = AbstractDataProviderType::get(SoftBoolOrNothingType, opts);
                    break;

                case "Edm.Binary":
                    type = AbstractDataProviderType::get(SoftBinaryOrNothingType, opts);
                    break;

                case "Edm.DateTimeOffset":
                    record_info.date_time_fields{attr.Name} = True;
                    type = AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                    break;

                default:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    break;
            }
            # get description
            *string desc;
            foreach hash<auto> i in (field.Annotation) {
                if (i."^attributes^".Term == "Org.OData.Core.V1.Description") {
                    desc = i."^attributes^".String;
                    break;
                }
            }
            record_info.field_types{attr.Name} = cdstype ?? type.getBaseTypeName();
            record_info.record_type{attr.Name} = new QoreDataField(attr.Name, desc, type);
        }
    }

    #! Determine the entity set name for the entity (CDS only)
    private setEntitySetName() {
        hash<auto> info;
        try {
            string uri = sprintf("EntityDefinitions(LogicalName='%s')?$select=EntitySetName", name);
            hash<auto> resp = rest.get(uri, NOTHING, \info);
            *string entity_set_name;
            if (!(entity_set_name = resp.body.EntitySetName)) {
                throw "CDS-ENTITY-SET-ERROR", sprintf("missing entity set name for %y in %y (query %y)", name, resp.body,
                    uri);
            }
            record_info.entity_name = entity_set_name;
            return;
        } catch (hash<ExceptionInfo> ex) {
            if (info."response-headers".status_code != 403) {
                rethrow ex.err, ex.desc, info{"request-uri", "response-uri"};
            }
        }
        # if we get a 403 Unauthorized response, then we determine the entity set name with a heuristic
        record_info.entity_name = name;
        if (name[-1] == "y") {
            splice record_info.entity_name, -1, 1, "ies";
        } else {
            record_info.entity_name += "s";
        }
    }

    #! Initializes record info by inferring schema from sample record (BC)
    private initRecordInfoFromSample() {
        # Business Central uses "id" as the primary key for most entities
        record_info.key = "id";
        record_info.entity_name = name;

        # Try to get schema from a sample record
        hash<auto> info;
        try {
            # Try to get a single record to infer schema
            string uri = sprintf("companies(%s)/%s?$top=1", companyId, name);
            hash<auto> resp = rest.get(uri, NOTHING, \info);
            if (resp.body.value && resp.body.value.size() > 0) {
                inferRecordTypeFromSample(resp.body.value[0]);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            # If we can't get schema, use a basic type
        }

        # Fallback: use generic auto type
        record_info.record_type = {
            "id": new QoreDataField("id", "Record ID", AbstractDataProviderType::get(SoftStringOrNothingType)),
        };
        record_info.field_types.id = "guid";
    }

    #! Infers record type from a sample record
    private inferRecordTypeFromSample(hash<auto> sample) {
        foreach hash<auto> i in (sample.pairIterator()) {
            # Skip OData metadata fields
            if (i.key =~ /^@/) {
                continue;
            }

            AbstractDataProviderType type;
            hash<auto> opts = {"qore.no_null": True};
            string fieldType;

            switch (i.value.typeCode()) {
                case NT_STRING:
                    # Check if it looks like a GUID
                    if (i.value =~ /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i) {
                        type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                        fieldType = "guid";
                    } else if (i.value =~ /^\d{4}-\d{2}-\d{2}T/) {
                        # ISO date/time format
                        type = AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                        fieldType = "datetime";
                        record_info.date_time_fields{i.key} = True;
                    } else {
                        type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                        fieldType = "string";
                    }
                    break;

                case NT_INT:
                    type = AbstractDataProviderType::get(SoftIntOrNothingType, opts);
                    fieldType = "int";
                    break;

                case NT_FLOAT:
                    type = AbstractDataProviderType::get(SoftFloatOrNothingType, opts);
                    fieldType = "double";
                    break;

                case NT_NUMBER:
                    type = AbstractDataProviderType::get(SoftFloatOrNothingType, opts);
                    fieldType = "decimal";
                    break;

                case NT_BOOLEAN:
                    type = AbstractDataProviderType::get(SoftBoolOrNothingType, opts);
                    fieldType = "bool";
                    break;

                case NT_DATE:
                    type = AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                    fieldType = "datetime";
                    record_info.date_time_fields{i.key} = True;
                    break;

                case NT_NULL:
                case NT_NOTHING:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    fieldType = "auto";
                    break;

                default:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    fieldType = "auto";
                    break;
            }

            record_info.field_types{i.key} = fieldType;
            record_info.record_type{i.key} = new QoreDataField(i.key, NOTHING, type);
        }
    }
}
}
