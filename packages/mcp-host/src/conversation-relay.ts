#!/usr/bin/env node

import { createServer, type IncomingMessage, type ServerResponse } from 'node:http';
import { stdin, stdout, stderr } from 'node:process';
import { createInterface } from 'node:readline';

const RELAY_HOST = '127.0.0.1';
const RELAY_PORT = 32148;
const RELAY_PATH = '/v1/conversation';
const RELAY_HEADER = 'x-superpower-conversation-bridge';
const MAX_BODY_BYTES = 128 * 1024;
const MAX_TEXT_LENGTH = 64 * 1024;

type ConversationEvent = {
  eventId: string;
  sessionId: string;
  source: 'chatgpt';
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
};

const writeMessage = (message: unknown): void => {
  stdout.write(`${JSON.stringify(message)}\n`);
};

const asRecord = (value: unknown): Record<string, unknown> | null => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  return value as Record<string, unknown>;
};

const isExtensionOrigin = (origin: string | undefined): boolean =>
  !origin || origin.startsWith('chrome-extension://') || origin.startsWith('moz-extension://');

const addCorsHeaders = (response: ServerResponse, origin: string | undefined): void => {
  if (origin && isExtensionOrigin(origin)) response.setHeader('access-control-allow-origin', origin);
  response.setHeader('access-control-allow-methods', 'POST, OPTIONS');
  response.setHeader('access-control-allow-headers', `content-type, ${RELAY_HEADER}`);
  response.setHeader('cache-control', 'no-store');
};

const sendJson = (response: ServerResponse, status: number, payload: unknown): void => {
  response.statusCode = status;
  response.setHeader('content-type', 'application/json; charset=utf-8');
  response.end(JSON.stringify(payload));
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
    source !== 'chatgpt' ||
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

const handleConversationRequest = async (request: IncomingMessage, response: ServerResponse): Promise<void> => {
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

  if (request.method !== 'POST' || request.url !== RELAY_PATH) {
    sendJson(response, 404, { ok: false, error: 'not_found' });
    return;
  }

  if (request.headers[RELAY_HEADER] !== '1') {
    sendJson(response, 403, { ok: false, error: 'bridge_header_required' });
    return;
  }

  try {
    const body = await readBody(request);
    const event = sanitizeEvent(JSON.parse(body) as unknown);
    if (!event) {
      sendJson(response, 400, { ok: false, error: 'invalid_conversation_event' });
      return;
    }

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

const server = createServer((request, response) => {
  void handleConversationRequest(request, response);
});

server.on('error', error => {
  stderr.write(`Conversation relay failed: ${error.message}\n`);
  process.exitCode = 1;
});

server.listen(RELAY_PORT, RELAY_HOST, () => {
  writeMessage({
    type: 'ready',
    protocol: 'superpower-conversation-relay',
    version: 1,
    transport: 'conversation',
    host: RELAY_HOST,
    port: RELAY_PORT,
  });
});

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
      if (method === 'close') {
        writeMessage({ id, ok: true, result: { closed: true } });
        break;
      }
    }
  } finally {
    readline.close();
    await new Promise<void>(resolve => server.close(() => resolve()));
  }
})().catch(error => {
  stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
  process.exitCode = 1;
});
