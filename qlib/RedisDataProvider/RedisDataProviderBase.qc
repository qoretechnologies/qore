# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProviderBase class definition

/** RedisDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RedisDataProvider module
public namespace RedisDataProvider {
#! The Redis data provider base class
public class RedisDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The Redis client
        RedisClient redis;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object from a Redis client
    constructor(RedisClient redis) {
        self.redis = redis;
    }

    #! Returns the Redis client
    RedisClient getRedisClient() {
        return redis;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        redis.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }
}
}
