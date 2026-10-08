# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TabularProfiler class definition

/** TabularProfiler.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A rectangular range of cells
/** Rows and columns are 0-based and inclusive in a @ref TabularSheetGrid and 1-based and inclusive in a profile

    @since %DataProvider 3.9
*/
public hashdecl TabularCellRange {
    #! The first row
    int first_row;
    #! The last row
    int last_row;
    #! The first column
    int first_col;
    #! The last column
    int last_col;
}

#! The raw cells of one sheet of a spreadsheet or of a text file such as a CSV file
/** @since %DataProvider 3.9
*/
public hashdecl TabularSheetGrid {
    #! The sheet name
    string name;

    #! True if the sheet is hidden
    bool hidden = False;

    #! The cell values read, row by row from the first row of the sheet; a row can omit trailing empty cells
    list<list<auto>> rows = ();

    #! The number of rows in the sheet, when more rows exist than were read
    *int row_count;

    #! Merged cell ranges, 0-based
    list<hash<TabularCellRange>> merged = ();

    #! Formula text by A1 cell reference (for example \c "B7"), without the leading \c "="
    hash<string, string> formulas = {};
}

#! The text format of a delimited text file
/** @since %DataProvider 3.9
*/
public hashdecl TabularTextFormat {
    #! The field separator
    string separator;

    #! The character encoding
    string encoding;

    #! True if the file starts with a byte order mark
    bool bom = False;

    #! The line ending: \c "\n", \c "\r\n", or \c "\r"
    string eol;

    #! The field quote character
    /** @since %DataProvider 3.9 */
    string quote = "\"";
}

#! The raw cells of a workbook or text file, the input to @ref TabularProfiler
/** @since %DataProvider 3.9
*/
public hashdecl TabularWorkbookGrid {
    #! The source format, for example \c "xlsx", \c "ods", or \c "csv"
    string format;

    #! The sheets; a text file has one sheet
    list<hash<TabularSheetGrid>> sheets = ();

    #! Named ranges by name, as references such as \c "Sheet1!$A$1:$D$20"
    hash<string, string> named_ranges = {};

    #! The text format of a delimited text file
    *hash<TabularTextFormat> text_format;
}

#! Options for @ref TabularProfiler::profile()
/** @since %DataProvider 3.9
*/
public hashdecl TabularProfileOptions {
    #! The number of sample data rows returned per sheet
    int sample_rows = 5;

    #! If True, sample values and counter-examples are redacted to their shape
    bool redact = True;

    #! The maximum number of counter-examples returned per column
    int max_counter_examples = 3;

    #! The number of leading rows searched for the header row
    int header_search_rows = 20;
}

#! A finding about a sheet: a structural feature or a problem that affects how the data is read
/** @since %DataProvider 3.9
*/
public hashdecl TabularProfileFinding {
    #! The finding code; see @ref TabularProfiler for the codes
    string code;

    #! The 1-based row the finding refers to, if any
    *int row;

    #! The 1-based column the finding refers to, if any
    *int col;

    #! A detail value, such as the duplicated header or a list of formula functions
    auto detail;
}

#! How a column writes amounts and rates as text: the parts around the number
/** Describes the number mask shapes \c "(...)", a trailing \c "-", a currency symbol or code before or after the
    number, and \c "%"

    @since %DataProvider 3.9
*/
public hashdecl TabularAmountFormat {
    #! The currency symbol (such as \c "€") or ISO 4217 code (such as \c "EUR") written with the numbers, if any
    *string currency;

    #! Where the currency is written: \c "before" or \c "after" the number
    *string currency_position;

    #! True if a space separates the currency from the number
    bool currency_space = False;

    #! True if the numbers are percentages, written with \c "%"
    bool percent = False;

    #! True if negative numbers are written in parentheses, as in \c "(1,234.00)"
    bool parentheses = False;

    #! True if negative numbers are written with a trailing minus, as in \c "1.234,56-"
    bool trailing_minus = False;
}

#! The profile of one column of a sheet's data region
/** @since %DataProvider 3.9
*/
public hashdecl TabularColumnProfile {
    #! The 1-based column index
    int index;

    #! The column letter, for example \c "A"
    string letter;

    #! The header label, if the sheet has a header row and the cell is not empty
    *string header;

    #! The inferred type: \c "string", \c "int", \c "number", \c "date", \c "bool", or \c "empty"
    string type;

    #! The share of non-empty cells matching the inferred type, from 0 to 1
    float confidence = 1.0;

    #! The number of non-empty data cells
    int non_empty = 0;

    #! The number of empty data cells
    int empty = 0;

    #! True if some dates are stored as text
    bool dates_as_text = False;

    #! The pattern of dates stored as text, for example \c "DD.MM.YYYY"
    *string date_format;

    #! The format of numbers stored as text: \c "decimal_comma" or \c "decimal_point"
    *string number_format;

    #! For numbers stored as text with a currency, \c "%", parentheses or a trailing minus: how they are written
    /** @since %DataProvider 3.9 */
    *hash<TabularAmountFormat> amount_format;

    #! True if the column holds digit strings with leading zeros, such as identifiers
    bool leading_zeros = False;

    #! A text column with only two values that are not a known yes/no pair (redacted unless disabled)
    /** @since %DataProvider 3.9 */
    *list<auto> two_values;

    #! For a yes/no column: the values read as true, as the file writes them
    /** @since %DataProvider 3.9 */
    *list<string> bool_true;

    #! For a yes/no column: the values read as false, as the file writes them
    /** @since %DataProvider 3.9 */
    *list<string> bool_false;

    #! For a yes/no column marked with one value (such as \c x or \c ✓): an empty cell means false
    /** @since %DataProvider 3.9 */
    bool empty_is_false = False;

    #! Cells that do not match the inferred type: \c row (1-based) and \c value
    list<hash<auto>> counter_examples = ();

    #! Text cells that stand for a missing value, such as \c n/a or \c -: each marker with its number of cells
    /** Markers come from the fixed @ref TabularProfiler::EmptyMarkers vocabulary and are matched exactly as written
        (after trimming whitespace), so they are reported unredacted: no other cell text is reported here.  No value
        if the column has none.

        @since %DataProvider 3.9
    */
    *hash<string, int> empty_markers;
}

