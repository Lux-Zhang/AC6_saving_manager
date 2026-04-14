# Golden Corpus Baseline

## 目的
- 固化 M0 / M0.5 / M1 所需的 golden corpus 哈希清单。
- 为 readback、round-trip、rollback rehearsal 提供稳定输入集合。

## 目录约定
- `manifest.example.json`：示例清单结构。
- 真实 corpus 样本不随仓库提交；仅提交结构与校验规则。

## 必填元数据
- `logical_name`
- `relative_path`
- `sha256`
- `byte_size`
- `tags`
- `expectations`

## 使用约束
- 样本采集必须脱敏。
- 样本变更必须刷新 sha256，并记录变更原因。
- 未经验证的 AC mutation 样本不得进入本目录。
