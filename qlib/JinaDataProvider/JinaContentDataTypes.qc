# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaContentDataTypes class definition

/** JinaContentDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
#! JinaBatchInputBody fields
public class JinaBatchInputBodyDataType inherits HashDataType {
    public {
        const Fields = {
            "input": {
                "display_name": "Text",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."string", AbstractDataProviderType::get("list<string>"))),
                "desc": "Text or a list of texts to embed for this record.",
                "attr": {"required": True},
                "example_value": "Delivery status",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaBatchInputBody") {
        addQoreFields(Fields);
    }
}

public const JinaBatchInputBodyDataType = new JinaBatchInputBodyDataType();

#! JinaBatchInputRecord fields
public class JinaBatchInputRecordDataType inherits HashDataType {
    public {
        const Fields = {
            "custom_id": {
                "display_name": "Custom ID",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Your identifier for matching this record to its result.",
                "attr": {"required": True},
                "example_value": "ticket-1",
            },
            "body": {
                "display_name": "Request Body",
                "type": JinaBatchInputBodyDataType,
                "desc": "Embedding input for this individual batch record.",
                "attr": {"required": True},
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaBatchInputRecord") {
        addQoreFields(Fields);
    }

    #! Rejects records that Jina's asynchronous worker cannot associate with a result
    /** @param value the inline batch record
        @return the validated record
        @throw JINA-REQUEST-ERROR the custom ID or input is empty
    */
    auto acceptsValue(auto value) {
        hash<auto> record = HashDataType::acceptsValue(value);
        if (!record.custom_id.val() || !record.body.input.val()) {
            throw "JINA-REQUEST-ERROR", "batch records require a nonempty custom_id and body.input";
        }
        return record;
    }
}

public const JinaBatchInputRecordDataType = new JinaBatchInputRecordDataType();

