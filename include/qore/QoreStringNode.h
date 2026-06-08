/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreStringNode.h

    QoreStringNode Class Definition

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORESTRINGNODE_H

#define _QORE_QORESTRINGNODE_H

#include <qore/AbstractQoreNode.h>
#include <qore/QoreString.h>

#include <cstring>

//! Qore's string value type, reference counted, dynamically-allocated only
/** for a version that can be used on the stack, use QoreString
    Each QoreStringNode is tagged with a specific encoding implemented by
    QoreEncoding.  Character encodings can be converted with QoreStringNode::convertEncoding()
    @see QoreString
    @see QoreEncoding
 */
class QoreStringNode : public SimpleValueQoreNode, public QoreString {
private:
    QoreStringNode(QoreString* str) = delete;
    QoreStringNode& operator=(const QoreStringNode&) = delete;

    // note implemented; declared private here to probit them from being called
    DLLLOCAL virtual bool getAsBoolImpl() const;
    DLLLOCAL virtual int getAsIntImpl() const;
    DLLLOCAL virtual int64 getAsBigIntImpl() const;
    DLLLOCAL virtual double getAsFloatImpl() const;

protected:
    //! destructor only called when references = 0, use deref() instead
    DLLEXPORT virtual ~QoreStringNode();

public:
    //! creates an empty string and assigns the default encoding QCS_DEFAULT
    DLLEXPORT QoreStringNode();

    //! creates a new object from a string and sets the character encoding
    /**
        @param str the string data is copied to the new QoreStringNode object
        @param enc the encoding for the string
    */
    DLLEXPORT QoreStringNode(const char* str, const QoreEncoding* enc = QCS_DEFAULT);

    //! creates a new QoreStringNode from an existing QoreString reference
    /**
        @param str copies the data into the new QoreStringNode
    */
    DLLEXPORT QoreStringNode(const QoreString& str);

    //! creates a new QoreStringNode from an existing QoreStringNode reference
    /**
        @param str copies the data into the new QoreStringNode
    */
    DLLEXPORT QoreStringNode(const QoreStringNode& str);

    //! creates a new object from a std::string and sets the character encoding
    /**
        @param str the string data is copied to the new QoreStringNode object
        @param enc the encoding for the string
    */
    DLLEXPORT QoreStringNode(const std::string& str, const QoreEncoding* enc = QCS_DEFAULT);

    // copies binary object and makes a base64-encoded string out of it
    DLLEXPORT QoreStringNode(const BinaryNode* b);

    //! creates a new string as the base64-encoded value of the binary object passed and ensures the maximum line length for the base64-encoded output
    DLLEXPORT QoreStringNode(const BinaryNode* bin, size_t maxlinelen);

    //! creates a new object; takes ownership of the char pointer passed, all parameters are mandatory
    /**
        @param nbuf the pointer for the character data
        @param nlen the length of the string in bytes (not including the trailing '\\0')
        @param nallocated the number of bytes allocated for this buffer (if unknown, set to nlen + 1)
        @param enc the encoding for the string
    */
    DLLEXPORT QoreStringNode(char* nbuf, size_t nlen, size_t nallocated, const QoreEncoding* enc);

    //! copies the c-string passed (up to len) and assigns the encoding passed
    DLLEXPORT QoreStringNode(const char* str, size_t len, const QoreEncoding* new_qorecharset = QCS_DEFAULT);

    // creates a string from a single character
    DLLEXPORT QoreStringNode(char c);

    //! concatenates the string data in double quotes to an existing QoreString
    /** used for %n and %N printf formatting.  An exception may be thrown if there is an encoding conversion error
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param format_offset for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 for exception raised, 0 = OK
    */
    DLLEXPORT int getAsString(QoreString& str, int format_offset, ExceptionSink* xsink) const;

    //! returns a QoreString giving the string data in double quotes
    /** used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
        @param format_offset for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT QoreString* getAsString(bool& del, int format_offset, ExceptionSink* xsink) const;

    //! returns the current string and sets del to false
    /** NOTE: do not call this function directly, use QoreStringValueHelper instead
        @param del output parameter: always sets del to false
        @see QoreStringValueHelper
    */
    DLLEXPORT virtual QoreString* getStringRepresentation(bool& del) const;

