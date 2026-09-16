#!/usr/bin/env node

import { createServer, type IncomingMessage, type ServerResponse } from 'node:http';
import { stdin, stdout, stderr } from 'node:process';
import { createInterface } from 'node:readline';

const RELAY_HOST = '127.0.0.1';
const RELAY_PORT = 32148;
const RELAY_PATH = '/v1/conversation';
const PROMPT_NEXT_PATH = '/v1/prompt/next';
const PROMPT_ACK_PATH = '/v1/prompt/ack';
const RELAY_HEADER = 'x-superpower-conversation-bridge';
const MAX_BODY_BYTES = 128 * 1024;
const MAX_TEXT_LENGTH = 64 * 1024;
const MAX_PROMPT_LENGTH = 32 * 1024;
const MAX_PROMPT_QUEUE = 20;
const PROMPT_TIMEOUT_MS = 30_000;
const PROVIDER_STATUS_INTERVAL_MS = 5_000;

const PROVIDERS = ['chatgpt', 'gemini', 'grok', 'perplexity'] as const;
type ProviderId = (typeof PROVIDERS)[number];
type PromptTarget = ProviderId | 'auto';

type ConversationEvent = {
  eventId: string;
  sessionId: string;
  source: ProviderId;
  role: 'user' | 'assistant';
  text: string;
  phase: 'streaming' | 'completed';
  url: string;
  title: string;
  timestamp: number;
};

type RelayRequest = {
  id?: string | number;
  method?: unknown;
  params?: unknown;
};

type PendingPrompt = {
  requestId: string | number;
  promptId: string;
  text: string;
  provider: PromptTarget;
  timestamp: number;
  claimedAt?: number;
  claimedBy?: ProviderId;
};

const pendingPrompts: PendingPrompt[] = [];
const claimedPrompts = new Map<string, PendingPrompt>();
let nextPromptId = 1;
let lastReportedProvider: ProviderId | null = null;
let lastProviderReportAt = 0;

const writeMessage = (message: unknown): void => {
  stdout.write(`${JSON.stringify(message)}\n`);
};

const asRecord = (value: unknown): Record<string, unknown> | null => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  return value as Record<string, unknown>;
};

const isProvider = (value: unknown): value is ProviderId =>
  typeof value === 'string' && (PROVIDERS as readonly string[]).includes(value);

const isPromptTarget = (value: unknown): value is PromptTarget => value === 'auto' || isProvider(value);

const providerDisplayName = (provider: PromptTarget): string => {
  if (provider === 'chatgpt') return 'ChatGPT';
  if (provider === 'gemini') return 'Gemini';
  if (provider === 'grok') return 'Grok';
  if (provider === 'perplexity') return 'Perplexity';
  return 'supported AI';
};

const isExtensionOrigin = (origin: string | undefined): boolean =>
  !origin || origin.startsWith('chrome-extension://') || origin.startsWith('moz-extension://');

const addCorsHeaders = (response: ServerResponse, origin: string | undefined): void => {
  if (origin && isExtensionOrigin(origin)) response.setHeader('access-control-allow-origin', origin);
  response.setHeader('access-control-allow-methods', 'GET, POST, OPTIONS');
  response.setHeader('access-control-allow-headers', `content-type, ${RELAY_HEADER}`);
  response.setHeader('cache-control', 'no-store');
};

const sendJson = (response: ServerResponse, status: number, payload: unknown): void => {
  response.statusCode = status;
  response.setHeader('content-type', 'application/json; charset=utf-8');
  response.end(JSON.stringify(payload));
};

const sendRequestError = (
  id: string | number,
  code: string,
  message: string,
  details: Record<string, unknown> = {},
): void => {
  writeMessage({ id, ok: false, error: { code, message, details } });
};

const readBody = async (request: IncomingMessage): Promise<string> =>
  new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let total = 0;
    let exceeded = false;

    request.on('data', (chunk: Buffer) => {
      if (exceeded) return;
      total += chunk.length;
      if (total > MAX_BODY_BYTES) {
        exceeded = true;
        return;
      }
      chunks.push(chunk);
    });
    request.on('end', () => {
      if (exceeded) {
        reject(new Error('payload_too_large'));
        return;
      }
      resolve(Buffer.concat(chunks).toString('utf8'));
    });
    request.on('error', reject);
  });

