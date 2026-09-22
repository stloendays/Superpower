# Changelog

This changelog tracks published Superpower releases and notable work on the active development line.

## Unreleased - V1.5 development

### Added

- Added a browser-first **Copilot** workspace on supported AI sites, with page/selection-aware prompts submitted through the current site's AI composer.
- Added one-click Summarize, Explain, Improve writing, Translate, Action items, and Study notes workflows.
- Added local **Knowledge** capture for selected text with source metadata, local persistence, copy/remove controls, and Markdown export.
- Added Knowledge search across title/text/URL/tags, source-type filters, editable tags, multi-select, Ask attachments, multi-source synthesis, and selected-item Markdown export.
- Added review-first Knowledge handoff to compatible notes/file MCP tools such as Notion, Drive, Dropbox, or similar connected capabilities.
- Added **Knowledge Projects** with persistent non-destructive source grouping, Project filtering, bulk add/unlink, whole-Project Ask attachment, synthesis, Markdown export, and MCP handoff.
- Added a `Ctrl/⌘ + Shift + K` shortcut to surface the Copilot workspace on supported AI pages.
- Added dynamic MCP action suggestions inside Copilot based on the currently connected tool set.
- Added a lightweight Page Assistant on ordinary HTTP(S) pages for selection/page summarization, explanation, AI handoff, action extraction, and Knowledge capture.
- Added cross-tab prompt routing into the most recently used supported AI workspace, with ChatGPT fallback when no supported AI tab is available.
- Added YouTube watch-page summarization and Save to Knowledge using loaded transcript segments when available, with metadata-only fallback when transcript text is unavailable.
- Added Connected Actions for detected notes, mail, calendar, task, messaging, and file-storage MCP tools while preserving review-first execution.
- Added a per-user Windows Setup installer for Superpower Desktop.
- Added installer-based automatic update support for managed Desktop installations.
- Added a Simplified Chinese README and an English/Chinese language switcher on the repository homepage.

### Changed

- Hardened MCP connection liveness with protocol-level ping checks, heartbeat-driven recovery, browser online/foreground recovery, bounded exponential reconnect backoff, and tool refresh after reconnect.
- Fixed same-transport endpoint switching so changing the MCP URI no longer reuses an unrelated existing session.
- Made recovery reset clear stale MCP client/transport state instead of acting as a no-op.
- Tool discovery failures now surface as failures instead of silently appearing as a connected server with zero tools.
- Added explicit Auto Execute, Auto Insert, and Auto Submit controls with synchronized preferences and configurable delays.
- Auto Execute now performs an MCP connection preflight and resumes after a successful reconnect, while never automatically replaying a tool request once it may have reached the server.
- Made **Streamable HTTP** the preferred local MCP transport for new setups.
- Standardized the default local MCP endpoint as `http://localhost:3006/mcp` across the extension background client, MCP client defaults, and content-side connection state.
- Updated HTTP transport auto-detection so explicit `/sse` endpoints remain SSE while other HTTP(S) MCP endpoints default to Streamable HTTP.
- Updated the README connection guidance to match the current local MCP defaults.

### Fixed

- Synced Knowledge and Projects across open extension surfaces in real time, and pruned stale Project references when saved Knowledge is removed or evicted by the local cap.
- Shared generated MCP instructions across content bundles so the MCP popover can immediately reuse instructions already generated in the sidebar.
- Preserved Instruction Manager styling while fixing cross-bundle instruction state synchronization.
- Aligned the Streamable HTTP migration path with the current connection defaults.

## 1.4.1 - 2026-09-16

- Added first-run onboarding for the native Desktop app.
- Added the Workspace Home health dashboard.
- Added the natural-language Action Router.
- Hardened the Action Router review flow.
- Added the review-first Workflow Planner with explicit multi-step bindings.
- Added the review-gated Workflow Runner with guarded, one-step-at-a-time execution.
- Refreshed the GitHub README homepage visuals and cleaned up the repository entry surface.
- Published browser and Desktop release packages for v1.4.1.

See the published release: [Superpower v1.4.1](https://github.com/stloendays/Superpower/releases/tag/v1.4.1).

## 1.4.0 - 2026-09-15

- Added the native **Qt 6** Superpower Desktop MCP client.
- Added Desktop stdio configuration and a portable Windows package.
- Added the Desktop workspace model: **Connections → Apps → Actions → Runs**.
- Added live ChatGPT conversation sync for the Desktop app, independent from the Desktop MCP server connection.
- Added browser-first MCP setup and automatic tool routing work that had been developed under the MCP v1.3 line.
- Hardened compact MCP schema instructions and made tool-router fallback behavior more conservative.
- Hardened MCP connection secret handling and normalized guarded execution policy matching.
- Cleaned up content-script TypeScript and modernized GitHub Actions runtimes.
- Refreshed project documentation for the Oxford × NUS collaboration.
- Added the Tony status display panel contribution.
- Published browser and Desktop Windows packages with checksums and installer entry points.

See the published release: [Superpower v1.4.0](https://github.com/stloendays/Superpower/releases/tag/v1.4.0).

## Intermediate development milestones

The repository used **1.2.0** and **1.3.0** as intermediate development/release-preparation targets, but no `v1.2.0` or `v1.3.0` Git tags or GitHub Releases were published. Their browser-first setup, routing, release-preparation, security, and product metadata work was subsequently incorporated into the v1.4.0 release line.

## 1.1.0 - 2026-08-25

- Added a Windows DIY installer bootstrap for easier first-time setup.
- Added a double-click launcher for Windows users.
- Added automatic environment checks for Node.js and pnpm during source installation.
- Added release-oriented installation flow preparation.
- Improved the transition from developer setup to user-facing installation.
- Added automated GitHub Release packaging with a Windows ZIP, checksum, installer entry points, and generated release notes.

The original GitHub release used the `V1` tag and packaged the v1.1.0 release-ready ZIP: [V1 release](https://github.com/stloendays/Superpower/releases/tag/V1).

## 1.0.0 - 2026-08-24

- Rebranded the extension as Superpower.
- Added a new minimalist black-and-white Superpower icon.
- Set the Chrome extension release version to 1.0.0 with display version V1.
- Updated MCP instruction generation for dependency-aware tool-call batching and separate function-call blocks.
- Added ChatGPT MCP-file submission refresh handling.
- Added release-oriented repository cleanup, environment templates, attribution, and security guidance.

[Unreleased]: https://github.com/stloendays/Superpower/compare/v1.4.1...main
[1.4.1]: https://github.com/stloendays/Superpower/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/stloendays/Superpower/compare/V1...v1.4.0