    //! concatentates the value of the type to an existing QoreString reference
    /**
        @param str a reference to a QoreString where the value of the type will be concatenated
    */
    DLLEXPORT virtual void getStringRepresentation(QoreString& str) const;

    //! returns the DateTime representation of this string
    /** NOTE: Use the DateTimeValueHelper class instead of using this function directly
        @param del output parameter: if del is true, then the returned DateTime pointer belongs to the caller (and must be deleted manually), if false, then it must not be
        @see DateTimeValueHelper
    */
    DLLEXPORT virtual DateTime* getDateTimeRepresentation(bool& del) const;

    //! assigns the date representation of this string to the DateTime reference passed
    /**
        @param dt the DateTime reference to be assigned
    */
    DLLEXPORT virtual void getDateTimeRepresentation(DateTime& dt) const;

    //! returns a copy of the object, the caller owns the reference count
    DLLEXPORT virtual AbstractQoreNode* realCopy() const;

    //! tests for equality ("deep compare" including all contained values for container types) with possible type and character encoding conversion (soft compare)
    /** An exception could be raised if character set encoding is required to do the compare the the conversion fails
        @param v the value to compare
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! tests for equality ("deep compare" including all contained values for container types) without type or character encoding conversions (hard compare)
    /** if the character encodings of the two strings differ, the comparison fails immediately
        this function does not throw any Qore-language exceptions as no character set encoding conversions are made
        @param v the value to compare
        @param xsink is not used in this implementation of the function
    */
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    DLLEXPORT virtual const char* getTypeName() const;

    //! converts the encoding of the string to the specified encoding, returns 0 if an error occurs, the caller owns the pointer returned
    /** if the encoding is the same as the current encoding, a copy of the string is returned
        @param nccs the encoding for the new string
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the new string with the desired encoding or 0 if an error occured
    */
    DLLEXPORT QoreStringNode* convertEncoding(const QoreEncoding* nccs, ExceptionSink* xsink) const;

    //! returns a new string consisting of all the characters from the current string starting with character position "offset"
    /** offset is a character offset and not a byte offset
        @param offset the offset in characters from the beginning of the string (starting with 0), can be negative
        @param xsink an invalid multibyte character encoding can cause an exception to be thrown
        @return the new string
    */
    DLLEXPORT QoreStringNode* substr(qore_offset_t offset, ExceptionSink* xsink) const;

    //! returns a new string consisting of "length" characters from the current string starting with character position "offset"
    /** offset and length spoecify characters, not bytes
        @param offset the offset in characters from the beginning of the string (starting with 0), can be negative
        @param length the number of characters to take for the new substring, can be negative
        @param xsink an invalid multibyte character encoding can cause an exception to be thrown
        @return the new string
    */
    DLLEXPORT QoreStringNode* substr(qore_offset_t offset, qore_offset_t length, ExceptionSink* xsink) const;

    //! return a QoreStringNode with the characters reversed
    DLLEXPORT QoreStringNode* reverse() const;

    //! performs perl5-compatible regular expression substitution
    /** @param match the pattern to match
        @param subst the substitution string
        @param opts regex options
        @param xsink invalid multi-byte encodings can cause an exception to be thrown

        @return the new string if the operation was successful

        @since %Qore 1.1.0
    */
    DLLEXPORT QoreStringNode* regexSubst(QoreString& match, QoreString& subst, int opts, ExceptionSink* xsink) const;

    // copy function
    DLLEXPORT QoreStringNode* copy() const;

    //! creates a new QoreStringNode from a string and converts its encoding
    DLLEXPORT static QoreStringNode* createAndConvertEncoding(const char* str, const QoreEncoding* from, const QoreEncoding* to, ExceptionSink* xsink);