#! The profile of one sheet
/** @since %DataProvider 3.9
*/
public hashdecl TabularSheetProfile {
    #! The sheet name
    string name;

    #! True if the sheet is hidden
    bool hidden = False;

    #! The number of rows in the sheet
    int row_count = 0;

    #! The 1-based header row, if one was found
    *int header_row;

    #! The 1-based rows above the header that hold titles or banners
    list<int> title_rows = ();

    #! The 1-based data region below the header
    *hash<TabularCellRange> data_region;

    #! The 1-based totals and subtotal rows in the data region
    list<int> totals_rows = ();

    #! Runs of empty rows inside the data region: \c first_row and \c last_row (1-based)
    list<hash<auto>> blank_row_gaps = ();

    #! The columns of the data region
    list<hash<TabularColumnProfile>> columns = ();

    #! Sample data rows, one value per column, redacted unless redaction is disabled
    list<list<auto>> samples = ();

    #! The number of formulas in the sheet
    int formula_count = 0;

    #! Findings about the sheet
    list<hash<TabularProfileFinding>> findings = ();
}

#! The profile of a workbook or text file
/** @since %DataProvider 3.9
*/
public hashdecl TabularWorkbookProfile {
    #! The source format
    string format;

    #! The sheet profiles, in workbook order
    list<hash<TabularSheetProfile>> sheets = ();

    #! Named ranges by name
    hash<string, string> named_ranges = {};

    #! The text format of a delimited text file
    *hash<TabularTextFormat> text_format;
}

#! Deterministically profiles spreadsheets and delimited text files
/** Spreadsheets that people maintain by hand rarely start with a clean header in row 1: they have title rows and
    merged banners, totals rows, blank-row gaps, dates and numbers stored as text in a local format, and identifiers
    with leading zeros.  This class reads the raw cells of a workbook, supplied by a format reader as a
    @ref TabularWorkbookGrid, and describes how the data is laid out so that a reader can be configured correctly and
    the remaining questions can be asked precisely.

    The profile is computed from the cells alone, without any model, so the same file always produces the same
    profile.

    For each sheet the profile identifies:
    - the header row, the title rows above it, and the data region below it
    - totals and subtotal rows and blank-row gaps, which are excluded from type inference
    - each column's inferred type with a confidence and redacted counter-examples
    - dates stored as text with their pattern, numbers stored as text in a local format, and leading-zero identifiers

    Finding codes:
    - \c ambiguous_date_order: dates stored as text could be day-first or month-first
    - \c two_values: a text column holds only two words (letters only, at least four cells) that are not a
      known yes/no pair, such as good/bad; the detail lists them (redacted unless redaction is disabled) - it
      may be a yes/no column
    - \c blank_row_gap: empty rows inside the data region
    - \c dates_as_text: a column holds dates stored as text
    - \c decimal_comma_numbers: a column holds numbers stored as text with a decimal comma
    - \c duplicate_header: two columns have the same header
    - \c empty_header: a column has data but no header
    - \c formulas: the sheet has formulas; the detail lists the functions used
    - \c hidden_sheet: the sheet is hidden
    - \c leading_zero_identifiers: a column holds digit strings with leading zeros
    - \c merged_cells_in_data: merged cells overlap the data region
    - \c mixed_types: a column's values do not all match its inferred type
    - \c no_header_row: no header row was found
    - \c partial_profile: the sheet has more rows than were read
    - \c title_rows: rows above the header hold titles or banners
    - \c totals_row: a row holds totals or subtotals: its first cell with content is a totals label
      (@ref isTotalsLabel()), or one of its formulas totals the rows above it in its own column
      (@ref isTotalsFormula()); a formula across its own row, such as a line total, does not make a totals row
    - \c wide_period_columns: several columns are periods such as weeks, months, or dates, a layout that is usually
      unpivoted into rows

    @par Example
    @code{.py}
hash<TabularWorkbookProfile> profile = TabularProfiler::profile(<TabularWorkbookGrid>{
    "format": "csv",
    "sheets": (<TabularSheetGrid>{
        "name": "orders",
        "rows": (("PO", "Qty", "Ship date"), ("PO-1", "5", "10.10.2026"), ("PO-2", "7", "24.10.2026")),
    },),
});
# profile.sheets[0].columns[2].type == "date", .date_format == "DD.MM.YYYY"
    @endcode

    @since %DataProvider 3.9
*/
public class TabularProfiler {
    public {
        #! Labels of totals rows in common languages
        /** A row whose first cell with content is text matching this regular expression (without regard to case,
            with Unicode support) is a totals row; see @ref isTotalsRow()

            @since %DataProvider 3.9
        */
        const TotalsRegex = "^\\s*(grand\\s+total|sub\\s*-?\\s*total|total|totals|sum|summe|gesamt|zwischensumme"
            "|celkem|mezisou[cč]et|spolu|razem|suma|totale|totaal|итого|всього|합계|소계|合計|小計|总计|小计)\\b";

        #! Formula functions that aggregate the rows above a totals row
        /** Matched against formula text without the leading \c "=" and without regard to case; see
            @ref isTotalsFormula()

            @since %DataProvider 3.9
        */
        const TotalsFormulaRegex = "^\\s*(SUM|SUBTOTAL|AGGREGATE)\\s*\\(";

        #! Text that stands for a missing value in spreadsheets and exported data
        /** Reported in @ref TabularColumnProfile::empty_markers "TabularColumnProfile::empty_markers" when a cell's
            trimmed text is exactly one of these values; the vocabulary is fixed, so the reported markers never
            contain data from the file.  Case variants are listed explicitly.

            @since %DataProvider 3.9
        */
        const EmptyMarkers = (
            # not applicable / not available
            "n/a", "N/A", "N/a", "n.a.", "N.A.", "na", "NA", "n.v.", "N.V.", "k.A.", "k. A.", "K.A.", "n.d.", "N.D.",
            # dashes
            "-", "--", "---", "—", "–", "-/-",
            # null and none
            "null", "NULL", "Null", "nil", "NIL", "none", "None", "NONE", "(none)", "(blank)", "(empty)", "empty",
            "unknown", "Unknown", "UNKNOWN",
            # to be determined / announced
            "TBD", "tbd", "TBA", "tba", "TBC", "tbc",
            # placeholders
            "?", "??", "???", ".", "..", "...", "…", "*", "#",
            # spreadsheet error values written as text
            "#N/A", "#NA", "#N/V", "#NV", "#NULL!", "#REF!", "#VALUE!", "#DIV/0!", "#NUM!", "#NAME?",
        );
    }

