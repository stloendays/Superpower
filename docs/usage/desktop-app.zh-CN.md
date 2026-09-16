# Superpower Desktop 使用指南

Superpower Desktop 是原生 MCP 工作台，用于管理连接、审核 Action、运行 Workflow，以及在桌面端查看和发起浏览器 AI 对话。

## 1. 启动桌面端

打开 Windows 打包版本，或从 `desktop/qt` 源码构建。Home 页面用于查看连接与工作流状态；Actions 页面用于管理 MCP 能力和审核后的执行。

## 2. 连接 MCP Server

在 Connections 中配置桌面端要使用的 MCP endpoint。对于标准本地 Superpower Proxy，推荐使用 Streamable HTTP：

```text
http://localhost:3006/mcp
```

浏览器扩展和桌面应用是两个独立的 MCP Client。它们可以连接同一个 MCP Server，但各自维护自己的连接状态。

## 3. Conversation 与 Quick Ask

通过 **View → Conversation** 或快捷键 **Ctrl+Shift+C** 打开 Conversation 面板。该面板通过本地 loopback relay 同步支持的 ChatGPT 浏览器对话。

面板底部的 **Quick Ask** 输入条可以直接从 Superpower Desktop 向当前活动的 ChatGPT 浏览器标签页提问：

1. 保持 Superpower Desktop 运行。
2. 安装或加载 Superpower 浏览器扩展。
3. 在 Chromium 浏览器中打开 ChatGPT，并让目标对话保持为活动标签页。
4. 在桌面端 **Quick Ask** 中输入问题，按 **Enter** 或点击 **Send**。
5. 提问会通过本地 relay 写入活动 ChatGPT 的输入框并提交；浏览器里的后续对话又会同步回桌面端 Conversation 面板。

Quick Ask 使用本地 conversation relay：`127.0.0.1:32148`。它和 MCP 地址 `localhost:3006/mcp` 是两条不同的通路。

如果 30 秒内没有活动的 ChatGPT 标签页接收问题，桌面端会返回发送失败，而不是假装已经成功发送。

## 4. 第一次建议测试

先使用只读请求，例如：

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

如果你更喜欢网页模型的推理和对话体验，可以继续把浏览器作为主要工作区；Desktop 则负责原生连接管理、审核、运行状态和本地工作流控制。

## 6. 常见问题

### Quick Ask 提示 relay 不可用

重启 Superpower Desktop。conversation relay 会随桌面程序启动，并且只监听 `127.0.0.1:32148`。

### Quick Ask 30 秒超时

确认 ChatGPT 已经在浏览器中打开、Superpower 扩展已启用，而且目标 ChatGPT 对话是活动标签页。如果你刚重新加载本地开发版扩展，还要刷新 ChatGPT 网页。

### MCP 已连接，但 Quick Ask 不工作

两者不是同一条连接。MCP 使用你配置的 MCP endpoint（例如 `http://localhost:3006/mcp`），Quick Ask 使用端口 `32148` 的本地浏览器对话 relay。

### Quick Ask 正常，但工具不能调用

单独检查 MCP Connection 和 Available Tools。conversation relay 能工作，并不代表 MCP Server 已连接。

## 7. 本地开发测试

桌面端开发时，需要构建 Qt 应用以及 `packages/mcp-host`，确保 conversation relay 可用。浏览器端可以在 `chrome://extensions` 打开开发者模式，把构建后的 `dist/` 作为“已解压扩展程序”加载。每次重新构建扩展后，要重新加载扩展并刷新 ChatGPT 页面。

相关文档：

- [浏览器扩展操作指南](browser-extension.zh-CN.md)
- [`desktop/qt/README.md`](../../desktop/qt/README.md)
