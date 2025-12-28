# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AsyncApiChannelDataProvider module definition

/** AsyncApiChannelDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AsyncApiDataProvider module
public namespace AsyncApiDataProvider {
#! The AsyncAPI channel data provider class
public class AsyncApiChannelDataProvider inherits AsyncApiDataProviderBase {
    public {
        #! The channel name
        string channel_name;

        #! The channel object
        AsyncApi::ChannelObject channel;
    }

    #! Creates the object from the arguments
    constructor(AsyncApi::AsyncApi3Schema schema, *AbstractAsyncApiTransport transport, string channel_name,
            AsyncApi::ChannelObject channel) : AsyncApiDataProviderBase(transport) {
        self.schema = schema;
        self.channel_name = channel_name;
        self.channel = channel;
    }

    #! Returns the data provider name
    string getName() {
        return channel_name;
    }

    #! Returns the data provider description
    *string getDesc() {
        string desc = sprintf("AsyncAPI channel %y", channel_name);
        if (channel.address) {
            desc += sprintf(" (address: %s)", channel.address);
        }
        if (channel.desc) {
            desc += sprintf("\n\n%s", channel.desc);
        }
        return desc;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        # return all operations that reference this channel
        list<string> ops = ();
        foreach string opId in (keys schema.operations) {
            *AsyncApi::OperationObject op = schema.getOperation(opId);
            if (op && op.channelName == channel_name) {
                ops += opId;
            }
        }
        return ops;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *AsyncApi::OperationObject op = schema.getOperation(name);
        if (op && op.channelName == channel_name) {
            return new AsyncApiOperationDataProvider(schema, transport, channel_name, channel, name, op);
        }
    }
}
}