    private {
        #! ISO 4217 codes recognized as currencies around numbers stored as text
        const CurrencyCodes = (
            "EUR", "USD", "GBP", "CHF", "JPY", "CNY", "CZK", "PLN", "HUF", "SEK", "NOK", "DKK", "RON", "BGN",
            "HRK", "RUB", "UAH", "TRY", "CAD", "AUD", "NZD", "INR", "BRL", "MXN", "ZAR", "KRW", "SGD", "HKD", "TWD",
            "ILS", "AED", "SAR",
        );

        #! @ref CurrencyCodes for quick lookups
        const CurrencyCodeSet = map {$1: True}, CurrencyCodes;

        #! @ref EmptyMarkers for quick lookups
        const EmptyMarkerSet = map {$1: True}, EmptyMarkers;

        #! Header labels that name periods
        const PeriodHeaderRegex = "^\\s*(W(K|eek)?\\s*\\d{1,2}|KW\\s*\\d{1,2}|Q[1-4]|\\d{4}[-/]\\d{1,2}|\\d{1,2}[-/]\\d{4}"
            "|jan(uary)?|feb(ruary)?|mar(ch)?|apr(il)?|may|june?|july?|aug(ust)?|sep(tember)?|oct(ober)?"
            "|nov(ember)?|dec(ember)?)(\\s+\\d{2,4})?\\s*$";

        #! The minimum number of period headers reported as wide period columns
        const MinPeriodColumns = 4;

        #! Values longer than this are truncated when redacted
        const MaxRedactedLength = 24;
    }

    #! Profiles a workbook or text file
    /** @param grid the raw cells, from a format reader
        @param opts profile options

        @return the profile
    */
    static hash<TabularWorkbookProfile> profile(hash<TabularWorkbookGrid> grid, *hash<TabularProfileOptions> opts) {
        hash<TabularProfileOptions> options = opts ?? <TabularProfileOptions>{};
        hash<TabularWorkbookProfile> rv({
            "format": grid.format,
            "named_ranges": grid.named_ranges,
            "text_format": grid.text_format,
        });
        foreach hash<TabularSheetGrid> sheet in (grid.sheets) {
            rv.sheets += profileSheet(sheet, options);
        }
        return rv;
    }

    #! Converts an untyped grid, such as one returned by a Java reader, to a @ref TabularWorkbookGrid
    /** @param grid a hash with the keys of @ref TabularWorkbookGrid; sheets, merged ranges, and the text format are
        plain hashes, and rows are plain lists

        @return the typed grid
    */
    static hash<TabularWorkbookGrid> gridFromHash(hash<auto> grid) {
        hash<TabularWorkbookGrid> rv({
            "format": grid.format,
        });
        foreach hash<auto> sheet in (grid.sheets) {
            hash<TabularSheetGrid> s({
                "name": sheet.name,
                "hidden": sheet.hidden ?? False,
                "row_count": sheet.row_count,
            });
            foreach auto row in (sheet.rows) {
                list<auto> values = row ?? ();
                push s.rows, values;
            }
            foreach hash<auto> range in (sheet.merged) {
                s.merged += cast<hash<TabularCellRange>>(range{"first_row", "last_row", "first_col", "last_col"});
            }
            map s.formulas{$1.key} = $1.value.toString(), sheet.formulas.pairIterator();
            rv.sheets += s;
        }
        map rv.named_ranges{$1.key} = $1.value.toString(), grid.named_ranges.pairIterator();
        if (grid.text_format) {
            rv.text_format = cast<hash<TabularTextFormat>>(grid.text_format);
        }
        return rv;
    }

    #! Classifies one cell value
    /** @param value the cell value

        @return \c type (\c "empty", \c "string", \c "int", \c "number", \c "date", or \c "bool"), \c text (True if
        the value is a string), and for strings \c date_format, \c number_format, \c leading_zero, and the day and
        month components \c d1 and \c d2 of a day/month date
    */
    static hash<auto> classify(auto value) {
        switch (value.typeCode()) {
            case NT_NOTHING:
            case NT_NULL:
                return {"type": "empty"};
            case NT_BOOLEAN:
                return {"type": "bool"};
            case NT_INT:
                return {"type": "int"};
            case NT_FLOAT:
            case NT_NUMBER:
                return {"type": value == value.toInt() ? "int" : "number"};
            case NT_DATE:
                return {"type": "date"};
            case NT_STRING:
                return classifyString(value);
        }
        return {"type": "string"};
    }

