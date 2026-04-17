# AC6 Data Manager Native C++ Acceptance Baseline

- 文档状态：Approved Planning Baseline
- 版本：v1.0-native-cpp-baseline
- 最后更新：2026-04-14
- 关联 PRD：`../ac6-data-manager.native-cpp.prd.md`
- 关联技术方案：`../ac6-data-manager.native-cpp.technical-design.md`

## 1. 文档目的

本文档定义 Native C++ 主线在 N0 / N1 / N1.5 阶段的验收口径、测试范围、阻塞条件与证据要求。

当前基线只覆盖：
- Emblem stable lane
- 普通用户 GUI 主流程
- Windows x64 portable 发布

当前明确不验收：
- rollback
- 历史与诊断页面
- AC mutation

## 2. 环境基线

### 验收环境
- 机器：`ZPX-GE77`
- 系统：Windows 10 25H2 x64
- 杀软：Huorong 开启
- 游戏版本：Steam 1.9.0.0
- 存档来源：`E:\codex_conversation\AC6\测试存档`

### 输入物
- 真实 `.sl2` 存档
- bundled `third_party/WitchyBND/`
- Native C++ Windows x64 portable 目录或 zip

## 3. N0 验收基线：Native Foundation

### 3.1 范围
- 工程可编译
- 应用可启动
- 可以选择真实 `.sl2`
- 通过 sidecar 在 staging workspace 中完成打开存档链路
- sidecar qualification suite
- manifest gate / no-PATH-fallback

### 3.2 必测项
1. 首次启动成功
2. 选择真实 `.sl2` 成功
3. 中文路径成功
4. OneDrive 路径成功
5. staging workspace 正常生成
6. staging 根目录位于本地、短路径、ASCII-safe 的应用私有目录
7. 原始存档目录零副产物
8. manifest gate 通过
9. sidecar 失败时有 return code / stdout / stderr 摘要

### 3.3 通过条件
- `打开存档` 成功率 100%
- 源目录零副产物 100%
- sidecar 调用失败时 fail-closed
- UI 能进入 real-save 徽章浏览态
- manifest gate 通过率 100%

### 3.4 阻塞条件
- 任何一次打开存档导致源目录出现 unpack/repack 副产物
- 任意真实 `.sl2` 无法稳定打开
- 中文路径或 OneDrive 路径任一失败
- manifest gate 失败或发生 PATH fallback

### 3.5 证据产物
- 启动截图
- 打开存档成功截图
- staging workspace 目录截图或清单
- sidecar qualification suite 报告（建议固定为 `artifacts/acceptance/n0/sidecar-qualification-report.json`）
- manifest gate 验证结果（建议固定为 `artifacts/acceptance/n0/manifest-gate-result.json`）
- 源目录前后对比清单
- 错误日志（若失败）

## 4. N1 验收基线：User-first Emblem MVP

### 4.1 范围
- 首页
- 徽章库页
- 导入向导
- 保存结果页
- real-save emblem browse / preview / share->user import / save-as-new

### 4.2 GUI E2E 主流程
1. 启动程序
2. 点击 `打开存档`
3. 选择真实 `.sl2`
4. 浏览 user/share 徽章
5. 选中一个分享徽章
6. 点击 `导入所选徽章`
7. 在导入向导中确认输出路径
8. 点击 `开始导入`
9. 看到“保存为新存档成功”
10. 在游戏中打开新存档验证

当前 MVP 约束：
- 只支持单个分享徽章
- 只支持空闲 user 槽位
- 不支持批量导入
- 不支持替换已占用槽位

### 4.3 通过条件
- UI 不要求普通用户理解 `dry-run / apply / rollback`
- 分享徽章可导入到使用者槽位
- 新存档成功写出到 fresh destination
- 游戏内可见 / 可编辑 / 可保存
- 原始存档保持不变
- 普通用户不需要理解 rollback，也不依赖历史/诊断页面

### 4.4 阻塞条件
- 任何原地覆盖原始存档的行为
- 写后无法重新解析
- 游戏内不可见或不可编辑
- 任何不可恢复损档

### 4.5 故障注入
- 输出路径已存在
- 存档被占用
- WitchyBND 调用失败
- 预览缺失
- user 槽位已满

### 4.6 故障通过条件
- 全部 fail-closed
- 不输出不可信的新存档
- 错误提示为普通用户可理解文案
- 至少提供一种最小支持出口：复制错误详情 / 打开支持包目录 / 导出故障包