#! JinaViewport fields
public class JinaViewportDataType inherits HashDataType {
    public {
        const Fields = {
            "width": {
                "display_name": "Width",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Browser width in pixels.",
                "example_value": 1280,
                "attr": {
                    "required": True,
                },
            },
            "height": {
                "display_name": "Height",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Browser height in pixels.",
                "example_value": 720,
                "attr": {
                    "required": True,
                },
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaViewport") {
        addQoreFields(Fields);
    }
}

public const JinaViewportDataType = new JinaViewportDataType();

#! JinaTokenUsage fields
public class JinaTokenUsageDataType inherits HashDataType {
    private {
        const Fields = {
            "tokens": {
                "display_name": "Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Tokens used by content processing.",
            },
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Tokens consumed by the input.",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Tokens generated in the response.",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Total tokens consumed.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaTokenUsage") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaTokenUsageDataType = new JinaTokenUsageDataType();

#! JinaChatImage fields
public class JinaChatImageDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Image URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Public image URL or base64 data URI.",
                "example_value": "https://example.com/product.png",
                "attr": {
                    "required": True,
                },
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaChatImage") {
        addQoreFields(Fields);
    }
}

public const JinaChatImageDataType = new JinaChatImageDataType();

#! JinaChatPart fields
public class JinaChatPartDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Content Type",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Choose text or an image for this part.",
                "allowed_values": (<AllowedValueInfo>{"value": "text", "display_name": "Text"}, <AllowedValueInfo>{"value": "image_url", "display_name": "Image URL"}),
                "attr": {
                    "required": True,
                },
            },
            "text": {
                "display_name": "Text",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Text when `type` is `text`.",
                "example_value": "Describe this product.",
                "attr": {
                    "exclusive_with": ("image_url",),
                },
            },
            "image_url": {
                "display_name": "Image",
                "type": JinaChatImageDataType.getOrNothingType(),
                "desc": "Image details when `type` is `image_url`.",
                "attr": {
                    "exclusive_with": ("text",),
                },
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaChatPart") {
        addQoreFields(Fields);
    }
}

public const JinaChatPartDataType = new JinaChatPartDataType();

#! Jina ContentOptions fields
public class JinaContentOptionsDataType inherits HashDataType {
    public {
        const Fields = {
            "region": {
                "display_name": "Processing Region",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Select `global` or `eu` infrastructure for Reader and Search.",
                "allowed_values": (<AllowedValueInfo>{"value": "global", "display_name": "Global"}, <AllowedValueInfo>{"value": "eu", "display_name": "European Union"}),
                "default_value": "global",
            },
            "return_format": {
                "display_name": "Content Format",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose the returned content format. Screenshots return image URLs.",
                "allowed_values": (<AllowedValueInfo>{"value": "markdown", "display_name": "Markdown"}, <AllowedValueInfo>{"value": "html", "display_name": "HTML"}, <AllowedValueInfo>{"value": "text", "display_name": "Text"}, <AllowedValueInfo>{"value": "screenshot", "display_name": "Screenshot"}, <AllowedValueInfo>{"value": "pageshot", "display_name": "Full Page Screenshot"}),
                "attr": {
                    "preselected": True,
                },
            },
            "engine": {
                "display_name": "Browser Engine",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose `browser` for rendered pages, `direct` for speed, or the experimental Cloudflare renderer.",
                "allowed_values": (<AllowedValueInfo>{"value": "browser", "display_name": "Browser"}, <AllowedValueInfo>{"value": "direct", "display_name": "Direct"}, <AllowedValueInfo>{"value": "cf-browser-rendering", "display_name": "Cloudflare Browser Rendering"}),
            },
            "with_links_summary": {
                "display_name": "Links Summary",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Include unique links or all links in the output.",
                "allowed_values": (<AllowedValueInfo>{"value": "true", "display_name": "True"}, <AllowedValueInfo>{"value": "all", "display_name": "All"}),
            },
            "with_images_summary": {
                "display_name": "Images Summary",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Include unique images or all images in the output.",
                "allowed_values": (<AllowedValueInfo>{"value": "true", "display_name": "True"}, <AllowedValueInfo>{"value": "all", "display_name": "All"}),
            },
            "no_cache": {
                "display_name": "Bypass Cache",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Use `True` to fetch current content instead of a cached response.",
            },
            "with_generated_alt": {
                "display_name": "Generate Image Captions",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Use `True` to describe images that lack alternative text.",
            },
            "retain_images": {
                "display_name": "Retain Images",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Control whether image URLs or alternative text remain in the content.",
                "allowed_values": (<AllowedValueInfo>{"value": "none", "display_name": "None"}, <AllowedValueInfo>{"value": "all", "display_name": "All"}, <AllowedValueInfo>{"value": "alt", "display_name": "Alternative Text Only"}, <AllowedValueInfo>{"value": "all_p", "display_name": "Images with Generated Captions"}, <AllowedValueInfo>{"value": "alt_p", "display_name": "Generated Captions Only"}),
            },
            "locale": {
                "display_name": "Browser Locale",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Browser locale for localized pages, such as `en-US`.",
                "example_value": "en-US",
            },
            "set_cookie": {
                "display_name": "Forward Cookies",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Cookies to send when fetching a page that requires an authenticated session.",
                "attr": {
                    "sensitive": True,
                },
            },
            "proxy_url": {
                "display_name": "Proxy URL",
                "type": AbstractDataProviderTypeMap."*uri",
                "desc": "Proxy URL for fetching the requested page. May contain proxy credentials.",
                "attr": {
                    "sensitive": True,
                },
            },
            "timeout": {
                "display_name": "Page Timeout",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum seconds to load a page.",
            },
            "token_budget": {
                "display_name": "Token Budget",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum tokens this content request may consume.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaContentOptions") {
        addQoreFields(Fields);
    }
}

public const JinaContentOptionsDataType = new JinaContentOptionsDataType();

#! Jina ReaderRequest fields
public class JinaReaderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Source URL",
                "type": AbstractDataProviderTypeMap."*uri",
                "desc": "The web page or document to read; also resolves relative links in uploaded HTML.",
                "example_value": "https://example.com",
                "attr": {
                    "required_groups": ("source",),
                    "preselected": True,
                },
            },
            "html": {
                "display_name": "HTML Content",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Convert HTML directly, optionally using `url` as its base.",
                "example_value": "<h1>Delivery update</h1><p>Your order has shipped.</p>",
                "attr": {
                    "required_groups": ("source",),
                    "exclusive_with": ("file",),
                },
            },
            "file": {
                "display_name": "Document File",
                "type": FileDataType.getOrNothingType(),
                "desc": "A PDF or Office document to convert. Supply its `name`, `mime_type`, and binary or base64 `content`.",
                "attr": {
                    "required_groups": ("source",),
                    "exclusive_with": ("html",),
                },
            },
        } + JinaContentOptionsDataType::Fields + {
            "page": {
                "display_name": "Document Page",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Read one page of an uploaded document, starting at 1.",
            },
            "viewport": {
                "display_name": "Browser Viewport",
                "type": JinaViewportDataType.getOrNothingType(),
                "desc": "Browser dimensions as `width` and `height` in pixels.",
                "example_value": {"width": 1280, "height": 720},
            },
            "injectPageScript": {
                "display_name": "Page Preparation Script",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "JavaScript source or a script URL to run before extracting content.",
                "example_value": "document.querySelector('nav')?.remove();",
            },
            "preset": {
                "display_name": "Extraction Preset",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Apply a group of extraction settings for reading, indexing, research, agents, or crawling.",
                "allowed_values": (<AllowedValueInfo>{"value": "reader", "display_name": "Reader"}, <AllowedValueInfo>{"value": "index", "display_name": "Index"}, <AllowedValueInfo>{"value": "research", "display_name": "Research"}, <AllowedValueInfo>{"value": "agent", "display_name": "Agent"}, <AllowedValueInfo>{"value": "spider", "display_name": "Spider"}),
            },
            "respond_with": {
                "display_name": "Reader Model",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Use `readerlm-v2` for model-assisted HTML conversion, or `vlm` for visual understanding.",
                "allowed_values": (<AllowedValueInfo>{"value": "readerlm-v2", "display_name": "Readerlm V2"}, <AllowedValueInfo>{"value": "vlm", "display_name": "Vlm"}),
            },
            "retain_links": {
                "display_name": "Retain Links",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Keep links, keep anchor text only, omit links, or use model citation formatting.",
                "allowed_values": (<AllowedValueInfo>{"value": "none", "display_name": "None"}, <AllowedValueInfo>{"value": "all", "display_name": "All"}, <AllowedValueInfo>{"value": "text", "display_name": "Text"}, <AllowedValueInfo>{"value": "gpt-oss", "display_name": "Gpt Oss"}),
            },
            "retain_media": {
                "display_name": "Retain Media",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Control how embedded audio and video appear in the returned content.",
                "allowed_values": (<AllowedValueInfo>{"value": "none", "display_name": "None"}, <AllowedValueInfo>{"value": "text", "display_name": "Text"}, <AllowedValueInfo>{"value": "link", "display_name": "Link"}, <AllowedValueInfo>{"value": "image", "display_name": "Image"}, <AllowedValueInfo>{"value": "html", "display_name": "HTML"}),
            },
            "respond_timing": {
                "display_name": "Page Ready Event",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose when rendering is complete enough to extract the page.",
                "allowed_values": (<AllowedValueInfo>{"value": "html", "display_name": "HTML"}, <AllowedValueInfo>{"value": "visible-content", "display_name": "Visible Content"}, <AllowedValueInfo>{"value": "mutation-idle", "display_name": "Mutation Idle"}, <AllowedValueInfo>{"value": "resource-idle", "display_name": "Resource Idle"}, <AllowedValueInfo>{"value": "media-idle", "display_name": "Media Idle"}, <AllowedValueInfo>{"value": "network-idle", "display_name": "Network Idle"}),
            },
            "base": {
                "display_name": "Link Base",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Resolve relative links using the initial or final URL after redirects.",
                "allowed_values": (<AllowedValueInfo>{"value": "initial", "display_name": "Initial"}, <AllowedValueInfo>{"value": "final", "display_name": "Final"}),
            },
            "markdown_chunking": {
                "display_name": "Markdown Chunking",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Split Markdown by heading level or by structural depth.",
                "allowed_values": (<AllowedValueInfo>{"value": "true", "display_name": "True"}, <AllowedValueInfo>{"value": "h1", "display_name": "H1"}, <AllowedValueInfo>{"value": "h2", "display_name": "H2"}, <AllowedValueInfo>{"value": "h3", "display_name": "H3"}, <AllowedValueInfo>{"value": "h4", "display_name": "H4"}, <AllowedValueInfo>{"value": "h5", "display_name": "H5"}, <AllowedValueInfo>{"value": "structured", "display_name": "Structured"}, <AllowedValueInfo>{"value": "s1", "display_name": "S1"}, <AllowedValueInfo>{"value": "s2", "display_name": "S2"}, <AllowedValueInfo>{"value": "s3", "display_name": "S3"}, <AllowedValueInfo>{"value": "s4", "display_name": "S4"}, <AllowedValueInfo>{"value": "s5", "display_name": "S5"}),
            },
            "target_selector": {
                "display_name": "Include CSS Selectors",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Extract only elements matching these CSS selectors.",
            },
            "wait_for_selector": {
                "display_name": "Wait for CSS Selector",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Extract after the selected page element appears.",
            },
            "remove_selector": {
                "display_name": "Exclude CSS Selectors",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Remove navigation, banners, or other selected elements.",
            },
            "proxy": {
                "display_name": "Proxy Country",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "A country code, `auto`, or `none` for the fetching proxy.",
            },
            "user_agent": {
                "display_name": "User Agent",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Override the browser identification sent to the page.",
            },
            "referer": {
                "display_name": "Referer",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Referer URL to send when fetching the page.",
            },
            "robots_txt": {
                "display_name": "Robots User Agent",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Check robots.txt using this bot name.",
            },
            "md_hr": {
                "display_name": "Markdown Horizontal Rule",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Markdown text to use for horizontal rules.",
            },
            "with_iframe": {
                "display_name": "Include Iframes",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Extract content from embedded frames.",
            },
            "with_shadow_dom": {
                "display_name": "Include Shadow DOM",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Extract content from shadow DOM elements.",
            },
            "keep_img_data_url": {
                "display_name": "Keep Inline Images",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Retain inline base64 image URLs.",
            },
            "do_not_track": {
                "display_name": "Do Not Cache or Track",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Prevent caching and tracking of this request by Jina.",
            },
            "remove_overlay": {
                "display_name": "Remove Overlays",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Remove page overlays before extraction.",
            },
            "detach_invisibles": {
                "display_name": "Remove Invisible Elements",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Remove invisible elements before extraction.",
            },
            "cache_tolerance": {
                "display_name": "Cache Tolerance",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Accept cached content no older than this many seconds; zero requests fresh content.",
            },
            "no_gfm": {
                "display_name": "GitHub Markdown Features",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "true", "display_name": "True"}, <AllowedValueInfo>{"value": "false", "display_name": "False"}, <AllowedValueInfo>{"value": "table", "display_name": "Table"}),
            },
            "md_heading_style": {
                "display_name": "Markdown Heading Style",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "atx", "display_name": "Hash Headings (ATX)"}, <AllowedValueInfo>{"value": "setext", "display_name": "Underlined Headings (Setext)"}),
            },
            "md_bullet_list_marker": {
                "display_name": "Markdown Bullet Marker",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "*", "display_name": "Asterisk (*)"}, <AllowedValueInfo>{"value": "-", "display_name": "Hyphen (-)"}, <AllowedValueInfo>{"value": "+", "display_name": "Plus (+)"}),
            },
            "md_em_delimiter": {
                "display_name": "Markdown Emphasis Marker",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "_", "display_name": "Underscore (_)"}, <AllowedValueInfo>{"value": "*", "display_name": "Asterisk (*)"}),
            },
            "md_strong_delimiter": {
                "display_name": "Markdown Strong Marker",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "**", "display_name": "Double Asterisk (**)"}, <AllowedValueInfo>{"value": "__", "display_name": "Double Underscore (__)"}),
            },
            "md_link_style": {
                "display_name": "Markdown Link Style",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "inlined", "display_name": "Inlined"}, <AllowedValueInfo>{"value": "referenced", "display_name": "Referenced"}, <AllowedValueInfo>{"value": "discarded", "display_name": "Discarded"}),
            },
            "md_link_reference_style": {
                "display_name": "Markdown Reference Style",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose how this Markdown feature is represented.",
                "allowed_values": (<AllowedValueInfo>{"value": "full", "display_name": "Full"}, <AllowedValueInfo>{"value": "collapsed", "display_name": "Collapsed"}, <AllowedValueInfo>{"value": "shortcut", "display_name": "Shortcut"}),
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaReaderRequest") {
        addQoreFields(Fields);
    }
}

