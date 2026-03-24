# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractDataProviderFactory.qc provides the abstract definition for a data provider factory

/** AbstractDataProviderFactory.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
/** @defgroup data_provider_api_profiles DataProvider API Profiles
    These constants define supported server profiles for data provider factories that provide API management services
*/
#/@{
#! Data provider HTTP server profile
/** This profile indicates that the given data provider factory supports serving APIs with REST and a REST schema
*/
public const DPSP_REST = "rest";

/** This profile indicates that the given data provider factory supports serving APIs with SOAP and a WSDL schema
*/
public const DPSP_SOAP = "soap";
#/@}

#! Data provider factory info
public hashdecl DataProviderFactoryInfo {
    #! The factory name
    string name;
    #! The factory description
    string desc;
    #! Does this factory create a data provider suitable for server-side API management
    bool api_management = False;
    #! Can any child data providers offer API services (request - response data providers)?
    bool children_can_support_apis = False;
    #! Can any child data providers offer record-based providers?
    bool children_can_support_records = False;
    #! Can any child data providers support the observer pattern / event API?
    bool children_can_support_observers = False;
    #! Can any child data providers support messages?
    bool children_can_support_messages = False;
    #! Can any child data providers support transaction management?
    /** @since DataProvider 2.8
    */
    bool children_can_support_transaction_management = False;
    #! Which server profiles are supported for API management (if any)?
    *softlist<string> api_profiles;
    #! Info about providers created from this factory (without the \a name and \a children attributes)
    hash<DataProviderInfo> provider_info;
}

#! Data provider factory class
public class AbstractDataProviderFactory {
    public {}

    #! Creates an object from the given constructor options
    AbstractDataProvider create(*hash<auto> options) {
        ConstructorMethodVariant con;
        try {
            con = getClass().getConstructorVariant(AutoHashOrNothingType);
        } catch (hash<ExceptionInfo> ex) {
            error("DATA-PROVIDER-ERROR", "data provider does not support a constructor from an option hash");
        }
        return con.call(options);
    }

    #! Creates an object from the given constructor options
    AbstractDataProvider createWithTemplate(*hash<auto> template_options, *hash<auto> options) {
        ConstructorMethodVariant con;
        try {
            con = getClass().getConstructorVariant(AutoHashType, AutoHashOrNothingType);
        } catch (hash<ExceptionInfo> ex) {
            try {
                con = getClass().getConstructorVariant(AutoHashOrNothingType);
            } catch (hash<ExceptionInfo> ex) {
                error("DATA-PROVIDER-ERROR", "data provider does not support a constructor from a template option "
                    "hash and an option hash nor a constructor from an option hash");
            }
            return con.call(options);
        }
        return con.call(template_options, options);
    }

    #! Returns the name of the data provider factory
    string getName() {
        return getNameImpl();
    }

    #! Returns the class for the data provider object
    Class getClass() {
        return getClassImpl();
    }

    #! Returns static factory information
    /** @note The \a name and \a children attributes in \a provider_info are not returned as they are dynamic
        attributes
    */
    hash<DataProviderFactoryInfo> getInfo() {
        return <DataProviderFactoryInfo>{
            "provider_info": getProviderInfo(),
        } + getInfoImpl();
    }

    #! Returns static provider information as data; no objects are returned
    /** @param with_type_info the raw info with objects describing the types

        @note The \a name and \a children attributes in \a provider_info are not returned as they are dynamic
        attributes
    */
    hash<auto> getInfoAsData(*bool with_type_info) {
        return {
            "provider_info": DataProvider::getInfoAsData(getProviderInfo(), with_type_info),
        } + getInfoImpl();
    }

    #! Returns static provider information
    /** @note The \a name and \a children attributes are not returned as they are dynamic attributes
    */
    hash<DataProviderInfo> getProviderInfo() {
        return getProviderInfoImpl();
    }

    #! Returns a data provider given the example data and constructor options as arguments
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return a data provider created from the given options

        @throw DATA-PROVIDER-ERROR invalid construtor options, API not supported

