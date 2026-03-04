/*
    QoreStringNode.cpp

    QoreStringNode Class Definition

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include <qore/QoreEnumDecl.h>

#include "qore/intern/qore_string_private.h"

#include <cstdarg>

QoreStringNodeMaker::QoreStringNodeMaker(const char* fmt, ...) {
    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }
}

QoreStringNode::QoreStringNode() : SimpleValueQoreNode(NT_STRING) {
    //sset.add(this);
}

QoreStringNode::~QoreStringNode() {
    //sset.del(this);
}

QoreStringNode::QoreStringNode(const char* str, const QoreEncoding* enc) : SimpleValueQoreNode(NT_STRING),
        QoreString(str, enc) {
    //sset.add(this);
}

// copies str
QoreStringNode::QoreStringNode(const QoreString &str) : SimpleValueQoreNode(NT_STRING), QoreString(str) {
    //sset.add(this);
}

// copies str
QoreStringNode::QoreStringNode(const QoreStringNode &str) : SimpleValueQoreNode(NT_STRING), QoreString(str) {
    //sset.add(this);
}

// copies str
QoreStringNode::QoreStringNode(const std::string &str, const QoreEncoding* enc) : SimpleValueQoreNode(NT_STRING),
        QoreString(str, enc) {
    //sset.add(this);
}

QoreStringNode::QoreStringNode(char c) : SimpleValueQoreNode(NT_STRING), QoreString(c) {
    //sset.add(this);
}

QoreStringNode::QoreStringNode(const BinaryNode* b) : SimpleValueQoreNode(NT_STRING), QoreString(b) {
    //sset.add(this);
}

QoreStringNode::QoreStringNode(const BinaryNode* b, size_t maxlinelen) : SimpleValueQoreNode(NT_STRING),
        QoreString(b, maxlinelen) {
    //sset.add(this);
}

QoreStringNode::QoreStringNode(struct qore_string_private *p) : SimpleValueQoreNode(NT_STRING), QoreString() {
    qore_string_private::adopt(*this, p);
    //sset.add(this);
}

QoreStringNode::QoreStringNode(char* nbuf, size_t nlen, size_t nallocated, const QoreEncoding* enc)
        : SimpleValueQoreNode(NT_STRING), QoreString(nbuf, nlen, nallocated, enc) {
    //sset.add(this);
}

QoreStringNode::QoreStringNode(const char* str, size_t len, const QoreEncoding* new_qore_encoding)
        : SimpleValueQoreNode(NT_STRING), QoreString(str, len, new_qore_encoding) {
    //sset.add(this);
}

// virtual function
int QoreStringNode::getAsIntImpl() const {
    return (int)strtoll(c_str(), 0, 10);
}

int64 QoreStringNode::getAsBigIntImpl() const {
    return strtoll(c_str(), 0, 10);
}

double QoreStringNode::getAsFloatImpl() const {
    return q_strtod(c_str());
}

QoreString* QoreStringNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    TempString str;
    str->concat('"');
    str->concatEscape(this, '\"', '\\', xsink);
    if (xsink && *xsink) {
        return 0;
    }
    str->concat('"');
    return str.release();
}

int QoreStringNode::getAsString(QoreString &str, int foff, ExceptionSink* xsink) const {
    assert(str.getEncoding()->isAsciiCompat());
    str.concat('"');
    str.concatEscape(this, '\"', '\\', xsink);
    if (xsink && *xsink) {
        return -1;
    }
    str.concat('"');
    return 0;
}

bool QoreStringNode::getAsBoolImpl() const {
    // check if we should do perl-style boolean evaluation
    if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL)) {
        return q_strtod(c_str());
    }
    if (priv->len == 1 && priv->buf[0] == '0') {
        return false;
    }
    return !empty();
}

// get the value of the type in a string context, empty string for complex types (default implementation)
QoreString* QoreStringNode::getStringRepresentation(bool& del) const {
    del = false;
    return const_cast<QoreStringNode*>(this);
}

QoreStringNode* QoreStringNode::convertEncoding(const QoreEncoding* nccs, ExceptionSink* xsink) const {
    printd(5, "QoreStringNode::convertEncoding() from '%s' to '%s'\n", getEncoding()->getCode(), nccs->getCode());

    if (nccs == priv->encoding) {
        ref();
        return const_cast<QoreStringNode*>(this);
    }
    if (!priv->len) {
        return new QoreStringNode(nccs);
    }

    QoreStringNode* targ = new QoreStringNode(nccs);

    if (qore_string_private::convert_encoding_intern(priv->buf, priv->len, priv->encoding, *targ, nccs, xsink)) {
        targ->deref();
        return 0;
    }
    return targ;
}

// DLLLOCAL constructor
QoreStringNode::QoreStringNode(const char* str, const QoreEncoding* from, const QoreEncoding* to,
        ExceptionSink* xsink) : SimpleValueQoreNode(NT_STRING), QoreString(to) {
    qore_string_private::convert_encoding_intern(str, ::strlen(str), from, *this, to, xsink);
}

// static function
QoreStringNode* QoreStringNode::createAndConvertEncoding(const char* str, const QoreEncoding* from,
        const QoreEncoding* to, ExceptionSink* xsink) {
    QoreStringNodeHolder rv(new QoreStringNode(str, from, to, xsink));
    return *xsink ? nullptr : rv.release();
}

AbstractQoreNode* QoreStringNode::realCopy() const {
    return copy();
}

QoreStringNode* QoreStringNode::copy() const {
    return new QoreStringNode(*this);
}

QoreStringNode* QoreStringNode::substr(qore_offset_t offset, ExceptionSink* xsink) const {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(priv->encoding));

    int rc;
    if (!getEncoding()->isMultiByte()) {
        rc = priv->substr_simple(*str, offset);
    } else {
        rc = priv->substr_complex(*str, offset, xsink);
    }

    return rc ? nullptr : str.release();
}

QoreStringNode* QoreStringNode::substr(qore_offset_t offset, qore_offset_t length, ExceptionSink* xsink) const {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(priv->encoding));

    int rc;
    if (!getEncoding()->isMultiByte()) {
        rc = priv->substr_simple(*str, offset, length);
    } else {
        rc = priv->substr_complex(*str, offset, length, xsink);
    }

    return rc ? nullptr : str.release();
}

QoreStringNode* QoreStringNode::reverse() const {
    QoreStringNode* str = new QoreStringNode(priv->encoding);
    priv->concat_reverse(*str->priv);
    return str;
}

QoreStringNode* QoreStringNode::regexSubst(QoreString& match, QoreString& subst, int opts,
        ExceptionSink* xsink) const {
    QoreRegexSubst regex;
    priv->setRegexOpts(regex, opts);
    if (regex.parseRT(&match, xsink)) {
        assert(*xsink);
        return nullptr;
    }
    return *xsink ? nullptr : regex.exec(this, &subst, xsink);
}

QoreStringNode* QoreStringNode::parseBase64ToString(const QoreEncoding* qe, ExceptionSink* xsink) const {
    SimpleRefHolder<BinaryNode> b(::parseBase64(priv->buf, priv->len, xsink));
    return binary_to_string<QoreStringNode>(b.release(), qe);
}

QoreStringNode* QoreStringNode::parseBase64ToString(ExceptionSink* xsink) const {
    return parseBase64ToString(QCS_DEFAULT, xsink);
}

QoreStringNode* QoreStringNode::parseBase64UrlToString(const QoreEncoding* qe, ExceptionSink* xsink) const {
    SimpleRefHolder<BinaryNode> b(::parseBase64Url(priv->buf, priv->len, xsink));
    return binary_to_string<QoreStringNode>(b.release(), qe);
}

QoreStringNode* QoreStringNode::parseBase64UrlToString(ExceptionSink* xsink) const {
    return parseBase64ToString(QCS_DEFAULT, xsink);
}

void QoreStringNode::getStringRepresentation(QoreString &str) const {
    qore_string_private::get(str)->concat(this);
}

// if del is true, then the returned DateTime * should be deleted, if false, then it should not
DateTime *QoreStringNode::getDateTimeRepresentation(bool& del) const {
    del = true;
    return new DateTime(c_str());
}

// assign date representation to a DateTime * (no action for complex types = default implementation)
void QoreStringNode::getDateTimeRepresentation(DateTime& dt) const {
    dt.setDate(c_str());
}

bool QoreStringNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    assert(xsink);
    if (get_node_type(v) == NT_STRING) {
        return equalSoft(*reinterpret_cast<const QoreStringNode*>(v), xsink);
    }
    QoreStringValueHelper str(v, getEncoding(), xsink);
    if (*xsink) {
        return false;
    }
    return equal(*str);
}

bool QoreStringNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    // note that "hard" equality checks now do encoding conversions if necessary
    if (get_node_type(v) == NT_STRING) {
        return equalSoft(*reinterpret_cast<const QoreStringNode*>(v), xsink);
    }
    return false;
}

QoreStringNode* QoreStringNode::stringRefSelf() const {
    ref();
    return const_cast<QoreStringNode*>(this);
}

const char* QoreStringNode::getTypeName() const {
    return getStaticTypeName();
}

int QoreStringNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = stringTypeInfo;
    return 0;
}

QoreStringNode* QoreStringNode::extract(qore_offset_t offset, ExceptionSink* xsink) {
    QoreStringNode* str = new QoreStringNode(priv->encoding);
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset = priv->check_offset(offset);
        if (n_offset != priv->len)
            priv->splice_simple(n_offset, priv->len - n_offset, str);
    } else {
        priv->splice_complex(offset, xsink, str);
    }
    return str;
}

QoreStringNode* QoreStringNode::extract(qore_offset_t offset, qore_offset_t num, ExceptionSink* xsink) {
    QoreStringNode* str = new QoreStringNode(priv->encoding);
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset, n_num;
        priv->check_offset(offset, num, n_offset, n_num);
        if (n_offset != priv->len && n_num)
            priv->splice_simple(n_offset, n_num, str);
    } else {
        priv->splice_complex(offset, num, xsink, str);
    }
    return str;
}

QoreStringNode* QoreStringNode::extract(qore_offset_t offset, qore_offset_t num, QoreValue strn,
        ExceptionSink* xsink) {
    QoreStringNodeValueHelper tmp(strn, priv->encoding, xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!tmp->strlen())
        return extract(offset, num, xsink);

    QoreStringNode* rv = new QoreStringNode(priv->encoding);
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset, n_num;
        priv->check_offset(offset, num, n_offset, n_num);
        if (n_offset == priv->len) {
            if (!tmp->strlen())
                return rv;
            n_num = 0;
        }
        priv->splice_simple(n_offset, n_num, tmp->c_str(), tmp->strlen(), rv);
    } else {
        priv->splice_complex(offset, num, *tmp, xsink, rv);
    }
    return rv;
}

QoreNodeAsStringHelper::QoreNodeAsStringHelper(const AbstractQoreNode* n, int format_offset, ExceptionSink* xsink) {
    if (n) {
        str = n->getAsString(del, format_offset, xsink);
    } else {
        str = format_offset == FMT_YAML_SHORT ? &YamlNullString : &NothingTypeString;
        del = false;
    }
}

QoreNodeAsStringHelper::QoreNodeAsStringHelper(const QoreValue n, int format_offset, ExceptionSink* xsink) {
    str = n.getAsString(del, format_offset, xsink);
}

QoreNodeAsStringHelper::~QoreNodeAsStringHelper() {
    if (del) {
        delete str;
    }
}

QoreString* QoreNodeAsStringHelper::giveString() {
    if (!str) {
        return nullptr;
    }
    if (!del) {
        return str->copy();
    }

    QoreString* rv = str;
    del = false;
    str = nullptr;
    return rv;
}

const QoreString* QoreNodeAsStringHelper::operator->() {
    return str;
}

const QoreString* QoreNodeAsStringHelper::operator*() {
    return str;
}

void QoreStringValueHelper::setup(ExceptionSink* xsink, const QoreValue n, const QoreEncoding* enc) {
    if (n.isEnum()) {
        setup(xsink, n.getEnumMember()->getValue(), enc);
        return;
    }
    qore_type_t t = n.getType();
    if (t == NT_BOOLEAN || t == NT_INT) {
        str = new QoreStringMaker(QLLD, n.getAsBigInt());
        del = true;
    } else if (t == NT_FLOAT) {
        str = q_fix_decimal(new QoreStringMaker("%.9g", n.getAsFloat()), 0);
        del = true;
    } else if (n.hasNode()) {
        const AbstractQoreNode* node = n.getInternalNode();
        //optimization to remove the need for a virtual function call in the most common case
        if (node->getType() == NT_STRING) {
            del = false;
            str = const_cast<QoreStringNode*>(n.get<const QoreStringNode>());
        } else {
            str = n.get<const AbstractQoreNode>()->getStringRepresentation(del);
        }
        if (xsink && enc && str->getEncoding() != enc) {
            QoreString* t = str->convertEncoding(enc, xsink);
            if (!t)
                return;
            if (del)
                delete str;
            str = t;
            del = true;
        }
    } else if (t == NT_NOTHING || t == NT_NULL) {
        str = NullString;
        del = false;
    } else {
        assert(false);
    }
}

QoreStringValueHelper::QoreStringValueHelper(const QoreValue n) {
    setup(nullptr, n);
}

QoreStringValueHelper::QoreStringValueHelper(const QoreValue n, const QoreEncoding* enc, ExceptionSink* xsink) {
    setup(xsink, n, enc);
}

QoreStringValueHelper::~QoreStringValueHelper() {
    if (del) {
        delete str;
    }
}

const QoreString* QoreStringValueHelper::operator->() {
    return str;
}

const QoreString* QoreStringValueHelper::operator*() {
    return str;
}

QoreString* QoreStringValueHelper::giveString() {
    if (!str) {
        return nullptr;
    }
    if (!del) {
        return str->copy();
    }

    QoreString* rv = str;
    del = false;
    str = nullptr;
    return rv;
}

char* QoreStringValueHelper::giveBuffer() {
    if (!str) {
        return nullptr;
    }
    if (!del) {
        return strdup(str->c_str());
    }
    char* rv = str->giveBuffer();
    delete str;
    del = false;
    str = nullptr;
    return rv;
}

bool QoreStringValueHelper::is_temp() const {
    return del;
}

void QoreStringNodeValueHelper::setup(ExceptionSink* xsink, const QoreValue n, const QoreEncoding* enc) {
    if (n.isEnum()) {
        setup(xsink, n.getEnumBaseValue(), enc);
        return;
    }
    qore_type_t t = n.getType();
    if (t == NT_BOOLEAN || t == NT_INT) {
        str = new QoreStringNodeMaker(QLLD, n.getAsBigInt());
        del = true;
    } else if (t == NT_FLOAT) {
        str = q_fix_decimal(new QoreStringNodeMaker("%.9g", n.getAsFloat()), 0);
        del = true;
    } else if (n.hasNode()) {
        const AbstractQoreNode* node = n.getInternalNode();
        //optimization to remove the need for a virtual function call in the most common case
        if (node->getType() == NT_STRING) {
            del = false;
            str = const_cast<QoreStringNode*>(n.get<const QoreStringNode>());
        } else {
            del = true;
            str = new QoreStringNode;
            n.get<const AbstractQoreNode>()->getStringRepresentation(*str);
        }
        if (xsink && enc && str->getEncoding() != enc) {
            QoreStringNode* t = str->convertEncoding(enc, xsink);
            if (!t)
                return;
            if (del)
                str->deref();
            str = t;
            del = true;
        }
    } else if (t == NT_NOTHING || t == NT_NULL) {
        str = NullString;
        del = false;
    } else {
        assert(false);
    }
}

QoreStringNodeValueHelper::QoreStringNodeValueHelper(const QoreValue n) {
    setup(nullptr, n);
}

QoreStringNodeValueHelper::QoreStringNodeValueHelper(const QoreValue n, const QoreEncoding* enc, ExceptionSink* xsink) {
    setup(xsink, n, enc);
}

QoreStringNodeValueHelper::~QoreStringNodeValueHelper() {
    if (del) {
        str->deref();
    }
}

const QoreStringNode* QoreStringNodeValueHelper::operator->() {
    return str;
}

const QoreStringNode* QoreStringNodeValueHelper::operator*() {
    return str;
}

//! returns true if the referenced being managed is temporary
bool QoreStringNodeValueHelper::is_temp() const {
    return del;
}

QoreStringNode* QoreStringNodeValueHelper::getReferencedValue() {
    if (del) {
        del = false;
    } else if (str) {
        str->ref();
    }
    return str;
}
