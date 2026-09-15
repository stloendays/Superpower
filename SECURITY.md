# Security Policy

Superpower can invoke Model Context Protocol (MCP) tools that may access local files, processes, shells, networks, cloud services, or other privileged resources depending on the MCP server connected by the user. This document describes the project's security expectations and responsible vulnerability-reporting process.

## Recommended deployment

- Connect only to MCP servers you trust.
- Prefer loopback or another explicitly trusted interface for local proxy services.
- Do not expose an unauthenticated MCP proxy directly to the public Internet.
- Review MCP server permissions and tool capabilities before enabling automatic execution.
- Use encrypted transports such as HTTPS or WSS where appropriate.
- Keep credentials, tokens, private keys, `.env` files, and machine-specific configuration out of Git.
- Disable or remove MCP servers that are no longer needed.
- Keep Superpower, its dependencies, and connected MCP software up to date.

## Issues in scope

Examples of security issues that are especially useful to report include:

- unauthorized MCP tool execution;
- credential, token, API key, or secret exposure caused by Superpower;
- unsafe extension message passing or cross-origin behavior;
- extension permission misuse;
- injection vulnerabilities in Superpower-controlled UI or tool-result rendering;
- unsafe remote-configuration handling that can lead to code execution;
- sensitive data being transmitted to an unexpected destination; and
- exploitable dependency vulnerabilities that materially affect Superpower users.

## Third-party systems

Vulnerabilities that exist only in a third-party AI website, MCP server, model provider, API, or external tool are generally outside this project's scope unless Superpower introduces or materially amplifies the vulnerability.

Users are responsible for selecting and trusting the MCP servers and tools they connect. Third-party MCP services may define their own authentication, authorization, logging, data-retention, and execution policies.

## Reporting a vulnerability

Please do **not** publish credentials, private user data, or actionable exploit details in a public GitHub issue.

For potentially sensitive vulnerabilities, use the private publisher contact information provided in the Chrome Web Store listing. If private security reporting is available through this GitHub repository, that channel may also be used.

A useful report should include:

- affected Superpower version;
- browser and operating system;
- affected supported website or MCP transport, if relevant;
- clear reproduction steps;
- expected and observed behavior;
- security impact;
- a minimal proof of concept when safe to provide; and
- any proposed mitigation or additional context.

Do not include live API keys, passwords, session cookies, private conversations, or other secrets.

## Coordinated disclosure

The project will make a reasonable effort to validate credible reports, determine scope, develop an appropriate remediation, and coordinate disclosure for confirmed vulnerabilities. Public disclosure should avoid unnecessarily exposing users before a practical fix or mitigation is available.

## Good-faith research

Security testing should be limited to systems and data you are authorized to test. Avoid privacy violations, destructive activity, service disruption, persistence on third-party systems, or accessing information beyond what is necessary to demonstrate the issue.

For the public web version of this policy, see [`docs/security.html`](docs/security.html).