### 4.7 最小支持出口约束
- 触发条件：仅在“打开存档失败”或“保存为新存档失败”时显示
- 成功路径不得主动展示“支持包”“诊断”“审计”主入口
- UI 文案约束：
  - `无法打开该存档，请确认文件未被占用。`
  - `保存失败，未生成新存档。`
  - `复制错误详情`
  - `打开支持包目录`

## 5. N1.5 验收基线：Native Packaging & Live Acceptance

### 5.1 范围
- Windows x64 portable 交付
- 首启与二次启动
- Huorong 开启环境
- 游戏内复核

### 5.2 必测项
1. unzip 成功
2. 首启成功
3. 打开真实存档成功
4. 导入并另存为成功
5. 二次启动成功
6. 游戏内验证成功

### 5.3 包体目标
- 由于 WitchyBND 保持 bundled，不设不现实目标
- N1.5 参考目标：portable zip 尽量控制在 `120MB-150MB`
- 若超标，必须附带体积分解说明：
  - 主程序
  - Qt 依赖
  - WitchyBND sidecar
  - 其他资源

### 5.4 通过条件
- portable 可直接解压运行
- sidecar 可用
- 打开真实存档成功
- 导入并另存为成功
- 游戏内通过人工复核

### 5.5 阻塞条件
- Huorong 阻断且无法完成必要操作
- 首启失败
- sidecar 缺失或不可用
- 打开真实存档失败

## 6. 普通用户可用性判定

本项目在当前阶段增加一条额外硬性判定：

### 通过条件
以下描述必须对普通用户成立：
- 我知道怎么打开存档
- 我知道怎么选一个分享徽章
- 我知道怎么开始导入
- 我知道结果会保存成一个新存档

### 阻塞条件
以下任一成立即判定为产品不可验收：
- 主流程依赖开发者术语
- 需要理解 `dry-run / apply / rollback`
- 需要进入诊断页面才能完成正常导入
- 需要理解“恢复上一次更改”之类当前并不存在的用户功能

## 7. 证据清单

N0 / N1 / N1.5 每轮至少保留：
1. 首启截图
2. 打开存档成功截图
3. 徽章库页面截图
4. 导入向导截图
5. 保存成功截图
6. 源目录前后对比清单
7. 新存档输出路径记录
8. 游戏内结果截图或复核记录
9. 包体清单与 SHA256

## 8. 推荐验证命令

### 构建
```text
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
```

### UI / 集成回归
```text
ctest --test-dir build/native --output-on-failure -C Release
```

## 9. Ralph 执行建议

后续统一使用 Ralph + subagent：
- Ralph 主 agent：统一规划、阶段推进、最终收尾
- Subagent-1：构建与打包
- Subagent-2：tool adapter / workspace
- Subagent-3：emblem core
- Subagent-4：用户化 GUI
- Subagent-5：verification / evidence / acceptance harness

执行约束：
- subagent 之间禁止共享写集
- 主 agent 先定公共接口后再分派

建议按以下 scope 执行：
- Subagent-1：只负责 `native/` 工程骨架、CMake/Qt6/MSVC、Release/portable 打包；不碰业务逻辑。
- Subagent-2：只负责 WitchyBND adapter、manifest gate、no-PATH-fallback、ASCII-safe staging、源目录零副产物；不碰 GUI。
- Subagent-3：只负责 `USER_DATA007` emblem parse/serialize、catalog、preview、single-share -> empty-user-slot import；不碰外部进程。
- Subagent-4：只负责普通用户 GUI 主流程与文案：首页、打开存档、徽章库、导入向导、保存结果页；不碰 core 编解码。
- Subagent-5：只负责测试、qualification suite、golden corpus、evidence manifest、live acceptance checklist；不主导生产代码。
- Ralph 主 agent：只负责接口冻结、集成收口、最终 verdict；不长期接管任一 subagent 的 lane 实现。

推荐执行命令：

```text
$ralph "按 docs/ac6-data-manager.native-cpp.prd.md、docs/ac6-data-manager.native-cpp.technical-design.md 与 docs/appendices/ac6-data-manager.native-cpp.acceptance-baseline.md 执行 N0、N1。执行方式：Ralph 先冻结接口和写集边界，再启用 5 个 subagent。Subagent-1 负责构建与打包；Subagent-2 负责 WitchyBND adapter、manifest gate、ASCII-safe staging 与源目录零副产物；Subagent-3 负责 emblem core；Subagent-4 负责普通用户 GUI；Subagent-5 负责 verification、qualification suite 与 evidence。待 subagent 完成后由 Ralph 主 agent 统一集成、回归、更新文档并给出 verdict。验收：打开真实存档稳定、源目录零副产物、普通用户 GUI 主流程闭环、Windows x64 构建通过。"
```