const sanitizeEvent = (value: unknown): ConversationEvent | null => {
  const record = asRecord(value);
  if (!record) return null;

  const eventId = typeof record.eventId === 'string' ? record.eventId.trim() : '';
  const sessionId = typeof record.sessionId === 'string' ? record.sessionId.trim() : '';
  const source = record.source;
  const role = record.role;
  const text = typeof record.text === 'string' ? record.text : '';
  const phase = record.phase;

  if (
    !eventId ||
    !sessionId ||
    !isProvider(source) ||
    (role !== 'user' && role !== 'assistant') ||
    !text.trim() ||
    (phase !== 'streaming' && phase !== 'completed')
  ) {
    return null;
  }

  return {
    eventId: eventId.slice(0, 512),
    sessionId: sessionId.slice(0, 1024),
    source,
    role,
    text: text.slice(0, MAX_TEXT_LENGTH),
    phase,
    url: typeof record.url === 'string' ? record.url.slice(0, 2048) : '',
    title: typeof record.title === 'string' ? record.title.slice(0, 256) : '',
    timestamp:
      typeof record.timestamp === 'number' && Number.isFinite(record.timestamp) ? record.timestamp : Date.now(),
  };
};

const noteProviderSeen = (provider: ProviderId): void => {
  const now = Date.now();
  if (provider === lastReportedProvider && now - lastProviderReportAt < PROVIDER_STATUS_INTERVAL_MS) return;
  lastReportedProvider = provider;
  lastProviderReportAt = now;
  writeMessage({ type: 'provider', provider, timestamp: now });
};

const rejectExpiredPrompt = (prompt: PendingPrompt): void => {
  const target = providerDisplayName(prompt.provider);
  sendRequestError(
    prompt.requestId,
    'prompt_timeout',
    `No active ${target} tab accepted the desktop prompt within 30 seconds.`,
    { promptId: prompt.promptId, provider: prompt.provider },
  );
};

const expirePrompts = (): void => {
  const now = Date.now();
  for (let index = pendingPrompts.length - 1; index >= 0; index -= 1) {
    if (now - pendingPrompts[index].timestamp < PROMPT_TIMEOUT_MS) continue;
    const [expired] = pendingPrompts.splice(index, 1);
    rejectExpiredPrompt(expired);
  }

  for (const [promptId, prompt] of claimedPrompts.entries()) {
    const startedAt = prompt.claimedAt ?? prompt.timestamp;
    if (now - startedAt < PROMPT_TIMEOUT_MS) continue;
    claimedPrompts.delete(promptId);
    rejectExpiredPrompt(prompt);
  }
};

const handleConversationRequest = async (request: IncomingMessage, response: ServerResponse): Promise<void> => {
  try {
    const body = await readBody(request);
    const event = sanitizeEvent(JSON.parse(body) as unknown);
    if (!event) {
      sendJson(response, 400, { ok: false, error: 'invalid_conversation_event' });
      return;
    }

    noteProviderSeen(event.source);
    writeMessage({ type: 'conversation', event });
    sendJson(response, 200, { ok: true });
  } catch (error) {
    const tooLarge = error instanceof Error && error.message === 'payload_too_large';
    sendJson(response, tooLarge ? 413 : 400, {
      ok: false,
      error: tooLarge ? 'payload_too_large' : 'invalid_json',
    });
  }
};

const handlePromptNextRequest = (request: IncomingMessage, response: ServerResponse): void => {
  expirePrompts();

  const requestUrl = new URL(request.url ?? PROMPT_NEXT_PATH, `http://${RELAY_HOST}:${RELAY_PORT}`);
  const providerValue = requestUrl.searchParams.get('provider');
  if (!isProvider(providerValue)) {
    sendJson(response, 400, { ok: false, error: 'invalid_provider' });
    return;
  }
  const provider = providerValue;
  noteProviderSeen(provider);

  const promptIndex = pendingPrompts.findIndex(
    prompt => prompt.provider === 'auto' || prompt.provider === provider,
  );
  if (promptIndex < 0) {
    sendJson(response, 200, { ok: true, prompt: null });
    return;
  }

  const [prompt] = pendingPrompts.splice(promptIndex, 1);
  prompt.claimedAt = Date.now();
  prompt.claimedBy = provider;
  claimedPrompts.set(prompt.promptId, prompt);
  sendJson(response, 200, {
    ok: true,
    prompt: {
      promptId: prompt.promptId,
      text: prompt.text,
      provider: prompt.provider,
      claimedBy: provider,
      timestamp: prompt.timestamp,
    },
  });
};

const handlePromptAckRequest = async (request: IncomingMessage, response: ServerResponse): Promise<void> => {
  try {
    const body = await readBody(request);
    const record = asRecord(JSON.parse(body) as unknown);
    const promptId = typeof record?.promptId === 'string' ? record.promptId.trim() : '';
    const success = record?.success;
    const message = typeof record?.message === 'string' ? record.message.slice(0, 1024) : '';

    if (!promptId || typeof success !== 'boolean') {
      sendJson(response, 400, { ok: false, error: 'invalid_prompt_ack' });
      return;
    }

    const prompt = claimedPrompts.get(promptId);
    if (!prompt) {
      sendJson(response, 404, { ok: false, error: 'prompt_not_found' });
      return;
    }
    claimedPrompts.delete(promptId);

    if (success) {
      writeMessage({
        id: prompt.requestId,
        ok: true,
        result: {
          promptId,
          submitted: true,
          provider: prompt.claimedBy ?? prompt.provider,
          message,
        },
      });
    } else {
      sendRequestError(
        prompt.requestId,
        'prompt_delivery_failed',
        message || `The active ${providerDisplayName(prompt.claimedBy ?? prompt.provider)} tab could not submit the desktop prompt.`,
        { promptId, provider: prompt.claimedBy ?? prompt.provider },
      );
    }

    sendJson(response, 200, { ok: true });
  } catch (error) {
    const tooLarge = error instanceof Error && error.message === 'payload_too_large';
    sendJson(response, tooLarge ? 413 : 400, {
      ok: false,
      error: tooLarge ? 'payload_too_large' : 'invalid_json',
    });
  }
};

