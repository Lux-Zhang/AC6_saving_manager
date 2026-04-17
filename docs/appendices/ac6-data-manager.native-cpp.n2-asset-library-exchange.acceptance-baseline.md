# AC6 Data Manager Native C++ N2 Acceptance Baseline：Asset Library & Exchange

- 文档状态：Active Baseline / N2A Verified / N2B-N2D Qualification Open
- 版本：v2.0-native-cpp-n2-baseline
- 最后更新：2026-04-15（N2B/N2C/N2D qualification lane 收口）
- 关联 PRD：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md`
- 关联技术方案：`../ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md`

## 1. 文档目的

本文档定义 N2 阶段，尤其是当前执行范围 `N2A` 的：
- 功能验收口径
- GUI 覆盖口径
- 阻塞条件
- 证据要求
- 后续 N2B / N2C / N2D 的准入门槛

## 1.1 当前阶段状态

- `N2A`：已有自动化回归与 probe 证据，进入 verified/implemented 状态。
- `N2B`：进入 qualification 中，当前关注 `.ac6emblemdata` 真实导入导出闭环。
- `N2C`：进入 gate 记录阶段，是否允许 AC mutation 由 gate 文档决定。
- `N2D`：仅可在 `N2C` 全部 gate 明确 `PASS` 后进入。

## 2. 环境基线

### 2.1 验收环境
- 机器：`ZPX-GE77`
- 系统：Windows 10 25H2 x64（OS Build `26200.8037`）
- 杀软：Huorong 开启
- 游戏版本：Steam `1.9.0.0`
- 当前默认交付：未压缩 release 目录 + `.exe`

### 2.2 输入物
- 真实 `.sl2` 存档样本：已由用户提供，满足当前 N2 规划所需 A 类资料
- bundled `third_party/WitchyBND/`
- Native C++ release 目录

### 2.3 持续约束
- 源存档只读
- 工作区在程序目录内
- 不得 in-place overwrite
- 所有写后都必须 readback
- 当前 N2A 禁止 AC mutation

## 3. N2A 验收基线：统一资产目录壳 + 徽章稳定产品化 + AC 只读探测

## 3.1 范围
N2A 必测范围：
1. 打开真实 `.sl2`
2. 徽章五类来源分类
3. AC 五类来源分类
4. GUI 双视图：`徽章库 / AC库`
5. 徽章详情与预览
6. AC 详情与 preview provenance
7. `将所选徽章导入使用者栏`
8. 目标使用者栏位选择弹窗
9. 徽章写入后的 readback
10. AC 未开放动作的 disabled + reason 展示

## 3.2 GUI 覆盖硬门槛

N2A 增加一条硬门槛：

### 通过条件
以下所有能力都必须能通过 GUI 直接测试：
- 打开存档
- 切换到徽章库
- 切换到 AC 库
- 按五类来源浏览
- 选择一项徽章并查看详情
- 触发“将所选徽章导入使用者栏”
- 在弹窗中选择目标使用者栏位
- 观察成功或失败结果
- 查看 AC 的 preview provenance 状态

### 阻塞条件
以下任一成立即判定 N2A 不可验收：
- 功能只在探针或命令行里可测，GUI 无入口
- GUI 中仍要求普通用户理解 `dry-run / apply / rollback`
- 目标使用者栏位只能后台自动选定，GUI 无选择弹窗
- AC 只读状态与 disabled reason 在 GUI 不可见

## 3.3 目录分类通过条件

### 徽章
- 至少能把当前打开存档中的徽章项分入：
  - 使用者1
  - 使用者2
  - 使用者3
  - 使用者4
  - 分享

### AC
- 至少能把当前打开存档中的 AC 项分入：
  - 使用者1
  - 使用者2
  - 使用者3
  - 使用者4
  - 分享

### 阻塞条件
- 仍停留在 `user / share` 两类，不细分 4 个使用者栏位
- GUI 显示分类与底层 record-map 不一致

## 3.4 徽章导入通过条件

主流程：
1. 打开真实存档
2. 在徽章库选择一个分享徽章
3. 点击 `将所选徽章导入使用者栏`
4. 弹出目标使用者栏位选择框
5. 用户明确选择 `使用者1/2/3/4` 之一
6. 程序执行写入
7. 程序完成 readback
8. 输出新存档
9. 重新打开输出新存档，目标栏位中能看到结果

### 通过条件
- 来源必须是分享徽章
- 目标栏位由用户显式选择
- 输出为 fresh destination
- 原始存档不变
- readback 成功
- 游戏内或至少二次打开结果可见、可归类

### 阻塞条件
- 不弹出栏位选择框
- 目标栏位选择后实际写入到别的栏位
- 原始存档被改写
- 写后无法重新打开或重新解析

## 3.5 AC 只读探测通过条件

N2A 对 AC 的通过条件不是“能写入”，而是：
- 能读出五类来源
- 能显示详情
- 能显示 preview provenance 状态
- 未开放的 AC 按钮在 GUI 中 visible but disabled，且原因明确

### 阻塞条件
- 在 N2A 提前开放 AC 写入
- AC 视图为空壳，只有占位文本没有真实读出结果
- preview provenance 不可见，用户无法知道当前是 native / derived / unavailable / unknown

## 3.6 预览通过条件

### 徽章
- 若有原生预览：显示图像，并标记 `native_embedded`
- 若通过衍生渲染：显示图像，并标记 `derived_render`
- 若无预览：明确显示 `无预览`，并标记 `unavailable`

### AC
- 若能读取原生预览：显示图像，并标记 `native_embedded`
- 若尚未确认：可暂时不显示图像，但必须显示 `unknown`
- 若确认当前无预览：显示 `unavailable`

### 阻塞条件
- 将 `unknown` 伪装为“已稳定预览”
- GUI 中完全看不到 provenance 状态

## 3.6.1 Disabled Reason 合同基线

N2A 虽不开放真实文件交换，但必须冻结 future action / disabled reason 合同，至少覆盖：
- `导入徽章文件到使用者栏`
- `导出所选徽章`
- `将所选AC导入使用者栏`
- `导入AC文件到使用者栏`
- `导出所选AC`

### 通过条件
- 每个动作都有稳定 `actionId`
- 每个动作都有稳定 `disabledReasonCode`
- GUI 后续接线时可直接显示普通用户可理解的 `disabledReasonText`
- N2A 不得借接口冻结之名实现任何真实文件导入导出

### 阻塞条件
- 只写文档，没有代码合同落点
- disabled reason 只能写死在 GUI 文案里，没有稳定 reason code
- 借接口冻结偷偷实现真实文件读写

## 3.7 故障注入

N2A 至少覆盖以下故障：
1. 存档被占用
2. WitchyBND 调用失败
3. 指定目标使用者栏位当前不允许写入
4. 预览缺失
5. AC 写入功能被误开放

### 故障通过条件
- 全部 fail-closed
- 不输出不可信新存档
- 原始存档不变
- GUI 给出普通用户能理解的错误提示或禁用原因

## 4. N2B 准入门槛：徽章交换格式 v1

只有在 N2A 全部通过后，才允许进入 N2B。N2B 额外需要：
- `.ac6emblemdata` 扩展名固定
- 导出与导入闭环可验证
- GUI 中新增：
  - `导出所选徽章`
  - `导入徽章文件到使用者栏`
- 目标栏位选择弹窗可复用于文件导入

## 5. N2C 准入门槛：AC 资格认证

进入 N2C 的前提：
- N2A 已稳定
- GUI 已有 AC 只读真实视图
- AC 目录与 preview provenance 有初步可用证据

N2C 必须通过三道 Gate：
- `Gate-AC-01`：record-map stable
- `Gate-AC-02`：preview provenance stable
- `Gate-AC-03`：写入指定使用者栏位并 readback 成功

若三道 Gate 任一未通过，则 N2D 不得启动。

## 6. N2D 准入门槛：AC 交换格式 v1

进入 N2D 的前提：
- N2C 三道 Gate 全部通过
- `.ac6acdata` 交换格式字段集冻结
- GUI 中 AC 相关 disabled 按钮可切换为真实启用动作

## 7. 证据清单

N2A 每轮至少保留：
1. 打开存档成功截图
2. 徽章库五类来源截图
3. AC 库五类来源截图
4. 徽章预览截图
5. AC 预览或 provenance 截图
6. 目标使用者栏位选择弹窗截图
7. 徽章导入成功结果截图
8. readback 验证记录
9. 原始存档前后对比清单
10. 输出新存档路径与再次打开记录
11. AC disabled action with reason 截图

## 8. 推荐验证命令

### 构建
```text
cmake -S native -B build/native -G "Visual Studio 17 2022" -A x64
cmake --build build/native --config Release
```

### 自动化回归
```text
ctest --test-dir build/native --output-on-failure -C Release
```

### N2A 关键人工验收
```text
1. 启动 exe
2. 打开真实 .sl2
3. 查看徽章库五类来源
4. 查看 AC 库五类来源
5. 选择分享徽章
6. 打开目标使用者栏位选择弹窗
7. 选择指定使用者栏位并导入
8. 重新打开结果存档
9. 确认 AC 写入仍保持 disabled 且说明原因
```

## 9. N2A Ralph 执行建议

```text
$ralph "按 docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.prd.md、docs/ac6-data-manager.native-cpp.n2-asset-library-exchange.technical-design.md 与 docs/appendices/ac6-data-manager.native-cpp.n2-asset-library-exchange.acceptance-baseline.md 执行 N2A。目标：完成统一资产目录壳，并落地徽章稳定产品化 + AC 只读探测。范围：1）徽章与 AC 均按 使用者1/2/3/4 + 分享 五类来源建模；2）GUI 新增徽章库 / AC库双视图；3）实现‘将所选徽章导入使用者栏’并加入目标使用者栏位选择弹窗；4）AC 只做五类来源读出、详情、preview provenance 调查与原生预览链验证，未资格化前禁止 AC 写入；5）引入 preview provenance 与最小统一目录模型；6）所有写操作继续遵守 shadow/staging workspace、fresh-destination-only、no in-place overwrite、post-write readback。执行方式：Ralph + 5 个 subagents；Subagent-1 负责 catalog / record-map，Subagent-2 负责 preview，Subagent-3 负责 emblem mutation，Subagent-4 预留 exchange lane 但本阶段只冻结接口不开放 AC/Emblem 文件交换，Subagent-5 负责 GUI / validation。验收：GUI 可浏览徽章与 AC 的五类来源；徽章可导入指定使用者栏位；AC 至少可读、可分类、可展示预览状态；未资格化功能必须 disabled 且说明原因。"
```

## 10. N2B 验收与证据基线

### 10.1 通过条件
- `.ac6emblemdata` 扩展名与字段集符合接口冻结文档。
- GUI 可完成“导出所选徽章”与“导入徽章文件到使用者栏”。
- 文件导入仍必须经过目标栏位选择弹窗。
- export -> import -> reopen -> reclassify 闭环成功。

### 10.2 必留证据
- 导出的 `.ac6emblemdata` 样本路径
- 字段摘录或检查输出
- 导入成功后的输出 `.sl2` 路径
- reopen 成功记录
- 至少 1 条失败样本记录

### 10.3 文档落点
- `docs/appendices/ac6-data-manager.native-cpp.n2b-qualification-and-evidence.md`

## 11. N2C AC Gate 基线

### 11.1 Gate-AC-01：record-map stable
通过条件：
- `USER_DATA002..006 -> user1..user4/share` 映射在当前样本集上稳定；
- 自动化测试与人工 spot check 一致；
- GUI 分类与底层映射一致。

### 11.2 Gate-AC-02：preview provenance stable
通过条件：
- 明确当前 preview 来源链；
- 不再把批量结果停留在“纯 unknown”；
- GUI 能稳定显示 provenance 结果。

### 11.3 Gate-AC-03：指定使用者栏位 copy/write + readback
通过条件：
- 从 share AC 到指定 user slot 的内部 copy 成功；
- 输出 fresh destination；
- readback 匹配；
- reopen save 成功。

### 11.4 Gate 记录约束
- 每个 gate 必须单独记录：`Status / Evidence / Sample Set / Command / Result / Blocker / Next Step`。
- 若任一 gate 不是 `PASS`，N2D 必须保持 blocked。

### 11.5 文档落点
- `docs/appendices/ac6-data-manager.native-cpp.n2c-ac-gate-record.md`

## 12. N2D 验收与证据基线

### 12.1 通过条件
- `N2C` 三道 gate 全部 `PASS`。
- `.ac6acdata` 导出/导入闭环成功。
- GUI 中 AC 与徽章动作都处于普通用户可理解的稳定状态。

### 12.2 必留证据
- AC export/import/reopen/readback 全链路记录
- GUI 最终主流程截图
- known limitations 与 release verdict

### 12.3 文档落点
- `docs/appendices/ac6-data-manager.native-cpp.n2d-qualification-and-evidence.md`
