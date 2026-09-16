# Superpower Desktop Guide

Superpower Desktop is the native control surface for MCP connections, reviewed actions, workflow runs, and the local browser conversation bridge.

## 1. Start the desktop app

Open the packaged Windows app or build `desktop/qt` from source. The Home workspace summarizes connection state and workflow activity. The Actions workspace is where MCP capabilities and reviewed execution are managed.

## 2. Connect an MCP server

Use the Connections area to configure the MCP endpoint you want the desktop client to use. For the standard local Superpower proxy, the recommended Streamable HTTP endpoint is:

```text
http://localhost:3006/mcp
```

The browser extension and the desktop app are separate MCP clients. They may point at the same MCP server, but each keeps its own client connection.

## 3. Use Conversation and Quick Ask

Open **View → Conversation** or press **Ctrl+Shift+C**. The Conversation panel mirrors supported ChatGPT browser conversations through the local loopback relay.

The **Quick Ask** bar at the bottom can submit a question from Superpower Desktop into the active ChatGPT browser tab:

1. Keep Superpower Desktop running.
2. Install or load the Superpower browser extension.
3. Open ChatGPT in a Chromium browser and keep the target conversation as the active tab.
4. Type a question in **Quick Ask** and press **Enter** or **Send**.
5. The prompt is routed locally to the active ChatGPT composer and submitted. The resulting browser conversation is mirrored back into the desktop Conversation panel.

The Quick Ask route uses the local conversation relay at `127.0.0.1:32148`. It is separate from the MCP endpoint at `localhost:3006/mcp`.

If no active ChatGPT tab accepts the prompt within 30 seconds, Desktop reports a delivery error instead of silently claiming that the prompt was sent.

## 4. Safe first test

Start with a read-only request, for example:

```text
List the files on my Desktop and summarize them. Do not modify, move, or delete anything.
```

If MCP tools are enabled in the browser workflow, review the generated tool call before allowing state-changing actions.

## 5. Actions and workflows

Superpower V1.5 keeps execution review-gated:

- **Action Router** maps a natural-language request to available MCP capabilities.
- **Workflow Planner** creates ordered multi-step plans with explicit bindings.
- **Workflow Review** lets you inspect parameters and policy decisions before execution.
- **Workflow Runner** advances one controlled step at a time.

Use the browser conversation for model reasoning when that is your preferred workspace; use Desktop for native connection management, review, run state, and local workflow control.

## 6. Troubleshooting

### Quick Ask says the relay is unavailable

Restart Superpower Desktop. The conversation relay starts with the desktop process and listens only on `127.0.0.1:32148`.

### Quick Ask times out

Confirm that ChatGPT is open in the browser, the Superpower extension is enabled, and the intended ChatGPT conversation is the active tab. Refresh ChatGPT after reloading a development build of the extension.

### MCP connects but Quick Ask does not

These are separate paths. MCP uses your configured MCP endpoint (for example `http://localhost:3006/mcp`), while Quick Ask uses the local browser conversation relay on port `32148`.

### Quick Ask works but tools do not

Check the MCP connection and Available Tools independently. A working conversation relay does not imply that an MCP server is connected.

## 7. Local development

For desktop development, build the Qt application and `packages/mcp-host` so the conversation relay is available. For browser development, load the built `dist/` directory from `chrome://extensions` with Developer mode enabled. After rebuilding the extension, reload the extension and refresh the ChatGPT page.

See also:

- [Browser extension guide](browser-extension.md)
- [`desktop/qt/README.md`](../../desktop/qt/README.md)