public const JinaReaderRequestDataType = new JinaReaderRequestDataType();

#! Jina SearchRequest fields
public class JinaSearchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "q": {
                "display_name": "Search Query",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "What to find on the web. Standard search operators are supported.",
                "example_value": "Qore programming language",
                "attr": {
                    "required": True,
                },
            },
        } + JinaContentOptionsDataType::Fields + {
            "site": {
                "display_name": "Website Filter",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Restrict results to this website.",
                "example_value": "qore.org",
            },
            "gl": {
                "display_name": "Search Country",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Two-letter country code for search results.",
                "example_value": "us",
            },
            "hl": {
                "display_name": "Search Language",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Language code for search results.",
                "example_value": "en",
            },
            "location": {
                "display_name": "Search Location",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "City or region from which the search should originate.",
                "example_value": "Prague, Czech Republic",
            },
            "num": {
                "display_name": "Result Count",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum number of search results.",
                "attr": {
                    "preselected": True,
                },
            },
            "page": {
                "display_name": "Result Offset",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Skip this many results for pagination.",
            },
            "respond_with": {
                "display_name": "Result Content",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose `no-content` to return search metadata without reading the result pages.",
                "allowed_values": (<AllowedValueInfo>{"value": "no-content", "display_name": "No Content"},),
                "attr": {
                    "preselected": True,
                },
            },
            "with_favicons": {
                "display_name": "Include Favicons",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include site icons with the results.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaSearchRequest") {
        addQoreFields(Fields);
    }
}

