# Changelog

## 1.3.0 - 2026-09-15

- Added browser-first **Quick Connect** for MCP URLs and common JSON configurations, with automatic WebSocket, SSE, and Streamable HTTP transport selection.
- Added safe Recent connections with secret-aware URL/config filtering and bounded transport-mismatch repair.
- Added automatic local Tool Router selection from the current AI composer, replacing the normal-path Task Focus workflow.
- Added context-budgeted MCP instruction generation with compact schemas, bounded parameter semantics, and safe schema fallback.
- Removed the previous 500 ms instruction-regeneration polling loop in favor of state-driven updates.
- Added standalone `@superpower/mcp-core` and optional `@superpower/mcp-host` packages for routing, budgeting, policy, telemetry, local stdio, and browserless workflows.
- Hardened guarded execution policy matching across snake_case, camelCase, punctuation, and natural-language descriptions.
- Added deterministic router and execution-policy smoke tests to the MCP Quality Gate.
- Made the content-script TypeScript baseline fully clean and replaced error-delta CI with a strict type-check.
- Modernized GitHub Actions runtimes and kept Chrome/Firefox E2E, extension build, MCP core/host, and formatting checks green.

## 1.1.0 - 2026-08-25

- Added a Windows DIY installer bootstrap for easier first-time setup.
- Added a double-click launcher for Windows users.
- Added automatic environment checks for Node.js and pnpm during source installation.
- Added release-oriented installation flow preparation.
- Improved the transition from developer setup to user-facing installation.
- Added automated GitHub Release packaging with a Windows ZIP, checksum, installer entry points, and generated release notes.

## 1.0.0 - 2026-08-24

- Rebranded the extension as Superpower.
- Added a new minimalist black-and-white Superpower icon.
- Set the Chrome extension release version to 1.0.0 with display version V1.
- Updated MCP instruction generation for dependency-aware tool-call batching and separate function-call blocks.
- Added ChatGPT MCP-file submission refresh handling.
- Added release-oriented repository cleanup, environment templates, attribution, and security guidance.
