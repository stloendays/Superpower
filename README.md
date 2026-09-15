<div align="center">
  <img src="chrome-extension/public/icon-128.png" alt="Superpower" width="88" height="88" />

  <h1>Superpower</h1>

  <p><strong>One MCP layer for browser AI and native desktop workflows.</strong></p>

  <p>
    <a href="https://github.com/stloendays/Superpower-V1/stargazers"><img src="https://img.shields.io/github/stars/stloendays/Superpower-V1?style=flat-square&logo=github&label=Stars" alt="GitHub stars" /></a>
    <img src="https://img.shields.io/badge/version-1.5-111827?style=flat-square" alt="Version 1.5" />
    <img src="https://img.shields.io/badge/Protocol-MCP-4F46E5?style=flat-square" alt="Model Context Protocol" />
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-16A34A?style=flat-square" alt="MIT License" /></a>
  </p>

  <p>
    <a href="#products">Products</a> ·
    <a href="#why-superpower">Why Superpower</a> ·
    <a href="#features">Features</a> ·
    <a href="#supported-platforms">Platforms</a> ·
    <a href="#how-it-works">Architecture</a> ·
    <a href="#quick-start">Quick start</a>
  </p>
</div>

<p align="center">
  <img src="docs/readme/hero.svg" alt="Superpower Browser Extension and Desktop App" width="100%" />
</p>

<div align="center">
  <strong>Oxford × NUS collaboration · led by Tony</strong><br/>
  <sub>Practical MCP workflows across browser and desktop surfaces.</sub>
</div>

<br/>

## Products

<table>
<tr>
<td width="50%" valign="top">

### Browser Extension

Bring MCP tools directly into supported AI websites without leaving the conversation.

- Works inside ChatGPT, Gemini, Perplexity, Grok, Qwen and more
- Detects structured tool calls and returns results to the active chat
- Supports SSE, Streamable HTTP and WebSocket MCP connections
- Includes tool visibility, automation and review controls

**Best for:** browser-native AI workflows.

</td>
<td width="50%" valign="top">

### Desktop App

Use Superpower as a native MCP workspace outside the browser.

- Native **Qt 6** Windows application
- Organizes **Connections → Apps → Actions → Runs**
- Provides a standalone MCP client and review surface
- Distributed as a portable Windows package in GitHub Releases

**Best for:** managing MCP connections and actions from one desktop workspace.

</td>
</tr>
</table>

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

<p align="center">
  <img src="docs/readme/product-overview.svg" alt="Superpower product overview" width="100%" />
</p>

## Features

- Shared MCP execution model across browser and desktop surfaces
- Browser-side tool discovery, structured call detection and result injection
- Native desktop workspace for connections, apps, actions and runs
- Manual and automated execution flows
- Local and remote MCP endpoints
- Multi-tool and dependency-aware workflows
- Persistent controls and generated MCP instructions

<table>
<tr>
<td width="50%" align="center" valign="top">
  <img src="docs/readme/sidebar-overview.svg" alt="Superpower browser sidebar overview" width="100%" />
  <br /><sub>Browser extension: connections, tools and automation controls.</sub>
</td>
<td width="50%" align="center" valign="top">
  <img src="docs/readme/tool-flow.svg" alt="Superpower MCP tool execution flow" width="100%" />
  <br /><sub>Structured tool calls routed through MCP and returned to the workflow.</sub>
</td>
</tr>
</table>

### Natural-language Action Router

The V1.5 development line adds a review-first Action Router that maps natural-language intent to MCP capabilities, drafts schema-backed parameters, surfaces risk, and sends the action through an explicit review step before execution.

<p align="center">
  <img src="docs/readme/action-router.svg" alt="Superpower natural-language Action Router" width="100%" />
</p>

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

1. Download the [latest release](https://github.com/stloendays/Superpower-V1/releases/latest).
2. Extract it and run `Install-Superpower.cmd`.
3. Open `chrome://extensions/` and enable **Developer mode**.
4. Select **Load unpacked** and choose the generated `dist/` folder.

See [`RELEASE_INSTALL.md`](RELEASE_INSTALL.md) for the full extension installation guide.

### Desktop App

1. Open the [latest release](https://github.com/stloendays/Superpower-V1/releases/latest).
2. Download `Superpower-Desktop-*-Windows-x64.zip`.
3. Extract the archive and launch the packaged desktop application.

### Build the browser extension from source

Requirements: **Node.js 22.12+**, **pnpm 9.x**, and a Chromium-based browser.

```bash
git clone https://github.com/stloendays/Superpower-V1.git
cd Superpower-V1
pnpm install
pnpm base-build
```

Configure an MCP proxy, start it with your preferred transport, then load `dist/` as an unpacked extension.

## Development

```bash
pnpm dev          # Development build
pnpm base-build   # Production build
pnpm type-check   # Type checking
pnpm lint         # Lint
```

## Project

Superpower is an **Oxford × NUS collaborative project led by Tony**, focused on practical human–AI workflows and MCP-based tool use across browser and desktop environments.

Superpower V1 is a modified derivative of **MCP SuperAssistant**. The original MIT license and upstream attribution are preserved in [`LICENSE`](LICENSE) and [`NOTICE.md`](NOTICE.md).

## Security

MCP servers may expose filesystem, database, developer-tool, or third-party API access. Only connect endpoints you trust and keep credentials outside the repository. See [`SECURITY.md`](SECURITY.md).

## Contributing

Issues and pull requests are welcome. For bug reports, include the affected surface (browser or desktop), platform/browser version, and reproduction steps.

## License

Released under the [MIT License](LICENSE), with upstream attribution described in [NOTICE.md](NOTICE.md).