public const JinaSearchRequestDataType = new JinaSearchRequestDataType();

#! Jina Page fields
public class JinaPageDataType inherits HashDataType {
    private {
        const Fields = {
            "title": {
                "display_name": "Title",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Page or document title.",
            },
            "url": {
                "display_name": "Source URL",
                "type": AbstractDataProviderTypeMap."*uri",
                "desc": "URL of the source page.",
            },
            "content": {
                "display_name": "Content",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Extracted text or Markdown.",
            },
            "description": {
                "display_name": "Description",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Page summary or search snippet.",
            },
            "links": {
                "display_name": "Links",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Discovered links keyed by label.",
            },
            "images": {
                "display_name": "Images",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Discovered images keyed by label.",
            },
            "usage": {
                "display_name": "Token Usage",
                "type": JinaTokenUsageDataType.getOrNothingType(),
                "desc": "Tokens consumed while reading this item.",
            },
            "screenshotUrl": {
                "display_name": "Screenshot URL",
                "type": AbstractDataProviderTypeMap."*uri",
                "desc": "URL of a rendered page screenshot.",
            },
            "pageshotUrl": {
                "display_name": "Full Page Screenshot URL",
                "type": AbstractDataProviderTypeMap."*uri",
                "desc": "URL of a full-page screenshot.",
            },
            "metadata": {
                "display_name": "Metadata",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Metadata extracted from the document.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaPage") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaPageDataType = new JinaPageDataType();

#! Jina ReaderResponse fields
public class JinaReaderResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "display_name": "HTTP Code",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Jina response code.",
            },
            "status": {
                "display_name": "Service Status",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Jina service status code.",
            },
            "meta": {
                "display_name": "Response Metadata",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Request usage and additional metadata.",
            },
            "data": {
                "display_name": "Results",
                "type": JinaPageDataType,
                "desc": "Content returned by Jina.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaReaderResponse") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaReaderResponseDataType = new JinaReaderResponseDataType();

#! Jina SearchResponse fields
public class JinaSearchResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "code": {
                "display_name": "HTTP Code",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Jina response code.",
            },
            "status": {
                "display_name": "Service Status",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Jina service status code.",
            },
            "meta": {
                "display_name": "Response Metadata",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Request usage and additional metadata.",
            },
            "data": {
                "display_name": "Results",
                "type": new ListDataType("JinaSearchResults", JinaPageDataType),
                "desc": "Content returned by Jina.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaSearchResponse") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaSearchResponseDataType = new JinaSearchResponseDataType();

#! Jina SegmentRequest fields
public class JinaSegmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "content": {
                "display_name": "Text Content",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Text to count or split into chunks.",
                "example_value": "Delivery update. Your order has shipped.",
                "attr": {
                    "required": True,
                },
            },
            "tokenizer": {
                "display_name": "Tokenizer",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Tokenizer used to count the input.",
                "allowed_values": (<AllowedValueInfo>{"value": "cl100k_base", "display_name": "Cl100K Base"}, <AllowedValueInfo>{"value": "o200k_base", "display_name": "O200K Base"}, <AllowedValueInfo>{"value": "p50k_base", "display_name": "P50K Base"}, <AllowedValueInfo>{"value": "r50k_base", "display_name": "R50K Base"}, <AllowedValueInfo>{"value": "p50k_edit", "display_name": "P50K Edit"}, <AllowedValueInfo>{"value": "gpt2", "display_name": "Gpt2"}),
                "default_value": "cl100k_base",
            },
            "return_tokens": {
                "display_name": "Return Tokens",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include tokens and their numeric IDs.",
            },
            "return_chunks": {
                "display_name": "Return Chunks",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include semantically grouped text chunks.",
                "default_value": True,
                "attr": {
                    "preselected": True,
                },
            },
            "max_chunk_length": {
                "display_name": "Maximum Chunk Length",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum characters per chunk.",
                "attr": {
                    "preselected": True,
                },
            },
            "head": {
                "display_name": "First Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Return only this many leading tokens.",
                "attr": {
                    "exclusive_with": ("tail",),
                },
            },
            "tail": {
                "display_name": "Last Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Return only this many trailing tokens.",
                "attr": {
                    "exclusive_with": ("head",),
                },
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaSegmentRequest") {
        addQoreFields(Fields);
    }
}

public const JinaSegmentRequestDataType = new JinaSegmentRequestDataType();

#! Jina SegmentResponse fields
public class JinaSegmentResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "num_tokens": {
                "display_name": "Token Count",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Number of tokens in the selected text.",
            },
            "tokenizer": {
                "display_name": "Tokenizer",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Tokenizer used for this response.",
            },
            "num_chunks": {
                "display_name": "Chunk Count",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Number of returned chunks.",
            },
            "chunks": {
                "display_name": "Chunks",
                "type": AbstractDataProviderType::get("*list<string>"),
                "desc": "Text chunks in their original order.",
            },
            "tokens": {
                "display_name": "Tokens",
                "type": AbstractDataProviderTypeMap."*list",
                "desc": "Token text and ID groups, organized by chunk.",
            },
            "chunk_positions": {
                "display_name": "Chunk Positions",
                "type": AbstractDataProviderType::get("*list<list<int>>"),
                "desc": "Start and end character offsets for each chunk.",
            },
            "usage": {
                "display_name": "Usage",
                "type": JinaTokenUsageDataType.getOrNothingType(),
                "desc": "API usage information.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaSegmentResponse") {
        addQoreFields(Fields);
    }
}

public const JinaSegmentResponseDataType = new JinaSegmentResponseDataType();

#! Jina ChatMessage fields
public class JinaChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Who supplied this conversation message.",
                "allowed_values": (<AllowedValueInfo>{"value": "user", "display_name": "User"}, <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"}),
                "attr": {
                    "required": True,
                },
            },
            "content": {
                "display_name": "Content",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."string",
                    new ListDataType("JinaChatParts", JinaChatPartDataType))),
                "desc": "Message text or a list of text and image parts.",
                "example_value": "Describe the product in this image.",
                "attr": {
                    "required": True,
                },
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaChatMessage") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaChatMessageDataType = new JinaChatMessageDataType();

