# Superpower Desktop (Qt 6 / C++20)

Superpower Desktop is the native shell for the browser-first MCP stack. It deliberately reuses the existing `@superpower/mcp-host` gateway instead of reimplementing MCP routing, context budgeting, execution policy, telemetry, or transport logic in C++.

## Architecture

```text
Qt 6 Desktop UI (C++20)
        |
        | newline-delimited JSON over QProcess stdin/stdout
        v
@superpower/mcp-host bridge (Node.js)
        |
        v
MCP Gateway / Tool Router / Action Planner / Guarded Policy / Telemetry
        |
        +--> Streamable HTTP MCP server
        +--> local stdio MCP server
```

## Current capabilities

- Persistent MCP session instead of reconnecting for each tool call.
- Streamable HTTP endpoints and local stdio MCP server processes.
- Routed tool catalog using the existing local Tool Router and context budget.
- Workspace Home with MCP health, Browser Conversation relay health, Apps, Actions, Runs, and recent activity.
- Natural-language Action Router that ranks the full MCP catalog, drafts schema-backed parameters, and opens the selected Action for review.
- Router review shows confidence, matched terms, missing required fields, alternative candidates, and execution-policy risk before any call is made.
- Tool search, input-schema viewer, and generated JSON argument template.
- Direct MCP tool execution with formatted JSON output.
- `audit` and `guarded` execution modes.
- Native confirmation dialog for high/critical guarded actions.
- Privacy-safe session telemetry summary.
- No credential persistence in the Qt shell.

## Action Router

Connect an MCP server, return to **Home**, and enter a request such as:

```text
Find issue #29 in stloendays/Superpower-V1
```

or:

```text
给 paula@example.com 发邮件，主题 "Project update"，内容 "The latest build is ready for review"
```

The Desktop sends the request to the existing MCP Host `plan` method. MCP Core performs deterministic zero-model routing against the full tool catalog, drafts only schema values that can be inferred conservatively, evaluates the selected tool with the existing execution policy, and returns a proposal.

The proposal is then opened in **Actions**. Nothing executes automatically. Missing required fields remain for the user to complete, and high/critical actions still pass through the existing guarded confirmation flow when **Run reviewed action** is pressed.

If multiple Action Router requests are submitted quickly, only the newest plan is allowed to update the review workspace; stale responses are ignored.

## Prerequisites for source builds

- Node.js matching the repository `.nvmrc`.
- pnpm.
- CMake 3.21+.
- Qt 6.4+ with the Widgets module.
- A C++20 compiler (MSVC 2022, Clang, or GCC).

## 1. Build the MCP host

From the repository root:

```bash
pnpm install --frozen-lockfile
pnpm -F @superpower/mcp-host build
```

This produces `packages/mcp-host/dist/cli.js`, whose `bridge` command supports both Streamable HTTP and stdio connections.

## 2. Build the desktop client

### Windows / Qt Online Installer

Adjust the Qt path to your installed kit:

```powershell
cmake -S desktop/qt -B desktop/qt/build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build desktop/qt/build --config Release
```

### Linux

With Qt 6 development packages installed:

```bash
cmake -S desktop/qt -B desktop/qt/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build desktop/qt/build
```

## 3. Connect

Start `Superpower Desktop`. The source build searches for Node on `PATH` and for `packages/mcp-host/dist/cli.js` in the repository.

For **Streamable HTTP**, enter an `http://` or `https://` MCP endpoint.

For **Local stdio**, enter a server command plus arguments as a JSON array of strings. For example:

```text
Command: npx
Arguments: ["@modelcontextprotocol/server-filesystem", "."]
```

The desktop client defaults to `guarded` mode. When a tool is classified as high/critical risk, the existing MCP execution policy blocks the call and the Qt shell asks for explicit approval before retrying it.

## Windows portable package

The `Desktop Qt Build` GitHub Actions workflow validates a Windows MSVC/Qt build and produces `Superpower-Desktop-Windows-x64.zip`.

The portable package contains:

- `Superpower Desktop.exe`
- Qt runtime DLLs and `qwindows.dll`
- the complete portable Node distribution under `runtime/`, including `node`, `npm`, and `npx`
- a single-file bundled MCP host under `bridge/`

The packaged app auto-detects those bundled runtimes and prepends the Node runtime directory to the MCP bridge `PATH`. End users therefore do not need to install Qt or Node, and common `npx ...` stdio MCP configurations can use the bundled npx command. Third-party MCP packages themselves are not pre-bundled.

## Security notes

- Action Router planning is review-first and never auto-executes a selected tool.
- Action queries and drafted parameters are session-oriented in the Desktop UI and are not added to profile persistence.
- Do not embed long-lived credentials in endpoint URLs or visible stdio arguments.
- The Qt client does not save endpoint, argument, or credential fields to disk.
- The JSON bridge emits policy metadata and tool results, but never introduces a second policy implementation.
- Local stdio servers inherit the MCP host process environment.
- Authenticated remote-server configuration should continue to use the host's environment-reference model; a dedicated secret-aware desktop configuration UI is preferable to storing raw tokens.
- The portable build is not code-signed yet; code signing and an installer are release-engineering follow-ups.
