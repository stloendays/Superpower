import type { ConnectionType } from '../../../types/stores';

export interface ParsedConnectionInput {
  uri: string;
  connectionType: ConnectionType;
  label?: string;
  source: 'url' | 'json';
  ignoredAuth: boolean;
}

export interface RecentConnection {
  uri: string;
  connectionType: ConnectionType;
  label?: string;
  lastUsedAt: number;
}

export type ConnectionInputResult = { ok: true; value: ParsedConnectionInput } | { ok: false; error: string };

const RECENT_CONNECTIONS_KEY = 'superpower:mcp-recent-connections:v1';
const MAX_RECENT_CONNECTIONS = 5;
const MAX_RECENT_LABEL_CHARS = 120;
const MAX_AUTH_SCAN_DEPTH = 3;

const normalizeCredentialKey = (key: string): string => key.toLowerCase().replace(/[^a-z0-9]/g, '');

/**
 * Credential-like names that should never be imported from pasted JSON or
 * persisted in Recent connection URLs. Generic "key" is only matched exactly
 * so harmless names such as "monkey" do not become false positives.
 */
const isSensitiveCredentialKey = (key: string): boolean => {
  const normalized = normalizeCredentialKey(key);
  if (!normalized) return false;
  if (normalized === 'key' || normalized === 'code') return true;

  const sensitiveTerms = [
    'token',
    'apikey',
    'secret',
    'auth',
    'oauth',
    'authorization',
    'signature',
    'sig',
    'credential',
    'credentials',
    'password',
    'passwd',
    'jwt',
    'bearer',
    'clientsecret',
    'accesskey',
    'sessionid',
    'sessiontoken',
  ];

  return sensitiveTerms.some(term => normalized === term || normalized.endsWith(term));
};

const isSensitiveConfigKey = (key: string): boolean => {
  const normalized = normalizeCredentialKey(key);
  return normalized === 'header' || normalized.endsWith('headers') || isSensitiveCredentialKey(key);
};

