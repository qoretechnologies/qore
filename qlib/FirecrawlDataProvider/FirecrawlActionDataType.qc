# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlActionDataType.qc browser action type for Firecrawl scrape requests

/*  FirecrawlActionDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Browser action type for Firecrawl scrape requests
/** Each action has a \c type field selecting the action kind, plus action-specific parameters.

    @par Available action types
    - \c wait — wait for a duration (\c milliseconds) or element (\c selector)
    - \c click — click an element (\c selector, optional \c all)
    - \c write — type text into the focused element (\c text)
    - \c press — press a keyboard key (\c key)
    - \c scroll — scroll the page (\c direction, optional \c selector)
    - \c screenshot — take a screenshot (\c fullPage, \c quality, \c viewport)
    - \c scrape — capture the page content at its current state
    - \c executeJavascript — run JavaScript code (\c script)
    - \c pdf — generate a PDF (\c format, \c landscape, \c scale)
*/
public class FirecrawlActionDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Action Type",
                "short_desc": "The type of browser action to perform",
                "desc": "The type of browser action to perform.\n\n"
                    "**Available actions**:\n"
                    "- `wait` — wait for duration (`milliseconds`) or element (`selector`)\n"
                    "- `click` — click an element (`selector`; set `all` for all matches)\n"
                    "- `write` — type text into the focused element (`text`)\n"
                    "- `press` — press a keyboard key (`key`)\n"
                    "- `scroll` — scroll the page (`direction`: `up`/`down`; optional "
                    "`selector`)\n"
                    "- `screenshot` — take a screenshot (`fullPage`, `quality`)\n"
                    "- `scrape` — capture the page content at its current state\n"
                    "- `executeJavascript` — run JavaScript code (`script`)\n"
                    "- `pdf` — generate a PDF (`format`, `landscape`, `scale`)",
                "type": StringType,
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "wait", "display_name": "Wait"},
                    <AllowedValueInfo>{"value": "click", "display_name": "Click"},
                    <AllowedValueInfo>{"value": "write", "display_name": "Write"},
                    <AllowedValueInfo>{"value": "press", "display_name": "Press Key"},
                    <AllowedValueInfo>{"value": "scroll", "display_name": "Scroll"},
                    <AllowedValueInfo>{"value": "screenshot", "display_name": "Screenshot"},
                    <AllowedValueInfo>{"value": "scrape", "display_name": "Scrape"},
                    <AllowedValueInfo>{"value": "executeJavascript",
                        "display_name": "Execute JavaScript"},
                    <AllowedValueInfo>{"value": "pdf", "display_name": "PDF"},
                ),
            },
            "selector": {
                "display_name": "Selector",
                "short_desc": "CSS selector for the target element",
                "desc": "CSS selector for the target element. Used by `wait`, `click`, and "
                    "`scroll` actions",
                "type": StringOrNothingType,
                "example_value": "#content",
            },
            "milliseconds": {
                "display_name": "Milliseconds",
                "short_desc": "Wait duration in milliseconds",
                "desc": "Duration to wait in milliseconds. Used by the `wait` action when "
                    "waiting for a fixed duration instead of a selector",
                "type": SoftIntOrNothingType,
                "example_value": 2000,
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text to type",
                "desc": "Text to type into the currently focused element. Used by the "
                    "`write` action",
                "type": StringOrNothingType,
            },
            "key": {
                "display_name": "Key",
                "short_desc": "Keyboard key to press",
                "desc": "Keyboard key to press (e.g., `Enter`, `Tab`, `Escape`). Used by "
                    "the `press` action",
                "type": StringOrNothingType,
                "example_value": "Enter",
            },
            "script": {
                "display_name": "Script",
                "short_desc": "JavaScript code to execute",
                "desc": "JavaScript code to execute in the page context. Used by the "
                    "`executeJavascript` action",
                "type": StringOrNothingType,
            },
            "direction": {
                "display_name": "Direction",
                "short_desc": "Scroll direction",
                "desc": "Scroll direction. Used by the `scroll` action. Defaults to `down`",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "up", "display_name": "Up"},
                    <AllowedValueInfo>{"value": "down", "display_name": "Down"},
                ),
            },
            "fullPage": {
                "display_name": "Full Page",
                "short_desc": "Capture the full page",
                "desc": "If `True`, captures the full scrollable page. Used by the "
                    "`screenshot` action. Defaults to `False`",
                "type": SoftBoolOrNothingType,
            },
            "quality": {
                "display_name": "Quality",
                "short_desc": "Screenshot quality (1-100)",
                "desc": "Image quality for the screenshot, in the range `1`-`100`. Used by "
                    "the `screenshot` action",
                "type": SoftIntOrNothingType,
                "example_value": 80,
            },
            "viewport": {
                "display_name": "Viewport",
                "short_desc": "Viewport dimensions for the screenshot",
                "desc": "Viewport dimensions for the screenshot, with `width` and `height` "
                    "keys (both integers, in pixels). Used by the `screenshot` action",
                "type": new FirecrawlViewportDataType(AutoHashOrNothingType),
                "example_value": {"width": 1280, "height": 720},
            },
            "all": {
                "display_name": "All Matches",
                "short_desc": "Click all matching elements",
                "desc": "If `True`, all elements matching the `selector` are clicked instead "
                    "of just the first match. Used by the `click` action. Defaults to `False`",
                "type": SoftBoolOrNothingType,
            },
            "format": {
                "display_name": "Format",
                "short_desc": "PDF page format",
                "desc": "Page format for the generated PDF. Used by the `pdf` action. "
                    "Defaults to `Letter`",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "A0", "display_name": "A0"},
                    <AllowedValueInfo>{"value": "A1", "display_name": "A1"},
                    <AllowedValueInfo>{"value": "A2", "display_name": "A2"},
                    <AllowedValueInfo>{"value": "A3", "display_name": "A3"},
                    <AllowedValueInfo>{"value": "A4", "display_name": "A4"},
                    <AllowedValueInfo>{"value": "A5", "display_name": "A5"},
                    <AllowedValueInfo>{"value": "A6", "display_name": "A6"},
                    <AllowedValueInfo>{"value": "Letter", "display_name": "Letter"},
                    <AllowedValueInfo>{"value": "Legal", "display_name": "Legal"},
                    <AllowedValueInfo>{"value": "Tabloid", "display_name": "Tabloid"},
                    <AllowedValueInfo>{"value": "Ledger", "display_name": "Ledger"},
                ),
            },
            "landscape": {
                "display_name": "Landscape",
                "short_desc": "Use landscape orientation",
                "desc": "If `True`, the PDF is generated in landscape orientation. Used by "
                    "the `pdf` action. Defaults to `False`",
                "type": SoftBoolOrNothingType,
            },
            "scale": {
                "display_name": "Scale",
                "short_desc": "PDF rendering scale",
                "desc": "Scale factor for PDF rendering. Used by the `pdf` action. Defaults "
                    "to `1`",
                "type": SoftFloatOrNothingType,
                "example_value": 1.0,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
