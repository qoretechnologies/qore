# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DescribedDataField class definition


/** DescribedDataField.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A data field restored from its description
/** The description of a field (@ref DataProvider::AbstractDataField::getInfo() "AbstractDataField::getInfo()")
    gives the field's example value as the field gives it, which can depend on what the description does not carry:
    a field built from an API schema, for example, gives no example when the schema's constraints reject every
    candidate, and a union built from one checks a value against the schemas of its alternatives.  A field restored
    from the description as a @ref DataProvider::QoreDataField "QoreDataField" would generate an example from its
    allowed values or its type instead, which the original field does not give and the API can reject.

    @ref DataProvider::AbstractDataProviderType::get() "AbstractDataProviderType::get()" therefore restores the fields
    of a type as fields of this class, which give the example that their descriptions give, and none when the
    description gives none.

    @par Example:
    @code{.py}
AbstractDataProviderType restored = AbstractDataProviderType::get(type.getInfo());
# each field of the restored type gives the example that the field of the original type gives
    @endcode

    @since DataProvider 3.11
*/
public class DescribedDataField inherits QoreDataField {
    #! Creates the field from its description
    /** @param h the description of the field (@ref DataProvider::DataFieldInfo "DataFieldInfo"), with the keys
        that a @ref DataProvider::QoreDataField "QoreDataField" is created from; its example value is the one that the
        description gives, if any

        @throw QORE-DATA-FIELD-ERROR missing or invalid key \c "type"
        @throw RUNTIME-TYPE-ERROR the example value or the default value is not accepted by the type
    */
    constructor(hash<auto> h) : QoreDataField(h) {
    }

    #! Returns the example value that the field's description gives, if any
    /** @param emap ignored; the example is not generated
        @param fname ignored; the example is not generated

        @return the example value that the field's description gives, or @ref nothing if it gives none
    */
    auto getExampleValue(*hash<string, bool> emap, *string fname) {
        return example_value;
    }
}
}