    //! Creates a zero-copy read-only view over \a parent's buffer starting at \a byte_offset, of \a byte_len bytes.
    /** The returned string shares \a parent's bytes; the parent is ref'd and kept alive as long as the view exists.
        Reads on the view go through the parent's buffer. On any mutation, the view promotes itself to an owned
        copy (copy-on-write) and releases the parent.

        Arguments are **byte** offsets and lengths; the caller is responsible for ensuring that \a byte_offset and
        \a byte_offset + \a byte_len fall on codepoint boundaries for multibyte encodings. (In debug builds an
        assertion enforces that the offsets are <= parent->size().)

        If \a parent is itself a view, the returned view points at \a parent's parent instead, avoiding chained
        indirection in read accessors.

        @param parent the string to view; must not be null and must outlive any mutations on the view (the view
                      itself refs the parent to guarantee this)
        @param byte_offset byte offset into \a parent's buffer
        @param byte_len number of bytes in the view
        @return a new QoreStringNode in view state, owned by the caller

        @since %Qore 3.0
    */
    DLLEXPORT static QoreStringNode* makeView(QoreStringNode* parent, size_t byte_offset, size_t byte_len);

    //! parses the string as a base64-encoded binary and returns the decoded value as a QoreStringNode
    DLLEXPORT QoreStringNode* parseBase64ToString(ExceptionSink* xsink) const;

    //! parses the current string data as base64-encoded data and returns it as a QoreStringNode pointer owned by the caller
    /**
        @param enc the encoding to tag the decoded string with
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return a QoreStringNode of the decoded data (0 if an exception occurs), the QoreStringNode pointer is owned by the caller
    */
    DLLEXPORT QoreStringNode* parseBase64ToString(const QoreEncoding* enc, ExceptionSink* xsink) const;

    //! parses the string as a base64-url-encoded binary and returns the decoded value as a QoreStringNode
    DLLEXPORT QoreStringNode* parseBase64UrlToString(ExceptionSink* xsink) const;

    //! parses the current string data as base64-url-encoded data and returns it as a QoreStringNode pointer owned by the caller
    /**
        @param enc the encoding to tag the decoded string with
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return a QoreStringNode of the decoded data (0 if an exception occurs), the QoreStringNode pointer is owned by the caller
    */
    DLLEXPORT QoreStringNode* parseBase64UrlToString(const QoreEncoding* enc, ExceptionSink* xsink) const;

    //! references the object and returns a non-const pointer to "this"
    DLLEXPORT QoreStringNode* stringRefSelf() const;

    //! removes characters from the string starting at position "offset" and returns a string of the characters removed
    /** values are for characters, not bytes.  If no characters a removed, an empty string is returned
        @param offset character position to start (rest of the string is removed) (offset starts with 0, negative offset means that many positions from the end of the string)
        @param xsink is ignored
        @return a string of the characters removed; if no characters a removed, an empty string is returned
    */
    DLLEXPORT QoreStringNode* extract(qore_offset_t offset, ExceptionSink* xsink);

    //! removes "length" characters from the string starting at position "offset" and returns a string of the characters removed
    /** values are for characters, not bytes.  If no characters a removed, an empty string is returned
        @param offset character position to start (rest of the string is removed) (offset starts with 0, negative offset means that many positions from the end of the string)
        @param length the number of characters (not bytes) to remove (negative length means all but that many characters from the end of the string)
        @param xsink invalid multi-byte encodings can cause an exception to be thrown
        @return a string of the characters removed; if no characters a removed, an empty string is returned, however if an exception is raised converting encodings, then 0 is returned
    */
    DLLEXPORT QoreStringNode* extract(qore_offset_t offset, qore_offset_t length, ExceptionSink* xsink);

    //! removes "length" characters from the string starting at position "offset" and replaces them with the string passed, then returns a string of the characters removed
    /** values are for characters, not bytes.  If no characters a removed, an empty string is returned
        @param offset character position to start (rest of the string is removed) (offset starts with 0, negative offset means that many positions from the end of the string)
        @param length the number of characters (not bytes) to remove (negative length means all but that many characters from the end of the string)
        @param strn the string to insert at character position "offset" after "length" characters are removed
        @param xsink invalid multi-byte encodings can cause an exception to be thrown
        @return a string of the characters removed; if no characters a removed, an empty string is returned, however if an exception is raised converting encodings, then 0 is returned
    */
    DLLEXPORT QoreStringNode* extract(qore_offset_t offset, qore_offset_t length, QoreValue strn, ExceptionSink* xsink);

