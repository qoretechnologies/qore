# -*- mode: qore; indent-tabs-mode: nil -*-
#! Scoped BFL job routing and credential-free asset retrieval
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace BflRestClient {
#! Validates server-issued identities and runs independent deadline-bound waits
public class BflJobs {
    #! Checks a routed polling identity without making a request
    /** @param client configured API client, including an explicit loopback fixture if applicable
        @param job id and polling_url returned by a start action
        @return validated origin and relative result path
        @throw BFL-JOB-ERROR malformed identity or untrusted destination
    */
    static hash<auto> route(BflRestClientIo client, hash<auto> job) {
        if (job.id.typeCode() != NT_STRING || job.id !~ /^[a-zA-Z0-9_.:-]{1,256}$/
                || job.polling_url.typeCode() != NT_STRING) {
            throw "BFL-JOB-ERROR", "a service job ID and its polling_url are required";
        }
        hash<auto> url;
        try { url = parse_url(job.polling_url); }
        catch () { throw "BFL-JOB-ERROR", "invalid polling URL"; }
        if (url.username.val() || url.password.val() || job.polling_url =~ /[@#\r\n\\]/
                || url.path !~ /^\/v1\/get_result\?id=[a-zA-Z0-9_.:%-]+$/
                || url.path =~ /%(?![a-fA-F0-9]{2})/) {
            throw "BFL-JOB-ERROR", "polling URL must identify exactly one BFL result";
        }
        if (decode_url(url.path.substr(18)) != job.id) {
            throw "BFL-JOB-ERROR", "polling URL does not match the job ID";
        }
        string origin = url.protocol + "://" + url.host + (url.port ? ":" + url.port : "");
        # Client copies apply the same global/regional scope as every authenticated request.
        client.copyWithUrl(origin);
        return {"origin": origin, "path": url.path};
    }

    #! Checks a submission response before presenting it as a resumable job
    /** @param client selected API client
        @param result response object
        @param webhook whether the request explicitly selected callback delivery
        @return validated response, preserving costs and service metadata
        @throw BFL-JOB-ERROR invalid response shape
    */
    static hash<auto> submitted(BflRestClientIo client, auto result, bool webhook = False) {
        if (result.typeCode() != NT_HASH || result.hasKey("error")
                || result.id.typeCode() != NT_STRING || result.id !~ /^[a-zA-Z0-9_.:-]{1,256}$/) {
            throw "BFL-JOB-ERROR", "submission did not return a valid job ID; do not resubmit automatically";
        }
        if (result.hasKey("polling_url")) {
            route(client, result);
        } else if (!webhook || result.webhook_url.typeCode() != NT_STRING
                || result.status.typeCode() != NT_STRING) {
            throw "BFL-JOB-ERROR", "submission did not return the selected polling or callback identity",
                {"job_id": result.id};
        }
        return result;
    }

    #! Validates every documented state and preserves the response's typed metadata
    /** @param result decoded response
        @param id expected job ID
        @return validated result
        @throw BFL-JOB-ERROR missing or mismatched identity, unknown status, or malformed Ready payload
    */
    static hash<auto> checkedStatus(auto result, string id) {
        if (result.typeCode() != NT_HASH || result.id !== id
                || !inlist(result.status, ("Pending", "Reasoning", "Generating", "Task not found",
                    "Request Moderated", "Content Moderated", "Ready", "Error", "Failed"))) {
            throw "BFL-JOB-ERROR", "result has a missing or mismatched job ID or unknown status", {"job_id": id};
        }
        if (result.status == "Ready" && (result.result.typeCode() != NT_HASH
                || result.result.sample.typeCode() != NT_STRING || !result.result.sample.val())) {
            throw "BFL-JOB-ERROR", "Ready result does not contain a media sample", {"job_id": id};
        }
        return result;
    }

    #! Gets a job through its original regional polling URL
    /** @param client API client
        @param job original id and polling_url
        @param budget request/setup budget in milliseconds
        @return status response with its routed polling identity
        @throw BFL-JOB-ERROR invalid identity or response
    */
    static hash<auto> get(BflRestClientIo client, hash<auto> job, timeout budget = 30s) {
        date start = now_us();
        hash<auto> target = route(client, job);
        BflRestClientIo routed = cast<BflRestClientIo>(client.copyWithUrl(target.origin));
        int remaining = int(budget) - get_duration_milliseconds(now_us() - start);
        if (remaining <= 0) { timedOut(job); }
        routed.setTimeout(min(client.getTimeout(), remaining));
        routed.setConnectTimeout(min(client.getConnectTimeout(), remaining));
        hash<auto> response = routed.restGet(target.path);
        hash<auto> status = checkedStatus(response.body, job.id);
        if (get_duration_milliseconds(now_us() - start) >= int(budget)) { timedOut(job, status); }
        status.polling_url = job.polling_url;
        hash<auto> headers = map {$1.key.lwr(): $1.value}, response.headers.pairIterator();
        if (headers."retry-after".typeCode() == NT_STRING && headers."retry-after" =~ /^\d+$/) {
            status.retry_after = int(headers."retry-after");
        }
        return status;
    }

    #! Waits without changing shared client or poller state
    /** @param client API client
        @param job original routed identity
        @param budget overall wait budget
        @param interval polling interval
        @param sleeper optional test sleeper captured for this job only
        @return Ready status with original routed identity
        @throw BFL-JOB-TIMEOUT deadline expired; error arguments preserve resumable identity
        @throw BFL-JOB-ERROR terminal failure or invalid response
    */
    static hash<auto> wait(BflRestClientIo client, hash<auto> job, timeout budget = DefaultJobTimeout,
            timeout interval = DefaultPollInterval, *code<nothing(timeout)> sleeper) {
        date start = now_us();
        validateTiming(client, budget, interval);
        route(client, job);
        *hash<auto> last;
        int delay = int(interval);
        QoreAsyncJobUtils::AsyncJobPoller poller(hash<auto> sub (string id) {
            int remaining = int(budget) - get_duration_milliseconds(now_us() - start);
            if (remaining <= 0) { timedOut(job, last); }
            last = get(client, job, remaining);
            delay = max(int(interval), (last.retry_after ?? 0) * 1000);
            return last;
        }, bool sub (hash<auto> status) {
            return !inlist(status.status, ("Pending", "Reasoning", "Generating"));
        }, "BFL job", "BFL-JOB-TIMEOUT");
        poller.setSleeper(sub (timeout suggested) {
            int remaining = max(0, int(budget) - get_duration_milliseconds(now_us() - start));
            timeout amount = min(delay, remaining);
            if (sleeper) { sleeper(amount); } else { usleep(int(amount) * 1000); }
        });
        hash<auto> result;
        try {
            int remaining = int(budget) - get_duration_milliseconds(now_us() - start);
            if (remaining <= 0) { timedOut(job, last); }
            result = poller.poll(job.id, remaining, interval);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err =~ /TIMEOUT/) { timedOut(job, last); }
            throw "BFL-JOB-ERROR", "job polling failed; retain its identity before retrying retrieval",
                safeIdentity(job, last) + {"cause": ex.err};
        }
        if (result.status != "Ready") {
            throw "BFL-JOB-ERROR", "BFL job ended with status " + result.status, safeIdentity(job, result);
        }
        return result;
    }

    #! Checks timeout controls before a paid submission
    /** @param client selected client
        @param budget total operation budget
        @param interval polling delay
        @throw BFL-INPUT-ERROR invalid timeouts or excessive production polling
    */
    static validateTiming(BflRestClientIo client, timeout budget, timeout interval) {
        if (budget <= 0 || interval <= 0 || (client.getUrl() !~ /^http:/ && interval < 500)) {
            throw "BFL-INPUT-ERROR", "timeouts must be positive; production polling requires at least 500 milliseconds";
        }
    }

    #! Validates caller controls before any paid or resumable operation
    /** @param client selected API client
        @param options caller controls; absent or null controls use documented defaults
        @throw BFL-INPUT-ERROR invalid timeout or download limit
    */
    static validateControls(BflRestClientIo client, hash<auto> options) {
        foreach string name in (("job_timeout", "poll_interval")) {
            if (exists options{name} && options{name}.typeCode() != NT_NULL
                    && !inlist(options{name}.typeCode(), (NT_INT, NT_DATE))) {
                throw "BFL-INPUT-ERROR", "timeout controls must be integer milliseconds or relative dates";
            }
            if (options{name}.typeCode() == NT_DATE && !options{name}.relative()) {
                throw "BFL-INPUT-ERROR", "timeout controls require relative durations";
            }
        }
        validateTiming(client, options.job_timeout ?? DefaultJobTimeout, options.poll_interval ?? DefaultPollInterval);
        if (exists options.max_bytes && options.max_bytes.typeCode() != NT_NULL
                && (options.max_bytes.typeCode() != NT_INT || options.max_bytes <= 0 || options.max_bytes > 536870912)) {
            throw "BFL-INPUT-ERROR", "max_bytes must be an integer from 1 through 536870912";
        }
    }

    #! Returns resumable identity with no signed delivery URLs or arbitrary service error text
    /** @param job validated job identity
        @param last most recent status
        @return safe exception context
    */
    static hash<auto> safeIdentity(hash<auto> job, *hash<auto> last) {
        return {"job_id": job.id, "polling_url": job.polling_url,
            "last_status": last{"id", "status", "progress", "cost"}};
    }

    #! Raises a timeout carrying the identity needed to resume retrieval
    /** @param job validated identity
        @param last most recent status
        @throw BFL-JOB-TIMEOUT always
    */
    static timedOut(hash<auto> job, *hash<auto> last) {
        throw "BFL-JOB-TIMEOUT", "BFL operation deadline expired; retrieve the existing job to resume",
            safeIdentity(job, last);
    }
}

#! Downloads a temporary BFL delivery URL using a fresh client with no API credentials
/** @param url signed delivery URL; never included in diagnostics
    @param budget total setup and download budget
    @param max_bytes maximum accepted asset size
    @param fixture_origin explicit loopback origin, allowed only for localhost fixture clients
    @param draft_bundle True for an opaque encrypted draft bundle; its MIME and size are checked
    @return verified data, MIME type, and image dimensions when applicable
    @throw BFL-DOWNLOAD-ERROR untrusted URL, expiry, redirect, size violation, or invalid media
    @throw BFL-DOWNLOAD-TIMEOUT overall deadline expired
*/
public hash<auto> sub downloadMedia(string url, timeout budget = 60s, int max_bytes = 104857600,
        *string fixture_origin, bool draft_bundle = False) {
    date start = now_us();
    if (budget <= 0 || max_bytes <= 0 || max_bytes > 536870912) {
        throw "BFL-DOWNLOAD-ERROR", "download timeout and maximum size must be positive; size cannot exceed 512 MiB";
    }
    hash<auto> parsed;
    try { parsed = parse_url(url); }
    catch () { throw "BFL-DOWNLOAD-ERROR", "invalid delivery URL"; }
    string origin = parsed.protocol + "://" + parsed.host + (parsed.port ? ":" + parsed.port : "");
    bool fixture = fixture_origin && fixture_origin =~ /^http:/ && origin == fixture_origin;
    if (fixture) { BflRestClientBase::normalizeEndpoint(origin); }
    if (parsed.username.val() || parsed.password.val() || url =~ /[@#\r\n\\]/
            || parsed.path !~ /^\/[^\s]*$/ || (!fixture && (parsed.protocol != "https"
                || parsed.host !~ /^delivery\.[a-z0-9]+(?:-[a-z0-9]+)*\.bfl\.ai$/
                || (parsed.port && parsed.port != 443)))) {
        throw "BFL-DOWNLOAD-ERROR", "use an HTTPS BFL delivery URL from a completed job";
    }
    *HttpClientIo::HttpClientStreamHandle stream;
    hash<auto> headers;
    binary content;
    on_exit { if (stream) { stream.cancel(); } }
    try {
        HttpClientIo::HttpClientConnectionManager manager(<HttpClientIo::HttpClientConnectionManagerOptions>{"request_timeout": budget,
            "connect_timeout": min(10000, int(budget)), "ssl_verify_mode": SSL_VERIFY_PEER,
            "accept_all_certs": False, "max_request_retries": 0});
        stream = manager.acquireStream(origin);
        stream.startStreaming("GET", parsed.path, {"Accept": draft_bundle ? "application/octet-stream" : "image/*, video/mp4",
            "Accept-Encoding": "identity"});
        bool checked = False;
        while (True) {
            int remaining = int(budget) - get_duration_milliseconds(now_us() - start);
            if (remaining <= 0) { throw "BFL-DOWNLOAD-TIMEOUT", "asset download deadline expired"; }
            auto chunk = stream.readData(remaining);
            if (!exists chunk) { throw "BFL-DOWNLOAD-TIMEOUT", "asset download deadline expired"; }
            if (!checked) {
                *hash<auto> response = stream.getResponse();
                if (response) {
                    if (response.status_code != 200) {
                        throw "BFL-DOWNLOAD-ERROR", "asset download failed; the URL may have expired",
                            {"status_code": response.status_code};
                    }
                    headers = map {$1.key.lwr(): $1.value}, response.headers.pairIterator();
                    if (headers."content-type".typeCode() != NT_STRING
                            || (headers."content-length" =~ /^\d+$/ && int(headers."content-length") > max_bytes)
                            || (headers."content-encoding" && headers."content-encoding" != "identity")) {
                        throw "BFL-DOWNLOAD-ERROR", "invalid media response headers or excessive content length";
                    }
                    checked = True;
                }
            }
            if (chunk.typeCode() == NT_HASH) {
                if (chunk.error) { throw "BFL-DOWNLOAD-ERROR", "asset transfer failed"; }
                if (chunk.end_stream) { break; }
                continue;
            }
            if (!checked || !inlist(chunk.typeCode(), (NT_BINARY, NT_STRING)) || content.size() + chunk.size() > max_bytes) {
                throw "BFL-DOWNLOAD-ERROR", "asset exceeds the configured size or lacks response headers";
            }
            content += chunk.typeCode() == NT_BINARY ? chunk : binary(chunk);
        }
        if (!checked || !content.size()) { throw "BFL-DOWNLOAD-ERROR", "asset download returned no media"; }
        if (get_duration_milliseconds(now_us() - start) >= int(budget)) {
            throw "BFL-DOWNLOAD-TIMEOUT", "asset download deadline expired";
        }
        if (draft_bundle) {
            if (trim(headers."content-type".split(";")[0]).lwr() != "application/octet-stream") {
                throw "BFL-DOWNLOAD-ERROR", "draft bundles require application/octet-stream";
            }
            return {"data": content, "content_type": "application/octet-stream"};
        }
        string mime = QoreMediaUtils::validateMediaContentType(content, headers."content-type");
        if (!inlist(mime, ("image/jpeg", "image/png", "image/webp", "video/mp4"))) {
            throw "BFL-DOWNLOAD-ERROR", "unsupported generated media type";
        }
        hash<auto> result = {"data": content, "content_type": mime};
        if (mime =~ /^image\//) { result += QoreMediaUtils::getImageDimensions(content); }
        else { result += QoreMediaUtils::getMp4VideoInfo(content); }
        return result;
    } catch (hash<ExceptionInfo> ex) {
        # Neither transport exceptions nor signed URL queries may escape through diagnostics.
        if (ex.err =~ /TIMEOUT/) { throw "BFL-DOWNLOAD-TIMEOUT", "asset download deadline expired"; }
        throw "BFL-DOWNLOAD-ERROR", "asset download failed validation or transfer",
            {"cause": ex.err, "status_code": ex.arg.status_code};
    }
}
}
