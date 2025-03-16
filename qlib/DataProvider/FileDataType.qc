# -*- mode: qore; indent-tabs-mode: nil -*-

/*  FileDataType.qc Copyright (C) 2025 Qore Technologies s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! A type that indicates a file
public const FileDataType = new FileDataType();

#! A type that indicates a file
public class FileDataType inherits AbstractDataProviderType {
    public {
        #! File option types
        const SupportedOptions = {
            "accept": <DataProviderTypeOptionInfo>{
                "type": "list<string>",
                "desc": "Set accepted file MIME types",
            },
            "autofocus": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Set to true to focus the root element on render",
            },
            #"children": <DataProviderTypeOptionInfo>{
            #    "type": "code"",
            #    "desc": "Render function that exposes the dropzone state and prop getter fns",
            #},
            "disabled": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Enable/disable the dropzone",
            },
            #"getFilesFromEvent": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Use this to provide a custom file aggregator",
            #},
            "maxFiles": <DataProviderTypeOptionInfo>{
                "type": "int",
                "desc": "Maximum accepted number of files The default value is 0 which means there is no limitation "
                    "to how many files are accepted",
            },
            "maxSize": <DataProviderTypeOptionInfo>{
                "type": "int",
                "desc": "Maximum file size (in bytes)",
            },
            "minSize": <DataProviderTypeOptionInfo>{
                "type": "int",
                "desc": "Minimum file size (in bytes)",
            },
            "multiple": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Allow drag 'n' drop (or selection from the file dialog) of multiple files",
            },
            "noClick": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, disables click to open the native file selection dialog",
            },
            "noDrag": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, disables drag 'n' drop",
            },
            "noDragEventsBubbling": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, stops drag event propagation to parents",
            },
            "noKeyboard": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, disables SPACE/ENTER to open the native file selection dialog. Note that it also "
                    "stops tracking the focus state",
            },
            #"onDragEnter": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the dragenter event occurs",
            #},
            #"onDragLeave": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the dragleave event occurs",
            #},
            #"onDragOver": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the dragover event occurs",
            #},
            #"onDrop": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the drop event occurs",
            #},
            #"onDropAccepted": Code<DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the drop event occurs",
            #}Type,
            #"onDropRejected": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the drop event occurs",
            #},
            #"onError": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when there's some error from any of the promises",
            #},
            #"onFileDialogCancel": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when closing the file dialog with no selection",
            #},
            #"onFileDialogOpen": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when opening the file dialog",
            #},
            "preventDropOnDocument": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If false, allow dropped items to take over the current browser window",
            },
            "useFsAccessApi": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Set to true to use the "
                    "https://developer.mozilla.org/en-US/docs/Web/API/File_System_Access_API to open the file picker "
                    "instead of using an <input type=\"file\"> click event",
            },
            #"validator": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Custom validation function",
            #},
        };
    }

    #! creates the field from the argument
    constructor(*hash<auto> options, *hash<auto> tags, *string path)
            : AbstractDataProviderType(options, tags, path) {
    }

    #! Returns the type name
    string getName() {
        return "file";
    }

    #! Returns the type's display name
    /** @return the type's display name

        @since DataProvider 3.0
    */
    string getDisplayName() {
        return "File";
    }

    #! Returns the type's short description
    /** @return the plain-text short description for the type

        @since DataProvider 3.0
    */
    string getShortDescription() {
        return "Indicates a file";
    }

    #! Returns the description
    string getDesc() {
        return "Indicates a file";
    }

    #! Returns the base type for the type, if any
    *Type getValueType() {
        return DataType;
    }

    #! Returns the subtype (for lists or hashes) if there is only one
    *AbstractDataProviderType getElementType() {
        # intentionally empty
    }

    #! Returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        # intentionally empty
    }

    #! Returns a hash of types accepted by this type; keys are type names
    hash<string, bool> getAcceptTypeHash(*bool simple) {
        return {
            Type::String: True,
            Type::Binary: True,
        };
    }

    #! Returns a hash of types returned by this type; keys are type names
    hash<string, bool> getReturnTypeHash(*bool simple) {
        return {
            Type::String: True,
            Type::Binary: True,
        };
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        return DataType.acceptsValue(value);
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable "or nothing" type

        @return an "or nothing" type equivalent to the current type
    */
    AbstractDataProviderType getOrNothingType() {
        return FileOrNothingDataType;
    }

    #! returns supported options
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return SupportedOptions;
    }
}
}