        @note if constructor options provide actual input data, then this overrides the example data, in this way this
        API can be used for the example and also with real data

        @since DataProvider 2.1
    */
    AbstractDataProvider getProviderFromExample(data example, *hash<auto> constructor_options) {
        return example.typeCode() == NT_STRING
            ? getProviderFromExampleImpl(new StringInputStream(example), constructor_options)
            : getProviderFromExampleImpl(new BinaryInputStream(example), constructor_options);
    }

    #! Returns a data provider given the example data and constructor options as arguments
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return a data provider created from the given options

        @throw DATA-PROVIDER-ERROR invalid construtor options, API not supported

        @note if constructor options provide actual input data, then this overrides the example data, in this way this
        API can be used for the example and also with real data

        @since DataProvider 2.1
    */
    AbstractDataProvider getProviderFromExample(InputStream example, *hash<auto> constructor_options) {
        return getProviderFromExampleImpl(example, constructor_options);
    }

    #! Returns information about a data provider constructor given the example data as an argument
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return example record data parsed from the given options

        @throw DATA-PROVIDER-ERROR invalid construtor options, API not supported

        @since DataProvider 2.1
    */
    list<hash<auto>> getExampleProviderRecordOutput(data example, *hash<auto> constructor_options) {
        return example.typeCode() == NT_STRING
            ? getExampleProviderRecordOutputImpl(new StringInputStream(example), constructor_options)
            : getExampleProviderRecordOutputImpl(new BinaryInputStream(example), constructor_options);
    }

    #! Returns information about a data provider constructor given the example data as an argument
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return example record data parsed from the given options

        @throw DATA-PROVIDER-ERROR invalid construtor options, API not supported

        @since DataProvider 2.1
    */
    list<hash<auto>> getExampleProviderRecordOutput(InputStream example, *hash<auto> constructor_options) {
        return getExampleProviderRecordOutputImpl(example, constructor_options);
    }

    #! thrown an exception
    private error(string err, string fmt, ...) {
        throw err, sprintf("data provider factory %y (%y): %s", getName(), self.className(), vsprintf(fmt, argv));
    }

    #! Returns a data provider given the example data and constructor options as arguments
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return a data provider created from the given options

        @throw DATA-PROVIDER-ERROR invalid construtor options, API not supported

        @note
        - If constructor options provide actual input data, then this overrides the example data, in this way this
          API can be used for the example and also with real data
        - This base class method throws a \c DATA-PROVIDER-ERROR exception by default; override in subclasses to
          implement this method properly

        @since DataProvider 2.1
    */
    private AbstractDataProvider getProviderFromExampleImpl(InputStream example, *hash<auto> constructor_options) {
        throw "DATA-PROVIDER-ERROR", sprintf("ths %s data provider does not support this API", self.className());
    }

    #! Returns information about a data provider constructor given the example data as an argument
    /**
        @param example example data to process
        @param constructor_options constructor options for the data provider

        @return example record data parsed from the given options

        @throw DATA-PROVIDER-ERROR invalid construtor options, API not supported

        @note This base class method throws a \c DATA-PROVIDER-ERROR exception by default; override in subclasses to
        implement this method properly

        @since DataProvider 2.1
    */
    private list<hash<auto>> getExampleProviderRecordOutputImpl(InputStream example, *hash<auto> constructor_options) {
        throw "DATA-PROVIDER-ERROR", sprintf("ths %s data provider does not support this API", self.className());
    }

    #! Returns the name of the data provider factory
    private string getNameImpl() {
        return getInfoImpl().name;
    }

    #! Returns static factory information without \a provider_info
    /** @return static factory information without \a provider_info which is provided by @ref getProviderInfo()
    */
    private abstract hash<DataProviderFactoryInfo> getInfoImpl();

    #! Returns the class for the data provider object
    private abstract Class getClassImpl();

    #! Returns static provider information
    /** @note The \a name and \a children attributes are not returned as they are dynamic attributes
    */
    private abstract hash<DataProviderInfo> getProviderInfoImpl();
}
}
