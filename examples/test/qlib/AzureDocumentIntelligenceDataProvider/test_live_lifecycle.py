#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
"""Offline safety checks for the opt-in live runner; never contacts Azure."""
import contextlib
import io
import unittest
from unittest.mock import Mock

from live_lifecycle import LiveRun, VerificationError


class CleanupSafetyTest(unittest.TestCase):
    def runner(self):
        run = object.__new__(LiveRun)
        run.cleanup = []
        run.pending = {}
        run.storage_attempted = False
        run.role_id = None
        return run

    def test_existing_model_prevents_mutation(self):
        run = self.runner()
        run.action = Mock(return_value={"modelId": "existing"})
        with self.assertRaises(VerificationError):
            run.operation("build-model", {"modelId": "existing"},
                          ("delete-model", {"model_id": "existing"}, "get-model"))
        self.assertEqual([c.args[0] for c in run.action.call_args_list], ["get-model"])
        self.assertFalse(run.cleanup)

    def test_accepted_job_is_retained_on_poll_failure(self):
        run = self.runner()
        run.action = Mock(side_effect=[{"error": "REST-RESPONSE-ERROR", "argument": {"status_code": 404}},
                                       {"status_code": 202}])
        run.accepted_id = Mock(return_value="operation")
        run.poll = Mock(side_effect=VerificationError("timeout"))
        owned = ("delete-model", {"model_id": "created"}, "get-model")
        with self.assertRaises(VerificationError):
            run.operation("build-model", {}, owned)
        self.assertEqual(run.cleanup, [owned])
        self.assertEqual(run.pending, {"operation": "build-model"})

    def test_auth_failure_is_not_successful_deletion(self):
        run = self.runner()
        run.cleanup = [("delete-model", {"model_id": "created"}, "get-model")]
        run.action = Mock(side_effect=[{"status_code": 204},
                                      {"error": "REST-RESPONSE-ERROR", "argument": {"status_code": 401}}])
        with self.assertRaises(VerificationError):
            run.close()

    def test_completed_delete_requires_404(self):
        run = self.runner()
        run.cleanup = [("delete-model", {"model_id": "created"}, "get-model")]
        run.action = Mock(side_effect=[{"status_code": 204},
                                      {"error": "REST-RESPONSE-ERROR", "argument": {"status_code": 404}}])
        with contextlib.redirect_stdout(io.StringIO()) as output:
            run.close()
        self.assertIn("204 and subsequent retrieval returned 404", output.getvalue())
        self.assertEqual([c.args[0] for c in run.action.call_args_list], ["delete-model", "get-model"])

    def test_active_training_retains_storage_and_reports_cleanup(self):
        run = self.runner()
        run.pending = {"operation": "build-model"}
        run.storage_attempted = True
        run.storage = "task-storage"
        run.cleanup = [("delete-model", {"model_id": "created"}, "get-model")]
        run.action = Mock(return_value={"status": "running"})
        run.az = Mock()
        with self.assertRaisesRegex(VerificationError, "storage retained for active operation"):
            run.close()
        run.az.assert_not_called()
        self.assertEqual([c.args[0] for c in run.action.call_args_list], ["get-operation"])

    def test_copy_reservation_remains_owned_when_copy_fails(self):
        run = self.runner()
        run.prefix = "test-run"
        run.action = Mock(side_effect=[{"error": "REST-RESPONSE-ERROR", "argument": {"status_code": 404}},
                                       {"accessToken": "fixture-authorization"}])
        run.operation = Mock(side_effect=VerificationError("copy submission failed"))
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(VerificationError):
            run.verify_copy("model", "source")
        self.assertEqual(run.cleanup, [("delete-model", {"model_id": "test-run-model-copy"}, "get-model")])
        self.assertEqual([c.args[0] for c in run.action.call_args_list], ["get-model", "authorize-model-copy"])
        run.operation.assert_called_once_with("copy-model", {"model_id": "source", "accessToken": "fixture-authorization"})


if __name__ == "__main__":
    unittest.main()
