<div align="center">
  <img src="chrome-extension/public/icon-128.png" alt="Superpower" width="88" height="88" />

  <h1>Superpower</h1>

  <p><strong>连接浏览器 AI 与原生桌面工作流的统一 MCP 层。</strong></p>

  <p>
    <a href="https://github.com/stloendays/Superpower/stargazers"><img src="https://img.shields.io/github/stars/stloendays/Superpower?style=flat-square&logo=github&label=Stars" alt="GitHub stars" /></a>
    <a href="https://github.com/stloendays/Superpower/releases/tag/v1.4.1"><img src="https://img.shields.io/badge/stable-v1.4.1-111827?style=flat-square" alt="稳定版 v1.4.1" /></a>
    <img src="https://img.shields.io/badge/development-V1.5-6B7280?style=flat-square" alt="V1.5 开发中" />
    <a href="https://chromewebstore.google.com/detail/eioecjdcckpdakngpgikbinalieickob?utm_source=item-share-cb"><img src="https://img.shields.io/badge/Chrome_Web_Store-Install-4285F4?style=flat-square&logo=googlechrome&logoColor=white" alt="从 Chrome Web Store 安装 Superpower" /></a>
    <img src="https://img.shields.io/badge/Protocol-MCP-4F46E5?style=flat-square" alt="Model Context Protocol" />
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-16A34A?style=flat-square" alt="MIT License" /></a>
  </p>

  <p>
    <a href="README.md">English</a> · <strong>简体中文</strong>
  </p>

  <p>
    <a href="#产品形态">产品</a> ·
    <a href="#为什么使用-superpower">为什么使用</a> ·
    <a href="#主要功能">功能</a> ·
    <a href="#支持的平台">平台</a> ·
    <a href="#工作原理">架构</a> ·
    <a href="#快速开始">快速开始</a> ·
    <a href="#仓库结构">仓库结构</a>
  </p>
</div>

<p align="center">
  <img src="docs/readme/hero.svg" alt="Superpower 浏览器扩展与桌面应用" width="100%" />
</p>

<div align="center">
  <strong>牛津大学 × 新加坡国立大学合作项目 · Tony 主导</strong><br/>
  <sub>面向浏览器与桌面环境的实用 MCP 工作流。</sub>
</div>

<br/>

> **版本状态：** v1.4.1 是当前公开稳定版本；V1.5 为主线开发版本，正在加入以审核为核心的自然语言路由、多步骤工作流规划与受控执行能力。

## 产品形态

<table>
<tr>
<td width="50%" valign="top">

### 浏览器扩展

把 MCP 工具直接带入支持的 AI 网站，在对话页面内完成工具调用，而不需要频繁切换应用。

- 可在 ChatGPT、Gemini、Perplexity、Grok、Qwen 等网站中使用
- 识别结构化工具调用，并把 MCP 执行结果返回当前对话
- 支持 **Streamable HTTP、SSE 和 WebSocket** MCP 连接
- 新的本地配置默认推荐使用 **Streamable HTTP**
- 提供工具可见性、自动化和审核控制

