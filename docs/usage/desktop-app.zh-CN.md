# Superpower Desktop 使用指南

Superpower Desktop 是原生 MCP 工作台，用于管理连接、审核 Action、运行 Workflow，以及从桌面端向浏览器 AI 对话发起提问。

## 1. 启动桌面端

打开 Windows 打包版本，或从 `desktop/qt` 源码构建。Home 页面用于查看连接与工作流状态；Actions 页面用于管理 MCP 能力和审核后的执行。

## 2. 连接 MCP Server

在 Connections 中配置桌面端要使用的 MCP endpoint。对于标准本地 Superpower Proxy，推荐使用 Streamable HTTP：

```text
http://localhost:3006/mcp
```

浏览器扩展和桌面应用是两个独立的 MCP Client。它们可以连接同一个 MCP Server，但各自维护自己的连接状态。

## 3. Conversation、AI Provider 与 Quick Ask

通过 **View → Conversation** 或快捷键 **Ctrl+Shift+C** 打开 Conversation 面板。面板中现在包含 **AI Provider** 选择条和 **Quick Ask** 输入条。

当前 Quick Ask 支持：

| Provider | 桌面端提问路由 | 实时对话同步 |
| --- | --- | --- |
| ChatGPT | 支持 | 支持 |
| Gemini | 支持 | 暂未开启 |
| Grok | 支持 | 暂未开启 |
| Perplexity | 支持 | 暂未开启 |

Provider 下拉框提供 **Auto · active supported tab**，以及 ChatGPT、Gemini、Grok、Perplexity 四个显式目标。建议默认使用 **Auto**：只有你当前正在看的受支持 AI 标签页才能领取桌面端问题，避免把问题发到后台的其他会话。

使用方法：

1. 保持 Superpower Desktop 运行。
2. 安装或加载 Superpower 浏览器扩展。
3. 在 Chromium 浏览器中打开 ChatGPT、Gemini、Grok 或 Perplexity，并让目标对话保持为活动标签页。
4. 在 Desktop 中打开 **View → Conversation**。
5. `AI Provider` 保持 **Auto**，或者手动选择目标 Provider。
6. 在 **Quick Ask** 中输入问题，按 **Enter** 或点击 **Send**。
7. Desktop 会先把问题加入本地队列；活动且匹配的浏览器标签页领取后，由对应网站已有的 input adapter 写入并提交。

Desktop 中的 Provider 状态会显示当前本地 relay 检测到的活动浏览器 AI。若你显式选择了某个 Provider，问题不会误发给另一个 Provider。

ChatGPT 目前拥有相对稳定的消息 role 标记，因此 Superpower 可以继续把 ChatGPT 的用户/助手消息实时同步回 Desktop。Gemini、Grok 和 Perplexity 这一版先保持为“可发送、暂不同步回复”，避免依赖容易失效的猜测型 DOM selector。

Quick Ask 使用本地 conversation relay：`127.0.0.1:32148`。它和 MCP 地址 `localhost:3006/mcp` 是两条独立通路。

如果 30 秒内没有活动且匹配的 Provider 标签页接收问题，Desktop 会返回明确的超时错误，而不会假装发送成功。

## 4. 第一次建议测试

先做一个不依赖工具的纯传输测试：

```text
Reply with exactly: Superpower provider bridge OK
```

先用 **Auto** 测一次，再显式指定当前 Provider 测一次。浏览器端应该每次只收到一条问题。

随后再做 MCP 只读测试，例如：

```text
列出我桌面上的文件并简单总结。不要修改、移动或删除任何文件。
```

如果浏览器工作流启用了 MCP tools，在允许会改变状态的操作前先检查生成的工具调用和参数。

## 5. Actions 与 Workflow

Superpower V1.5 保持 review-gated 的执行方式：

- **Action Router**：把自然语言需求映射到可用 MCP 能力。
- **Workflow Planner**：生成有顺序的多步骤计划，并显式记录跨步骤绑定。
- **Workflow Review**：在执行前检查参数和策略判断。
- **Workflow Runner**：一次只推进一个受控步骤。

如果你更喜欢网页模型的推理和对话体验，可以继续把浏览器作为主要工作区；Desktop 负责原生连接管理、审核、运行状态、Provider 路由和本地工作流控制。

## 6. 常见问题

### Quick Ask 提示 relay 不可用

重启 Superpower Desktop。conversation relay 会随桌面程序启动，并且只监听 `127.0.0.1:32148`。

### Desktop 一直显示没有检测到活动 Provider

打开 ChatGPT、Gemini、Grok 或 Perplexity 页面，并让该标签页保持活动。如果你刚重新加载了本地开发版扩展，还需要刷新对应 Provider 网页。

### Quick Ask 30 秒超时

检查 Desktop 里选择的 Provider 是否和浏览器当前活动标签页一致。例如 Desktop 选的是 Gemini，但你当前停留在 ChatGPT，那么这个问题不会误发给 ChatGPT，而是等待 Gemini 成为活动标签页，直到超时。

### 浏览器已经收到文字，但没有自动发送

通常说明该网站更新了 composer DOM。Superpower 复用了各网站已有的 input adapter；需要根据具体 Provider 更新 selector。此时 Desktop 会报告插入或自动提交失败，而不会把失败当成功。

### ChatGPT 可以同步回复，其他 Provider 为什么没有

这是当前版本的预期行为。ChatGPT 已开启 transcript mirroring；Gemini、Grok、Perplexity 目前先作为 Quick Ask 发送目标。

### MCP 已连接，但 Quick Ask 不工作

两者不是同一条连接。MCP 使用你配置的 MCP endpoint（例如 `http://localhost:3006/mcp`），Quick Ask 使用端口 `32148` 的本地浏览器 conversation relay。

### Quick Ask 正常，但工具不能调用

单独检查 MCP Connection 和 Available Tools。conversation relay 能工作，并不代表 MCP Server 已连接。

## 7. 本地开发测试

桌面端开发时，需要构建 Qt 应用以及 `packages/mcp-host`，确保 conversation relay 可用。浏览器端可以在 `chrome://extensions` 打开开发者模式，把构建后的 `dist/` 作为“已解压扩展程序”加载。每次重新构建扩展后，要重新加载扩展并刷新正在测试的 Provider 页面。

完整 smoke test 建议检查：

- Auto 只把问题交给当前活动的受支持 Provider。
- 显式选择 Provider 时，不会把问题交给其他 Provider。
- ChatGPT 仍然可以把用户和助手消息实时同步到 Desktop。
- Gemini、Grok、Perplexity 可以通过各自已有的输入适配器接收 Desktop 问题。
- 没有活动 Provider 或 Provider 不匹配时，会超时并报错，不会出现 false success。

相关文档：

- [浏览器扩展操作指南](browser-extension.zh-CN.md)
- [`desktop/qt/README.md`](../../desktop/qt/README.md)