#! Jina ChatRequest fields
public class JinaChatRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "messages": {
                "display_name": "Conversation",
                "type": new ListDataType("JinaChatMessages", JinaChatMessageDataType),
                "desc": "Conversation messages containing text or multimodal content.",
                "example_value": ({"role": "user", "content": "Describe this product."},),
                "attr": {
                    "required": True,
                },
            },
            "model": {
                "display_name": "Model",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Vision-language model used to answer the conversation.",
                "allowed_values": (<AllowedValueInfo>{"value": "jina-ocr-v1", "display_name": "Jina OCR v1"},),
                "default_value": "jina-ocr-v1",
            },
            "max_tokens": {
                "display_name": "Maximum Output Tokens",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum length of the generated response.",
                "attr": {
                    "preselected": True,
                },
            },
            "temperature": {
                "display_name": "Temperature",
                "type": AbstractDataProviderTypeMap."*float",
                "desc": "Sampling temperature controlling response variation.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaChatRequest") {
        addQoreFields(Fields);
    }
}

public const JinaChatRequestDataType = new JinaChatRequestDataType();

#! Conversation messages for DeepSearch, which also supports system instructions
public class JinaDeepSearchMessageDataType inherits HashDataType {
    public {
        const Fields = JinaChatMessageDataType::Fields + {
            "role": JinaChatMessageDataType::Fields.role + {
                "allowed_values": (<AllowedValueInfo>{"value": "system", "display_name": "System"}, <AllowedValueInfo>{"value": "user", "display_name": "User"}, <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"}),
            },
        };
    }

    #! Creates the DeepSearch message type
    constructor() : HashDataType("JinaDeepSearchMessage") {
        addQoreFields(Fields);
    }
}

public const JinaDeepSearchMessageDataType = new JinaDeepSearchMessageDataType();

#! Structured DeepSearch output with a directly embedded JSON Schema
public class JinaResponseFormatDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Format",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Return an answer matching the supplied JSON Schema.",
                "allowed_values": (<AllowedValueInfo>{"value": "json_schema", "display_name": "JSON Schema"},),
                "default_value": "json_schema",
                "attr": {"required": True},
            },
            "json_schema": {
                "display_name": "JSON Schema",
                "type": AbstractDataProviderTypeMap."hash",
                "desc": "JSON Schema describing the answer. Put the schema directly here, including `type` and `properties`.",
                "example_value": {"type": "object", "properties": {"answer": {"type": "string"}}, "required": ("answer",)},
                "attr": {"required": True},
            },
        };
    }

    #! Creates the structured output type
    constructor() : HashDataType("JinaResponseFormat") {
        addQoreFields(Fields);
    }
}

