/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreURL.h

    Network functions and macros

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

#ifndef _QORE_QOREURL_H

#define _QORE_QOREURL_H

#define QURL_KEEP_BRACKETS (1 << 0)
#define QURL_DECODE        (1 << 1)
#define QURL_DECODE_PATH   (1 << 2)
#define QURL_MAINTAIN_CASE (1 << 3)
#define QURL_DECODE_ANY    (QURL_DECODE | QURL_DECODE_PATH)

//! qore_resolve_url() option: reject arguments that are not valid URI references
/** Controls, space, malformed percent-encoded octets, and a relative-path reference whose first segment contains a
    colon are rejected
*/
#define QRU_STRICT          (1 << 0)
//! qore_resolve_url() option: remove any fragment from the target
#define QRU_NO_FRAGMENT     (1 << 1)
//! qore_resolve_url() option: accept a base that is a relative reference
#define QRU_RELATIVE_BASE   (1 << 2)
//! qore_resolve_url() option: percent-encode octets that cannot appear in a URI in the target (IRI to URI mapping)
#define QRU_ENCODE          (1 << 3)

class QoreString;
class QoreStringNode;
class ExceptionSink;

//! Resolves a URI reference against a base URI according to RFC 3986 section 5.2
/** @param base the base URI; must be an absolute URI unless @ref QRU_RELATIVE_BASE is given
    @param reference the reference to resolve
    @param options a bitfield of \c QRU_* options
    @param xsink if an error occurs, the Qore-language exception information will be added here

    @return the target in UTF-8 encoding (the caller owns the reference), or nullptr if an exception was raised

    @since %Qore 3.0
*/
DLLEXPORT QoreStringNode* qore_resolve_url(const QoreString& base, const QoreString& reference, int options,
        ExceptionSink* xsink);

//! helps with parsing URLs and provides access to URL components through Qore data structures
class QoreURL {
public:
    //! creates an empty structure
    /** @see QoreURL::parse()
    */
    DLLEXPORT QoreURL();

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call
        @param url the URL string to parse
    */
    DLLEXPORT QoreURL(const char* url);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call
        @param url the URL string to parse
    */
    DLLEXPORT QoreURL(const QoreString* url);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call
        @param url the URL string to parse
        @param keep_brackets if this argument is true then if the hostname or address is enclosed in square brackets,
        then the brackets will be included in the \c "host" key output as well
    */
    DLLEXPORT QoreURL(const char* url, bool keep_brackets);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call
        @param url the URL string to parse
        @param keep_brackets if this argument is true then if the hostname or address is enclosed in square brackets,
        then the brackets will be included in the \c "host" key output as well
    */
    DLLEXPORT QoreURL(const QoreString* url, bool keep_brackets);

    //! parses the URL string passed
    /**
         you can check if the URL was valid by calling QoreURL::isValid() after this call
        @param url the URL string to parse
        @param keep_brackets if this argument is true then if the hostname or address is enclosed in square brackets, then the brackets will be included in the \c "host" key output as well
        @param xsink for Qore-language exceptions

        @note the input string will be converted to UTF-8 before parsing

        @since Qore 0.8.12.8
    */
    DLLEXPORT QoreURL(const QoreString* url, bool keep_brackets, ExceptionSink* xsink);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @since %Qore 1.0.10
    */
    DLLEXPORT QoreURL(const char* url, int options);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @since %Qore 1.0.10
    */
    DLLEXPORT QoreURL(const QoreString& url, int options = 0);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @since %Qore 1.0.10
    */
    DLLEXPORT QoreURL(const std::string& url, int options = 0);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call

        @param xsink for Qore-language exceptions
        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @since %Qore 1.0.10
    */
    DLLEXPORT QoreURL(ExceptionSink* xsink, const char* url, int options);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call
        @param xsink for Qore-language exceptions
        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @since %Qore 1.0.10
    */
    DLLEXPORT QoreURL(ExceptionSink* xsink, const QoreString& url, int options = 0);

    //! parses the URL string passed
    /** you can check if the URL was valid by calling QoreURL::isValid() after this call

        @param xsink for Qore-language exceptions
        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @since %Qore 1.0.10
    */
    DLLEXPORT QoreURL(ExceptionSink* xsink, const std::string& url, int options = 0);

    //! frees all memory and destroys the structure
    DLLEXPORT ~QoreURL();

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.
        You can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
    */
    DLLEXPORT int parse(const char* url);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.
        You can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
    */
    DLLEXPORT int parse(const QoreString* url);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.
        You can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
        @param keep_brackets if this argument is true then if the hostname or address is enclosed in square brackets,
        then the brackets will be included in the \c "host" key output as well

        @return 0 if the URL was parsed successfully, -1 if not
    */
    DLLEXPORT int parse(const char* url, bool keep_brackets);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.
        You can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
        @param keep_brackets if this argument is true then if the hostname or address is enclosed in square brackets,
        then the brackets will be included in the \c "host" key output as well

