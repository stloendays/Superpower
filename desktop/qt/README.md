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
MCP Gateway / Tool Router / Guarded Policy / Telemetry
        |
        +--> Streamable HTTP MCP server
        +--> stdio MCP server (host capability; desktop UI support follows)
```

The first desktop UI supports Streamable HTTP endpoints. The host bridge itself already supports the same HTTP and stdio connection primitives as the CLI.

## Current capabilities

- Persistent MCP session instead of reconnecting for each tool call.
- Routed tool catalog using the existing local Tool Router and context budget.
- Tool search, input-schema viewer, and generated JSON argument template.
- Direct MCP tool execution with formatted JSON output.
- `audit` and `guarded` execution modes.
- Native confirmation dialog for high/critical guarded actions.
- Privacy-safe session telemetry summary.
- No credential persistence in the Qt shell.

## Prerequisites

- Node.js matching the repository `.nvmrc`.
- pnpm.
- CMake 3.21+.
- Qt 6.5+ with the Widgets module.
- A C++20 compiler (MSVC 2022, Clang, or GCC).

## 1. Build the MCP host

From the repository root:

```bash
pnpm install --frozen-lockfile
pnpm -F @superpower/mcp-host build
```

This produces `packages/mcp-host/dist/cli.js`, which also exposes the new `bridge` command.

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

## 3. Run

Start `Superpower Desktop`, confirm the detected Node executable and host script, enter an `http://` or `https://` MCP endpoint, then select **Connect**.

The desktop client defaults to `guarded` mode. When a tool is classified as high/critical risk, the existing MCP execution policy blocks the call and the Qt shell asks for explicit approval before retrying it.

## Security notes

- Do not embed long-lived credentials in endpoint URLs.
- The Qt client does not save endpoint, argument, or credential fields to disk.
- The JSON bridge emits policy metadata and tool results, but never introduces a second policy implementation.
- Authenticated server configuration should continue to use the host's environment-reference model; a dedicated secret-aware desktop configuration UI is planned rather than storing raw tokens.
