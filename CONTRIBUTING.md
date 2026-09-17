# Contributing to Superpower

Thanks for helping improve Superpower. The project spans a browser extension, a native Qt desktop application, and reusable MCP packages, so keeping changes scoped and reviewable matters.

## Before you start

- Search existing issues and pull requests before opening a duplicate.
- Prefer one focused change per pull request.
- For behavior changes, describe the user-visible impact and the validation you performed.
- Do not include API keys, tokens, cookies, private conversations, `.env` files, or machine-specific secrets.
- Security-sensitive findings should follow [`SECURITY.md`](SECURITY.md) rather than being disclosed in a public issue.

## Good first contributions

Low-risk contributions are especially welcome in these areas:

- documentation and examples;
- translations and README synchronization;
- browser UI polish and accessibility;
- MCP tool search, filtering, favorites, and other local UI state;
- adapter compatibility fixes for supported AI websites;
- tests, fixtures, and CI reliability improvements;
- installation and troubleshooting guidance.

Changes to MCP execution, workflow policies, review gates, authentication, secrets, release automation, or updater behavior should be narrowly scoped and include validation because they affect the project's security boundary.

## Development setup

Requirements:

- Node.js 22.12 or newer;
- pnpm 9.x;
- a Chromium-based browser for extension testing;
- Qt 6.4 or newer plus CMake 3.21 or newer for desktop development.

Install dependencies:

```bash
pnpm install
```

Common commands:

```bash
pnpm build                  # Production browser-extension build
pnpm type-check             # Workspace TypeScript checks
pnpm lint                   # Lint workspace packages
pnpm e2e                    # Chrome end-to-end tests
pnpm e2e:firefox            # Firefox end-to-end tests
pnpm check:release-metadata # Verify release-version consistency
```

For the Qt desktop application:

```bash
cmake -S desktop/qt -B desktop/qt/build
cmake --build desktop/qt/build
```

The desktop version is derived from the root `package.json`. Do not hard-code a second product version in the Qt project or extension manifest.

## Browser adapter changes

When adding or modifying support for an AI website:

- keep site-specific DOM selectors inside the relevant adapter;
- avoid changing MCP Core unless the protocol behavior itself needs to change;
- test text insertion and submission separately;
- keep failure behavior non-destructive when the target website changes its DOM;
- document any newly required host permission;
- avoid broad permissions when a narrower hostname/path is sufficient.

## Pull requests

A useful pull request should include:

1. a concise explanation of the problem;
2. the implementation approach;
3. user-visible behavior changes;
4. tests or manual validation performed;
5. any permissions, privacy, security, or compatibility implications.

Keep formatting-only changes separate from functional changes when practical.

## Commit style

Use short imperative commit subjects where practical, for example:

```text
feat: add favorite MCP tools
fix: handle ChatGPT composer changes
chore: align release metadata
 docs: clarify local MCP setup
```

## Release-sensitive files

Changes to the following deserve extra review:

- `chrome-extension/manifest.ts` and `chrome-extension/manifest.js`;
- `packages/mcp-core/`;
- `packages/mcp-host/`;
- workflow execution and policy code;
- `.github/workflows/release.yml`;
- `desktop/qt/src/updatemanager.*`;
- authentication, credential, or remote-configuration code.

The root `package.json` is the product-version source of truth. CI verifies that the browser build and Qt desktop metadata remain aligned with it.

## Reporting problems

For ordinary bugs, include the Superpower version, browser/OS, affected AI website or MCP transport, reproduction steps, expected behavior, and observed behavior. For sensitive vulnerabilities, follow [`SECURITY.md`](SECURITY.md).