        @return 0 if the URL was parsed successfully, -1 if not
    */
    DLLEXPORT int parse(const QoreString* url, bool keep_brackets);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.
        You can check if the URL was valid by calling QoreURL::isValid() after this call

        @param url the URL string to parse
        @param keep_brackets if this argument is true then if the hostname or address is enclosed in square brackets,
        then the brackets will be included in the \c "host" key output as well
        @param xsink for Qore-language exceptions

        @return 0 if the URL was parsed successfully, -1 if not

        @note the input string will be converted to UTF-8 before parsing

        @since Qore 0.8.12.8
    */
    DLLEXPORT int parse(const QoreString* url, bool keep_brackets, ExceptionSink* xsink);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.

        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @return 0 if the URL was parsed successfully, -1 if not

        @since %Qore 1.0.10
    */
    DLLEXPORT int parse(const char* url, int options);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.

        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @return 0 if the URL was parsed successfully, -1 if not

        @since %Qore 1.0.10
    */
    DLLEXPORT int parse(const QoreString& url, int options = 0);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.

        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @return 0 if the URL was parsed successfully, -1 if not

        @since %Qore 1.0.10
    */
    DLLEXPORT int parse(const std::string& url, int options = 0);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.

        @param xsink for Qore-language exceptions
        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @return 0 if the URL was parsed successfully, -1 if not

        @since %Qore 1.0.10
    */
    DLLEXPORT int parse(ExceptionSink* xsink, const char* url, int options = 0);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.

        @param xsink for Qore-language exceptions
        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @return 0 if the URL was parsed successfully, -1 if not

        @since %Qore 1.0.10
    */
    DLLEXPORT int parse(ExceptionSink* xsink, const QoreString& url, int options = 0);

    //! parses the URL string passed
    /** If a url was already parsed previously, all memory is freed before parsing the new string.

        @param xsink for Qore-language exceptions
        @param url the URL string to parse
        @param options a bitfield of %Qore URL options

        @return 0 if the URL was parsed successfully, -1 if not

        @since %Qore 1.0.10
    */
    DLLEXPORT int parse(ExceptionSink* xsink, const std::string& url, int options = 0);

    //! returns true if the URL string parsed is valid
    /** @return true if the URL string parsed is valid
    */
    DLLEXPORT bool isValid() const;

    //! returns a hash of the parameters parsed, destructive: zeros out all elements, caller owns the reference count returned
    /** hash keys are:
        - protocol
        - path
        - username
        - password
        - host
        - port
        .
        each key is either a QoreStringNode or 0 except for port which is an integer
        @note the caller must call QoreHashNode::deref() manually on the value returned if it's not 0 (or use the ReferenceHolder helper class)
        @return a hash of the parameters parsed
    */
    DLLEXPORT QoreHashNode* getHash();

    //! returns the hostname of the URL
    /** @return the hostname of the URL
    */
    DLLEXPORT const QoreString* getHost() const;

    //! returns the user name in the URL or 0 if none given
    /** @return the user name in the URL or 0 if none given
    */
    DLLEXPORT const QoreString* getUserName() const;

    //! returns the password in the URL or 0 if none given
    /** @return the password in the URL or 0 if none given
    */
    DLLEXPORT const QoreString* getPassword() const;

    //! returns the path component of the URL or 0 if none given
    /** @return the path component of the URL or 0 if none given
    */
    DLLEXPORT const QoreString* getPath() const;

    //! returns the protocol component of the URL or 0 if none given
    DLLEXPORT const QoreString* getProtocol() const;

    //! returns the port number given in the URL or 0 if none present
    /** @return the port number given in the URL or 0 if none present
    */
    DLLEXPORT int getPort() const;

    // the "take" methods return the char*  pointers for the data
    // the caller owns the memory

    //! returns a pointer to the path (0 if none present), caller owns the memory returned
    /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
         @return a pointer to the path (0 if none present), caller owns the memory returned
    */
    DLLEXPORT char* take_path();

    //! returns a pointer to the username in the URL (0 if none present), caller owns the memory returned
    /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
         @return a pointer to the username (0 if none present), caller owns the memory returned
    */
    DLLEXPORT char* take_username();

    //! returns a pointer to the password in the URL (0 if none present), caller owns the memory returned
    /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
         @return a pointer to the password (0 if none present), caller owns the memory returned
    */
    DLLEXPORT char* take_password();

    //! returns a pointer to the hostname in the URL (0 if none present), caller owns the memory returned
    /** if this function returns a non-zero pointer, the memory must be manually freed by the caller
         @return a pointer to the hostname (0 if none present), caller owns the memory returned
    */
    DLLEXPORT char* take_host();

private:
    //! private implementation of the class
    struct qore_url_private* priv;

    DLLLOCAL void zero();
    DLLLOCAL void reset();
    DLLLOCAL void parseIntern(const char* url, ExceptionSink* xsink);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreURL(const QoreURL&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreURL& operator=(const QoreURL&);
};

#endif
