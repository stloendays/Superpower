# 浏览器扩展操作指南

这份指南说明 Superpower 浏览器扩展的标准使用流程，以及如何在不上传 Chrome Web Store 的情况下测试本地开发版本。

## 1. 先启动 MCP Server 或 Proxy

Superpower 本身是浏览器侧 MCP Client。浏览器扩展不能直接启动本地 stdio 命令，因此本地工具需要通过浏览器可以访问的 MCP endpoint 暴露出来。

标准本地 Superpower 配置建议使用：

```text
Transport: Streamable HTTP
Endpoint:  http://localhost:3006/mcp
```

使用扩展期间需要保持本地 MCP Proxy 运行。

## 2. 连接 Superpower

1. 打开一个受支持的 AI 网站，例如 ChatGPT。
2. 打开 Superpower Sidebar。
3. 在 **Connect MCP** 中粘贴 MCP endpoint，或者粘贴支持的 MCP JSON 配置。
4. 除非正在排查兼容性问题，否则保持 transport 自动识别。
5. 点击 **Connect**。

对于标准本地配置，Superpower 会把 `http://localhost:3006/mcp` 自动识别为 **Streamable HTTP**。

连接成功后应该看到 **MCP ready**，并显示可用工具数量。

## 3. 检查工具和 Instructions

连接后：

1. 打开 **Available Tools**，确认需要的工具已经出现。
2. 打开 **Instructions**，检查自动生成的 MCP instructions。
3. Custom Instructions 主要用于补充执行偏好和安全规则；工具 schema 会由 Superpower 自动生成，不需要手工重复粘贴。

如果刚刚重新加载过扩展，而 AI 网页此前已经打开，请刷新网页一次，让最新 content script 重新注入。

## 4. 第一次先做只读测试

建议先用不会修改系统状态的任务验证完整链路，例如：

> 列出我桌面上的文件，并简单总结一下。不要修改、移动或删除任何文件。

预期链路是：

```text
网页 AI
  -> 结构化工具调用
  -> Superpower
  -> MCP Server / Proxy
  -> 本地工具
  -> 工具结果
  -> 返回同一个 AI 对话
```

确认工具调用和结果都正确后，再尝试写文件、删除、进程控制、系统配置等状态变更操作。

## 5. 状态变更操作

对于可能修改机器或外部系统的动作：

- 当前状态未知时，先读取或检查目标；
- 优先做局部、明确的修改，不做无关的大范围重写；
- 执行前检查路径和参数；
- 在不知道上一次操作是否已经成功时，不要盲目重试状态变更调用；
- 多步骤任务尽量使用 review-gated workflow。

## 不上传商店，直接测试本地版本

开发阶段不需要每改一次就重新上传 Chrome Web Store。

### 构建

要求：Node.js 22.12+、pnpm 9.x 和 Chromium 系浏览器。

```bash
git clone https://github.com/stloendays/Superpower.git
cd Superpower
pnpm install
pnpm base-build
```

生产扩展会构建到 `dist/`。

### 加载已解压扩展

1. 打开 `chrome://extensions/`。
2. 开启 **开发者模式**。
3. 暂时关闭 Chrome Web Store 版本的 Superpower，确保只启用一个 Superpower 实例。
4. 点击 **加载已解压的扩展程序**。
5. 选择仓库中包含 `manifest.json` 的 `dist/` 目录。
6. 打开或刷新准备测试的 AI 网站。

### 后续继续改代码时

1. 再运行一次 `pnpm base-build`。
2. 回到 `chrome://extensions/`。
3. 对本地 Superpower 点击 **重新加载**。
4. 刷新所有已经打开的 AI 网站标签页。

这样可以避免同时启用两个扩展实例，以及旧 content script 没有刷新导致的状态错乱。

## 快速排障

### 出现 `SSE error: 404`

先确认保存的 endpoint 和 transport。标准本地 Proxy 应该是：

```text
http://localhost:3006/mcp
streamable-http
```

错误文本中出现 `SSE` 字样，本身并不能单独证明当前 transport 真的是 SSE；要以实际保存的配置和真实请求 URL 为准。

### Instructions 一直停在 `Loading instructions...`

重新加载扩展并刷新 AI 网页。当前版本已经让不同 content bundle 共享生成后的 instruction state，因此后打开的 Popover 应该可以立即复用 Sidebar 已生成的 Instructions。

### 页面里出现两套 Superpower UI 或状态不一致

检查 `chrome://extensions/`，确认商店版和本地已解压版没有同时启用。

### 本地 stdio 配置无法连接

浏览器扩展不能直接启动本地 stdio MCP 进程。需要先通过 Superpower Host 或其他浏览器可访问的 MCP Proxy 暴露 endpoint，再让扩展连接该 endpoint。
