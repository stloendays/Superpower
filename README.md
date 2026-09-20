<div align="center">
  <img src="chrome-extension/public/icon-128.png" alt="Superpower" width="88" height="88" />

  <h1>Superpower</h1>

  <p><strong>One MCP layer for browser AI and native desktop workflows.</strong></p>

  <p>
    <a href="https://github.com/stloendays/Superpower/stargazers"><img src="https://img.shields.io/github/stars/stloendays/Superpower?style=flat-square&logo=github&label=Stars" alt="GitHub stars" /></a>
    <a href="https://github.com/stloendays/Superpower/releases/tag/v1.4.1"><img src="https://img.shields.io/badge/stable-v1.4.1-111827?style=flat-square" alt="Stable release v1.4.1" /></a>
    <img src="https://img.shields.io/badge/development-V1.5-6B7280?style=flat-square" alt="V1.5 development" />
    <a href="https://chromewebstore.google.com/detail/eioecjdcckpdakngpgikbinalieickob?utm_source=item-share-cb"><img src="https://img.shields.io/badge/Chrome_Web_Store-Install-4285F4?style=flat-square&logo=googlechrome&logoColor=white" alt="Install Superpower from Chrome Web Store" /></a>
    <img src="https://img.shields.io/badge/Protocol-MCP-4F46E5?style=flat-square" alt="Model Context Protocol" />
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-16A34A?style=flat-square" alt="MIT License" /></a>
  </p>

  <p>
    <strong>English</strong> · <a href="README.zh-CN.md">简体中文</a>
  </p>

  <p>
    <a href="#what-problem-does-superpower-solve">Problem</a> ·
    <a href="#products">Products</a> ·
    <a href="#product-tour">Tour</a> ·
    <a href="#why-superpower">Why Superpower</a> ·
    <a href="#features">Features</a> ·
    <a href="#supported-platforms">Platforms</a> ·
    <a href="#how-it-works">Architecture</a> ·
    <a href="#quick-start">Quick start</a> ·
    <a href="docs/usage/browser-extension.md">Operation guide</a> ·
    <a href="#repository-layout">Repository</a>
  </p>
</div>

<p align="center">
  <img src="docs/readme/hero.svg" alt="Superpower Browser Extension and Desktop App" width="100%" />
</p>

<div align="center">
  <strong>Oxford × NUS collaboration</strong><br/>
  <sub>Practical MCP workflows across browser and desktop surfaces.</sub>
</div>

<br/>

> **Release status:** v1.4.1 is the current public stable release. V1.5 is the active development line and adds a browser Copilot workspace, any-page capture, YouTube summarization, local Knowledge capture, connected MCP actions, and review-first workflow execution.

## What problem does Superpower solve?

Web AI chats are good reasoning surfaces, but they normally cannot directly use your local files, command-line tools, private MCP servers, or desktop workflows. Superpower connects those two worlds: the web model can decide what tool work is needed, while MCP executes the requested action through a local or remote tool server and returns the result to the same conversation.

That makes Superpower useful when you want to keep working in **ChatGPT, Gemini, Perplexity, Grok, Copilot, Qwen or another supported web chat** while still using real MCP tools. It is not a replacement for coding agents such as Codex; instead, it provides a browser-first execution layer for users who prefer the web chat as the planning interface, want their own MCP stack, need a review step before execution, or want lightweight tool use without consuming a separate local-agent model quota.

**In one line:** use the AI chat you already have as the reasoning surface, and use MCP as the execution layer.

## Products

<table>
<tr>
<td width="50%" valign="top">

### Browser Extension

Bring MCP tools directly into supported AI websites without leaving the conversation.

- Works inside ChatGPT, Gemini, Perplexity, Grok, Qwen and more
- Detects structured tool calls and returns results to the active chat
- Supports Streamable HTTP, SSE and WebSocket MCP connections
- Uses **Streamable HTTP** as the recommended local default for new setups
- Includes tool visibility, automation and review controls

