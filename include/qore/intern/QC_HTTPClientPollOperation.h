/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HTTPClientPollOperation.h

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

#ifndef _QORE_CLASS_HTTPCLIENTPOLLOPERATION_H

#define _QORE_CLASS_HTTPCLIENTPOLLOPERATION_H

#include "qore/intern/QC_AbstractPollOperation.h"
#include "qore/QoreHttpClientObject.h"

// goals: connect
constexpr int HCPG_CONNECT = 1;

// states:
constexpr int HCPS_NONE = 0;

constexpr int SPS_CONNECTING = 1;
constexpr int SPS_CONNECTING_SSL = 2;
constexpr int SPS_CONNECTED = 3;

class HTTPClientPollOperation : public AbstractPollOperation {
public:
    DLLLOCAL HTTPClientPollOperation(ExceptionSink* xsink, const QoreStringNode* goal, const QoreObject* client_obj,
            QoreHttpClientObject* client) : AbstractPollOperation(goal, client_obj), client(client) {
        if (*goal == "connect") {
            sgoal = HCPG_CONNECT;

            state = SPS_CONNECTING;
        } else {
            xsink->raiseException("INVALID-GOAL", "invalid goal \"%s\"; known goal: \"connect\"", goal->c_str());
        }
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            client->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_CONNECTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) {
        QoreHashNode* rv = nullptr;

        switch (state) {
            case SPS_CONNECTING: {
                int rc = checkContinuePoll(xsink);
                if (rc != 0) {
                    rv = *xsink ? nullptr : getHTTPClientPollInfoHash(xsink, rc);
                    break;
                }

                // if we are just connecting, we are done
                if (sgoal == SPG_CONNECT) {
                    state = SPS_CONNECTED;
                    break;
                }

                assert(sgoal == SPG_CONNECT_SSL);
                state = SPS_CONNECTING_SSL;

                poll_state.reset(
                    client->startSslConnect(
                        xsink,
                        client->priv->cert ? client->priv->cert->getData() : nullptr,
                        client->priv->pk ? client->priv->pk->getData() : nullptr
                    )
                );
                if (*xsink) {
                    poll_state.reset();
                    state = SPS_NONE;
                    break;
                }
            }
            // fall down to next case

            case SPS_CONNECTING_SSL: {
                int rc = checkContinuePoll(xsink);
                if (rc != 0) {
                    rv = *xsink ? nullptr : getHTTPClientPollInfoHash(xsink, rc);
                    break;
                }

                state = SPS_CONNECTED;
                break;
            }

            default:
                assert(false);
        }

        if (!rv) {
            if (*xsink) {
                state = SPS_NONE;
            } else {
                state = SPS_CONNECTED;
            }
        }
        return rv;
    }

private:
    unique_ptr<AbstractPollState> poll_state;
    QoreHttpClientObject* client;

    int sgoal = 0;
    int state = SPS_NONE;

    DLLLOCAL virtual const char* getStateImpl() const {
        const char* str;
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_CONNECTING:
                return "connecting";
            case SPS_CONNECTING_SSL:
                return "connecting-ssl";
            case SPS_CONNECTED:
                return "connected";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink) {
        assert(poll_state.get());

        // see if we are able to continue
        int rc = poll_state->continuePoll(xsink);
        //printd(5, "HTTPClientPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
        //    (int)*xsink);
        if (*xsink) {
            assert(rc < 0);
            state = SPS_NONE;
            return -1;
        }
        if (!rc) {
            // release the AbstractPollState value
            poll_state.reset();
        }
        return rc;
    }
};

DLLLOCAL QoreClass* initHTTPClientPollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTPCLIENTPOLLOPERATION_H
