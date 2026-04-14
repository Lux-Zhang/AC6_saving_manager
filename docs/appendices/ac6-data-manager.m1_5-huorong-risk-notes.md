# AC6 Data Manager M1.5 Huorong Risk Notes

## 1. 目标

为 `ZPX-GE77 + Huorong enabled` live acceptance 提供统一的风险分类、操作建议与 verdict 影响说明。本文只定义记录标准，不伪造真实结论。

## 2. 记录等级

| 等级 | 定义 | 对 verdict 的影响 |
|---|---|---|
| `pass` | Huorong 未阻断关键流程，且无需人工放行 | 不单独改变 verdict |
| `manual-allow` | Huorong 要求人工允许；允许后流程稳定通过 | 证据完整时会把最终 verdict 压到 `conditional-pass` |
| `blocked` | Huorong 阻断关键流程或隔离核心文件，流程无法继续 | 直接触发 `fail` |

## 3. 风险对象

- `AC6 saving manager.exe`
- `third_party/WitchyBND/WitchyBND.exe`
- portable zip / unzip 目录
- `artifacts/preflight/` 下的 proof / report 输出
- `artifacts/live_acceptance/` 下的 publish / readback / rollback / incident 证据

## 4. Huorong 风险矩阵

| 阶段 | 触达对象 | 典型 Huorong 反应 | 操作员动作 | `pass` 判定 | `manual-allow` 判定 | `blocked` 判定 |
|---|---|---|---|---|---|---|
| unzip | portable zip、解压目录 | 扫描压缩包或新目录 | 记录是否拦截、隔离、删改文件 | 解压后目录完整可继续 | 允许后目录保持完整，且只需一次允许 | 压缩包/目录被隔离、删除、损坏，无法继续 |
| first launch | `AC6 saving manager.exe` | 首次执行拦截、未知程序提示 | 保存弹窗截图与允许范围 | 无拦截，程序可正常启动 | 允许后程序可启动，后续重复启动不再异常 | 程序无法启动或被持续拦截 |
| preflight | `run_preflight_check.py`、proof/report 输出 | 对 exe 或输出目录拦截 | 记录是否阻断 proof/report 生成 | proof / report 正常生成 | 放行后 proof / report 正常生成 | proof / report 生成失败，或 exe 被阻断 |
| sidecar | `third_party/WitchyBND/WitchyBND.exe`、manifest | 对 sidecar 可执行文件拦截 | 只允许 release root 内 sidecar；不得改用 PATH | sidecar gate 通过 | 一次人工允许后 sidecar gate 稳定通过 | sidecar 被隔离或只能依赖 PATH 才能继续 |
| dry-run | release exe、shadow workspace | 对临时目录或进程扫描 | 记录是否影响 dry-run 完成 | dry-run 正常完成 | 允许后 dry-run 完成，结果一致 | dry-run 被中断或产生不完整输出 |
| apply | publish 输出目录、fresh destination | 对写出结果或证据目录拦截 | 保存目标目录与弹窗关系 | publish-audit/readback 均生成 | 放行后 apply 与证据生成成功 | apply 被阻断，或证据不完整 |
| rollback | rollback 输出目录 | 对 rollback 复制结果或证据拦截 | 保存 rollback 输出路径 | rollback 结果与证据正常生成 | 放行后 rollback 成功 | rollback 被阻断或输出不可信 |
| second launch | 二次启动 exe、游戏验证前置 | 复扫或再次弹窗 | 记录是否重复弹窗 | 二次启动无异常 | 二次允许后可继续游戏内复核 | 二次启动失败，无法进入游戏复核 |

## 5. 记录规范

建议至少补以下人工证据：

- `artifacts/huorong/huorong-matrix.json` 或 `.md`
- `artifacts/huorong/screenshots/`
- `artifacts/huorong/notes.md`

每条记录至少包含：

- 阶段
- 时间
- 触达对象（进程 / 路径 / 文件）
- Huorong 结果等级
- 是否需要人工允许
- 放行后的稳定性（是否再次弹窗）
- 对 release verdict 的影响

## 6. 当前状态

本会话仅完成仓内实现与自动化验证，未在 `ZPX-GE77 + Huorong enabled` 上执行真实记录，因此当前发布判定保持 `release-pending`。