const server = createServer((request, response) => {
  const origin = typeof request.headers.origin === 'string' ? request.headers.origin : undefined;
  addCorsHeaders(response, origin);

  if (!isExtensionOrigin(origin)) {
    sendJson(response, 403, { ok: false, error: 'origin_not_allowed' });
    return;
  }

  if (request.method === 'OPTIONS') {
    response.statusCode = 204;
    response.end();
    return;
  }

  if (request.headers[RELAY_HEADER] !== '1') {
    sendJson(response, 403, { ok: false, error: 'bridge_header_required' });
    return;
  }

  if (request.method === 'POST' && request.url === RELAY_PATH) {
    void handleConversationRequest(request, response);
    return;
  }
  if (request.method === 'GET' && request.url?.startsWith(PROMPT_NEXT_PATH)) {
    handlePromptNextRequest(request, response);
    return;
  }
  if (request.method === 'POST' && request.url === PROMPT_ACK_PATH) {
    void handlePromptAckRequest(request, response);
    return;
  }

  sendJson(response, 404, { ok: false, error: 'not_found' });
});

server.on('error', error => {
  stderr.write(`Conversation relay failed: ${error.message}\n`);
  process.exitCode = 1;
});

server.listen(RELAY_PORT, RELAY_HOST, () => {
  writeMessage({
    type: 'ready',
    protocol: 'superpower-conversation-relay',
    version: 3,
    transport: 'conversation',
    host: RELAY_HOST,
    port: RELAY_PORT,
    capabilities: [
      'browser-conversation-sync',
      'desktop-prompt-submit',
      'provider-routing',
      'provider-presence',
    ],
    providers: PROVIDERS,
  });
});

const promptExpiryTimer = setInterval(expirePrompts, 1000);
promptExpiryTimer.unref();

const readline = createInterface({ input: stdin, crlfDelay: Infinity });

void (async () => {
  try {
    for await (const rawLine of readline) {
      const line = rawLine.trim();
      if (!line) continue;

      let request: RelayRequest;
      try {
        request = JSON.parse(line) as RelayRequest;
      } catch {
        continue;
      }

      const id = request.id;
      if (id === undefined || id === null) continue;
      const method = typeof request.method === 'string' ? request.method : '';
      if (method === 'ping') {
        writeMessage({ id, ok: true, result: { pong: true } });
        continue;
      }
      if (method === 'submit_prompt') {
        const params = asRecord(request.params);
        const text = typeof params?.text === 'string' ? params.text.trim() : '';
        const rawProvider = params?.provider;
        const provider: PromptTarget | null = rawProvider === undefined ? 'auto' : isPromptTarget(rawProvider) ? rawProvider : null;
        if (!text) {
          sendRequestError(id, 'invalid_prompt', 'Desktop prompt text is required.');
          continue;
        }
        if (!provider) {
          sendRequestError(id, 'unsupported_provider', 'Desktop Quick Ask provider is not supported.', {
            supportedProviders: ['auto', ...PROVIDERS],
          });
          continue;
        }
        if (text.length > MAX_PROMPT_LENGTH) {
          sendRequestError(id, 'prompt_too_large', `Desktop prompt exceeds ${MAX_PROMPT_LENGTH} characters.`);
          continue;
        }
        if (pendingPrompts.length + claimedPrompts.size >= MAX_PROMPT_QUEUE) {
          sendRequestError(id, 'prompt_queue_full', 'Desktop prompt queue is full. Wait for the browser to catch up.');
          continue;
        }

        const promptId = `desktop-${Date.now()}-${nextPromptId++}`;
        pendingPrompts.push({
          requestId: id,
          promptId,
          text,
          provider,
          timestamp: Date.now(),
        });
        continue;
      }
      if (method === 'close') {
        writeMessage({ id, ok: true, result: { closed: true } });
        break;
      }

      sendRequestError(id, 'method_not_found', `Unknown conversation relay method: ${method}`);
    }
  } finally {
    clearInterval(promptExpiryTimer);
    readline.close();
    await new Promise<void>(resolve => server.close(() => resolve()));
  }
})().catch(error => {
  stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
  process.exitCode = 1;
});
