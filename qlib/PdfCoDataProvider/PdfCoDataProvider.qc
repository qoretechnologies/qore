# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoDataProvider.qc PDF.co root data provider

/*  PdfCoDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! The main PDF.co data provider class (root provider)
public class PdfCoDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "pdfco",
            "desc": "PDF.co data provider for document processing: PDF conversion, manipulation, "
                "data extraction, barcode operations, and more",
            "type": "PdfCoDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "ai-invoice-parser": Class::forName("PdfCoAiInvoiceParserDataProvider"),
            "add-password": Class::forName("PdfCoAddPasswordDataProvider"),
            "add-text-images": Class::forName("PdfCoAddTextImagesDataProvider"),
            "compress-pdf": Class::forName("PdfCoCompressPdfDataProvider"),
            "convert-html-to-pdf": Class::forName("PdfCoConvertHtmlToPdfDataProvider"),
            "convert-images-to-pdf": Class::forName("PdfCoConvertImagesToPdfDataProvider"),
            "convert-from-pdf": Class::forName("PdfCoConvertFromPdfDataProvider"),
            "convert-from-pdf-to-images": Class::forName("PdfCoConvertFromPdfToImagesDataProvider"),
            "convert-to-pdf": Class::forName("PdfCoConvertToPdfDataProvider"),
            "create-fillable-form": Class::forName("PdfCoCreateFillableFormDataProvider"),
            "document-classifier": Class::forName("PdfCoDocumentClassifierDataProvider"),
            "fill-pdf-form": Class::forName("PdfCoFillPdfFormDataProvider"),
            "generate-barcode": Class::forName("PdfCoGenerateBarcodeDataProvider"),
            "get-pdf-info": Class::forName("PdfCoGetPdfInfoDataProvider"),
            "job-check": Class::forName("PdfCoJobCheckDataProvider"),
            "make-api-call": Class::forName("PdfCoMakeApiCallDataProvider"),
            "merge-pdf": Class::forName("PdfCoMergePdfDataProvider"),
            "parse-document": Class::forName("PdfCoParseDocumentDataProvider"),
            "read-barcode": Class::forName("PdfCoReadBarcodeDataProvider"),
            "remove-password": Class::forName("PdfCoRemovePasswordDataProvider"),
            "search-text": Class::forName("PdfCoSearchTextDataProvider"),
            "search-and-delete-text": Class::forName("PdfCoSearchAndDeleteTextDataProvider"),
            "search-and-replace-text": Class::forName("PdfCoSearchAndReplaceTextDataProvider"),
            "search-and-replace-with-image": Class::forName("PdfCoSearchAndReplaceWithImageDataProvider"),
            "send-email": Class::forName("PdfCoSendEmailDataProvider"),
            "split-pdf": Class::forName("PdfCoSplitPdfDataProvider"),
            "upload-file": Class::forName("PdfCoUploadFileDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : PdfCoDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) : PdfCoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(PdfCoRestClient::PdfCoRestConnection conn) : PdfCoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "pdfco";
    }

    #! Returns the provider description
    *string getDesc() {
        return "PDF.co data provider for document processing: PDF conversion, manipulation, "
            "data extraction, barcode operations, and more";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