**Install:** [Chrome Web Store](https://chromewebstore.google.com/detail/eioecjdcckpdakngpgikbinalieickob?utm_source=item-share-cb)

**Best for:** browser-native AI workflows.

</td>
<td width="50%" valign="top">

### Desktop App

**Guide:** [Desktop usage and Quick Ask](docs/usage/desktop-app.md)


Use Superpower as a native MCP workspace outside the browser.

- Native **Qt 6** Windows application
- Organizes **Connections → Apps → Actions → Runs**
- Provides a standalone MCP client, workflow review and guarded execution surface
- Distributed as a portable Windows package in GitHub Releases

**Best for:** managing MCP connections and actions from one desktop workspace.

</td>
</tr>
</table>

## Product tour

<p align="center">
  <img src="docs/readme/product-tour.png" alt="Illustrative Superpower product tour showing research, writing, actions and browser workflows" width="92%" />
  <br /><sub>Illustrative product concept showing how Superpower can bring research, writing and tool-driven actions into one browser-first workflow. For the current compatibility list, see <a href="#supported-platforms">Supported platforms</a>.</sub>
</p>

## Why Superpower?

Superpower keeps tool execution close to the interface where the work happens. Use the browser extension when the conversation is the workspace, or use the desktop app when you want a dedicated MCP control surface.

<table>
<tr>
<td width="33%" valign="top">

### Stay in context
Use MCP capabilities without constantly switching tools or consoles.

</td>
<td width="33%" valign="top">

### Connect real tools
Use local or remote MCP servers across browser and desktop workflows.

</td>
<td width="33%" valign="top">

### Keep control
Choose exposed tools and review guarded actions before execution.

</td>
</tr>
</table>

## Features

- Shared MCP execution model across browser and desktop surfaces
- Browser **Copilot** workspace for page/selection-aware prompts inside supported AI sites
- Lightweight **Page Assistant** on ordinary HTTP(S) pages for selection-level Summarize, Explain, Ask AI, Save, page summarization, and action extraction
- Cross-tab AI routing: page actions go to the most recently used supported AI tab, or open ChatGPT when no supported AI workspace is available
- YouTube watch-page summarization that uses loaded transcript segments when available and falls back to supplied video metadata when they are not
- One-click **Summarize, Explain, Improve writing, Translate, Action items, and Study notes** workflows
- Local **Knowledge** capture for selected text with page title, source URL, timestamps, copy/remove controls, and Markdown export
- `Ctrl/⌘ + Shift + K` shortcut to surface the Copilot workspace from a supported AI page
- Browser-side tool discovery, structured call detection and result injection
- **Connected Actions** for detected notes, mail, calendar, task, Slack/messaging, and file-storage MCP tools, while preserving the review-first execution path
- Native desktop workspace for connections, apps, actions and runs
- Local and remote MCP endpoints
- Review-first natural-language Action Router
- Dependency-aware Workflow Planner with explicit cross-step bindings
- Session-only Workflow Runner with one-step-at-a-time guarded execution
- Persistent controls and generated MCP instructions

<p align="center">
  <img src="docs/readme/capability-overview.png" alt="Illustrative overview of Superpower summarization, writing and action workflows" width="92%" />
  <br /><sub>Illustrative capability overview. Superpower keeps reasoning in the AI surface while MCP provides the execution layer.</sub>
</p>

### V1.5: Copilot, any-page capture, Knowledge and connected actions

The V1.5 development line now starts from a **Copilot-first browser workspace**. On supported AI sites, Superpower can use the current selection or bounded page context to prepare structured prompts for summarization, explanation, rewriting, translation, action extraction, and study notes, then submit them through the site's existing AI composer. Selected text can also be saved locally into **Knowledge** with source metadata and exported as Markdown.

Outside the supported AI sites, a lightweight **Page Assistant** provides a small floating control without loading the full MCP/adapter stack. Selection and page-level tasks are routed to the most recently used supported AI tab; if none is available, Superpower opens ChatGPT and waits for the adapter before submitting. On YouTube watch pages, the assistant can build a video-summary request from the title, channel, description, and transcript segments when the transcript is available in the loaded page.

Connected MCP tools are also promoted into a **Connected Actions** panel. When compatible tools are detected, Superpower can surface actions such as Save note, Draft email, Plan event, Create task, Draft Slack, or Search files. These shortcuts still use the existing review-first MCP path rather than bypassing tool schemas or confirmation rules.

The natural-language Action Router continues to map intent to MCP capabilities, draft schema-backed parameters, plan multi-step workflows, make cross-step data bindings explicit, and keep guarded execution reviewable. Workflow runs advance at most one MCP Action at a time; reviewed bindings and policy checks remain authoritative.

<p align="center">
  <img src="docs/readme/action-router.svg" alt="Superpower natural-language Action Router" width="100%" />
</p>

## See Superpower in action

<p align="center">
  <img src="docs/readme/superpower-in-action.png" alt="Illustrative Superpower workflow gallery from research and writing to organized actions" width="100%" />
  <br /><sub>Illustrative workflow gallery: move from reading and synthesis to structured actions while staying in the same working context.</sub>
</p>

<details>
<summary><strong>More workflow examples</strong></summary>

<br />

<p align="center">
  <img src="docs/readme/usage-research.png" alt="Example Superpower research workflow for comparing sources, extracting insights and organizing notes" width="100%" />
  <br /><sub><strong>Research and synthesis:</strong> collect sources, compare evidence, extract useful highlights and keep reusable notes in context.</sub>
</p>

<p align="center">
  <img src="docs/readme/usage-writing.png" alt="Example Superpower writing workflow with rewrite, tone and outline actions" width="100%" />
  <br /><sub><strong>Write and refine:</strong> keep the draft visible while MCP-backed actions help rewrite, simplify, improve tone or build an outline.</sub>
</p>

<p align="center">
  <img src="docs/readme/usage-save-actions.png" alt="Example Superpower workflow for saving highlights, creating tasks and exporting notes" width="100%" />
  <br /><sub><strong>Capture and act:</strong> preserve useful highlights, turn them into tasks, organize them and export the result into the rest of your workflow.</sub>
</p>

</details>

## Supported platforms

The browser extension currently supports ChatGPT, Google Gemini, Perplexity, Google AI Studio, Grok, OpenRouter, DeepSeek, T3 Chat, GitHub Copilot, Mistral, Kimi, Qwen Chat, and Z.ai.

<p align="center">
  <img src="docs/readme/platform-grid.svg" alt="Supported AI platforms" width="100%" />
</p>

## How it works

<p align="center">
  <img src="docs/readme/architecture.svg" alt="Superpower architecture" width="100%" />
</p>

1. A browser or desktop workflow selects an MCP capability.
2. Superpower routes the structured request through the configured MCP connection.
3. The MCP server executes the tool.
4. The result is returned to the active browser conversation or desktop workflow.

## Quick start

### Browser Extension

1. Install [Superpower from the Chrome Web Store](https://chromewebstore.google.com/detail/eioecjdcckpdakngpgikbinalieickob?utm_source=item-share-cb).
2. Open the extension in Chrome and configure your MCP connection.
3. For the standard local proxy, use **Streamable HTTP** with `http://localhost:3006/mcp`.
4. Open a supported AI website. In the V1.5 development line, the sidebar opens to **Copilot** by default; use a quick action or press `Ctrl/⌘ + Shift + K` to return to it.
5. On ordinary web pages, use the small Superpower Page Assistant to summarize a selection/page, save content to **Knowledge**, or route the task into your active AI workspace. YouTube watch pages add a **Summarize video** action.
6. Inside Copilot, use **Connected Actions** when Superpower detects compatible MCP tools; switch to **Tools** when you want the full tool interface.

> **Connection compatibility:** new local setups default to Streamable HTTP. Explicit legacy SSE endpoints such as `http://localhost:3006/sse` and WebSocket endpoints remain supported. Existing saved connection settings are not overwritten.

**New to Superpower?** Follow the [Browser Extension Operation Guide](docs/usage/browser-extension.md) for the full connect → inspect tools → review instructions → run a safe first task workflow. The same guide also explains how to test an unpacked local build without publishing it to the Chrome Web Store.

For manual or development installation, see [`docs/install/windows-extension.md`](docs/install/windows-extension.md).

### Desktop App

1. Open the [latest release](https://github.com/stloendays/Superpower/releases/latest).
2. Download `Superpower-Desktop-*-Windows-x64.zip`.
3. Extract the archive and launch the packaged desktop application.

### Build the browser extension from source

Requirements: **Node.js 22.12+**, **pnpm 9.x**, and a Chromium-based browser.

```bash
git clone https://github.com/stloendays/Superpower.git
cd Superpower
pnpm install
pnpm base-build
```

Configure an MCP proxy, start it with your preferred transport, then load `dist/` as an unpacked extension. For the exact reload-and-refresh loop used during development, see [Test a local build without publishing](docs/usage/browser-extension.md#test-a-local-build-without-publishing).

## Development

```bash
pnpm dev          # Development build
pnpm base-build   # Production build
pnpm type-check   # Type checking
pnpm lint         # Lint
```

## Repository layout

The repository root is intentionally kept as an entry surface: standard project documents, workspace/build configuration, and the main product directories stay visible; implementation helpers and optional examples live under dedicated folders.

```text
Superpower/
├── chrome-extension/          # Browser extension shell and background integration
├── desktop/                   # Native Qt desktop application and desktop examples
├── docs/                      # Documentation, install guides and README/site assets
├── packages/                  # Reusable TypeScript packages, including MCP Core/Host
├── pages/                     # Browser extension pages and content scripts
├── scripts/
│   ├── install/               # Windows source/release installer helpers
│   └── shell/                 # Build, environment and version shell utilities
├── README.md                  # Project homepage
├── README.zh-CN.md            # Simplified Chinese homepage
├── CHANGELOG.md
├── SECURITY.md
└── LICENSE
```

## Security

Superpower executes user-configured MCP tools. Treat connected MCP servers and tool outputs as trusted only to the extent you trust their source and configuration.

- Review the MCP server configuration before connecting.
- Keep credentials out of public configuration files and issue reports.
- Prefer loopback-only local endpoints when remote access is unnecessary.
- Review guarded or state-changing actions before execution.

See [`SECURITY.md`](SECURITY.md) for the current security guidance.

## Contributing

Issues and pull requests are welcome. Please keep changes scoped, document behavior changes, and include validation where practical.

## Project discovery

Superpower is relevant to developers and researchers working with **Model Context Protocol (MCP)**, **AI agents**, **browser automation**, **Chrome extensions**, **agentic workflows**, **tool-using LLMs**, **local MCP servers**, **desktop AI workflows**, **Qt 6**, and **human-in-the-loop execution**.

Useful search terms: `MCP browser extension`, `Model Context Protocol Chrome extension`, `ChatGPT MCP tools`, `Gemini MCP`, `browser AI agent`, `agentic workflow`, `tool calling`, `MCP desktop client`, `MCP Streamable HTTP`, `human in the loop MCP`.

## License

Superpower is released under the [MIT License](LICENSE).