    //! returns the type name (useful in templates)
    DLLLOCAL static const char* getStaticTypeName() {
        return "string";
    }

    //! returns the type code (useful in templates)
    DLLLOCAL static qore_type_t getStaticTypeCode() {
        return NT_STRING;
    }

    static const qore_type_t TYPE = NT_STRING;

    //! returns the type information
    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

    //! constructor supporting createAndConvertEncoding(), not exported in the library
    DLLLOCAL QoreStringNode(const char* str, const QoreEncoding* from, const QoreEncoding* to, ExceptionSink* xsink);

    //! constructor using the private implementation of QoreString; not exported in the library
    DLLLOCAL QoreStringNode(struct qore_string_private* p);
};

class QoreStringNodeMaker : public QoreStringNode {
public:
    DLLEXPORT QoreStringNodeMaker(const char* fmt, ...);

private:
    QoreStringNodeMaker(const QoreStringNodeMaker& str) = delete;
    QoreStringNodeMaker& operator=(const QoreStringNodeMaker&) = delete;
};

extern QoreStringNode* NullString;

//! this class is used to safely manage calls to AbstractQoreNode::getStringRepresentation() when a simple QoreString value is needed, stack only, may not be dynamically allocated
/** the QoreString value returned by this function is managed safely in an exception-safe way with this class
    @code
    QoreStringValueHelper str(n);
    printf("str='%s'\n", str->getBuffer());
    @endcode
*/
class QoreStringValueHelper {
public:
    //! creates the object and acquires a pointer to the QoreString representation of the QoreValue passed
    DLLEXPORT QoreStringValueHelper(const QoreValue n);

    //! gets the QoreString representation and ensures that it's in the desired encoding
    /** a Qore-language exception may be thrown if an encoding error occurs
         @code
        // get a QoreString value from "node" and ensure it's in UTF-8 encoding
        QoreStringValueHelper t(node, QCS_UTF8, xsink);
        // return if there was an exception converting the encoding to UTF-8
        if (*xsink)
        return 0;

        // use the string value
        return new MStringData(t->getBuffer(), MEncoding::M_ASCII);
        @endcode
    */
    DLLEXPORT QoreStringValueHelper(const QoreValue n, const QoreEncoding* enc, ExceptionSink* xsink);

    //! destroys the object and deletes the QoreString pointer being managed if it was a temporary pointer
    DLLEXPORT ~QoreStringValueHelper();

    //! returns the object being managed
    /**
         @return the object being managed
    */
    DLLEXPORT const QoreString* operator->();

    //! returns the object being managed
    /**
         @return the object being managed
    */
    DLLEXPORT const QoreString* operator*();

    //! returns true if the object being managed is not null
    DLLEXPORT explicit operator bool() const;

    //! returns a copy of the QoreString that the caller owns
    /** the object may be left empty after this call
         @return a QoreString pointer owned by the caller
    */
    DLLEXPORT QoreString* giveString();

    //! returns a char* string that the caller owns and must free()
    /** the object is empty after this call
    */
    DLLEXPORT char* giveBuffer();

    //! returns true if the pointer being managed is temporary
    DLLEXPORT bool is_temp() const;

private:
    QoreString* str;
    bool del;

    QoreStringValueHelper(const QoreStringValueHelper&) = delete;
    QoreStringValueHelper& operator=(const QoreStringValueHelper&) = delete;
    void* operator new(size_t) = delete;

    //! sets up the object / common initialization
    DLLEXPORT void setup(ExceptionSink* xsink, const QoreValue n, const QoreEncoding* enc = nullptr);
};

//! this class is used to safely manage calls to AbstractQoreNode::getStringRepresentation() when a QoreStringNode value is needed, stack only, may not be dynamically allocated
/** the QoreStringNode value returned by this function is managed safely in an exception-safe way with this class
    @code
    QoreStringNodeValueHelper str(n);
    printf("str='%s'\n", str->getBuffer());
    return str.getReferencedValue();
    @endcode
*/
class QoreStringNodeValueHelper {
public:
    DLLEXPORT QoreStringNodeValueHelper(const QoreValue n);