    #! Returns the column letter of a 0-based column index
    /** @param col the 0-based column index

        @return the column letter, for example \c "A" for 0 and \c "AA" for 26
    */
    static string columnLetter(int col) {
        string rv;
        ++col;
        while (col > 0) {
            int rem = (col - 1) % 26;
            rv = chr(65 + rem) + rv;
            col = (col - 1) / 26;
        }
        return rv;
    }

    #! Returns True if a value labels a totals row
    /** A totals row is labelled by its first cell with content; this method tells whether that value is a totals
        label, such as \c "Total", \c "Summe" or \c "Celkem" (@ref TotalsRegex).

        @param value the value of the first cell with content in the row

        @return True if the value is text that labels a totals or subtotals row

        @par Example:
        @code{.py}
bool totals = TabularProfiler::isTotalsLabel("Grand total"); # True
        @endcode

        @see @ref isTotalsFormula() for the other way a row is recognized as a totals row

        @since %DataProvider 3.9
    */
    static bool isTotalsLabel(auto value) {
        return value.typeCode() == NT_STRING && regex(value, TotalsRegex, RE_Caseless | RE_Unicode);
    }

    #! Returns True if a formula makes its cell a total of the rows above it
    /** A formula marks a totals row when it is a \c SUM, \c SUBTOTAL or \c AGGREGATE (@ref TotalsFormulaRegex) with
        a cell range argument that includes the formula's own column and ends above the formula's row, as in
        \c B10 holding \c SUM(B2:B9) or \c SUBTOTAL(9,B2:B9).

        A \c SUM across its own row, such as a line total \c D3 holding \c SUM(B3:C3), and a \c SUM of another column
        or another sheet do not total the rows above; the row is a data row.

        This rule is used both when a sheet is profiled and when its records are read with their formulas, so a
        totals row is recognized the same way at design time and at run time.

        @param ref the A1 reference of the cell holding the formula, for example \c "B10"; \c "$" markers are allowed
        @param formula the formula text, with or without the leading \c "="

        @return True if the formula totals the rows above it in its own column

        @throw TABULAR-PROFILE-ERROR \a ref is not an A1 cell reference

        @par Example:
        @code{.py}
bool totals = TabularProfiler::isTotalsFormula("B10", "SUM(B2:B9)"); # True
bool line_total = TabularProfiler::isTotalsFormula("D3", "SUM(B3:C3)"); # False
        @endcode

        @see
        - @ref hasTotalsFormula() for the formulas of a row
        - @ref isTotalsLabel() for the other way a row is recognized as a totals row

        @since %DataProvider 3.9
    */
    static bool isTotalsFormula(string ref, string formula) {
        *list<*string> cell = (ref =~ x/^\s*\$?([A-Za-z]{1,3})\$?([1-9][0-9]*)\s*$/);
        if (!cell) {
            throw "TABULAR-PROFILE-ERROR", sprintf("%y is not an A1 cell reference", ref);
        }
        int col = columnIndex(cell[0]);
        int row = cell[1].toInt();
        formula =~ s/^\s*=//;
        if (!regex(formula, TotalsFormulaRegex, RE_Caseless)) {
            return False;
        }
        # the cell ranges of the formula that are not qualified by a sheet name; each match gives four strings: the
        # first column and row, and the last column and row
        list<*string> parts = (formula =~ x/(?<![A-Za-z0-9_.!'$])\$?([A-Za-z]{1,3})\$?([0-9]+):\$?([A-Za-z]{1,3})\$?([0-9]+)/g) ?? ();
        @assert(!(parts.size() % 4));
        for (int i = 0; i < parts.size(); i += 4) {
            int c1 = columnIndex(parts[i]);
            int c2 = columnIndex(parts[i + 2]);
            int r2 = max(parts[i + 1].toInt(), parts[i + 3].toInt());
            if (col >= min(c1, c2) && col <= max(c1, c2) && r2 < row) {
                return True;
            }
        }
        return False;
    }

    #! Returns True if one of the formulas of a row makes it a totals row
    /** @param formulas the formulas of the cells of one row, keyed by A1 cell reference, as in
        @ref TabularSheetGrid::formulas "TabularSheetGrid::formulas" and the formula field of a record read with its
        formulas

        @return True if one of the formulas totals the rows above it in its own column (see @ref isTotalsFormula())

        @throw TABULAR-PROFILE-ERROR a key is not an A1 cell reference

        @since %DataProvider 3.9
    */
    static bool hasTotalsFormula(*hash<string, string> formulas) {
        foreach hash<auto> i in (formulas.pairIterator()) {
            if (isTotalsFormula(i.key, i.value)) {
                return True;
            }
        }
        return False;
    }

    #! Returns the 0-based column index of a column letter, for example 0 for \c "A" and 26 for \c "AA"
    private static int columnIndex(string letter) {
        int rv = 0;
        foreach string c in (letter.upr().split("")) {
            rv = rv * 26 + (ord(c) - 64);
        }
        return rv - 1;
    }

    #! Redacts a value to its shape: letters become \c x or \c X and digits \c 9
    /** @param value the value

        @return the redacted value; @ref NOTHING and booleans are returned unchanged
    */
    static auto redact(auto value) {
        switch (value.typeCode()) {
            case NT_NOTHING:
            case NT_NULL:
            case NT_BOOLEAN:
                return value;
            case NT_DATE:
                return "9999-99-99";
        }
        # a spreadsheet stores whole numbers as floats; they are classified and redacted as integers
        string str = (inlist(value.typeCode(), (NT_FLOAT, NT_NUMBER)) && value == value.toInt())
            ? value.toInt().toString()
            : value.toString();
        string rv;
        int len = str.length();
        for (int i = 0; i < len; ++i) {
            if (i == MaxRedactedLength) {
                rv += "…";
                break;
            }
            string c = str[i];
            if (c =~ /^[0-9]$/) {
                rv += "9";
            } else if (c =~ /^\p{Lu}$/u) {
                rv += "X";
            } else if (c =~ /^\p{L}$/u) {
                rv += "x";
            } else {
                rv += c;
            }
        }
        return rv;
    }

    #! Classifies a string cell value
    private static hash<auto> classifyString(string value) {
        string s = trim(value);
        if (!s.val()) {
            return {"type": "empty"};
        }
        # an amount or rate: a number with a currency, "%", parentheses or a trailing minus
        *hash<auto> amount = parseAmount(s);
        if (amount) {
            hash<auto> number = classifyString(amount.number);
            if (number.type == "int" || number.type == "number") {
                return {
                    "type": "number",
                    "text": True,
                    "number_format": number.number_format,
                    "amount": amount - "number",
                };
            }
        }
        # every number and date starts with a digit or a sign
        if (s !~ /^[0-9+-]/) {
            return s =~ /^(true|false)$/i ? {"type": "bool", "text": True} : {"type": "string", "text": True};
        }
        if (s =~ /^[+-]?[0-9]+$/) {
            if (s =~ /^[+-]?0[0-9]/) {
                return {"type": "string", "text": True, "leading_zero": True};
            }
            return {"type": "int", "text": True};
        }
        if (s =~ /^[+-]?[0-9]+\.[0-9]+$/ || s =~ /^[+-]?[0-9]{1,3}(,[0-9]{3})+(\.[0-9]+)?$/) {
            return {"type": "number", "text": True, "number_format": "decimal_point"};
        }
        if (s =~ /^[+-]?[0-9]+,[0-9]+$/ || s =~ /^[+-]?[0-9]{1,3}(\.[0-9]{3})+(,[0-9]+)?$/
            || s =~ /^[+-]?[0-9]{1,3}( [0-9]{3})+(,[0-9]+)?$/) {
            return {"type": "number", "text": True, "number_format": "decimal_comma"};
        }
        if (s =~ /^[0-9]{4}-[0-9]{2}-[0-9]{2}([ T][0-9]{2}:[0-9]{2}(:[0-9]{2})?)?$/) {
            return {"type": "date", "text": True, "date_format": "YYYY-MM-DD"};
        }
        *list<*string> parts = (s =~ x/^([0-9]{1,2})([.\/-])([0-9]{1,2})[.\/-]([0-9]{4})$/);
        if (parts) {
            return {
                "type": "date",
                "text": True,
                "separator": parts[1],
                "d1": parts[0].toInt(),
                "d2": parts[2].toInt(),
            };
        }
        return {"type": "string", "text": True};
    }

    #! Splits a number written with amount or rate parts into the parts and the number
    /** Follows the number mask shapes \c "(...)", a trailing \c "-", one currency symbol (Unicode \c Sc) or
        ISO 4217 code before or after the number, and \c "%"

        @return the parts and the remaining \c number text, or @ref NOTHING if the text has none of these parts
    */
    private static *hash<auto> parseAmount(string s) {
        # U+2212 is a minus sign
        s = replace(s, "\u2212", "-");
        hash<auto> rv;
        if (s =~ /^\(.*\)$/u) {
            rv.parentheses = True;
            s = trim(s.substr(1, -1));
        } else if (s =~ /[0-9%\p{Sc}A-Z ]-$/u && s !~ /^-/) {
            rv.trailing_minus = True;
            s = trim(s.substr(0, -1));
        }
        string sign;
        if (s =~ /^[-+]/) {
            sign = s[0];
            s = trim(s.substr(1));
        }
        # a currency before the number
        *list<*string> pre = (s =~ x/^(\p{Sc}|[A-Z]{3})( *)(.*)$/u);
        if (pre && (pre[0] !~ /^[A-Z]{3}$/ || CurrencyCodeSet{pre[0]})) {
            rv.currency = pre[0];
            rv.currency_position = "before";
            rv.currency_space = pre[1].val();
            s = pre[2];
            if (!sign && s =~ /^[-+]/) {
                sign = s[0];
                s = trim(s.substr(1));
            }
        }
        # a currency after the number, then "%"
        if (s =~ /\s*%$/) {
            rv.percent = True;
            s = trim(s.substr(0, -1));
        }
        if (!rv.currency) {
            *list<*string> post = (s =~ x/^(.*?)( *)(\p{Sc}|[A-Z]{3})$/u);
            if (post && (post[2] !~ /^[A-Z]{3}$/ || CurrencyCodeSet{post[2]})) {
                rv.currency = post[2];
                rv.currency_position = "after";
                rv.currency_space = post[1].val();
                s = post[0];
            }
        }
        if (!rv) {
            return;
        }
        # negative at most once, and the rest must be the number itself
        if (sign && (rv.parentheses || rv.trailing_minus)) {
            return;
        }
        if (s !~ /^[0-9]/ || (rv.currency && rv.percent)) {
            return;
        }
        rv.number = s;
        return rv;
    }

    #! Profiles one sheet
    private static hash<TabularSheetProfile> profileSheet(hash<TabularSheetGrid> sheet,
            hash<TabularProfileOptions> options) {
        list<list<auto>> rows = sheet.rows;
        int read_rows = rows.size();
        # each cell is classified once; the passes below read the cell types
        list<list<string>> types = ();
        foreach list<auto> row in (rows) {
            list<string> row_types = (map classify($1).type, row) ?? ();
            push types, row_types;
        }
        hash<TabularSheetProfile> rv({
            "name": sheet.name,
            "hidden": sheet.hidden,
            "row_count": sheet.row_count ?? read_rows,
            "formula_count": sheet.formulas.size(),
        });
        if (sheet.hidden) {
            rv.findings += <TabularProfileFinding>{"code": "hidden_sheet"};
        }
        if (sheet.row_count > read_rows) {
            rv.findings += <TabularProfileFinding>{
                "code": "partial_profile",
                "detail": read_rows,
            };
        }
        if (sheet.formulas) {
            rv.findings += <TabularProfileFinding>{
                "code": "formulas",
                "detail": formulaFunctions(sheet.formulas),
            };
        }

        # the index of the last row with content
        int last_row = -1;
        for (int r = read_rows - 1; r >= 0; --r) {
            if (nonEmptyCount(types[r])) {
                last_row = r;
                break;
            }
        }
        if (last_row < 0) {
            return rv;
        }

        *int header = findHeaderRow(types, last_row, options.header_search_rows);
        int first_data;
        if (exists header) {
            rv.header_row = header + 1;
            first_data = header + 1;
            for (int r = 0; r < header; ++r) {
                if (nonEmptyCount(types[r])) {
                    rv.title_rows += r + 1;
                }
            }
            if (rv.title_rows) {
                rv.findings += <TabularProfileFinding>{
                    "code": "title_rows",
                    "detail": rv.title_rows,
                };
            }
        } else {
            rv.findings += <TabularProfileFinding>{"code": "no_header_row"};
            first_data = 0;
            while (!nonEmptyCount(types[first_data])) {
                ++first_data;
            }
        }

        # the columns: every column with a header or data
        int last_col = -1;
        for (int r = header ?? first_data; r <= last_row; ++r) {
            int width = lastNonEmptyIndex(types[r]);
            if (width > last_col) {
                last_col = width;
            }
        }
        int first_col = last_col;
        for (int r = header ?? first_data; r <= last_row; ++r) {
            *int first = firstNonEmptyIndex(types[r]);
            if (exists first && first < first_col) {
                first_col = first;
            }
        }
        if (first_data <= last_row) {
            rv.data_region = <TabularCellRange>{
                "first_row": first_data + 1,
                "last_row": last_row + 1,
                "first_col": first_col + 1,
                "last_col": last_col + 1,
            };
        }

        # data rows, totals rows, and blank-row gaps
        list<int> data_rows = ();
        *int gap_start;
        for (int r = first_data; r <= last_row; ++r) {
            if (!nonEmptyCount(types[r])) {
                if (!exists gap_start) {
                    gap_start = r;
                }
                continue;
            }
            if (exists gap_start) {
                rv.blank_row_gaps += {"first_row": gap_start + 1, "last_row": r};
                rv.findings += <TabularProfileFinding>{
                    "code": "blank_row_gap",
                    "row": gap_start + 1,
                    "detail": r - gap_start,
                };
                remove gap_start;
            }
            if (isTotalsRow(rows[r], types[r], r, sheet.formulas)) {
                rv.totals_rows += r + 1;
                rv.findings += <TabularProfileFinding>{"code": "totals_row", "row": r + 1};
                continue;
            }
            data_rows += r;
        }

        # merged cells overlapping the data region
        foreach hash<TabularCellRange> range in (sheet.merged) {
            if (range.last_row >= first_data && range.first_row <= last_row) {
                rv.findings += <TabularProfileFinding>{
                    "code": "merged_cells_in_data",
                    "row": range.first_row + 1,
                    "col": range.first_col + 1,
                    "detail": sprintf("%s%d:%s%d", columnLetter(range.first_col), range.first_row + 1,
                        columnLetter(range.last_col), range.last_row + 1),
                };
            }
        }

        # columns
        hash<string, int> header_seen = {};
        int period_headers = 0;
        for (int c = first_col; c <= last_col; ++c) {
            *string label;
            if (exists header) {
                auto cell = rows[header][c];
                string cell_type = types[header][c] ?? "empty";
                if (cell_type != "empty") {
                    label = trim(cell.toString());
                    if (cell_type == "date" || regex(label, PeriodHeaderRegex, RE_Caseless)) {
                        ++period_headers;
                    }
                }
            }
            hash<TabularColumnProfile> column = profileColumn(rows, types, data_rows, c, label, options);
            if (exists header) {
                if (!exists label) {
                    if (column.non_empty) {
                        rv.findings += <TabularProfileFinding>{"code": "empty_header", "col": c + 1};
                    }
                } else if (header_seen{label.lwr()}) {
                    rv.findings += <TabularProfileFinding>{
                        "code": "duplicate_header",
                        "col": c + 1,
                        "detail": label,
                    };
                } else {
                    header_seen{label.lwr()} = c + 1;
                }
            }
            rv.findings += columnFindings(column);
            rv.columns += column;
        }
        if (period_headers >= MinPeriodColumns) {
            rv.findings += <TabularProfileFinding>{
                "code": "wide_period_columns",
                "row": header + 1,
                "detail": period_headers,
            };
        }

        # samples
        foreach int r in (data_rows) {
            if (rv.samples.size() >= options.sample_rows) {
                break;
            }
            list<auto> sample = ();
            for (int c = first_col; c <= last_col; ++c) {
                auto value = rows[r][c];
                if (value.typeCode() == NT_STRING) {
                    value = trim(value);
                }
                sample += options.redact ? redact(value) : value;
            }
            rv.samples += (sample,);
        }
        return rv;
    }

    #! Returns the 0-based header row, if one is found
    /** The header is the first row, among the leading rows, whose cells are all labels (text that is not a
        number, date, or boolean), that is at least half as wide as the widest of the leading rows, and whose next
        row with content is data rather than more labels; a key/value line such as \c "Supplier: ACME" above the
        real header is therefore not taken as the header.  When every leading row is labels, as in a sheet of text
        columns, the first label row as wide as the widest row is the header.
    */
    private static *int findHeaderRow(list<list<string>> types, int last_row, int search_rows) {
        int limit = search_rows < last_row ? search_rows : last_row;
        int widest = 0;
        for (int r = 0; r <= limit; ++r) {
            int width = nonEmptyCount(types[r]);
            if (width > widest) {
                widest = width;
            }
        }
        int min_width = widest < 2 ? widest : 2;
        *int fallback;
        for (int r = 0; r < limit; ++r) {
            int width = nonEmptyCount(types[r]);
            if (width < min_width || width * 2 < widest || !isLabelRow(types[r])) {
                continue;
            }
            if (!exists fallback && width == widest) {
                fallback = r;
            }
            for (int n = r + 1; n <= last_row; ++n) {
                if (nonEmptyCount(types[n])) {
                    if (!isLabelRow(types[n])) {
                        return r;
                    }
                    break;
                }
            }
        }
        return fallback;
    }

    #! Returns True if every non-empty cell in the row is a label
    private static bool isLabelRow(*list<string> row) {
        foreach string type in (row) {
            if (type != "empty" && type != "string") {
                return False;
            }
        }
        return True;
    }

    #! The yes/no pairs a column of text is recognized by: each true value and its false value, in lower case
    const BoolPairs = (
        ("true", "false"), ("yes", "no"), ("y", "n"), ("t", "f"), ("on", "off"), ("ja", "nein"),
        ("oui", "non"), ("si", "no"), ("sí", "no"), ("ano", "ne"), ("áno", "nie"), ("tak", "nie"),
        ("wahr", "falsch"), ("✓", "✗"), ("✔", "✘"), ("☑", "☐"),
    );

    #! The values that mark a yes in a column whose other cells are empty
    const BoolMarks = ("x", "✓", "✔", "☑");

    #! Recognizes a yes/no column from its distinct values
    /** @param spellings the distinct non-empty values in lower case, each with its spellings and their counts
        @param empty the number of empty cells

        @return @c bool_true, @c bool_false and @c empty_is_false, or nothing
    */
    private static *hash<auto> boolColumn(hash<string, hash<string, int>> spellings, int empty) {
        list<string> values = keys spellings;
        # the most frequent way the file writes a value
        code written = string sub (string v) {
            string rv;
            int n = -1;
            foreach hash<auto> i in (spellings{v}.pairIterator()) {
                if (i.value > n) {
                    rv = i.key;
                    n = i.value;
                }
            }
            return rv;
        };
        if (values.size() == 1 && empty && inlist(values[0], BoolMarks)) {
            return {"bool_true": (written(values[0]),), "bool_false": (), "empty_is_false": True};
        }
        foreach list<string> pair in (BoolPairs) {
            # both values, or one of them, but nothing else
            if (values.size() <= 2 && !(select values, !inlist($1, pair))) {
                if (values.size() == 1 && !empty) {
                    # one value and no other: not enough to call it yes/no
                    return;
                }
                return {
                    "bool_true": inlist(pair[0], values) ? (written(pair[0]),) : (),
                    "bool_false": inlist(pair[1], values) ? (written(pair[1]),) : (),
                    "empty_is_false": False,
                };
            }
        }
    }

    #! Profiles one column of the data rows
    private static hash<TabularColumnProfile> profileColumn(list<list<auto>> rows, list<list<string>> types,
            list<int> data_rows, int col,
            *string label, hash<TabularProfileOptions> options) {
        hash<TabularColumnProfile> rv({
            "index": col + 1,
            "letter": columnLetter(col),
            "header": label,
            "type": "empty",
        });
        hash<string, int> counts = {};
        # day/month components of day/month dates stored as text
        bool first_over_12;
        bool second_over_12;
        *string date_separator;
        bool iso_dates;
        bool decimal_comma;
        bool decimal_point;
        # amount and rate parts of numbers stored as text
        *hash<TabularAmountFormat> amount_format;
        hash<string, int> currencies = {};
        # the distinct text values, for yes/no columns: lower case -> spelling -> count; up to a few only
        hash<string, hash<string, int>> spellings = {};
        bool many_values;
        # cells with text from the fixed empty-marker vocabulary
        hash<string, int> empty_markers = {};
        foreach int r in (data_rows) {
            if ((types[r][col] ?? "empty") == "empty") {
                ++rv.empty;
                continue;
            }
            if (rows[r][col].typeCode() == NT_STRING) {
                string text = trim(rows[r][col]);
                if (EmptyMarkerSet{text}) {
                    ++empty_markers{text};
                }
            }
            hash<auto> cls = classify(rows[r][col]);
            ++rv.non_empty;
            if (!many_values && (cls.type == "string" || cls.type == "bool")) {
                string v = trim(rows[r][col].toString());
                if (v.length() <= 40) {
                    ++spellings{v.lwr()}{v};
                }
                if (spellings.size() > 2) {
                    many_values = True;
                }
            } else if (cls.type != "string" && cls.type != "bool") {
                many_values = True;
            }
            ++counts{cls.type};
            if (cls.leading_zero) {
                rv.leading_zeros = True;
            }
            if (cls.type == "date" && cls.text) {
                rv.dates_as_text = True;
                if (cls.date_format) {
                    iso_dates = True;
                } else {
                    date_separator = cls.separator;
                    if (cls.d1 > 12) {
                        first_over_12 = True;
                    }
                    if (cls.d2 > 12) {
                        second_over_12 = True;
                    }
                }
            }
            if (cls.number_format == "decimal_comma") {
                decimal_comma = True;
            } else if (cls.number_format == "decimal_point") {
                decimal_point = True;
            }
            if (cls.amount) {
                if (!amount_format) {
                    amount_format = <TabularAmountFormat>{};
                }
                if (cls.amount.currency) {
                    ++currencies{cls.amount.currency};
                    if (!amount_format.currency_position) {
                        amount_format.currency_position = cls.amount.currency_position;
                        amount_format.currency_space = cls.amount.currency_space;
                    }
                }
                map amount_format{$1} = True, ("percent", "parentheses", "trailing_minus"), cls.amount{$1};
            }
        }
        if (empty_markers) {
            rv.empty_markers = empty_markers;
        }
        if (!rv.non_empty) {
            return rv;
        }

        # numbers include integers; leading-zero identifiers keep a column of digit strings a string column
        int numeric = (counts.int ?? 0) + (counts.number ?? 0);
        string type;
        int matching;
        if (counts.number && numeric * 2 > rv.non_empty) {
            type = "number";
            matching = numeric;
        } else {
            foreach hash<auto> i in (counts.pairIterator()) {
                if (i.value > matching) {
                    type = i.key;
                    matching = i.value;
                }
            }
        }
        rv.type = type;
        rv.confidence = matching.toFloat() / rv.non_empty;

        # a yes/no column written as text: a known pair, or a mark with empty cells
        if (!many_values && spellings && (type == "string" || type == "bool")) {
            *hash<auto> yes_no = boolColumn(spellings, rv.empty);
            if (yes_no) {
                rv.type = "bool";
                rv.confidence = 1.0;
                rv += yes_no;
            } else if (spellings.size() == 2 && type == "string" && rv.non_empty >= 4
                    && !(select keys spellings, $1 !~ /^\p{L}[\p{L} ]{0,19}$/u)) {
                # two words, neither a known pair, such as good/bad or pass/fail: maybe yes/no, but which is yes
                # is the user's to say; identifiers and codes with digits are not candidates
                rv.two_values = map options.redact ? redact($1) : $1,
                    (map (keys spellings{$1})[0], keys spellings);
            }
        }

        if (rv.dates_as_text && type == "date") {
            if (iso_dates && !date_separator) {
                rv.date_format = "YYYY-MM-DD";
            } else if (date_separator) {
                if (first_over_12 && !second_over_12) {
                    rv.date_format = sprintf("DD%sMM%sYYYY", date_separator, date_separator);
                } else if (second_over_12 && !first_over_12) {
                    rv.date_format = sprintf("MM%sDD%sYYYY", date_separator, date_separator);
                }
            }
        }
        if (decimal_comma) {
            rv.number_format = "decimal_comma";
        } else if (decimal_point) {
            rv.number_format = "decimal_point";
        }
        if (amount_format && rv.type == "number") {
            # the most frequent currency
            int n;
            foreach hash<auto> i in (currencies.pairIterator()) {
                if (i.value > n) {
                    amount_format.currency = i.key;
                    n = i.value;
                }
            }
            rv.amount_format = amount_format;
        }

        # counter-examples
        foreach int r in (data_rows) {
            if (rv.counter_examples.size() >= options.max_counter_examples) {
                break;
            }
            auto value = rows[r][col];
            string cell_type = types[r][col] ?? "empty";
            if (cell_type == "empty" || cell_type == type || (type == "number" && cell_type == "int")) {
                continue;
            }
            if (value.typeCode() == NT_STRING) {
                value = trim(value);
            }
            rv.counter_examples += {
                "row": r + 1,
                "value": options.redact ? redact(value) : value,
            };
        }
        return rv;
    }

    #! Returns the findings for one column
    private static list<hash<TabularProfileFinding>> columnFindings(hash<TabularColumnProfile> column) {
        list<hash<TabularProfileFinding>> rv = ();
        if (column.confidence < 1.0 && column.type != "empty") {
            rv += <TabularProfileFinding>{
                "code": "mixed_types",
                "col": column.index,
                "detail": column.type,
            };
        }
        if (column.dates_as_text && column.type == "date") {
            rv += <TabularProfileFinding>{
                "code": "dates_as_text",
                "col": column.index,
                "detail": column.date_format,
            };
            if (!column.date_format) {
                rv += <TabularProfileFinding>{"code": "ambiguous_date_order", "col": column.index};
            }
        }
        if (column.number_format == "decimal_comma") {
            rv += <TabularProfileFinding>{"code": "decimal_comma_numbers", "col": column.index};
        }
        if (column.leading_zeros) {
            rv += <TabularProfileFinding>{"code": "leading_zero_identifiers", "col": column.index};
        }
        if (column.two_values) {
            rv += <TabularProfileFinding>{"code": "two_values", "col": column.index, "detail": column.two_values};
        }
        return rv;
    }

    #! Returns True if the row holds totals or subtotals
    private static bool isTotalsRow(list<auto> row, list<string> row_types, int r, hash<string, string> formulas) {
        foreach auto cell in (row) {
            if (row_types[$#] == "empty") {
                continue;
            }
            # the first cell with content labels the row
            if (isTotalsLabel(cell)) {
                return True;
            }
            break;
        }
        int c = 0;
        foreach auto cell in (row) {
            string ref = columnLetter(c) + (r + 1);
            *string formula = formulas{ref};
            if (formula && isTotalsFormula(ref, formula)) {
                return True;
            }
            ++c;
        }
        return False;
    }

    #! Returns the sorted, unique function names used in formulas
    private static list<string> formulaFunctions(hash<string, string> formulas) {
        hash<string, bool> names = {};
        foreach string formula in (formulas.iterator()) {
            foreach string name in (formula =~ x/([A-Za-z][A-Za-z0-9.]*)\s*\(/g) {
                names{name.upr()} = True;
            }
        }
        return sort(keys names);
    }

    #! Returns the number of non-empty cells in a row
    private static int nonEmptyCount(*list<string> row) {
        int rv = 0;
        foreach string type in (row) {
            if (type != "empty") {
                ++rv;
            }
        }
        return rv;
    }

    #! Returns the 0-based index of the last non-empty cell in a row, or -1
    private static int lastNonEmptyIndex(*list<string> row) {
        for (int c = row.size() - 1; c >= 0; --c) {
            if (row[c] != "empty") {
                return c;
            }
        }
        return -1;
    }

    #! Returns the 0-based index of the first non-empty cell in a row
    private static *int firstNonEmptyIndex(*list<string> row) {
        int c = 0;
        foreach string type in (row) {
            if (type != "empty") {
                return c;
            }
            ++c;
        }
    }
}
}
