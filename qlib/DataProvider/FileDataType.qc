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
        #! File fields
        const Fields = {
            "name": new QoreDataField({
                "name": "name",
                "display_name": "Filename",
                "short_desc": "The filename",
                "desc": "The filename",
                "type": AbstractDataProviderTypeMap."string",
            }),
            "content": new QoreDataField({
                "name": "content",
                "display_name": "File Data",
                "short_desc": "Base64-encoded file data",
                "desc": "Base64-encoded file data",
                "type": AbstractDataProviderTypeMap."string",
            }),
            "size": new QoreDataField({
                "name": "size",
                "display_name": "File Size",
                "short_desc": "The size of the file in bytes",
                "desc": "The size of the file in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            }),
            "mime_type": new QoreDataField({
                "name": "mime_type",
                "display_name": "MIME Type",
                "short_desc": "The MIME type string for the file",
                "desc": "The MIME type string for the file",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": ExampleMimeTypeList,
                "allowed_values_creatable": True,
                "default_value": "auto",
            }),
        };

        #! Example MIME type list
        const ExampleMimeTypeList = (
            <AllowedValueInfo>{
                "display_name": "Autodetect",
                "short_desc": "Autodetect from the file name; valid only on input",
                "desc": "Autodetect from the file name; valid only on input",
                "value": "auto",
            },
            <AllowedValueInfo>{
                "display_name": "text/plain",
                "short_desc": "Plain text",
                "desc": "Plain text",
                "value": "text/plain",
            },
            <AllowedValueInfo>{
                "display_name": "text/html",
                "short_desc": "HTML text",
                "desc": "HTML text",
                "value": "text/html",
            },
            <AllowedValueInfo>{
                "display_name": "application/pdf",
                "short_desc": "PDF file",
                "desc": "PDF file",
                "value": "application/pdf",
            },
            <AllowedValueInfo>{
                "display_name": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                "short_desc": "MS Word docx document",
                "desc": "MS Word `docx` document",
                "value": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            },
            <AllowedValueInfo>{
                "display_name": "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                "short_desc": "MS PowerPoint pptx document",
                "desc": "MS PowerPoint `pptx` document",
                "value": "application/vnd.openxmlformats-officedocument.presentationml.presentation",
            },
            <AllowedValueInfo>{
                "display_name": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                "short_desc": "MS Excel xlsx document",
                "desc": "MS Excel `xlsx` document",
                "value": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
            },
        );

        #! File option types
        const SupportedOptions = {
            "accept": <DataProviderTypeOptionInfo>{
                "type": "softlist<string>",
                "desc": "Set accepted file MIME types",
                "publish": True,
            },
            "autofocus": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Set to true to focus the root element on render",
                "publish": True,
            },
            #"children": <DataProviderTypeOptionInfo>{
            #    "type": "code"",
            #    "desc": "Render function that exposes the dropzone state and prop getter fns",
            #    "publish": True,
            #},
            "disabled": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Enable/disable the dropzone",
                "publish": True,
            },
            #"getFilesFromEvent": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Use this to provide a custom file aggregator",
            #    "publish": True,
            #},
            "maxFiles": <DataProviderTypeOptionInfo>{
                "type": "int",
                "desc": "Maximum accepted number of files The default value is 0 which means there is no limitation "
                    "to how many files are accepted",
                "publish": True,
            },
            "maxSize": <DataProviderTypeOptionInfo>{
                "type": "int",
                "desc": "Maximum file size (in bytes)",
                "publish": True,
            },
            "minSize": <DataProviderTypeOptionInfo>{
                "type": "int",
                "desc": "Minimum file size (in bytes)",
                "publish": True,
            },
            "multiple": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Allow drag 'n' drop (or selection from the file dialog) of multiple files",
                "publish": True,
            },
            "noClick": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, disables click to open the native file selection dialog",
                "publish": True,
            },
            "noDrag": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, disables drag 'n' drop",
                "publish": True,
            },
            "noDragEventsBubbling": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, stops drag event propagation to parents",
                "publish": True,
            },
            "noKeyboard": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If true, disables SPACE/ENTER to open the native file selection dialog. Note that it also "
                    "stops tracking the focus state",
                "publish": True,
            },
            #"onDragEnter": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the dragenter event occurs",
            #    "publish": True,
            #},
            #"onDragLeave": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the dragleave event occurs",
            #    "publish": True,
            #},
            #"onDragOver": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the dragover event occurs",
            #    "publish": True,
            #},
            #"onDrop": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the drop event occurs",
            #    "publish": True,
            #},
            #"onDropAccepted": Code<DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the drop event occurs",
            #    "publish": True,
            #}Type,
            #"onDropRejected": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when the drop event occurs",
            #    "publish": True,
            #},
            #"onError": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when there's some error from any of the promises",
            #    "publish": True,
            #},
            #"onFileDialogCancel": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when closing the file dialog with no selection",
            #    "publish": True,
            #},
            #"onFileDialogOpen": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Cb for when opening the file dialog",
            #    "publish": True,
            #},
            "preventDropOnDocument": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "If false, allow dropped items to take over the current browser window",
                "publish": True,
            },
            "useFsAccessApi": <DataProviderTypeOptionInfo>{
                "type": "bool",
                "desc": "Set to true to use the "
                    "https://developer.mozilla.org/en-US/docs/Web/API/File_System_Access_API to open the file picker "
                    "instead of using an <input type=\"file\"> click event",
                "publish": True,
            },
            #"validator": <DataProviderTypeOptionInfo>{
            #    "type": "code",
            #    "desc": "Custom validation function",
            #    "publish": True,
            #},
        };
    }

    #! creates the field from the argument
    constructor(*hash<auto> options, *hash<auto> tags, *string path)
            : AbstractDataProviderType(options, {
                "qore.external_name": "file"
            } + tags, path) {
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
        return AutoHashType;
    }

    #! Returns the subtype (for lists or hashes) if there is only one
    *AbstractDataProviderType getElementType() {
        # intentionally empty
    }

    #! Returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        return Fields;
    }

    #! Returns a hash of types accepted by this type; keys are type names
    hash<string, bool> getAcceptTypeHash(*bool simple) {
        return {
            Type::Hash: True,
        };
    }

    #! Returns a hash of types returned by this type; keys are type names
    hash<string, bool> getReturnTypeHash(*bool simple) {
        return {
            Type::Hash: True,
        };
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        return staticAcceptsValue(self, value);
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    static auto staticAcceptsValue(AbstractDataProviderType type, auto value) {
        if (value.typeCode() != NT_HASH) {
            throw "RUNTIME-TYPE-ERROR", sprintf("Data provider type %y expects a hash on input; got type %y instead",
                type.getName(), value.fullType());
        }
        # process raw data on input
        if (value.hasKey("content")) {
            if (value.content.typeCode() != NT_STRING) {
                throw "RUNTIME-TYPE-ERROR", sprintf("Data provider type %y input \"content\" field has type %y; "
                    "expecting \"string\"", type.getName(), value.content.fullType());
            }
            (*string mime_type, *string base64) = (remove value.content =~ x/^data:([^;]+);base64,(.*)$/);
            if (mime_type.val() && base64.val()) {
                bool text;
                if (mime_type.val()) {
                    value.mime_type = mime_type;
                    text = is_mime_text(mime_type);
                }
                # parse base64 data and check data size
                data file_data = text ? parse_base64_string_to_string(base64) : parse_base64_string(base64);
                value.content = base64 ?? "";
                int size = file_data.size();
                if (value.hasKey("size")) {
                    checkSize(type, value.size, file_data.size());
                }
            } else if (!value.hasKey("size")) {
                value.size = value.content.size();
            } else {
                checkSize(type, value.size, value.content.size());
            }
        }
        *hash<auto> value_copy = value;
        # check fields if any are defined
        foreach AbstractDataField field in (Fields.iterator()) {
            string key = field.getName();
            auto val = remove value_copy{key};
            if (!exists val) {
                if (exists (auto dv = field.getDefaultValue())) {
                    value{key} = dv;
                    continue;
                }
                if (!field.isMandatory()) {
                    continue;
                }
            }
            try {
                value{key} = field.acceptsValue(val);
            } catch (hash<ExceptionInfo> ex) {
                rethrow "RUNTIME-TYPE-ERROR", sprintf("Error in type %y field %y: %s: %s", type.getName(), key,
                    ex.err, ex.desc);
            }
        }
        if (value_copy) {
            throw "RUNTIME-TYPE-ERROR", sprintf("Data provider type %s input has unknown fields: %y", type.getName(),
                keys value_copy);
        }
        if (value.mime_type == "auto" && value.name.val()) {
            value.mime_type = get_mime_type_from_ext(value.name);
        }
        return value;
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

    #! Verify that the declared size matches the actual data size
    static private checkSize(AbstractDataProviderType type, auto vsize, int dsize) {
        if (vsize.typeCode() != NT_INT) {
            throw "RUNTIME-TYPE-ERROR", sprintf("Data provider type %y input \"size\" field has type %y; "
                "expecting \"int\"", type.getName(), vsize.fullType());
        }
        if (dsize != vsize) {
            throw "RUNTIME-TYPE-ERROR", sprintf("Data provider type %y input \"size\" field indicates "
                "%d byte%s of file data; the actual file data is %d byte%s", type.getName(), vsize,
                    vsize == 1 ? "" : "s", dsize, dsize == 1 ? "" : "s");
        }
    }
}
}