    //! gets the QoreString representation and ensures that it's in the desired encoding
    /** a Qore-language exception may be thrown if an encoding error occurs
         @code
        // get a QoreString value from "node" and ensure it's in UTF-8 encoding
        QoreStringNodeValueHelper t(node, QCS_UTF8, xsink);
        // return if there was an exception converting the encoding to UTF-8
        if (*xsink)
            return 0;

        // use the string value
        return new MStringData(t->getBuffer(), MEncoding::M_ASCII);
        @endcode
    */
    DLLEXPORT QoreStringNodeValueHelper(const QoreValue n, const QoreEncoding* enc, ExceptionSink* xsink);

    //! destroys the object and dereferences the QoreStringNode if it is a temporary pointer
    DLLEXPORT ~QoreStringNodeValueHelper();

    //! returns the object being managed
    /**
         @return the object being managed
    */
    DLLEXPORT const QoreStringNode* operator->();

    //! returns the object being managed
    /**
         @return the object being managed
    */
    DLLEXPORT const QoreStringNode* operator*();

    //! returns true if the object being managed is not null
    DLLEXPORT explicit operator bool() const;

    //! returns a referenced value - the caller will own the reference
    /**
         The string is referenced if necessary (if it was a not a temporary value)
        @return the string value, where the caller will own the reference count
    */
    DLLEXPORT QoreStringNode* getReferencedValue();

    //! returns true if the reference being managed is temporary
    DLLEXPORT bool is_temp() const;

private:
    QoreStringNode* str;
    bool del;

    QoreStringNodeValueHelper(const QoreStringNodeValueHelper&) = delete;
    QoreStringNodeValueHelper& operator=(const QoreStringNodeValueHelper&) = delete;
    void* operator new(size_t) = delete;

    //! sets up the object / common initialization
    DLLEXPORT void setup(ExceptionSink* xsink, const QoreValue n, const QoreEncoding* enc = nullptr);
};

#include <qore/ReferenceHolder.h>

//! For use on the stack only: manages a QoreStringNode reference count
/**
   @see SimpleRefHolder
*/
typedef SimpleRefHolder<QoreStringNode> QoreStringNodeHolder;

extern QoreString NothingTypeString;

//! safely manages the return values to AbstractQoreNode::getAsString(), stack only, cannot be dynamically allocated
/**
     @code
    // FMT_NONE gives all information on a single line
    QoreNodeAsStringHelper str(n, FMT_NONE, xsink);
    if (*xsink)
        return 0;
    printf("str='%s'\n", str->getBuffer());
    @endcode
*/
class QoreNodeAsStringHelper {
public:
    //! makes the call to AbstractQoreNode::getAsString() and manages the return values
    DLLEXPORT QoreNodeAsStringHelper(const AbstractQoreNode* n, int format_offset, ExceptionSink* xsink);

    //! makes the call to AbstractQoreNode::getAsString() and manages the return values
    DLLEXPORT QoreNodeAsStringHelper(const QoreValue n, int format_offset, ExceptionSink* xsink);

    //! destroys the object and deletes the QoreString pointer being managed if it was a temporary pointer
    DLLEXPORT ~QoreNodeAsStringHelper();

    //! returns the object being managed
    /**
         @return the object being managed
    */
    DLLEXPORT const QoreString* operator->();

    //! returns the object being managed
    /**
         @return the object being managed
    */
    DLLEXPORT const QoreString* operator*();

    //! returns a copy of the QoreString that the caller owns
    /** the object may be left empty after this call
         @return a QoreString pointer owned by the caller
    */
    DLLEXPORT QoreString* giveString();

private:
    QoreString* str;
    bool del;

    QoreNodeAsStringHelper(const QoreNodeAsStringHelper&) = delete;
    QoreNodeAsStringHelper& operator=(const QoreNodeAsStringHelper&) = delete;
    void* operator new(size_t) = delete;
};

#endif
