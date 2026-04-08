# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetFolderDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Data type for Smartsheet folder
public class SmartSheetFolderDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Folder ID",
                "type": SoftIntType,
                "short_desc": "Unique folder identifier",
                "desc": "The unique identifier for the folder",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringType,
                "short_desc": "Folder name",
                "desc": "The name of the folder",
            },
            "permalink": {
                "display_name": "Permalink",
                "type": SoftStringOrNothingType,
                "short_desc": "Permanent URL",
                "desc": "URL that represents a direct link to the folder in Smartsheet",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for creating a folder
public class SmartSheetCreateFolderRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Folder Name",
                "type": SoftStringType,
                "short_desc": "Name for the new folder",
                "desc": "The name for the new folder",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for updating a folder
public class SmartSheetUpdateFolderRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "folderId": {
                "display_name": "Folder ID",
                "type": SoftIntType,
                "short_desc": "ID of the folder to update",
                "desc": "The unique identifier of the folder to update",
                "required": True,
                "ref_data": "folders",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringOrNothingType,
                "short_desc": "New folder name",
                "desc": "The new name for the folder",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for deleting a folder
public class SmartSheetDeleteFolderRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "folderId": {
                "display_name": "Folder ID",
                "type": SoftIntType,
                "short_desc": "ID of the folder to delete",
                "desc": "The unique identifier of the folder to delete",
                "required": True,
                "ref_data": "folders",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
