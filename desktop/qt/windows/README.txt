Superpower Desktop for Windows (portable)
=========================================

This folder is self-contained for the Superpower desktop shell itself:

- Superpower Desktop.exe
- Qt 6 runtime files and Windows platform plugin
- runtime/node.exe
- bridge/superpower-host.mjs

Launch
------

Double-click "Superpower Desktop.exe".

The app automatically detects the bundled Node runtime and MCP host bridge. No Qt installation is required.

Connections
-----------

1. Streamable HTTP
   Select "Streamable HTTP" and enter an http:// or https:// MCP endpoint.

2. Local stdio
   Select "Local stdio", enter the MCP server command, and provide its arguments as a JSON string array.

   Example command:
     npx

   Example arguments:
     ["@modelcontextprotocol/server-filesystem", "."]

Important: the portable bundle includes Node itself but does not include arbitrary third-party MCP servers such as npx packages, Python packages, or local executables. Those server commands must already be available on the user's machine or be addressed by an absolute path.

Security
--------

Guarded mode is the default. High/critical tool calls require explicit confirmation before execution.

The desktop shell does not persist raw endpoint credentials, tool arguments, or secrets. Local stdio servers inherit the host process environment; do not put secrets directly into the visible arguments field when an environment-based configuration is available.

This is a portable preview build. Code signing and an installer are separate release-engineering steps.