public const JinaResponseFormatDataType = new JinaResponseFormatDataType();

#! Jina DeepSearchRequest fields
public class JinaDeepSearchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "messages": {
                "display_name": "Conversation",
                "type": new ListDataType("JinaDeepSearchMessages", JinaDeepSearchMessageDataType),
                "desc": "Conversation messages containing text or multimodal content.",
                "example_value": ({"role": "user", "content": "Describe this product."},),
                "attr": {
                    "required": True,
                },
            },
            "model": {
                "display_name": "Model",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "DeepSearch model used for web research.",
                "default_value": "jina-deepsearch-v1",
                "allowed_values": (<AllowedValueInfo>{"value": "jina-deepsearch-v1", "display_name": "Jina Deepsearch V1"},),
            },
            "reasoning_effort": {
                "display_name": "Research Effort",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Choose a preset balance between response quality, time, and token usage.",
                "allowed_values": (<AllowedValueInfo>{"value": "low", "display_name": "Low"}, <AllowedValueInfo>{"value": "medium", "display_name": "Medium"}, <AllowedValueInfo>{"value": "high", "display_name": "High"}),
                "default_value": "low",
                "attr": {
                    "preselected": True,
                },
            },
            "budget_tokens": {
                "display_name": "Token Budget",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum tokens for the complete research process.",
                "attr": {
                    "preselected": True,
                },
            },
            "max_attempts": {
                "display_name": "Maximum Attempts",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum attempts to refine an answer.",
            },
            "team_size": {
                "display_name": "Research Team Size",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Number of research workers sharing the token budget.",
            },
            "no_direct_answer": {
                "display_name": "Always Search",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Force a web search before answering.",
            },
            "search_provider": {
                "display_name": "Search Provider",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Restrict research to arXiv when selected.",
                "allowed_values": (<AllowedValueInfo>{"value": "arxiv", "display_name": "arXiv"},),
            },
            "response_format": {
                "display_name": "Structured Output",
                "type": JinaResponseFormatDataType.getOrNothingType(),
                "desc": "JSON Schema response format for structured answers and fact-checking.",
                "example_value": {"type": "json_schema", "json_schema": {"type": "object", "properties": {"answer": {"type": "string"}}, "required": ("answer",)}},
            },
            "search_language_code": {
                "display_name": "Search Language",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Language code for search queries.",
                "example_value": "en",
            },
            "language_code": {
                "display_name": "Answer Language",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Language code for the answer and reasoning.",
                "example_value": "en",
            },
            "max_returned_urls": {
                "display_name": "Maximum Source URLs",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum source URLs to include with the answer.",
            },
            "boost_hostnames": {
                "display_name": "Preferred Domains",
                "type": AbstractDataProviderType::get("*list<string>"),
                "desc": "Prefer these domains when reading sources.",
                "example_value": ("qore.org",),
            },
            "bad_hostnames": {
                "display_name": "Excluded Domains",
                "type": AbstractDataProviderType::get("*list<string>"),
                "desc": "Exclude these domains from source reading.",
                "example_value": ("qore.org",),
            },
            "only_hostnames": {
                "display_name": "Allowed Domains",
                "type": AbstractDataProviderType::get("*list<string>"),
                "desc": "Read sources only from these domains.",
                "example_value": ("qore.org",),
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaDeepSearchRequest") {
        addQoreFields(Fields);
    }
}

