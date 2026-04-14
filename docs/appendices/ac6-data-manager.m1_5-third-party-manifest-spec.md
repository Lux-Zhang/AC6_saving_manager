# AC6 Data Manager M1.5 Third-Party Manifest Spec

## 1. 目标
定义 `third_party/WitchyBND/**` 的目录级 manifest gate 结构与校验规则。

## 2. 文件位置
- release 包内：`third_party/third_party_manifest.json`
- 仓库模板：`packaging/third_party/manifest.template.json`

## 3. 顶层字段
```json
{
  "schema_version": 1,
  "name": "WitchyBND",
  "source_baseline": "ACVIEmblemCreator bundled version",
  "version_label": "string",
  "entrypoint": "third_party/WitchyBND/WitchyBND.exe",
  "required": true,
  "preflight_policy": "fail_closed",
  "generated_at": "ISO8601",
  "files": []
}
```

## 4. files[] 字段
```json
{
  "path": "third_party/WitchyBND/WitchyBND.exe",
  "sha256": "hex",
  "required": true,
  "file_role": "entrypoint|dll|asset|config|other",
  "size_bytes": 0
}
```

## 5. 校验规则
1. entrypoint 必须存在。
2. `required=true` 的文件必须全部存在。
3. `sha256` 必须完全一致。
4. `version_label` 必须与 reference baseline 对齐。
5. 不允许运行时解析到 manifest 之外的 PATH 工具。
6. extra file 默认视为失败，除非 schema 未来引入 allowlist 机制。

## 6. 失败输出
manifest gate 失败时，至少输出：
- failure reason
- offending path
- expected hash / actual hash（如适用）
- baseline source
- fail-closed verdict
