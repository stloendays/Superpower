import { readFileSync } from 'node:fs';

const packageJson = JSON.parse(readFileSync('./package.json', 'utf8'));

/**
 * Chrome Manifest V3 configuration for Superpower.
 *
 * The extension intentionally requests only the AI web-app origins that have
 * adapters plus loopback origins used by the local MCP proxy. Remote MCP hosts
 * should be added explicitly (or via optional host permissions in a future UI)
 * rather than granting blanket access to every website.
 */
const manifest = {
  manifest_version: 3,
  default_locale: 'en',
  name: 'Superpower',
  version: packageJson.version,
  version_name: `V${packageJson.version}`,
  description: 'Connect supported AI web apps to local MCP tools with Superpower.',
  host_permissions: [
    '*://*.perplexity.ai/*',
    '*://*.chat.openai.com/*',
    '*://*.chatgpt.com/*',
    '*://*.grok.com/*',
    '*://*.x.com/*',
    '*://*.twitter.com/*',
    '*://*.gemini.google.com/*',
    '*://*.aistudio.google.com/*',
    '*://*.openrouter.ai/*',
    '*://*.google-analytics.com/*',
    '*://*.chat.deepseek.com/*',
    '*://*.t3.chat/*',
    '*://*.chat.mistral.ai/*',
    '*://*.github.com/*',
    '*://*.copilot.github.com/*',
    '*://*.kimi.com/*',
    '*://*.chat.z.ai/*',
    '*://*.chat.qwen.ai/*',
    'http://localhost/*',
    'https://localhost/*',
    'http://127.0.0.1/*',
    'https://127.0.0.1/*',
  ],

  permissions: ['storage', 'clipboardWrite'],
  background: {
    service_worker: 'background.js',
    type: 'module',
  },
  icons: {
    128: 'icon-128.png',
    34: 'icon-34.png',
    16: 'icon-16.png',
  },
  content_scripts: [
    {
      matches: ['*://*.perplexity.ai/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.chat.openai.com/*', '*://*.chatgpt.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.grok.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.x.com/*', '*://*.twitter.com/*', '*://*.x.com/i/grok*', '*://*.twitter.com/i/grok*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.gemini.google.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.aistudio.google.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.openrouter.ai/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.chat.deepseek.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.kagi.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.t3.chat/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.chat.mistral.ai/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.github.com/*', '*://*.copilot.github.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.kimi.com/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.chat.z.ai/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
    {
      matches: ['*://*.chat.qwen.ai/*'],
      js: ['content/index.iife.js'],
      run_at: 'document_idle',
    },
  ],
  web_accessible_resources: [
    {
      resources: ['*.js', '*.css', 'content/*.css', '*.svg', 'icon-128.png', 'icon-34.png', 'icon-16.png'],
      matches: ['*://*/*'],
    },
  ],
} satisfies chrome.runtime.ManifestV3;

export default manifest;
