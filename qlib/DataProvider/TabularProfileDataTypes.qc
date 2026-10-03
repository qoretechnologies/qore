# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore tabular profile data type definitions

/** TabularProfileDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The column types a @ref TabularProfiler infers
/** @since %DataProvider 3.9
*/
public const TabularColumnTypeValues = (
    <AllowedValueInfo>{"value": "string", "display_name": "Text", "short_desc": "Text values"},
    <AllowedValueInfo>{"value": "int", "display_name": "Integer", "short_desc": "Whole numbers"},
    <AllowedValueInfo>{"value": "number", "display_name": "Number", "short_desc": "Numbers with decimals"},
    <AllowedValueInfo>{"value": "date", "display_name": "Date", "short_desc": "Dates or date-times"},
    <AllowedValueInfo>{"value": "bool", "display_name": "Boolean", "short_desc": "True or false values"},
    <AllowedValueInfo>{"value": "empty", "display_name": "Empty", "short_desc": "The column has no values"},
);

#! The finding codes a @ref TabularProfiler reports
/** @since %DataProvider 3.9
*/
public const TabularFindingCodeValues = (
    <AllowedValueInfo>{"value": "ambiguous_date_order", "display_name": "Ambiguous Date Order",
        "short_desc": "Dates stored as text could be day-first or month-first"},
    <AllowedValueInfo>{"value": "blank_row_gap", "display_name": "Blank Row Gap",
        "short_desc": "Empty rows inside the data region"},
    <AllowedValueInfo>{"value": "dates_as_text", "display_name": "Dates As Text",
        "short_desc": "A column holds dates stored as text"},
    <AllowedValueInfo>{"value": "decimal_comma_numbers", "display_name": "Decimal Comma Numbers",
        "short_desc": "A column holds numbers stored as text with a decimal comma"},
    <AllowedValueInfo>{"value": "duplicate_header", "display_name": "Duplicate Header",
        "short_desc": "Two columns have the same header"},
    <AllowedValueInfo>{"value": "empty_header", "display_name": "Empty Header",
        "short_desc": "A column has data but no header"},
    <AllowedValueInfo>{"value": "formulas", "display_name": "Formulas",
        "short_desc": "The sheet has formulas"},
    <AllowedValueInfo>{"value": "hidden_sheet", "display_name": "Hidden Sheet",
        "short_desc": "The sheet is hidden"},
    <AllowedValueInfo>{"value": "leading_zero_identifiers", "display_name": "Leading Zero Identifiers",
        "short_desc": "A column holds digit strings with leading zeros"},
    <AllowedValueInfo>{"value": "merged_cells_in_data", "display_name": "Merged Cells In Data",
        "short_desc": "Merged cells overlap the data region"},
    <AllowedValueInfo>{"value": "mixed_types", "display_name": "Mixed Types",
        "short_desc": "A column's values do not all match its type"},
    <AllowedValueInfo>{"value": "no_header_row", "display_name": "No Header Row",
        "short_desc": "No header row was found"},
    <AllowedValueInfo>{"value": "partial_profile", "display_name": "Partial Profile",
        "short_desc": "The sheet has more rows than were read"},
    <AllowedValueInfo>{"value": "title_rows", "display_name": "Title Rows",
        "short_desc": "Rows above the header hold titles or banners"},
    <AllowedValueInfo>{"value": "totals_row", "display_name": "Totals Row",
        "short_desc": "A row holds totals or subtotals"},
    <AllowedValueInfo>{"value": "wide_period_columns", "display_name": "Wide Period Columns",
        "short_desc": "Several columns are periods such as weeks or months"},
);

#! Describes a rectangular cell range in a profile
/** @since %DataProvider 3.9
*/
public class TabularCellRangeDataType inherits HashDataType {
    private {
        const Fields = {
            "first_row": {"display_name": "First Row", "type": IntType, "desc": "The first row, 1-based"},
            "last_row": {"display_name": "Last Row", "type": IntType, "desc": "The last row, 1-based"},
            "first_col": {"display_name": "First Column", "type": IntType, "desc": "The first column, 1-based"},
            "last_col": {"display_name": "Last Column", "type": IntType, "desc": "The last column, 1-based"},
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "TabularCellRange") {
        addQoreFields(Fields);
    }
}

#! Describes the text format of a delimited text file
/** @since %DataProvider 3.9
*/
public class TabularTextFormatDataType inherits HashDataType {
    private {
        const Fields = {
            "separator": {"display_name": "Separator", "type": StringType, "desc": "The field separator"},
            "encoding": {"display_name": "Encoding", "type": StringType, "desc": "The character encoding"},
            "bom": {"display_name": "Byte Order Mark", "type": BoolType,
                "desc": "`True` if the file starts with a byte order mark"},
            "eol": {"display_name": "Line Ending", "type": StringType,
                "desc": "The line ending: `\\n`, `\\r\\n`, or `\\r`"},
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "TabularTextFormat") {
        addQoreFields(Fields);
    }
}

#! Describes a finding about a profiled sheet
/** @since %DataProvider 3.9
*/
public class TabularProfileFindingDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "display_name": "Code",
                "type": StringType,
                "desc": "The finding code",
                "allowed_values": TabularFindingCodeValues,
            },
            "row": {"display_name": "Row", "type": IntOrNothingType,
                "desc": "The 1-based row the finding refers to, if any"},
            "col": {"display_name": "Column", "type": IntOrNothingType,
                "desc": "The 1-based column the finding refers to, if any"},
            "detail": {"display_name": "Detail", "type": AutoType,
                "desc": "A detail value, such as the duplicated header or the functions used in formulas"},
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "TabularProfileFinding") {
        addQoreFields(Fields);
    }
}

