# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicSkillsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Beta header for Skills API
public const SkillsBetaHeader = {"anthropic-beta": "skills-2025-10-02"};

#! Skill response data type
public class AnthropicSkillResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Skill ID",
                "short_desc": "Unique skill identifier",
                "desc": "Unique identifier for the skill",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Object Type",
                "short_desc": "Object type",
                "desc": "Object type, always `skill`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "display_title": {
                "display_name": "Display Title",
                "short_desc": "Human-readable skill title",
                "desc": "The human-readable title of the skill",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "source": {
                "display_name": "Source",
                "short_desc": "Whether the skill is custom or built-in",
                "desc": "Whether the skill is custom or provided by Anthropic",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "custom", "display_name": "Custom"},
                    <AllowedValueInfo>{"value": "anthropic", "display_name": "Anthropic"},
                ),
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "When the skill was created",
                "desc": "RFC 3339 datetime when the skill was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updated_at": {
                "display_name": "Updated At",
                "short_desc": "When the skill was last updated",
                "desc": "RFC 3339 datetime when the skill was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() { addQoreFields(Fields); setDefaultOtherFieldType(AbstractDataProviderTypeMap."any"); }
    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields); setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const AnthropicSkillResponseDataType = new AnthropicSkillResponseDataType();

#! Skill ID request type
public class AnthropicSkillIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "skill_id": {
                "display_name": "Skill ID",
                "short_desc": "The ID of the skill",
                "desc": "The unique identifier for the skill",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicSkillIdRequestDataType = new AnthropicSkillIdRequestDataType();

#! Create/update skill request type
public class AnthropicSkillRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "display_title": {
                "display_name": "Display Title",
                "short_desc": "Human-readable skill title",
                "desc": "A human-readable title for the skill",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "My Custom Skill",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Description of the skill",
                "desc": "A description of what the skill does and when it should be used",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "Instructions for the skill",
                "desc": "Detailed instructions for the skill's behavior",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); setDefaultOtherFieldType(AbstractDataProviderTypeMap."any"); }
}

public const AnthropicSkillRequestDataType = new AnthropicSkillRequestDataType();

#! Update skill request type (includes skill_id)
public class AnthropicUpdateSkillRequestDataType inherits HashDataType {
    public {
        const Fields = AnthropicSkillIdRequestDataType::Fields + AnthropicSkillRequestDataType::Fields;
    }
    constructor() { addQoreFields(Fields); setDefaultOtherFieldType(AbstractDataProviderTypeMap."any"); }
}

public const AnthropicUpdateSkillRequestDataType = new AnthropicUpdateSkillRequestDataType();

#! Container provider for Skills API
public class AnthropicSkillsDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "skills",
            "type": "AnthropicSkillsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    private {
        const ChildMap = {
            "create": Class::forName("AnthropicDataProvider::AnthropicSkillCreateDataProvider"),
            "list": Class::forName("AnthropicDataProvider::AnthropicSkillListDataProvider"),
            "get": Class::forName("AnthropicDataProvider::AnthropicSkillGetDataProvider"),
            "update": Class::forName("AnthropicDataProvider::AnthropicSkillUpdateDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) { return; }
        return cls.newObject(rest);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Create a skill
public class AnthropicSkillCreateDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "type": "AnthropicSkillCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicSkillRequestDataType;
        const ResponseType = AnthropicSkillResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "skills", req, SkillsBetaHeader).body;
    }
}

#! List skills
public class AnthropicSkillListDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "AnthropicSkillListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = new ListDataType("AnthropicSkillList", AnthropicSkillResponseDataType, True);
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "skills", NOTHING, SkillsBetaHeader).body."data";
    }
}

#! Get a specific skill
public class AnthropicSkillGetDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "AnthropicSkillGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicSkillIdRequestDataType;
        const ResponseType = AnthropicSkillResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "skills/" + req.skill_id, NOTHING, SkillsBetaHeader).body;
    }
}

#! Update a skill
public class AnthropicSkillUpdateDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "type": "AnthropicSkillUpdateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicUpdateSkillRequestDataType;
        const ResponseType = AnthropicSkillResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string skill_id = remove req.skill_id;
        return doRestCommand("PUT", "skills/" + skill_id, req, SkillsBetaHeader).body;
    }
}
}
