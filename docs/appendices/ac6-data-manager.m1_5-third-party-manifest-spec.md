# AC6 Data Manager M1.5 Third-Party Manifest Spec

## 1. 文件位置
- 模板：`packaging/third_party/manifest.template.json`
- release：`third_party/third_party_manifest.json`

## 2. 顶层字段
- `schema_version`
- `name`
- `source_baseline`
- `version_label`
- `entrypoint`
- `required`
- `preflight_policy`
- `generated_at`
- `files[]`

## 3. files[] 字段
- `path`
- `sha256`
- `required`
- `file_role`
- `size_bytes`

## 4. 校验规则
- required 文件缺失 -> fail-closed
- extra file -> fail-closed
- sha256 mismatch -> fail-closed
- version mismatch -> fail-closed
- entrypoint 不存在 -> fail-closed
