# Superpower Windows Extension Installation

## Quick installation

1. Download the latest Superpower release package.
2. Extract the ZIP file.
3. Double-click `Install-Superpower.cmd`.
4. Wait for the installer to prepare the extension.
5. Open `chrome://extensions/`.
6. Enable **Developer mode**.
7. Select **Load unpacked**.
8. Choose the generated `dist/` folder.

## MCP connection

Superpower connects to MCP servers through the configured proxy.

Default local endpoint:

```text
http://localhost:3006/sse
```

## Developer installation

For source development:

```bash
pnpm install
pnpm build
```

The generated extension is located in `dist/`.

The Windows helper scripts live in `scripts/install/` in the source repository. Release packages copy them to the package root so users can still launch `Install-Superpower.cmd` directly after extraction.
