# Agent Benchmark Submissions

公开**提交仓**：被测 Agent / 调度器把每题解题源码 push 到这里，裁判从本仓拉取冻结提交进行私有隐藏测试评分。

对应题库：<https://github.com/changw9813/agent-benchmark-tasks>（公开题库 A，只读）

## 为什么必须 push

裁判**不会**读你的本地临时工作区。  
没有 push 到本仓的冻结 commit / tag，等于**没交卷**。

## 命名约定

```text
分支：submit/<task-id>/<run-id>
标签：submit/<task-id>/<run-id>          # 可选，冻结用
路径：<task-id>/...                      # 该题全部允许源文件
```

示例：

```text
submit/python-001/run-20260922-001
python-001/main.py
```

- `task-id`：题库 `catalog.json` 中的 ID（如 `python-001`）
- `run-id`：一次评测运行的唯一 ID（建议 `run-YYYYMMDD-NNN` 或 git short sha）

同一题允许多次提交；以裁判锁定的 `run-id` 对应 commit 为准。

## 提交内容

只包含该题协议允许的源文件（见 `PROTOCOL.md`）：

- 仅对应语言源文件；最多 100 文件、合计 2MiB
- 禁止符号链接、预编译二进制、自定义构建脚本
- 入口文件名按题库 `catalog.json` 的 `entry`（如 `main.rs` / `main.py`）

**不要**提交：

- 密钥、token、`.env`、gh 配置、钥匙串导出
- 题库 A 的题目/环境整仓拷贝
- 任何私有裁判库 B 的内容
- 依赖缓存、`target/`、`node_modules/`、虚拟环境

## 与 ADR-0038 四组基线对齐

一次完整评测（A/B/C/D）建议：

```text
submit/<task-id>/<run-id>-A   # single, no JEV
submit/<task-id>/<run-id>-B   # single + JEV
submit/<task-id>/<run-id>-C   # fusion, no JEV
submit/<task-id>/<run-id>-D   # fusion + JEV
```

或在 commit message / 轻量 JSON 元数据里标明 `arm`。元数据示例（可选，放题目录下 `run-meta.json`）：

```json
{
  "taskId": "python-001",
  "runId": "run-20260922-001",
  "arm": "D",
  "fusion": true,
  "jev": true,
  "agentProfile": "opencode+codegraph+jev-rebuild",
  "budgetTokens": 20000000
}
```

## 评分

- 由**私有裁判**在隐藏用例上重跑冻结源码
- 不接受 Agent 自报通过率
- `tools/bench.py smoke` 仅公开样例，不代表成绩

## 权限

- 本仓 **public**，供裁判只读拉取
- 写权限只给被测调度器使用的机器人/账号
- **禁止**把维护者凭据或裁判库 B 提供给被测 Agent

## License

提交源码默认 MIT（与题库一致）。若某题另有要求，以该题 `task.md` 为准。
