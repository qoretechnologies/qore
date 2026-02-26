# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearARefDataTypes.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains reference data types for embedded objects in Linear responses
public namespace LinearDataProvider {

#! State reference type (embedded in issue responses)
public class LinearStateRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The state ID",
                "desc": "The unique identifier of the workflow state",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The state name",
                "desc": "The name of the workflow state",
                "example_value": "In Progress",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The state type",
                "desc": "The type of the state (backlog, unstarted, started, completed, canceled)",
                "example_value": "started",
            },
        };
    }

    constructor() : HashDataType("StateRef") {
        addQoreFields(Fields);
    }
}

#! User reference type (embedded in issue/comment responses)
public class LinearUserRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the user",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The user name",
                "desc": "The display name of the user",
                "example_value": "John Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The user email",
                "desc": "The email address of the user",
                "example_value": "john@example.com",
            },
        };
    }

    constructor() : HashDataType("UserRef") {
        addQoreFields(Fields);
    }
}

#! Team reference type (embedded in issue/cycle/label responses)
public class LinearTeamRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The team ID",
                "desc": "The unique identifier of the team",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The team name",
                "desc": "The name of the team",
                "example_value": "Engineering",
            },
            "key": {
                "type": StringOrNothingType,
                "display_name": "Key",
                "short_desc": "The team key",
                "desc": "The unique key of the team (used in issue identifiers)",
                "example_value": "ENG",
            },
        };
    }

    constructor() : HashDataType("TeamRef") {
        addQoreFields(Fields);
    }
}

#! Project reference type (embedded in issue responses)
public class LinearProjectRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The project ID",
                "desc": "The unique identifier of the project",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The project name",
                "desc": "The name of the project",
                "example_value": "Q1 Launch",
            },
        };
    }

    constructor() : HashDataType("ProjectRef") {
        addQoreFields(Fields);
    }
}

#! Cycle reference type (embedded in issue responses)
public class LinearCycleRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The cycle ID",
                "desc": "The unique identifier of the cycle",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The cycle name",
                "desc": "The name of the cycle",
                "example_value": "Sprint 23",
            },
            "number": {
                "type": IntOrNothingType,
                "display_name": "Number",
                "short_desc": "The cycle number",
                "desc": "The number of the cycle within the team",
                "example_value": 23,
            },
        };
    }

    constructor() : HashDataType("CycleRef") {
        addQoreFields(Fields);
    }
}

#! Parent issue reference type (embedded in issue responses)
public class LinearParentIssueRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The issue ID",
                "desc": "The unique identifier of the parent issue",
            },
            "identifier": {
                "type": StringType,
                "display_name": "Identifier",
                "short_desc": "The issue identifier",
                "desc": "The human-readable identifier of the parent issue (e.g., ENG-123)",
                "example_value": "ENG-123",
            },
        };
    }

    constructor() : HashDataType("ParentIssueRef") {
        addQoreFields(Fields);
    }
}

#! Lead reference type (embedded in project responses) - same as user but without email
public class LinearLeadRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the project lead",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The user name",
                "desc": "The display name of the project lead",
                "example_value": "Jane Smith",
            },
        };
    }

    constructor() : HashDataType("LeadRef") {
        addQoreFields(Fields);
    }
}

#! Issue reference type (embedded in comment responses)
public class LinearIssueRefDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The issue ID",
                "desc": "The unique identifier of the issue",
            },
            "identifier": {
                "type": StringType,
                "display_name": "Identifier",
                "short_desc": "The issue identifier",
                "desc": "The human-readable identifier of the issue (e.g., ENG-123)",
                "example_value": "ENG-123",
            },
        };
    }

    constructor() : HashDataType("IssueRef") {
        addQoreFields(Fields);
    }
}

} # namespace LinearDataProvider