export const inferConnectionType = (uri: string): ConnectionType => {
  const normalized = uri.trim().toLowerCase();

  if (normalized.startsWith('ws://') || normalized.startsWith('wss://')) return 'websocket';

  try {
    const url = new URL(uri.trim());
    if (url.protocol === 'ws:' || url.protocol === 'wss:') return 'websocket';
    if (/\/sse\/?$/i.test(url.pathname)) return 'sse';
  } catch {
    if (/\/sse\/?(?:[?#].*)?$/i.test(normalized)) return 'sse';
  }

  return 'streamable-http';
};

const normalizeConnectionType = (value: unknown, uri: string): ConnectionType => {
  if (typeof value !== 'string') return inferConnectionType(uri);

  const normalized = value.trim().toLowerCase().replace(/_/g, '-');
  if (normalized === 'sse' || normalized === 'server-sent-events') return 'sse';
  if (normalized === 'websocket' || normalized === 'ws') return 'websocket';
  if (
    normalized === 'streamable-http' ||
    normalized === 'streamablehttp' ||
    normalized === 'http' ||
    normalized === 'https'
  ) {
    return 'streamable-http';
  }

  return inferConnectionType(uri);
};

const isRemoteUri = (value: unknown): value is string =>
  typeof value === 'string' && /^(https?|wss?):\/\//i.test(value.trim());

/**
 * Pasted MCP configs vary by client. Authentication can appear at the server
 * entry root or inside request/http/options objects, so only checking a few
 * top-level names can silently miss secrets. Scan keys recursively with a
 * strict depth cap; values are never copied or logged by this detector.
 */
const hasAuthMaterial = (value: Record<string, unknown>, depth = 0): boolean => {
  for (const [key, child] of Object.entries(value)) {
    if (isSensitiveConfigKey(key)) return true;
    if (depth >= MAX_AUTH_SCAN_DEPTH || !child || typeof child !== 'object') continue;

    if (Array.isArray(child)) {
      if (
        child.some(
          item => !!item && typeof item === 'object' && !Array.isArray(item) && hasAuthMaterial(item, depth + 1),
        )
      ) {
        return true;
      }
      continue;
    }

    if (hasAuthMaterial(child as Record<string, unknown>, depth + 1)) return true;
  }

  return false;
};

const extractRemoteEntry = (entry: Record<string, unknown>, label?: string): ParsedConnectionInput | null => {
  const rawUri = entry.url ?? entry.uri ?? entry.endpoint ?? entry.serverUrl ?? entry.server_url;
  if (!isRemoteUri(rawUri)) return null;

  const uri = rawUri.trim();
  return {
    uri,
    connectionType: normalizeConnectionType(entry.transport ?? entry.type ?? entry.connectionType, uri),
    label,
    source: 'json',
    ignoredAuth: hasAuthMaterial(entry),
  };
};

export const parseMcpConnectionInput = (rawInput: string): ConnectionInputResult => {
  const input = rawInput.trim();
  if (!input) return { ok: false, error: 'Paste an MCP server address or configuration first.' };

  if (/^(https?|wss?):\/\//i.test(input)) {
    return {
      ok: true,
      value: {
        uri: input,
        connectionType: inferConnectionType(input),
        source: 'url',
        ignoredAuth: false,
      },
    };
  }

  let parsed: unknown;
  try {
    parsed = JSON.parse(input);
  } catch {
    return {
      ok: false,
      error: 'That does not look like an MCP address or JSON configuration.',
    };
  }

  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return { ok: false, error: 'The MCP configuration must be a JSON object.' };
  }

  const root = parsed as Record<string, unknown>;
  const direct = extractRemoteEntry(root, typeof root.name === 'string' ? root.name : undefined);
  if (direct) return { ok: true, value: direct };

  const servers = root.mcpServers ?? root.servers;
  if (servers && typeof servers === 'object' && !Array.isArray(servers)) {
    let sawStdioConfig = false;

    for (const [name, candidate] of Object.entries(servers as Record<string, unknown>)) {
      if (!candidate || typeof candidate !== 'object' || Array.isArray(candidate)) continue;
      const entry = candidate as Record<string, unknown>;
      const remote = extractRemoteEntry(entry, name);
      if (remote) return { ok: true, value: remote };
      if (typeof entry.command === 'string') sawStdioConfig = true;
    }

    if (sawStdioConfig) {
      return {
        ok: false,
        error:
          'This is a local stdio MCP configuration. A browser extension cannot launch local commands directly; connect it through the Superpower Host or a browser-accessible MCP proxy.',
      };
    }
  }

  if (typeof root.command === 'string') {
    return {
      ok: false,
      error:
        'This is a local stdio MCP configuration. A browser extension cannot launch local commands directly; connect it through the Superpower Host or a browser-accessible MCP proxy.',
    };
  }

  return {
    ok: false,
    error: 'No browser-accessible MCP endpoint was found in that configuration.',
  };
};

const hasSensitiveUrlParameters = (params: URLSearchParams): boolean =>
  Array.from(params.keys()).some(isSensitiveCredentialKey);

const canRememberUri = (uri: string): boolean => {
  try {
    const url = new URL(uri);
    if (!['http:', 'https:', 'ws:', 'wss:'].includes(url.protocol)) return false;
    if (url.username || url.password) return false;
    if (hasSensitiveUrlParameters(url.searchParams)) return false;

    const fragment = url.hash.replace(/^#/, '');
    if (fragment && hasSensitiveUrlParameters(new URLSearchParams(fragment.replace(/^\?/, '')))) return false;

    return true;
  } catch {
    return false;
  }
};

const normalizeRecentLabel = (label?: string): string | undefined => {
  const normalized = label?.replace(/\s+/g, ' ').trim();
  return normalized ? normalized.slice(0, MAX_RECENT_LABEL_CHARS) : undefined;
};

const storageGet = <T>(key: string): Promise<T | undefined> =>
  new Promise(resolve => {
    try {
      chrome.storage.local.get(key, result => {
        if (chrome.runtime.lastError) {
          resolve(undefined);
          return;
        }
        resolve(result[key] as T | undefined);
      });
    } catch {
      resolve(undefined);
    }
  });

const storageSet = (value: Record<string, unknown>): Promise<void> =>
  new Promise(resolve => {
    try {
      chrome.storage.local.set(value, () => resolve());
    } catch {
      resolve();
    }
  });

export const loadRecentConnections = async (): Promise<RecentConnection[]> => {
  const parsed = await storageGet<unknown>(RECENT_CONNECTIONS_KEY);
  if (!Array.isArray(parsed)) return [];

  return parsed
    .filter(
      (item): item is RecentConnection =>
        !!item &&
        typeof item === 'object' &&
        typeof item.uri === 'string' &&
        canRememberUri(item.uri) &&
        ['sse', 'websocket', 'streamable-http'].includes(item.connectionType) &&
        typeof item.lastUsedAt === 'number' &&
        Number.isFinite(item.lastUsedAt) &&
        (item.label === undefined || typeof item.label === 'string'),
    )
    .sort((a, b) => b.lastUsedAt - a.lastUsedAt)
    .slice(0, MAX_RECENT_CONNECTIONS);
};

export const rememberRecentConnection = async (
  connection: Omit<RecentConnection, 'lastUsedAt'>,
): Promise<RecentConnection[]> => {
  const current = await loadRecentConnections();
  if (!canRememberUri(connection.uri)) return current;

  const next: RecentConnection[] = [
    { ...connection, label: normalizeRecentLabel(connection.label), lastUsedAt: Date.now() },
    ...current.filter(item => item.uri !== connection.uri),
  ].slice(0, MAX_RECENT_CONNECTIONS);

  await storageSet({ [RECENT_CONNECTIONS_KEY]: next });
  return next;
};
