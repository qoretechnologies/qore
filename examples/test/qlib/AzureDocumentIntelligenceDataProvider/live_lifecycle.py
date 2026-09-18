#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
"""Opt-in synthetic Azure lifecycle verification, with private subprocess transport.

Requires az login and a built Qore tree. --provision-storage and --temporary-role
explicitly authorize the corresponding temporary mutations. Only objects created
by this invocation are cleaned up; existing models and assignments are untouched.
No keys, tokens, SAS URLs, documents, or raw child diagnostics are logged.
"""
import argparse
import base64
import datetime
import json
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
import xml.etree.ElementTree as ET


class VerificationError(Exception):
    """Contains only deliberately public diagnostics."""


def require(condition, message):
    if not condition:
        raise VerificationError(message)


def report(message):
    print("live " + message, flush=True)


def pdf_document(kind, number=0):
    """Generate one original vector PDF page with no external dependencies."""
    def text(x, y, size, value):
        require(not any(c in value for c in "()\\"), "unsafe synthetic PDF text")
        return f"BT /F1 {size} Tf {x} {y} Td ({value}) Tj ET\n"
    if kind == "figure":
        stream = text(60, 735, 22, "QORE SYNTHETIC QUARTERLY REPORT")
        stream += text(60, 700, 12, "Quarterly synthetic sales are illustrated in Figure 1.")
        stream += "0 0 0 RG 2 w 90 310 m 90 630 l S 90 310 m 510 310 l S\n"
        for i, height in enumerate((100, 170, 250, 210)):
            x = 120 + i * 95
            stream += f"0.1 0.4 0.8 rg {x} 310 55 {height} re f 0 0 0 rg\n"
            stream += text(x, 285, 14, f"Q{i + 1}")
            stream += text(x + 10, 320 + height, 12, str(height))
        stream += text(90, 245, 12, "Figure 1. Synthetic sales by quarter - units sold.")
        stream += text(60, 180, 12, "All figures are invented for software verification.")
    elif kind == "invoice":
        stream = text(60, 735, 24, "QORE SYNTHETIC INVOICE")
        for y, value in ((680, f"Invoice number INV-{100 + number}"),
                         (630, "Supplier: Example Widgets"), (590, "Customer: Sample Buyer"),
                         (530, "Description: Synthetic software test service"),
                         (480, "Quantity: 1"), (400, f"TOTAL USD {12 + number}.34"),
                         (300, "Payment due in 30 days")):
            stream += text(60, y, 16, value)
    else:
        stream = text(150, 735, 24, "SYNTHETIC CAFE RECEIPT")
        for y, value in ((650, "Thank you for visiting Example Cafe"),
                         (590, f"Order {200 + number}"), (510, "Coffee 3.00"),
                         (460, "Sandwich 7.00"), (360, "PAID CASH 10.00"),
                         (290, "Have a nice day!")):
            stream += text(150, y, 16, value)
    payload = stream.encode("ascii")
    objects = [b"<< /Type /Catalog /Pages 2 0 R >>", b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
               b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
               b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
               f"<< /Length {len(payload)} >>\nstream\n".encode() + payload + b"endstream"]
    result = bytearray(b"%PDF-1.4\n")
    offsets = [0]
    for index, obj in enumerate(objects, 1):
        offsets.append(len(result))
        result.extend(f"{index} 0 obj\n".encode() + obj + b"\nendobj\n")
    xref = len(result)
    result.extend(b"xref\n0 6\n0000000000 65535 f \n")
    for offset in offsets[1:]:
        result.extend(f"{offset:010d} 00000 n \n".encode())
    result.extend(f"trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    return bytes(result)


class LiveRun:
    def __init__(self, args):
        self.args = args
        self.run_id = uuid.uuid4().hex[:12]
        self.prefix = "qore5421-" + self.run_id
        self.storage = "qore5421" + self.run_id
        self.storage_attempted = False
        self.role_id = None
        self.cleanup = []
        self.pending = {}
        self.env = dict(os.environ)
        self.subscription = self.az("account", "show")["id"]
        self.account = self.az("cognitiveservices", "account", "show", "-n", args.account,
                               "-g", args.resource_group)
        keys = self.az("cognitiveservices", "account", "keys", "list", "-n", args.account,
                       "-g", args.resource_group)
        self.env.update(AZURE_DI_ENDPOINT=self.account["properties"]["endpoint"], AZURE_DI_KEY=keys["key1"])
        self.refresh_token()

    def az(self, *arguments, env=None):
        cmd = ["az", *arguments, "--output", "json", "--only-show-errors"]
        if hasattr(self, "subscription"):
            cmd += ["--subscription", self.subscription]
        proc = subprocess.run(cmd, env=env, capture_output=True, timeout=300)
        require(proc.returncode == 0, "Azure CLI call failed: " + " ".join(arguments[:3]))
        return json.loads(proc.stdout) if proc.stdout.strip() else None

    def refresh_token(self):
        self.env["AZURE_DI_TOKEN"] = self.az("account", "get-access-token", "--resource",
                                              "https://cognitiveservices.azure.com/")["accessToken"]

    def action(self, name, options=None, bearer=False, bytes_field=None, allow_error=False):
        proc = subprocess.run([self.args.qore, "-penable-debug", str(Path(__file__).with_name("LiveAction.q"))],
                              input=json.dumps({"action": name, "options": options or {},
                                                "bearer": bearer, "bytes_field": bytes_field}),
                              env=self.env, capture_output=True, text=True, timeout=180)
        lines = [line[17:] for line in proc.stdout.splitlines() if line.startswith("QORE-LIVE-RESULT ")]
        require(len(lines) == 1, "Qore subprocess did not return its private protocol: " + name)
        value = json.loads(lines[0])
        if "error" in value and not allow_error:
            # Error classes are identifiers; service message/argument can contain credentials.
            code = value["error"]
            require(isinstance(code, str) and re.fullmatch(r"[A-Z0-9_-]+", code), "unexpected error class")
            raise VerificationError(name + " failed: " + code)
        require(proc.returncode == 0 or "error" in value, "Qore exited unsuccessfully: " + name)
        return value if "error" in value else value.get("result")

    def accepted_id(self, response, component):
        require(response["status_code"] == 202, "operation did not return 202")
        location = urllib.parse.urlsplit(response["headers"]["operation-location"])
        endpoint = urllib.parse.urlsplit(self.env["AZURE_DI_ENDPOINT"])
        require(location.scheme == endpoint.scheme and location.netloc == endpoint.netloc,
                "operation location has unexpected origin")
        require(component in location.path, "operation location has unexpected path")
        return location.path.rsplit("/", 1)[-1]

    def poll(self, action, options, seconds=1200, bearer=False):
        deadline = time.monotonic() + seconds
        last_report = 0
        while time.monotonic() < deadline:
            result = self.action(action, options, bearer=bearer)
            status = result["status"]
            if status == "succeeded":
                return result
            if status not in ("notStarted", "running"):
                codes = []
                error = result.get("error", {})
                while isinstance(error, dict):
                    code = error.get("code", "")
                    if re.fullmatch(r"[A-Za-z0-9_-]+", code):
                        codes.append(code)
                    error = error.get("innererror")
                raise VerificationError(action + " terminal state " + status + ": " + "/".join(codes))
            if time.monotonic() - last_report > 45:
                report(action + ": waiting for Azure")
                last_report = time.monotonic()
            time.sleep(5)
        raise VerificationError(action + " exceeded the live test deadline")

    @staticmethod
    def http_status(result):
        return (result.get("argument") or {}).get("status_code") if "error" in result else result.get("status_code")

    def operation(self, name, options, owned=None):
        if owned:
            require(self.http_status(self.action(owned[2], owned[1], allow_error=True)) == 404,
                    "refusing to replace an existing model or classifier")
        operation_id = self.accepted_id(self.action(name, options), "/operations/")
        if owned:
            self.cleanup.append(owned)
        self.pending[operation_id] = name
        result = self.poll("get-operation", {"operation_id": operation_id})
        del self.pending[operation_id]
        require(result.get("result"), name + " omitted its typed operation result")
        report(name + ": succeeded with typed operation result")
        return result["result"]

    def analyze(self, document, model="prebuilt-layout", output=None, bearer=False):
        options = {"model_id": model, "body": base64.b64encode(document).decode(), "pages": "1"}
        if output:
            options["output"] = output
        started = self.action("start-analysis-stream", options, bearer=bearer, bytes_field="body")
        params = {"model_id": model, "result_id": self.accepted_id(started, "/analyzeResults/")}
        self.cleanup.append(("delete-analysis-result", params, "get-analysis-result"))
        return self.poll("get-analysis-result", params, 180, bearer=bearer)["analyzeResult"], params

    def verify_entra(self):
        claims = json.loads(base64.urlsafe_b64decode(self.env["AZURE_DI_TOKEN"].split(".")[1] + "=="))
        if self.args.temporary_role:
            roles = self.az("role", "assignment", "list", "--assignee", claims["oid"],
                            "--scope", self.account["id"], "--include-inherited")
            if not any(r["roleDefinitionName"] == "Cognitive Services User" for r in roles):
                assignment = self.az("role", "assignment", "create", "--assignee-object-id", claims["oid"],
                                     "--assignee-principal-type", "ServicePrincipal" if claims.get("idtyp") == "app" else "User",
                                     "--role", "Cognitive Services User", "--scope", self.account["id"],
                                     "--name", str(uuid.uuid4()))
                self.role_id = assignment["id"]
                report("temporary Cognitive Services User assignment created at the single resource scope")
        for attempt in range(24):
            result = self.action("ping", bearer=True, allow_error=True)
            if result.get("ok"):
                break
            if not self.args.temporary_role:
                raise VerificationError("Entra authentication rejected; no IAM mutation authorized")
            if attempt % 3 == 0:
                report("Entra role propagation: waiting for Azure")
            time.sleep(10)
            self.refresh_token()
        require(result.get("ok"), "Entra authentication still rejected after propagation deadline")
        report("Entra: both connection constructors, sync/async clients and copies authenticated")
        real_token = self.env["AZURE_DI_TOKEN"]
        try:
            self.env["AZURE_DI_TOKEN"] = "invalid-qore-5421-bearer-token"
            require("error" in self.action("ping", bearer=True, allow_error=True), "invalid bearer token accepted")
            require(self.http_status(self.action("get-resource-info", bearer=True, allow_error=True)) == 401,
                    "invalid bearer token did not receive HTTP 401")
        finally:
            self.env["AZURE_DI_TOKEN"] = real_token
        result, _ = self.analyze(pdf_document("invoice"), model="prebuilt-read", bearer=True)
        require("12.34" in result["content"], "Entra analysis content mismatch")
        report("Entra: invalid bearer rejected; authenticated one-page analysis verified")

    def verify_figure(self):
        result, params = self.analyze(pdf_document("figure"), output=["figures"])
        require(result.get("figures"), "synthetic chart did not produce a live figure")
        figure = self.action("get-result-figure", params | {"figureId": result["figures"][0]["id"]})
        data = base64.b64decode(figure["binary_body"])
        require(figure["headers"]["content-type"].startswith("image/png") and data[:8] == b"\x89PNG\r\n\x1a\n",
                "figure media or PNG signature mismatch")
        width, height = struct.unpack(">II", data[16:24])
        require(width > 100 and height > 100, "figure dimensions are unexpectedly small")
        report(f"PNG: live figure verified, {len(data)} bytes, {width}x{height}")

    def storage_setup(self):
        require(self.args.provision_storage, "storage-dependent tests require explicit --provision-storage")
        require(self.az("storage", "account", "check-name", "--name", self.storage)["nameAvailable"],
                "temporary storage name unavailable")
        self.storage_attempted = True
        self.az("storage", "account", "create", "-n", self.storage, "-g", self.args.resource_group,
                "-l", self.args.location, "--sku", "Standard_LRS", "--kind", "StorageV2",
                "--https-only", "true", "--min-tls-version", "TLS1_2", "--allow-blob-public-access", "false",
                "--tags", "qore5421-run=" + self.run_id)
        report("temporary private storage account created: " + self.storage)
        key = self.az("storage", "account", "keys", "list", "-n", self.storage,
                      "-g", self.args.resource_group)[0]["value"]
        env = dict(os.environ, AZURE_STORAGE_ACCOUNT=self.storage, AZURE_STORAGE_KEY=key)
        self.urls = {}
        expiry = (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(hours=2)).strftime("%Y-%m-%dT%H:%MZ")
        for container in ("input", "output"):
            self.az("storage", "container", "create", "--name", container, env=env)
            sas = self.az("storage", "container", "generate-sas", "--name", container,
                          "--permissions", "racwl", "--expiry", expiry, "--https-only", env=env)
            self.urls[container] = f"https://{self.storage}.blob.core.windows.net/{container}?{sas}"

    def blob(self, container, name, data=None):
        url = self.urls[container]
        root, sas = url.split("?", 1)
        req = urllib.request.Request(root + "/" + urllib.parse.quote(name, safe="/") + "?" + sas,
                                     data=data, method="PUT" if data is not None else "GET",
                                     headers={"x-ms-blob-type": "BlockBlob"} if data is not None else {})
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            raise VerificationError(f"synthetic blob request failed: HTTP {error.code}") from None

    def verify_training(self):
        for kind in ("invoice", "receipt"):
            for number in range(5):
                document = pdf_document(kind, number)
                name = f"{kind}/{number}.pdf"
                self.blob("input", name, document)
                analysis, _ = self.analyze(document)
                self.blob("input", name + ".ocr.json", json.dumps({"status": "succeeded", "analyzeResult": analysis}).encode())
                if kind == "invoice":
                    page = analysis["pages"][0]
                    word = next(w for w in page["words"] if w["content"] == f"{12 + number}.34")
                    box = [coord / (page["width"] if i % 2 == 0 else page["height"])
                           for i, coord in enumerate(word["polygon"])]
                    labels = {"document": f"{number}.pdf", "labels": [{"label": "Total", "value": [
                        {"page": 1, "text": word["content"], "boundingBoxes": [box]}]}]}
                    self.blob("input", name + ".labels.json", json.dumps(labels).encode())
        self.blob("input", "invoice/fields.json", json.dumps({
            "$schema": "https://schema.cognitiveservices.azure.com/formrecognizer/2021-03-01/fields.json",
            "fields": [{"fieldKey": "Total", "fieldType": "string", "fieldFormat": "not-specified"}]}).encode())
        model = self.prefix + "-template"
        classifier = self.prefix + "-classifier"
        self.operation("build-model", {"modelId": model, "buildMode": "template",
                                      "azureBlobSource": {"containerUrl": self.urls["input"], "prefix": "invoice/"}},
                       ("delete-model", {"model_id": model}, "get-model"))
        require(self.action("get-model", {"model_id": model})["modelId"] == model, "built model identity mismatch")
        result, _ = self.analyze(pdf_document("invoice"), model=model)
        require(result["documents"][0]["fields"]["Total"]["content"] == "12.34", "trained extraction field mismatch")
        report("template training: five labeled pages; learned Total field verified")
        self.operation("build-classifier", {"classifierId": classifier, "docTypes": {
            kind: {"azureBlobSource": {"containerUrl": self.urls["input"], "prefix": kind + "/"}}
            for kind in ("invoice", "receipt")}},
                       ("delete-classifier", {"classifier_id": classifier}, "get-classifier"))
        require(self.action("get-classifier", {"classifier_id": classifier})["classifierId"] == classifier,
                "built classifier identity mismatch")
        started = self.action("start-classification-stream", {"classifier_id": classifier,
                              "body": base64.b64encode(pdf_document("receipt")).decode()}, bytes_field="body")
        params = {"classifier_id": classifier, "result_id": self.accepted_id(started, "/analyzeResults/")}
        # The official classifier surface has no delete-classification-result operation.
        classified = self.poll("get-classification-result", params, 180)["analyzeResult"]
        require(classified["documents"][0]["docType"] == "receipt", "trained classifier predicted the wrong class")
        report("classifier training: two classes, five pages each; receipt classification verified")
        for kind, source in (("model", model), ("classifier", classifier)):
            self.verify_copy(kind, source)

    def verify_copy(self, kind, source):
        destination = self.prefix + "-" + kind + "-copy"
        key = kind + "_id"
        camel_key = kind + "Id"
        require(self.http_status(self.action("get-" + kind, {key: destination}, allow_error=True)) == 404,
                "copy destination already exists")
        authorization = self.action("authorize-" + kind + "-copy", {camel_key: destination})
        # Authorization reserves the destination in Azure. It is now owned
        # by this invocation, even if the subsequent copy request fails.
        self.cleanup.append(("delete-" + kind, {key: destination}, "get-" + kind))
        report(kind + " copy authorization: task-owned destination reserved")
        self.operation("copy-" + kind, {key: source} | authorization)
        require(self.action("get-" + kind, {key: destination})[camel_key] == destination, "copy identity mismatch")
        report(kind + " copy: authorization, completed copy and destination read verified")

    def verify_batch(self):
        self.blob("input", "batch/only.pdf", pdf_document("invoice"))
        started = self.action("start-batch-analysis", {"model_id": "prebuilt-read",
                              "azureBlobSource": {"containerUrl": self.urls["input"], "prefix": "batch/"},
                              "resultContainerUrl": self.urls["output"], "resultPrefix": "results/"})
        params = {"model_id": "prebuilt-read", "result_id": self.accepted_id(started, "/analyzeBatchResults/")}
        self.cleanup.append(("delete-batch-result", params, "get-batch-result"))
        batch = self.poll("get-batch-result", params)["result"]
        require(batch["succeededCount"] == 1 and batch["failedCount"] == 0 and batch["skippedCount"] == 0,
                "one-document batch counts mismatch")
        root, sas = self.urls["output"].split("?", 1)
        try:
            with urllib.request.urlopen(root + "?restype=container&comp=list&" + sas, timeout=60) as response:
                names = [e.text for e in ET.fromstring(response.read()).findall("./Blobs/Blob/Name")]
        except urllib.error.HTTPError as error:
            raise VerificationError(f"batch output listing failed: HTTP {error.code}") from None
        require(len(names) == 1, "batch did not produce exactly one output blob")
        stored = json.loads(self.blob("output", names[0]))
        require("12.34" in stored["analyzeResult"]["content"], "persisted batch output content mismatch")
        listed = self.action("list-batch-results", {"model_id": "prebuilt-read"})
        require(any(v["resultId"] == params["result_id"] for v in listed["value"]), "batch missing from listing")
        report("batch: one document succeeded; counts, persisted extraction and listing verified")

    def close(self):
        failures = []
        # A timed-out training operation can still create a model later. Do not
        # mistake its current 404 for completed cleanup or remove its input data.
        for operation_id, name in list(self.pending.items()):
            try:
                operation = self.action("get-operation", {"operation_id": operation_id})
                require(operation["status"] in ("succeeded", "failed", "canceled"),
                        "Azure operation still active")
                del self.pending[operation_id]
            except Exception:
                failures.append(name + " operation " + operation_id)
        active = bool(self.pending)
        for delete_action, options, get_action in reversed(self.cleanup):
            if active and delete_action in ("delete-model", "delete-classifier"):
                failures.append(delete_action + " " + next(iter(options.values())))
                continue
            try:
                # All entries were obtained from successful task submissions or
                # copy authorizations. Reserved destinations return ModelNotReady
                # on GET but still support DELETE, so do not require a prior GET.
                reply = self.action(delete_action, options, allow_error=True)
                if self.http_status(reply) == 404:
                    require(self.http_status(self.action(get_action, options, allow_error=True)) == 404,
                            delete_action + " absence verification failed")
                    report(delete_action + ": object absent after failed build")
                    continue
                require(reply.get("status_code") == 204, delete_action + " did not return 204")
                require(self.http_status(self.action(get_action, options, allow_error=True)) == 404,
                        delete_action + " retrieval did not return 404")
                report(delete_action + ": 204 and subsequent retrieval returned 404")
            except Exception:
                failures.append(delete_action + " " + next(iter(options.values())))
        if self.storage_attempted and active:
            failures.append("storage retained for active operation: " + self.storage)
        elif self.storage_attempted:
            try:
                account = self.az("storage", "account", "show", "-n", self.storage, "-g", self.args.resource_group)
                require(account.get("tags", {}).get("qore5421-run") == self.run_id, "storage ownership tag mismatch")
                self.az("storage", "account", "delete", "--ids", account["id"], "--yes")
                remaining = self.az("storage", "account", "list", "-g", self.args.resource_group)
                require(not any(a["id"] == account["id"] for a in remaining), "temporary storage remains")
                report("temporary storage account deletion verified")
            except Exception:
                failures.append("storage " + self.storage)
        if self.role_id:
            try:
                self.az("role", "assignment", "delete", "--ids", self.role_id)
                roles = self.az("role", "assignment", "list", "--scope", self.account["id"])
                require(not any(r["id"] == self.role_id for r in roles), "temporary role remains")
                report("temporary scoped role assignment removal verified")
            except Exception:
                failures.append("role " + self.role_id)
        require(not failures, "cleanup needs attention: " + "; ".join(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--account", required=True)
    parser.add_argument("--resource-group", required=True)
    parser.add_argument("--location", default="swedencentral")
    parser.add_argument("--qore", default="build/qore")
    parser.add_argument("--provision-storage", action="store_true")
    parser.add_argument("--temporary-role", action="store_true")
    parser.add_argument("--only", choices=("all", "entra", "figure", "storage", "training"), default="all")
    args = parser.parse_args()
    if args.only in ("all", "storage", "training") and not args.provision_storage:
        parser.error("storage/training tests require explicit --provision-storage authorization")
    run = LiveRun(args)
    try:
        require(run.action("ping")["ok"], "subscription-key ping failed")
        if args.only in ("all", "entra"):
            run.verify_entra()
        if args.only in ("all", "figure"):
            run.verify_figure()
        if args.only in ("all", "storage", "training"):
            run.storage_setup()
            if args.only != "training":
                run.verify_batch()
            run.verify_training()
    finally:
        run.close()
    report("requested lifecycle verification completed successfully")


if __name__ == "__main__":
    def interrupted(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupted)
    try:
        main()
    except KeyboardInterrupt:
        report("interrupted; cleanup attempted for all registered task-created objects")
        sys.exit(130)
    except VerificationError as error:
        report(str(error))
        sys.exit(1)
    except Exception as error:
        # Never expose arbitrary exception text containing private URLs or child output.
        report("unexpected runner failure: " + type(error).__name__)
        sys.exit(1)
