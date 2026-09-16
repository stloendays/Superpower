# Superpower Desktop Guide

Superpower Desktop is the native control surface for MCP connections, reviewed actions, workflow runs, and the local browser AI conversation bridge.

## 1. Start the desktop app

Open the packaged Windows app or build `desktop/qt` from source. The Home workspace summarizes connection state and workflow activity. The Actions workspace is where MCP capabilities and reviewed execution are managed.

## 2. Connect an MCP server

Use the Connections area to configure the MCP endpoint you want the desktop client to use. For the standard local Superpower proxy, the recommended Streamable HTTP endpoint is:

```text
http://localhost:3006/mcp
```

The browser extension and the desktop app are separate MCP clients. They may point at the same MCP server, but each keeps its own client connection.

## 3. Use Conversation and Quick Ask

Open **View → Conversation** or press **Ctrl+Shift+C**. The Conversation panel contains the Desktop **AI Provider** bar and **Quick Ask** composer.

Quick Ask currently supports these browser providers:

| Provider | Desktop prompt routing | Live transcript mirroring |
| --- | --- | --- |
| ChatGPT | Yes | Yes |
| Gemini | Yes | Not yet |
| Grok | Yes | Not yet |
| Perplexity | Yes | Not yet |

The provider selector offers **Auto · active supported tab** plus explicit ChatGPT, Gemini, Grok, and Perplexity targets. Auto is the safest default: only the supported AI tab the user is actively viewing can claim the prompt.

To send a prompt:

1. Keep Superpower Desktop running.
2. Install or load the Superpower browser extension.
3. Open ChatGPT, Gemini, Grok, or Perplexity in a Chromium browser and keep the intended conversation as the active tab.
4. Open **View → Conversation** in Desktop.
5. Leave **AI Provider** on **Auto** or choose the provider explicitly.
6. Type a question in **Quick Ask** and press **Enter** or **Send**.
7. Desktop queues the prompt locally, the active matching browser tab claims it, and the extension inserts and submits it using that site's existing input adapter.

The provider-status line in Desktop reports which supported browser AI is currently being detected by the local relay. A specifically selected provider will not be sent to a different provider tab.

ChatGPT additionally exposes stable role markers that Superpower uses for live transcript mirroring. For Gemini, Grok, and Perplexity this release deliberately keeps the Desktop bridge send-only rather than relying on fragile guessed transcript selectors.

The Quick Ask route uses the local conversation relay at `127.0.0.1:32148`. It is separate from the MCP endpoint at `localhost:3006/mcp`.

If no active matching provider tab accepts the prompt within 30 seconds, Desktop reports a delivery error instead of silently claiming that the prompt was sent.

## 4. Safe first test

For a transport-only check, use a prompt that does not require tools, for example:

```text
Reply with exactly: Superpower provider bridge OK
```

Then try the same prompt with **Auto**, followed by a specific provider target. The browser tab should receive exactly one prompt.

For an MCP workflow test, start with a read-only request, for example:

```text
List the files on my Desktop and summarize them. Do not modify, move, or delete anything.
```

If MCP tools are enabled in the browser workflow, review generated tool calls before allowing state-changing actions.

## 5. Actions and workflows

Superpower V1.5 keeps execution review-gated:

- **Action Router** maps a natural-language request to available MCP capabilities.
- **Workflow Planner** creates ordered multi-step plans with explicit bindings.
- **Workflow Review** lets you inspect parameters and policy decisions before execution.
- **Workflow Runner** advances one controlled step at a time.

Use the browser AI for reasoning when that is your preferred workspace; use Desktop for native connection management, review, run state, provider routing, and local workflow control.

## 6. Troubleshooting

### Quick Ask says the relay is unavailable

Restart Superpower Desktop. The conversation relay starts with the desktop process and listens only on `127.0.0.1:32148`.

### Desktop says no active provider is detected

Open a supported provider page and keep that tab active. Current provider detection covers ChatGPT, Gemini, Grok, and Perplexity. Reload the page after reloading a development build of the extension.

### Quick Ask times out

Confirm that the selected provider matches the active browser tab. If **AI Provider** is set to Gemini while ChatGPT is the active tab, the prompt intentionally remains queued until Gemini becomes active or the 30-second timeout is reached.

### A provider receives text but does not submit

The provider may have changed its composer DOM. Superpower reuses each site's existing input adapter; report the provider and page URL so the adapter selector can be updated. Desktop will report that insertion or automatic submission failed rather than claiming success.

### ChatGPT mirrors replies but the other providers do not

That is expected in this release. ChatGPT transcript mirroring is enabled; Gemini, Grok, and Perplexity are currently Quick Ask send targets only.

### MCP connects but Quick Ask does not

These are separate paths. MCP uses your configured MCP endpoint (for example `http://localhost:3006/mcp`), while Quick Ask uses the local browser conversation relay on port `32148`.

### Quick Ask works but tools do not

Check the MCP connection and Available Tools independently. A working conversation relay does not imply that an MCP server is connected.

## 7. Local development

For desktop development, build the Qt application and `packages/mcp-host` so the conversation relay is available. For browser development, load the built `dist/` directory from `chrome://extensions` with Developer mode enabled. After rebuilding the extension, reload the extension and refresh the provider page you are testing.

For a full smoke test, verify:

- Auto routes only to the currently active supported provider tab.
- Explicit provider targeting does not leak to a different provider.
- ChatGPT still mirrors user and assistant transcript updates into Desktop.
- Gemini, Grok, and Perplexity can receive a Desktop prompt through their existing input handlers.
- A missing or mismatched active provider produces a timeout instead of a false success.

See also:

- [Browser extension guide](browser-extension.md)
- [`desktop/qt/README.md`](../../desktop/qt/README.md)
