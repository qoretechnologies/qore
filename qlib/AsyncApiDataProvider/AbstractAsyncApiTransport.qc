# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractAsyncApiTransport class definition

/** AbstractAsyncApiTransport.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Abstract base class for AsyncAPI message transports
/** This interface allows the AsyncApiDataProvider to work with different messaging
    protocols such as WebSocket, MQTT, Kafka, AMQP, etc.

    Implementations should handle the protocol-specific details of sending and
    receiving messages.
*/
public class AbstractAsyncApiTransport {
    #! Sends a message to the specified channel
    /** @param channel the channel/topic to send to
        @param msg the message data to send (already validated)
        @param headers optional headers/metadata for the message

        @throw TRANSPORT-ERROR if the message cannot be sent
    */
    abstract send(string channel, auto msg, *hash<auto> headers);

    #! Returns the protocol name for this transport
    /** @return the protocol name (e.g., "ws", "wss", "mqtt", "kafka", "amqp")
    */
    abstract string getProtocol();

    #! Returns True if the transport is currently connected
    /** @return True if connected, False otherwise
    */
    abstract bool isConnected();

    #! Connects the transport if not already connected
    /** @throw TRANSPORT-ERROR if connection fails
    */
    abstract connect();

    #! Disconnects the transport
    disconnect() {
        # default implementation does nothing
    }

    #! Sets a logger for the transport
    /** @param logger the logger to use, or NOTHING to clear
    */
    setLogger(*LoggerInterface logger) {
        # default implementation does nothing
    }
}

}
