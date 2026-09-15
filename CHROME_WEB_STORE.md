# Chrome Web Store release guide

This document describes the release path and store-facing presentation for the Superpower Chrome extension.

## Brand identity

Superpower uses the existing **S icon** shipped with the extension as its primary visual identity:

- `chrome-extension/public/icon-16.png`
- `chrome-extension/public/icon-34.png`
- `chrome-extension/public/icon-128.png`

These assets should be used consistently across the Chrome toolbar, extension management page, GitHub documentation, release materials, and Chrome Web Store listing. Supporting visuals should follow a clean, restrained, product-focused system that emphasizes the Superpower interface, MCP connectivity, and practical AI tool workflows.

## Store listing copy

### Product name

**Superpower**

### Short description

> Connect supported web AI assistants to MCP servers and tools, then run real workflows without leaving the conversation.

### Positioning line

> MCP tools, directly inside the AI interfaces you already use.

### Detailed description

Superpower brings Model Context Protocol (MCP) tools into supported AI web applications.

Connect an MCP server, choose the tools you want available, and let the assistant call those tools directly from the conversation. Superpower handles tool discovery, structured tool calls, execution, and result insertion so you can keep working in the same AI interface instead of moving between separate apps.

Superpower supports MCP workflows over SSE, Streamable HTTP, and WebSocket transports. It is designed for practical tool use such as files, developer workflows, research utilities, databases, custom services, and agent-style task execution.

The extension works as the browser interaction layer. Tool execution remains behind the MCP endpoint configured by the user, and users retain control over the connected server and enabled tools.

### Store feature bullets

- Connect supported AI web apps to MCP servers.
- Discover and manage available MCP tools.
- Run structured tool calls without leaving the conversation.
- Insert tool results back into the active AI workflow.
- Support SSE, Streamable HTTP, and WebSocket transports.
- Keep MCP execution behind a user-configured endpoint.
- Use the same Superpower workflow across multiple supported AI interfaces.

## Product scope

Superpower is a Manifest V3 browser extension that connects supported AI web applications to MCP tools exposed by a configured local proxy. The browser extension is the interaction layer; MCP execution remains in the configured MCP server/proxy.

The current store build is intentionally optimized for local MCP endpoints on `localhost` or `127.0.0.1`. Remote MCP deployments should use HTTPS and should be granted only the specific host access they require rather than broad access to all websites.

## Build an upload package

Run:

```bash
pnpm install --frozen-lockfile
pnpm base-build
cd dist
zip -r ../superpower-chrome-1.2.0.zip .
```

Alternatively, run the **Package Chrome Web Store Extension** GitHub Actions workflow. It produces an upload-ready ZIP artifact.

## Chrome Web Store dashboard

Before the first public submission:

1. Create a new extension item in the Chrome Web Store Developer Dashboard.
2. Upload the generated `superpower-chrome-<version>.zip` package.
3. Complete the Store Listing and Privacy sections.
4. Use the established Superpower brand assets and product-focused screenshots.
5. Describe the extension's single purpose as connecting supported AI web interfaces to user-configured MCP tools.
6. Review the requested host permissions and data disclosures before submitting for review.

A new package must increment the extension version before an update can be uploaded.

## Permission rationale

### `storage`

Stores extension preferences, selected MCP endpoint/transport, tool enablement state, and UI settings.

### `clipboardWrite`

Supports explicit copy actions from the extension UI.

### AI website host permissions

Required for the content-script adapters that detect MCP tool calls and insert MCP tool results into the supported AI web applications.

### `localhost` and `127.0.0.1`

Required for the extension service worker to communicate with the local MCP proxy. The default endpoints are:

- SSE: `http://localhost:3006/sse`
- Streamable HTTP: `http://localhost:3006/mcp`
- WebSocket: `ws://localhost:3006/message`

Do not replace these with blanket `http://*/*` or `https://*/*` host permissions solely for convenience.

## Privacy and security review

Before submission, verify the final build against the Chrome Web Store Privacy questionnaire. Superpower can process conversation content needed to identify MCP function calls and can send tool names/arguments to the MCP endpoint configured by the user. Tool results can be inserted back into the active AI conversation.

The repository also contains optional analytics support. If analytics is enabled in a production build, the Store privacy disclosures and public privacy policy must accurately describe the telemetry that is collected. If analytics is not required for the first public release, keep the analytics credentials unset so event transmission remains disabled.

Never commit MCP credentials, API keys, access tokens, private machine paths, or Chrome Web Store API credentials to the repository.

## Publishing automation

The current workflow deliberately stops at producing the upload ZIP. Chrome Web Store API publishing can be added after the Store item exists and its publisher/extension identifiers and Google Cloud authorization are configured as repository secrets.

Keep Store publishing credentials in GitHub Actions secrets. Do not embed them in the extension package.

## Upstream attribution

Superpower is a modified derivative of MCP SuperAssistant. Preserve `LICENSE` and `NOTICE.md` when publishing source or release packages.
