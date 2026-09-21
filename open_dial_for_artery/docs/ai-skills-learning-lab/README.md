# AI 编程 Skills 学习实验室

这是基于 `open_dial` 当前代码的**非生产演练包**。它不改仓库、不安装 OpenSpec 或 MattPocock Skills、不编译和部署到设备。

练习主题：给 `/tmp/dial_status` 的 `[dial]` 节模拟增加 `last_transition_reason` 字段，使运维人员能知道当前状态是因何发生最近一次状态机跳转。

## 使用顺序

1. 先读 `openspec-draft/proposal.md`、`specs/dial-status-transition-reason.md`、`design.md` 和 `tasks.md`。将它们当作一次 `/opsx:explore`、`/opsx:propose` 后的人工审阅材料；不要先看实施方案就写代码。
2. 在 `matt-pocock-draft/CONTEXT.md` 中核对术语，再读 ADR、Spec 和 Tickets。这模拟 `/grill-with-docs → /to-spec → /to-tickets`。
3. 用 `fixtures/` 中的场景先写下你期望的失败验证，再对照 `test-plan.md` 判断验证是否覆盖规格。
4. 填写 `review-checklist.md`，分别完成规格符合度和代码质量两条评审轴。
5. 最后阅读 `retrospective-template.md`，决定哪些步骤值得进入团队日常流程。

## 两种工具的对应关系

| 目标 | Claude Code（正式安装后） | Codex（当前环境） |
|---|---|---|
| 只分析 | `/opsx:explore` | 明确要求“只读分析、不得修改”，输出分析结论 |
| 产出变更计划 | `/opsx:propose` | 创建/评审 proposal、spec、design、tasks |
| 澄清领域与决策 | `/grill-with-docs` | 对话澄清，维护 CONTEXT 与 ADR |
| 施工与评审 | `/implement`、内部 `/tdd`、`/code-review` | 先验证、最小实现、复核规格和测试 |
| 归档 | `/opsx:archive` | 合并确认后的规格并记录复盘 |

不要在 Codex 中假定这些 Claude slash 命令可直接执行；重要的是产物和顺序，而不是命令名称。

## 完成标准

- 能用自己的话说出：spec 定义“完成是什么”，tasks 定义“如何完成”。
- 能解释 `last_transition_reason` 在首次启动、正常连接、通道切换、断线重拨时的值。
- 能列出至少一个本练习不该做的越界改动（例如改变切换策略或扫描 `/proc` 的行为）。
- 能完成两轴评审，并记录是否应正式接入某项工具或规则。
