# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleCloudStorageDataProvider module
public namespace GoogleCloudStorageDataProvider {
#! Google Cloud Storage app name
public const AppName = "GoogleCloudStorage";

#! Logo file
/** Read as a separate file rather than inlined so that the doc build and the app catalog share one
    source of truth for the image
*/
const GoogleCloudStorageLogo = File::readTextFile(get_script_dir() + "/google-cloud-storage-logo.svg");

#! Maximum number of results requested per page when listing
public const MaxPageSize = 1000;

#! Content size (in bytes) above which an upload is sent as a resumable upload
/** Google recommends a resumable upload for anything above roughly 8 MiB, because a single-request
    upload has to start again from the beginning if the connection drops.  Below the threshold a
    single request is both fewer round trips and simpler.
*/
public const ResumableUploadThreshold = 8 * 1024 * 1024;

#! The chunk size used for resumable uploads
/** Google requires every resumable chunk except the last to be a multiple of 256 KiB.
*/
public const ResumableChunkSize = 8 * 1024 * 1024;

#! The required multiple for a non-final resumable upload chunk
public const ResumableChunkMultiple = 256 * 1024;

#! Maximum number of source objects Google Cloud Storage will compose into one object
public const MaxComposeSources = 32;

#! Default lifetime in seconds for a generated signed URL
public const DefaultSignedUrlExpirySeconds = 3600;

#! Maximum lifetime in seconds Google Cloud Storage allows for a V4 signed URL (7 days)
public const MaxSignedUrlExpirySeconds = 604800;

#! The signing algorithm named in a V4 signed URL
public const SignedUrlAlgorithm = "GOOG4-RSA-SHA256";
}
