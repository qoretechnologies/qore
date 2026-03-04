# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoElementDataTypes.qc PDF.co element data types for structured list fields

/*  PdfCoElementDataTypes.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Element type for text annotations in add-text-images
public class PdfCoTextAnnotationDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "type": StringType,
                "display_name": "Text",
                "short_desc": "Text to add",
                "desc": "Text to add to the PDF",
                "required": True,
            },
            "x": {
                "type": SoftFloatType,
                "display_name": "X Position",
                "short_desc": "Horizontal position in points",
                "desc": "Horizontal position in points from the left edge",
                "required": True,
            },
            "y": {
                "type": SoftFloatType,
                "display_name": "Y Position",
                "short_desc": "Vertical position in points",
                "desc": "Vertical position in points from the top edge",
                "required": True,
            },
            "width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Text box width in points",
                "desc": "Text box width in points (enables text wrapping)",
            },
            "height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Text box height in points",
                "desc": "Text box height in points",
            },
            "size": {
                "type": SoftIntOrNothingType,
                "display_name": "Font Size",
                "short_desc": "Font size in points",
                "desc": "Font size in points (default: `12`)",
            },
            "fontName": {
                "type": StringOrNothingType,
                "display_name": "Font Name",
                "short_desc": "Font name",
                "desc": "Font name (default: `Arial`)",
            },
            "fontBold": {
                "type": SoftBoolOrNothingType,
                "display_name": "Bold",
                "short_desc": "Bold text",
                "desc": "Set to `True` for bold text",
            },
            "fontItalic": {
                "type": SoftBoolOrNothingType,
                "display_name": "Italic",
                "short_desc": "Italic text",
                "desc": "Set to `True` for italic text",
            },
            "fontUnderline": {
                "type": SoftBoolOrNothingType,
                "display_name": "Underline",
                "short_desc": "Underlined text",
                "desc": "Set to `True` for underlined text",
            },
            "fontStrikeout": {
                "type": SoftBoolOrNothingType,
                "display_name": "Strikeout",
                "short_desc": "Strikethrough text",
                "desc": "Set to `True` for strikethrough text",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "Text color as hex code",
                "desc": "Text color in `RRGGBB` or `AARRGGBB` format (default: `000000`)",
            },
            "alignment": {
                "type": StringOrNothingType,
                "display_name": "Alignment",
                "short_desc": "Text alignment",
                "desc": "Text alignment (default: `left`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "left", "desc": "Left-aligned"},
                    <AllowedValueInfo>{"value": "center", "desc": "Center-aligned"},
                    <AllowedValueInfo>{"value": "right", "desc": "Right-aligned"},
                ),
            },
            "link": {
                "type": StringOrNothingType,
                "display_name": "Link",
                "short_desc": "Clickable URL",
                "desc": "URL to open when the text is clicked",
            },
            "transparent": {
                "type": SoftBoolOrNothingType,
                "display_name": "Transparent",
                "short_desc": "Transparent background",
                "desc": "Set to `False` for an opaque background (default: `True`)",
            },
            "RotationAngle": {
                "type": SoftIntOrNothingType,
                "display_name": "Rotation Angle",
                "short_desc": "Rotation in degrees",
                "desc": "Rotation angle in degrees (default: `0`)",
            },
            "pages": {
                "type": StringOrNothingType,
                "display_name": "Pages",
                "short_desc": "Page indices",
                "desc": "Page indices (e.g., `0, 2-5`). Default: all pages",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Annotation Type",
                "short_desc": "Type of annotation",
                "desc": "Type of annotation to add",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Text", "desc": "Plain text"},
                    <AllowedValueInfo>{"value": "TextField", "desc": "Single-line text input field"},
                    <AllowedValueInfo>{"value": "TextFieldMultiline",
                        "desc": "Multi-line text input field"},
                    <AllowedValueInfo>{"value": "Checkbox", "desc": "Unchecked checkbox"},
                    <AllowedValueInfo>{"value": "CheckboxChecked", "desc": "Pre-checked checkbox"},
                ),
            },
        };
    }

    constructor() : HashDataType("PdfCoTextAnnotation") {
        addQoreFields(Fields);
    }
}

#! Element type for image annotations in add-text-images
public class PdfCoImageAnnotationDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "type": StringType,
                "display_name": "Image URL",
                "short_desc": "URL of the image to insert",
                "desc": "URL of the image to insert",
                "required": True,
            },
            "x": {
                "type": SoftFloatType,
                "display_name": "X Position",
                "short_desc": "Horizontal position in points",
                "desc": "Horizontal position in points from the left edge",
                "required": True,
            },
            "y": {
                "type": SoftFloatType,
                "display_name": "Y Position",
                "short_desc": "Vertical position in points",
                "desc": "Vertical position in points from the top edge",
                "required": True,
            },
            "width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Image width in points",
                "desc": "Image width in points (auto-scaled if omitted)",
            },
            "height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Image height in points",
                "desc": "Image height in points (auto-scaled if omitted)",
            },
            "pages": {
                "type": StringOrNothingType,
                "display_name": "Pages",
                "short_desc": "Page indices",
                "desc": "Page indices (e.g., `0, 2-5`). Default: all pages",
            },
        };
    }

    constructor() : HashDataType("PdfCoImageAnnotation") {
        addQoreFields(Fields);
    }
}

#! Element type for form field values in fill-pdf-form
public class PdfCoFormFieldValueDataType inherits HashDataType {
    public {
        const Fields = {
            "fieldName": {
                "type": StringType,
                "display_name": "Field Name",
                "short_desc": "The form field name or ID",
                "desc": "The name or ID of the form field to fill.\n\n"
                    "This corresponds to the `id` assigned when creating the field with the "
                    "**Create Fillable Form** action, or the field name embedded in the PDF.\n\n"
                    "Use the **Get PDF Info** action to retrieve the list of form field names "
                    "from an existing PDF.",
                "required": True,
            },
            "text": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "The value to set",
                "desc": "The value to set in the form field",
                "required": True,
            },
            "pages": {
                "type": StringOrNothingType,
                "display_name": "Pages",
                "short_desc": "Page index",
                "desc": "Page index (default: all pages)",
            },
        };
    }

    constructor() : HashDataType("PdfCoFormFieldValue") {
        addQoreFields(Fields);
    }
}

#! Element type for form field title/label in create-fillable-form
public class PdfCoFormFieldTitleDataType inherits HashDataType {
    public {
        const Fields = {
            "text": {
                "type": StringType,
                "display_name": "Title Text",
                "short_desc": "Label text to display",
                "desc": "Label text to display above the form field",
                "required": True,
            },
            "x": {
                "type": SoftFloatOrNothingType,
                "display_name": "X Position",
                "short_desc": "Custom horizontal position",
                "desc": "Custom horizontal position in points (default: same as the field)",
            },
            "y": {
                "type": SoftFloatOrNothingType,
                "display_name": "Y Position",
                "short_desc": "Custom vertical position",
                "desc": "Custom vertical position in points (default: above the field)",
            },
            "size": {
                "type": SoftIntOrNothingType,
                "display_name": "Font Size",
                "short_desc": "Font size in points",
                "desc": "Font size in points (default: `12`)",
            },
            "fontName": {
                "type": StringOrNothingType,
                "display_name": "Font Name",
                "short_desc": "Font name",
                "desc": "Font name (default: `Arial`)",
            },
            "fontBold": {
                "type": SoftBoolOrNothingType,
                "display_name": "Bold",
                "short_desc": "Bold text",
                "desc": "Set to `True` for bold text",
            },
            "fontItalic": {
                "type": SoftBoolOrNothingType,
                "display_name": "Italic",
                "short_desc": "Italic text",
                "desc": "Set to `True` for italic text",
            },
            "fontUnderline": {
                "type": SoftBoolOrNothingType,
                "display_name": "Underline",
                "short_desc": "Underlined text",
                "desc": "Set to `True` for underlined text",
            },
            "fontStrikeout": {
                "type": SoftBoolOrNothingType,
                "display_name": "Strikeout",
                "short_desc": "Strikethrough text",
                "desc": "Set to `True` for strikethrough text",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "Text color as hex code",
                "desc": "Text color in `RRGGBB` or `AARRGGBB` format (default: `000000`)",
            },
        };
    }

    constructor() : HashDataType("PdfCoFormFieldTitle") {
        addQoreFields(Fields);
    }
}

#! Element type for form field definitions in create-fillable-form
public class PdfCoFormFieldDefinitionDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Field Type",
                "short_desc": "Type of form field",
                "desc": "Type of form field to create",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "TextField", "desc": "Single-line text input"},
                    <AllowedValueInfo>{"value": "TextFieldMultiline",
                        "desc": "Multi-line text input"},
                    <AllowedValueInfo>{"value": "Checkbox", "desc": "Unchecked checkbox"},
                    <AllowedValueInfo>{"value": "CheckboxChecked", "desc": "Pre-checked checkbox"},
                ),
            },
            "id": {
                "type": StringType,
                "display_name": "Field ID",
                "short_desc": "Unique field name/identifier",
                "desc": "Unique field name used to reference this field when filling the form",
                "required": True,
            },
            "x": {
                "type": SoftFloatType,
                "display_name": "X Position",
                "short_desc": "Horizontal position in points",
                "desc": "Horizontal position in points from the left edge",
                "required": True,
            },
            "y": {
                "type": SoftFloatType,
                "display_name": "Y Position",
                "short_desc": "Vertical position in points",
                "desc": "Vertical position in points from the top edge",
                "required": True,
            },
            "width": {
                "type": SoftFloatType,
                "display_name": "Width",
                "short_desc": "Field width in points",
                "desc": "Field width in points",
                "required": True,
            },
            "height": {
                "type": SoftFloatType,
                "display_name": "Height",
                "short_desc": "Field height in points",
                "desc": "Field height in points",
                "required": True,
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Default Value",
                "short_desc": "Default text value",
                "desc": "Default text value for the field",
            },
            "title": {
                "type": (new PdfCoFormFieldTitleDataType()).getOrNothingType(),
                "display_name": "Title / Label",
                "short_desc": "Optional text label for the field",
                "desc": "Optional text label displayed above the form field. "
                    "The label is automatically positioned above the field unless "
                    "custom x/y coordinates are provided",
            },
            "size": {
                "type": SoftIntOrNothingType,
                "display_name": "Font Size",
                "short_desc": "Font size in points",
                "desc": "Font size in points (default: `12`)",
            },
            "fontName": {
                "type": StringOrNothingType,
                "display_name": "Font Name",
                "short_desc": "Font name",
                "desc": "Font name (default: `Arial`)",
            },
            "fontBold": {
                "type": SoftBoolOrNothingType,
                "display_name": "Bold",
                "short_desc": "Bold text",
                "desc": "Set to `True` for bold text",
            },
            "fontItalic": {
                "type": SoftBoolOrNothingType,
                "display_name": "Italic",
                "short_desc": "Italic text",
                "desc": "Set to `True` for italic text",
            },
            "fontUnderline": {
                "type": SoftBoolOrNothingType,
                "display_name": "Underline",
                "short_desc": "Underlined text",
                "desc": "Set to `True` for underlined text",
            },
            "fontStrikeout": {
                "type": SoftBoolOrNothingType,
                "display_name": "Strikeout",
                "short_desc": "Strikethrough text",
                "desc": "Set to `True` for strikethrough text",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "Text color as hex code",
                "desc": "Text color in `RRGGBB` or `AARRGGBB` format (default: `000000`)",
            },
            "pages": {
                "type": StringOrNothingType,
                "display_name": "Pages",
                "short_desc": "Page indices",
                "desc": "Page indices (e.g., `0, 2-5`). Default: all pages",
            },
        };
    }

    constructor() : HashDataType("PdfCoFormFieldDefinition") {
        addQoreFields(Fields);
    }
}

} # namespace PdfCoDataProvider
