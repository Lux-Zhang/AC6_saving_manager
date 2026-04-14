from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.gui import FixtureGuiDataProvider, ServiceBundleGuiDataProvider
from ac6_data_manager.release import build_provider_bundle, emit_provider_proof


class ReleaseRuntimeTests(unittest.TestCase):
    def test_default_runtime_uses_release_service_bundle(self) -> None:
        bundle = build_provider_bundle(argv=())

        self.assertIsInstance(bundle.provider, ServiceBundleGuiDataProvider)
        self.assertEqual(bundle.context.provider_kind, "service_bundle")
        self.assertFalse(bundle.context.is_demo)
        catalog = bundle.provider.get_catalog()
        self.assertGreaterEqual(len(catalog), 2)
        plan = bundle.provider.get_import_plan(("share:raven-02",))
        self.assertEqual(plan.target_slot, "USER_EMBLEM_00")
        self.assertEqual(plan.blockers, ())

    def test_demo_mode_requires_explicit_flag(self) -> None:
        bundle = build_provider_bundle(argv=("--demo",))

        self.assertIsInstance(bundle.provider, FixtureGuiDataProvider)
        self.assertTrue(bundle.context.is_demo)
        self.assertEqual(bundle.context.runtime_mode, "demo")

    def test_development_env_allows_fixture_provider(self) -> None:
        bundle = build_provider_bundle(argv=(), environ={"AC6DM_DEV_MODE": "1"})

        self.assertIsInstance(bundle.provider, FixtureGuiDataProvider)
        self.assertEqual(bundle.context.runtime_mode, "development")

    def test_emit_provider_proof_reports_non_demo_release_default(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            target = Path(tempdir) / "provider-proof.json"
            payload = emit_provider_proof(target, argv=())
            written = json.loads(target.read_text(encoding="utf-8"))

        self.assertEqual(payload["provider_kind"], "service_bundle")
        self.assertFalse(payload["is_demo"])
        self.assertEqual(written["provider_kind"], "service_bundle")

    def test_default_runtime_ignores_path_and_remains_bundled_only(self) -> None:
        bundle = build_provider_bundle(
            argv=(),
            environ={"PATH": str(Path("/tmp/fake-witchybnd"))},
        )

        self.assertIsInstance(bundle.provider, ServiceBundleGuiDataProvider)
        self.assertEqual(bundle.context.provider_kind, "service_bundle")
        self.assertEqual(bundle.context.runtime_mode, "release")
        self.assertEqual(bundle.context.toolchain_policy, "bundled-only")
        self.assertFalse(bundle.context.is_demo)
