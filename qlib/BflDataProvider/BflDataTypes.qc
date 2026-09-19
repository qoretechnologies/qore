# -*- mode: qore; indent-tabs-mode: nil -*-
#! Typed BFL job identities, results, and downloaded media
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

public namespace BflDataProvider {
#! Downloaded media with verified container metadata
public class BflMediaDataType inherits HashDataType {
    private {
        const Fields = {
            "data": {"display_name": "Media Bytes", "type": AbstractDataProviderTypeMap."binary", "required": True,
                "desc": "Downloaded image or video bytes with verified MIME and container metadata."},
            "content_type": {"display_name": "Content Type", "type": AbstractDataProviderTypeMap."string", "required": True,
                "desc": "Verified MIME type of the downloaded media."},
            "width": {"display_name": "Width", "type": AbstractDataProviderTypeMap."int", "required": True,
                "desc": "Media width in pixels."},
            "height": {"display_name": "Height", "type": AbstractDataProviderTypeMap."int", "required": True,
                "desc": "Media height in pixels."},
            "duration_seconds": {"display_name": "Duration", "type": AbstractDataProviderTypeMap."*float",
                "desc": "Video track duration in seconds."},
            "frame_count": {"display_name": "Frame Count", "type": AbstractDataProviderTypeMap."*int",
                "desc": "Number of encoded video samples."},
            "frame_rate": {"display_name": "Frame Rate", "type": AbstractDataProviderTypeMap."*float",
                "desc": "Average encoded video frames per second."},
            "has_audio": {"display_name": "Has Audio", "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Whether the video container declares an audio track."},
        };
    }
    #! Creates immutable media output fields
    constructor() : HashDataType("BflMedia") { addQoreFields(Fields); }
}
#! Shared verified media type
public const BflMedia = new BflMediaDataType();

#! Opaque encrypted draft bundle for a later full-quality render
public class BflDraftBundleDataType inherits HashDataType {
    private {
        const Fields = {
            "data": {"display_name": "Media Bytes", "type": AbstractDataProviderTypeMap."binary", "required": True,
                "desc": "Encrypted draft bundle bytes. Save them to reproduce the video after its URL expires."},
            "content_type": {"display_name": "Content Type", "type": AbstractDataProviderTypeMap."string", "required": True,
                "desc": "MIME type of the downloaded draft bundle."},
        };
    }
    #! Creates the typed opaque bundle result
    constructor() : HashDataType("BflDraftBundle") { addQoreFields(Fields); }
}
#! Download results preserve either verified media or an opaque encrypted bundle
public const BflDownloadedAsset = new QoreUnionDataType((BflMedia, new BflDraftBundleDataType()));

#! Common service result metadata, retaining vendor extensions
public class BflGenerateImageResultDataType inherits HashDataType {
    private {
        const Fields = {
            "sample": {"display_name": "Image URL", "type": AbstractDataProviderTypeMap."*uri",
                "desc": "Temporary signed image or video URL. Download it before it expires."},
            "seed": {"display_name": "Seed", "type": AbstractDataProviderTypeMap."*int",
                "desc": "Seed reported by the service, including zero."},
            "prompt": {"display_name": "Final Prompt", "type": AbstractDataProviderTypeMap."*string",
                "desc": "Final prompt used by the service."},
            "start_time": {"display_name": "Start Time", "type": AbstractDataProviderTypeMap."*float",
                "desc": "Generation start time as Unix seconds."},
            "end_time": {"display_name": "End Time", "type": AbstractDataProviderTypeMap."*float",
                "desc": "Generation end time as Unix seconds."},
            "draft_cache": {"display_name": "Draft Cache URL", "type": AbstractDataProviderTypeMap."*uri",
                "desc": "Temporary signed draft bundle URL for a later full-quality video render."},
        };
    }
    #! Creates result fields and preserves the service's open extensions
    constructor() : HashDataType("BflSample") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."auto");
    }
}
#! Shared service sample metadata type
public const BflSample = new BflGenerateImageResultDataType();

#! Job status and routed identity with optional verified media
public class BflGenerateImageResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {"display_name": "Job ID", "type": AbstractDataProviderTypeMap."string", "required": True,
                "desc": "BFL job identifier."},
            "status": {"display_name": "Status", "type": AbstractDataProviderTypeMap."string", "required": True,
                "desc": "Current service job status."},
            "polling_url": {"display_name": "Polling URL", "type": AbstractDataProviderTypeMap."*uri",
                "desc": "Original regional polling URL required to retrieve this job."},
            "result": {"display_name": "Result", "type": BflSample.getOrNothingType(),
                "desc": "Service result metadata, including the temporary media URL."},
            "media": {"display_name": "Downloaded Media", "type": BflMedia.getOrNothingType(),
                "desc": "Verified media bytes and dimensions after synchronous generation."},
            "cost": {"display_name": "Cost", "type": AbstractDataProviderTypeMap."*number",
                "desc": "Settled cost in credits, when reported by the service."},
            "progress": {"display_name": "Progress", "type": AbstractDataProviderTypeMap."*number",
                "desc": "Service-reported job progress."},
            "details": {"display_name": "Details", "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Additional job details; the service defines an open object."},
            "preview": {"display_name": "Preview", "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Additional preview metadata; the service defines an open object."},
        };
    }
    #! Creates immutable output fields and retains vendor extensions
    constructor() : HashDataType("BflJobResult") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."auto");
    }
}
#! Shared completed or pending job type
public const BflCompletedJob = new BflGenerateImageResponseDataType();

#! Original routed job identity with bounded wait and download controls
public class BflJobRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "Job ID", "type": AbstractDataProviderTypeMap."string", "required": True,
                "desc": "Job ID returned by a start action.", "example_value": "job-id-from-start"},
            "polling_url": {"display_name": "Polling URL", "type": AbstractDataProviderTypeMap."uri", "required": True,
                "desc": "Use the original regional `polling_url` returned with this job ID.",
                "example_value": "https://api.eu1.bfl.ai/v1/get_result?id=job-id-from-start"},
        };
        const Controls = {
            "job_timeout": {"display_name": "Overall Timeout", "type": AbstractDataProviderTypeMap."*timeout",
                "desc": "Maximum time for setup, submission, polling, and any required download.", "default_value": 5m},
            "poll_interval": {"display_name": "Poll Interval", "type": AbstractDataProviderTypeMap."*timeout",
                "desc": "Delay between polls. Production requests require at least 500 milliseconds.", "default_value": 2s},
            "max_bytes": {"display_name": "Maximum Download Size", "type": AbstractDataProviderTypeMap."*int",
                "desc": "Maximum downloaded bytes; the default is 100 MiB and the limit is 512 MiB.", "default_value": 104857600},
        };
    }
    #! Creates the static job form
    constructor() : HashDataType("BflJobRequest") { addQoreFields(Fields + Controls); }
}
#! Shared routed job input type
public const BflJobRequest = new BflJobRequestDataType();
}
