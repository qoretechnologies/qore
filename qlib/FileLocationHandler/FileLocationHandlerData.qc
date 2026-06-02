# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerData.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerData Copyright 2021 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FileLocationHandler module
public namespace FileLocationHandler {
#! The class for handling "data://" locations
/** This location can be used for reading immediate data; data:// locations cannot be written to
*/
class FileLocationHandlerData inherits AbstractFileLocationHandler {
    public {
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the location string without the scheme
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        return location;
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the location string without the scheme converted to binary data
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        return location.toBinary();
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        return new ImmediateDataPollOperation(binary(location));
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the location string without the scheme
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), location.encoding());
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new StringInputStream(source: location);
    }

    #! Not supported for data:// locations; throws an exception
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @throw DATA-WRITE-ERROR cannot write to data:// locations
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        throw "DATA-WRITE-ERROR", "data:// locations do not support writing";
    }

    #! Not supported for data:// locations; throws an exception
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @throw DATA-WRITE-ERROR cannot write to data:// locations
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        throw "DATA-WRITE-ERROR", "data:// locations do not support writing";
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return {};
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return {};
    }
}

#! This class allows immediate data to be returned for a polling operation
public class ImmediateDataPollOperation inherits AbstractPollOperation<auto> {
    private {
        #! The value to return
        auto value;
    }

    #! Creates the object with the value to be returned
    constructor(auto value) {
        self.value = value;
    }

    #! The goal is always \c "done"
    string getGoal() {
        return "done";
    }

    #! The state is always \c "done"
    string getState() {
        return "done";
    }

    #! Always returns @ref True
    bool goalReached() {
        return True;
    }

    #! Always returns @ref nothing
    *hash<SocketPollInfo> continuePoll() {
    }

    #! Returns the value
    auto getOutput() {
        return value;
    }
}
}
