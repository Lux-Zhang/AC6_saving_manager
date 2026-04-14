# AC6 Data Manager M1.5 Release Runbook

## 1. 目的
指导 Windows x64 one-folder portable release 的 build、preflight、zip 与 live acceptance 证据采集。

## 2. 关键命令
```bash
python scripts/build_windows_portable.py --third-party-source <WitchyBND baseline dir> --version-label <label> --skip-pyinstaller
python scripts/verify_third_party_manifest.py --release-root artifacts/release/AC6\ saving\ manager --expected-version-label <label>
python scripts/run_preflight_check.py --release-root artifacts/release/AC6\ saving\ manager --expected-version-label <label> --json-out artifacts/preflight/preflight-report.json --text-out artifacts/preflight/preflight-report.txt --provider-proof-out artifacts/preflight/real-entry-proof.json
python scripts/create_release_zip.py --release-root artifacts/release/AC6\ saving\ manager --output-zip artifacts/release/AC6-saving-manager-portable.zip
```

## 3. 证据要求
- `artifacts/release/content-manifest.json`
- `artifacts/release/evidence-manifest.json`
- `artifacts/preflight/third-party-report.json`
- `artifacts/preflight/preflight-report.json`
- `artifacts/preflight/real-entry-proof.json`
- `artifacts/live_acceptance/` 下的 publish/readback/rollback 证据
- `artifacts/huorong/` 下的结果矩阵

## 4. 当前结论
本仓库已达到 live-acceptance-ready，但尚未在 ZPX-GE77 上落下真机证据，因此不能宣布 `live-pass`。