public const JinaDeepSearchRequestDataType = new JinaDeepSearchRequestDataType();

#! Jina ChatChoice fields
public class JinaChatChoiceDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Choice Index",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Position of this generated choice.",
            },
            "message": {
                "display_name": "Message",
                "type": JinaChatMessageDataType,
                "desc": "The completed assistant response.",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Why generation stopped.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaChatChoice") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaChatChoiceDataType = new JinaChatChoiceDataType();

#! Jina ChatResponse fields
public class JinaChatResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Identifier of the generated response.",
            },
            "object": {
                "display_name": "Object Type",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Response object kind.",
            },
            "created": {
                "display_name": "Created",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "When the response was created.",
            },
            "model": {
                "display_name": "Model",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Model used to generate the answer.",
            },
            "choices": {
                "display_name": "Choices",
                "type": new ListDataType("JinaChatChoices", JinaChatChoiceDataType),
                "desc": "Completed answers and finish reasons.",
            },
            "usage": {
                "display_name": "Token Usage",
                "type": JinaTokenUsageDataType.getOrNothingType(),
                "desc": "Prompt, completion, and total token counts.",
            },
            "visitedURLs": {
                "display_name": "Visited URLs",
                "type": AbstractDataProviderType::get("*list<string>"),
                "desc": "URLs considered during DeepSearch.",
            },
            "readURLs": {
                "display_name": "Read URLs",
                "type": AbstractDataProviderType::get("*list<string>"),
                "desc": "Sources read during DeepSearch.",
            },
            "numURLs": {
                "display_name": "Source Count",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Number of discovered sources.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaChatResponse") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaChatResponseDataType = new JinaChatResponseDataType();

#! Jina BatchDownloadRequest fields
public class JinaBatchDownloadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "batch_id": {
                "display_name": "Batch",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Completed batch whose JSONL results or errors to download.",
                "example_value": "batch-id-from-create-batch",
                "attr": {
                    "required": True,
                    "ref_data": "batches",
                    "supports_custom_values": True,
                },
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaBatchDownloadRequest") {
        addQoreFields(Fields);
    }
}

