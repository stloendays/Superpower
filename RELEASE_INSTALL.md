# Superpower Release Installation Guide

## Quick Installation

1. Download the latest `Superpower-<version>-Windows.zip` release package.
2. Extract the ZIP file.
3. Double-click:

```text
Install-Superpower.cmd
```

4. Let the installer prepare the extension files.
5. Open Chrome and visit:

```text
chrome://extensions/
```

6. Enable **Developer mode**.
7. Select **Load unpacked**.
8. Choose the generated `dist` folder.

## Connect an MCP server

Open a supported AI web app, open the Superpower sidebar, and use **Quick Connect**.

Normal setup is:

**Paste MCP server address or configuration → Connect → Ask normally**

Superpower accepts browser-accessible `http://`, `https://`, `ws://`, and `wss://` MCP endpoints. It can also recognize common JSON configurations containing `mcpServers` or `servers` and extract a remote endpoint locally.

Transport selection is automatic for WebSocket, SSE, and Streamable HTTP. Manual transport controls remain available under **Advanced**.

After connection, Superpower reads the current AI composer locally and selects relevant MCP tools automatically. You do not need to maintain a normal-path Task Focus field or manually inject every connected tool into context.

### Local stdio MCP servers

A browser extension cannot directly launch local `command` / `args` stdio processes. For those servers, use the optional **Superpower Host** or expose the server through a browser-accessible MCP endpoint.

Example host usage from a source checkout:

```bash
pnpm install
pnpm -F @superpower/mcp-host build
node packages/mcp-host/dist/cli.js connect --stdio node --server-arg server.js
```

### Authentication safety

Quick Connect can recognize remote endpoints in pasted configurations, but it does not silently import authentication material into Recent connections. URLs containing credentials or sensitive token/key/secret/auth/signature-style parameters are excluded from Recent storage.

For authenticated servers, configure credentials through the server/host flow appropriate to that provider rather than committing secrets to this repository.

## Developer installation

For source development:

```bash
pnpm install
pnpm build
```

The generated extension is located in:

```text
dist/
```

Run the MCP quality checks through the repository workflows before preparing a release. The release workflow verifies that the built extension manifest version matches the root `package.json` version.