**安装：** [Chrome Web Store](https://chromewebstore.google.com/detail/eioecjdcckpdakngpgikbinalieickob?utm_source=item-share-cb)

**适合场景：** 以浏览器 AI 对话为主要工作界面的用户。

</td>
<td width="50%" valign="top">

### 桌面应用

在浏览器之外，把 Superpower 作为独立的原生 MCP 工作空间使用。

- 原生 **Qt 6** Windows 应用
- 使用 **Connections → Apps → Actions → Runs** 组织工作流
- 提供独立 MCP 客户端、工作流审核与受控执行界面
- 通过 GitHub Releases 提供便携式 Windows 安装包

**适合场景：** 希望在一个桌面工作区中集中管理 MCP 连接、动作和运行记录的用户。

</td>
</tr>
</table>

## 为什么使用 Superpower？

Superpower 的目标是让工具执行尽量靠近实际工作的界面。对话本身就是工作区时，可以使用浏览器扩展；需要独立的 MCP 控制面板时，则可以使用桌面应用。

<table>
<tr>
<td width="33%" valign="top">

### 保持上下文
无需不断在 AI、终端、控制台和其他工具之间来回切换。

</td>
<td width="33%" valign="top">

### 连接真实工具
可以把本地或远程 MCP Server 接入浏览器和桌面工作流。

</td>
<td width="33%" valign="top">

### 保留人工控制
决定哪些工具可以暴露，并在敏感或受保护操作执行前进行审核。

</td>
</tr>
</table>

<p align="center">
  <img src="docs/readme/product-overview.svg" alt="Superpower 产品概览" width="100%" />
</p>

## 主要功能

- 浏览器与桌面共享一套 MCP 执行模型
- 浏览器端工具发现、结构化调用识别与结果回填
- 原生桌面 Connections、Apps、Actions、Runs 工作区
- 支持本地与远程 MCP endpoint
- 以审核为核心的自然语言 Action Router
- 支持显式跨步骤数据绑定的 Workflow Planner
- 会话级 Workflow Runner，每次最多推进一个受控 MCP Action
- 持久化控制项与自动生成的 MCP Instructions

<table>
<tr>
<td width="50%" align="center" valign="top">
  <img src="docs/readme/sidebar-overview.svg" alt="Superpower 浏览器侧边栏" width="100%" />
  <br /><sub>浏览器扩展：连接、工具与自动化控制。</sub>
</td>
<td width="50%" align="center" valign="top">
  <img src="docs/readme/tool-flow.svg" alt="Superpower MCP 工具执行流程" width="100%" />
  <br /><sub>结构化工具调用通过 MCP 路由，并把结果返回当前工作流。</sub>
</td>
</tr>
</table>

### V1.5：审核优先的路由与工作流

V1.5 开发线会把自然语言意图映射到 MCP 能力，基于工具 schema 起草参数，规划多步骤工作流，并明确展示步骤之间的数据绑定关系。受保护的执行过程仍保持可审核：Workflow Runner 每次最多推进一个 MCP Action，已经审核过的绑定关系和策略检查仍然具有最终约束力。

<p align="center">
  <img src="docs/readme/action-router.svg" alt="Superpower 自然语言 Action Router" width="100%" />
</p>

## 支持的平台

浏览器扩展当前支持 ChatGPT、Google Gemini、Perplexity、Google AI Studio、Grok、OpenRouter、DeepSeek、T3 Chat、GitHub Copilot、Mistral、Kimi、Qwen Chat 和 Z.ai。

<p align="center">
  <img src="docs/readme/platform-grid.svg" alt="Superpower 支持的 AI 平台" width="100%" />
</p>

## 工作原理

<p align="center">
  <img src="docs/readme/architecture.svg" alt="Superpower 架构" width="100%" />
</p>

1. 浏览器或桌面工作流选择一个 MCP 能力。
2. Superpower 通过当前配置的 MCP 连接发送结构化请求。
3. MCP Server 执行对应工具。
4. 执行结果返回当前浏览器对话或桌面工作流。

## 快速开始

### 浏览器扩展

1. 从 [Chrome Web Store 安装 Superpower](https://chromewebstore.google.com/detail/eioecjdcckpdakngpgikbinalieickob?utm_source=item-share-cb)。
2. 在 Chrome 中打开扩展并配置 MCP 连接。
3. 使用标准本地代理时，推荐选择 **Streamable HTTP**，地址填写 `http://localhost:3006/mcp`。
4. 打开受支持的 AI 网站，即可在浏览器工作流中使用 Superpower。

> **连接兼容性：** 新的本地配置默认采用 Streamable HTTP。显式配置的旧版 SSE 地址（例如 `http://localhost:3006/sse`）以及 WebSocket endpoint 仍然支持；升级不会覆盖用户已经保存的连接设置。

需要手动安装或开发安装时，请查看 [`docs/install/windows-extension.md`](docs/install/windows-extension.md)。

### 桌面应用

1. 打开 [最新 GitHub Release](https://github.com/stloendays/Superpower/releases/latest)。
2. 下载 `Superpower-Desktop-*-Windows-x64.zip`。
3. 完整解压后，启动打包好的桌面应用。

### 从源码构建浏览器扩展

环境要求：**Node.js 22.12+**、**pnpm 9.x** 和 Chromium 系浏览器。

```bash
git clone https://github.com/stloendays/Superpower.git
cd Superpower
pnpm install
pnpm base-build
```

配置并启动 MCP proxy，然后在 Chrome 的扩展管理页面中以“加载已解压的扩展程序”方式加载 `dist/`。

## 开发

```bash
pnpm dev          # 开发构建
pnpm base-build   # 生产构建
pnpm type-check   # 类型检查
pnpm lint         # 代码检查
```

## 仓库结构

仓库根目录只保留真正的项目入口、标准文档和构建配置。具体实现、安装脚本和可选示例分别放入对应目录，避免根目录不断膨胀。

```text
Superpower/
├── chrome-extension/          # 浏览器扩展外壳与后台集成
├── desktop/                   # 原生 Qt 桌面应用及桌面示例
├── docs/                      # 文档、安装指南与 README/网站素材
├── packages/                  # 可复用 TypeScript 包，包括 MCP Core/Host
├── pages/                     # 浏览器扩展页面和 content scripts
├── scripts/
│   ├── install/               # Windows 源码/Release 安装辅助脚本
│   └── shell/                 # 构建、环境和版本工具
├── README.md                  # 英文项目主页
├── README.zh-CN.md            # 简体中文项目主页
├── SECURITY.md                # 安全模型与漏洞报告说明
├── CHANGELOG.md               # 版本历史
└── package.json               # Monorepo 入口
```

`pnpm-workspace.yaml`、`tsconfig.json`、`turbo.json`、`.nvmrc` 和 `eslint.config.ts` 等工具链文件继续保留在根目录，因为构建系统依赖这些路径。生成文件或实现细节文件不应随意加入根目录。

## 项目背景

Superpower 是一个**由牛津大学与新加坡国立大学合作、Tony 主导的项目**，重点探索浏览器和桌面环境中更实用的人机协作工作流与 MCP 工具使用方式。

Superpower V1 基于 **MCP SuperAssistant** 修改发展而来。原项目的 MIT License 和上游署名继续保留在 [`LICENSE`](LICENSE) 与 [`NOTICE.md`](NOTICE.md) 中。

## 安全

MCP Server 可能暴露文件系统、数据库、开发者工具或第三方 API 权限。只连接你信任的 endpoint，并避免把凭证提交到仓库。详细说明见 [`SECURITY.md`](SECURITY.md)。

## 贡献

欢迎提交 Issue 和 Pull Request。报告 Bug 时，建议注明受影响的界面（浏览器或桌面）、平台/浏览器版本以及可复现步骤。

## 许可证

本项目使用 [MIT License](LICENSE)，上游署名与衍生关系说明见 [NOTICE.md](NOTICE.md)。