public const JinaBatchDownloadRequestDataType = new JinaBatchDownloadRequestDataType();

#! JinaBatchRecordResponse fields
public class JinaBatchRecordResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "status_code": {
                "display_name": "HTTP Status",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "HTTP status of this individual embedding request.",
            },
            "request_id": {
                "display_name": "Request ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Identifier of the individual API request.",
            },
            "body": {
                "display_name": "Embeddings",
                "type": JinaEmbeddingResponseDataType.getOrNothingType(),
                "desc": "Embedding vectors and token usage for this record.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaBatchRecordResponse") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaBatchRecordResponseDataType = new JinaBatchRecordResponseDataType();

#! JinaBatchRecord fields
public class JinaBatchRecordDataType inherits HashDataType {
    private {
        const Fields = {
            "custom_id": {
                "display_name": "Custom ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Identifier supplied with the original document.",
            },
            "response": {
                "display_name": "Response",
                "type": JinaBatchRecordResponseDataType.getOrNothingType(),
                "desc": "Result of the individual embedding request.",
            },
            "error": {
                "display_name": "Error",
                "type": AbstractDataProviderTypeMap."any",
                "desc": "Service error details when this record failed; the structure is service-defined.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaBatchRecord") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const JinaBatchRecordDataType = new JinaBatchRecordDataType();

#! Jina BatchDownloadResponse fields
public class JinaBatchDownloadResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "records": {
                "display_name": "Records",
                "type": new ListDataType("JinaBatchRecords", JinaBatchRecordDataType),
                "desc": "Parsed JSONL records in their original order. Each record includes its custom ID and response or error.",
            },
        };
    }

    #! Creates the documented data type
    constructor() : HashDataType("JinaBatchDownloadResponse") {
        addQoreFields(Fields);
    }
}

public const JinaBatchDownloadResponseDataType = new JinaBatchDownloadResponseDataType();

}