#! Describes the profile of a column
/** @since %DataProvider 3.9
*/
public class TabularColumnProfileDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {"display_name": "Index", "type": IntType, "desc": "The 1-based column index"},
            "letter": {"display_name": "Letter", "type": StringType, "desc": "The column letter, for example `A`"},
            "header": {"display_name": "Header", "type": StringOrNothingType, "desc": "The header label, if any"},
            "type": {
                "display_name": "Type",
                "type": StringType,
                "desc": "The inferred type of the column's values",
                "allowed_values": TabularColumnTypeValues,
            },
            "confidence": {"display_name": "Confidence", "type": FloatType,
                "desc": "The share of non-empty cells matching the inferred type, from 0 to 1"},
            "non_empty": {"display_name": "Non-Empty Cells", "type": IntType,
                "desc": "The number of non-empty data cells"},
            "empty": {"display_name": "Empty Cells", "type": IntType, "desc": "The number of empty data cells"},
            "dates_as_text": {"display_name": "Dates As Text", "type": BoolType,
                "desc": "`True` if some dates are stored as text"},
            "date_format": {"display_name": "Date Format", "type": StringOrNothingType,
                "desc": "The pattern of dates stored as text, for example `DD.MM.YYYY`"},
            "number_format": {"display_name": "Number Format", "type": StringOrNothingType,
                "desc": "The format of numbers stored as text: `decimal_comma` or `decimal_point`"},
            "leading_zeros": {"display_name": "Leading Zeros", "type": BoolType,
                "desc": "`True` if the column holds digit strings with leading zeros, such as identifiers"},
            "counter_examples": {"display_name": "Counter-Examples", "type": AutoListType,
                "desc": "Cells that do not match the inferred type, each with its 1-based `row` and its `value`"},
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "TabularColumnProfile") {
        addQoreFields(Fields);
    }
}

#! Describes the profile of a sheet
/** @since %DataProvider 3.9
*/
public class TabularSheetProfileDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {"display_name": "Name", "type": StringType, "desc": "The sheet name"},
            "hidden": {"display_name": "Hidden", "type": BoolType, "desc": "`True` if the sheet is hidden"},
            "row_count": {"display_name": "Row Count", "type": IntType, "desc": "The number of rows in the sheet"},
            "header_row": {"display_name": "Header Row", "type": IntOrNothingType,
                "desc": "The 1-based header row, if one was found"},
            "title_rows": {"display_name": "Title Rows", "type": AutoListType,
                "desc": "The 1-based rows above the header that hold titles or banners"},
            "data_region": {"display_name": "Data Region", "type": AutoHashOrNothingType,
                "desc": "The 1-based data region below the header: `first_row`, `last_row`, `first_col`, and "
                    "`last_col`"},
            "totals_rows": {"display_name": "Totals Rows", "type": AutoListType,
                "desc": "The 1-based totals and subtotal rows in the data region"},
            "blank_row_gaps": {"display_name": "Blank Row Gaps", "type": AutoListType,
                "desc": "Runs of empty rows inside the data region, each with its 1-based `first_row` and `last_row`"},
            "columns": {"display_name": "Columns",
                "type": new ListDataType("TabularColumnProfileList", TabularColumnProfileDataType),
                "desc": "The columns of the data region"},
            "samples": {"display_name": "Samples", "type": AutoListType,
                "desc": "Sample data rows, one value per column; redacted unless redaction is disabled"},
            "formula_count": {"display_name": "Formula Count", "type": IntType,
                "desc": "The number of formulas in the sheet"},
            "findings": {"display_name": "Findings",
                "type": new ListDataType("TabularProfileFindingList", TabularProfileFindingDataType),
                "desc": "Findings about the sheet"},
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "TabularSheetProfile") {
        addQoreFields(Fields);
    }
}

#! Describes the profile of a workbook or text file
/** This is the output type of the \c profile actions of spreadsheet and CSV applications

    @since %DataProvider 3.9
*/
public class TabularWorkbookProfileDataType inherits HashDataType {
    private {
        const Fields = {
            "format": {"display_name": "Format", "type": StringType,
                "desc": "The source format, for example `xlsx`, `ods`, or `csv`"},
            "sheets": {"display_name": "Sheets",
                "type": new ListDataType("TabularSheetProfileList", TabularSheetProfileDataType),
                "desc": "The sheet profiles, in workbook order; a text file has one sheet"},
            "named_ranges": {"display_name": "Named Ranges", "type": AutoHashType,
                "desc": "Named ranges by name"},
            "text_format": {"display_name": "Text Format", "type": AutoHashOrNothingType,
                "desc": "The text format of a delimited text file: `separator`, `encoding`, `bom`, and `eol`"},
        };
    }

    #! Creates the type
    constructor() : HashDataType(AutoHashType, "TabularWorkbookProfile") {
        addQoreFields(Fields);
    }
}

#! Describes the profile of a cell range
/** @since %DataProvider 3.9
*/
public const TabularCellRangeDataType = new TabularCellRangeDataType();

#! Describes the text format of a delimited text file
/** @since %DataProvider 3.9
*/
public const TabularTextFormatDataType = new TabularTextFormatDataType();

#! Describes a finding about a profiled sheet
/** @since %DataProvider 3.9
*/
public const TabularProfileFindingDataType = new TabularProfileFindingDataType();

#! Describes the profile of a column
/** @since %DataProvider 3.9
*/
public const TabularColumnProfileDataType = new TabularColumnProfileDataType();

#! Describes the profile of a sheet
/** @since %DataProvider 3.9
*/
public const TabularSheetProfileDataType = new TabularSheetProfileDataType();

#! Describes the profile of a workbook or text file
/** @since %DataProvider 3.9
*/
public const TabularWorkbookProfileDataType = new TabularWorkbookProfileDataType();
}
