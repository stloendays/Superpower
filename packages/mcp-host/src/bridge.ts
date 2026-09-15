import { createServer, type IncomingMessage, type Server, type ServerResponse } from 'node:http';
import { createInterface } from 'node:readline';
import { stdin, stdout, stderr } from 'node:process';
import {
  McpGatewayConfirmationRequiredError,
  McpGatewayRejectedError,
  type ExecutionPolicyMode,
  type ExecutionPolicyResult,
} from '@superpower/mcp-core';
import { connectSuperpowerHost, type ConnectedSuperpowerHost, type HostConnection } from './host.js';

export interface BridgeRunOptions {
  connection: HostConnection;
  taskFocus?: string;
  policyMode?: ExecutionPolicyMode;
}

type BridgeRequestId = string | number;

type BridgeRequest = {
  id?: BridgeRequestId;
  method?: unknown;
  params?: unknown;
};

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

class BridgeRequestError extends Error {
  constructor(
    public readonly code: string,
    message: string,
    public readonly details?: Record<string, unknown>,
  ) {
    super(message);
    this.name = 'BridgeRequestError';
  }
}

const CONVERSATION_BRIDGE_PORT = 32147;
const CONVERSATION_BRIDGE_HOST = '127.0.0.1';
const CONVERSATION_BRIDGE_PATH = '/v1/conversation';
const CONVERSATION_BRIDGE_HEADER = 'x-superpower-conversation-bridge';
const MAX_CONVERSATION_BODY_BYTES = 128 * 1024;
const MAX_CONVERSATION_TEXT_LENGTH = 64 * 1024;
const MAX_ACTION_QUERY_LENGTH = 8 * 1024;

const asRecord = (value: unknown): Record<string, unknown> | null => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  return value as Record<string, unknown>;
};

const policyDetails = (policy: ExecutionPolicyResult): Record<string, unknown> => ({
  risk: policy.risk,
  reasons: policy.reasons,
  sensitiveArgumentKeys: policy.sensitiveArgumentKeys,
});

const writeMessage = (message: unknown): void => {
  stdout.write(`${JSON.stringify(message)}\n`);
};

const isExtensionOrigin = (origin: string | undefined): boolean =>
  !origin || origin.startsWith('chrome-extension://') || origin.startsWith('moz-extension://');

const addExtensionCorsHeaders = (response: ServerResponse, origin: string | undefined): void => {
  if (origin && isExtensionOrigin(origin)) response.setHeader('access-control-allow-origin', origin);
  response.setHeader('access-control-allow-methods', 'POST, OPTIONS');
  response.setHeader('access-control-allow-headers', `content-type, ${CONVERSATION_BRIDGE_HEADER}`);
  response.setHeader('cache-control', 'no-store');
};

const sendHttpJson = (response: ServerResponse, status: number, payload: unknown): void => {
  response.statusCode = status;
  response.setHeader('content-type', 'application/json; charset=utf-8');
  response.end(JSON.stringify(payload));
};

const readRequestBody = async (request: IncomingMessage): Promise<string> =>
  new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let total = 0;
    let exceeded = false;

    request.on('data', (chunk: Buffer) => {
      if (exceeded) return;
      total += chunk.length;
      if (total > MAX_CONVERSATION_BODY_BYTES) {
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

const sanitizeConversationEvent = (value: unknown): ConversationEvent | null => {
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
    text: text.slice(0, MAX_CONVERSATION_TEXT_LENGTH),
    phase,
    url: typeof record.url === 'string' ? record.url.slice(0, 2048) : '',
    title: typeof record.title === 'string' ? record.title.slice(0, 256) : '',
    timestamp:
      typeof record.timestamp === 'number' && Number.isFinite(record.timestamp) ? record.timestamp : Date.now(),
  };
};

const handleConversationRequest = async (request: IncomingMessage, response: ServerResponse): Promise<void> => {
  const origin = typeof request.headers.origin === 'string' ? request.headers.origin : undefined;
  addExtensionCorsHeaders(response, origin);

  if (!isExtensionOrigin(origin)) {
    sendHttpJson(response, 403, { ok: false, error: 'origin_not_allowed' });
    return;
  }

  if (request.method === 'OPTIONS') {
    response.statusCode = 204;
    response.end();
    return;
  }

  if (request.method !== 'POST' || request.url !== CONVERSATION_BRIDGE_PATH) {
    sendHttpJson(response, 404, { ok: false, error: 'not_found' });
    return;
  }

  if (request.headers[CONVERSATION_BRIDGE_HEADER] !== '1') {
    sendHttpJson(response, 403, { ok: false, error: 'bridge_header_required' });
    return;
  }

  try {
    const body = await readRequestBody(request);
    const event = sanitizeConversationEvent(JSON.parse(body) as unknown);
    if (!event) {
      sendHttpJson(response, 400, { ok: false, error: 'invalid_conversation_event' });
      return;
    }

    writeMessage({ type: 'conversation', event });
    sendHttpJson(response, 200, { ok: true });
  } catch (error) {
    const tooLarge = error instanceof Error && error.message === 'payload_too_large';
    sendHttpJson(response, tooLarge ? 413 : 400, {
      ok: false,
      error: tooLarge ? 'payload_too_large' : 'invalid_json',
    });
  }
};

const startConversationBridge = async (): Promise<Server | null> => {
  const server = createServer((request, response) => {
    void handleConversationRequest(request, response);
  });

  return new Promise(resolve => {
    const onError = (error: NodeJS.ErrnoException) => {
      server.off('listening', onListening);
      stderr.write(
        `Desktop conversation bridge unavailable on ${CONVERSATION_BRIDGE_HOST}:${CONVERSATION_BRIDGE_PORT}: ${error.message}\n`,
      );
      resolve(null);
    };
    const onListening = () => {
      server.off('error', onError);
      resolve(server);
    };
    server.once('error', onError);
    server.once('listening', onListening);
    server.listen(CONVERSATION_BRIDGE_PORT, CONVERSATION_BRIDGE_HOST);
  });
};

const closeConversationBridge = async (server: Server | null): Promise<void> => {
  if (!server) return;
  await new Promise<void>(resolve => server.close(() => resolve()));
};

const listTools = async (host: ConnectedSuperpowerHost): Promise<Record<string, unknown>> => {
  const routed = await host.gateway.listTools();
  const scoreByName = new Map(routed.ranked.map(item => [item.tool.name, item.score]));

  return {
    transport: host.transportKind,
    routed: routed.queryUsed,
    omitted: routed.omitted,
    tools: routed.tools.map(tool => ({
      name: tool.name,
      description: tool.description ?? '',
      inputSchema: tool.inputSchema ?? {},
      score: scoreByName.get(tool.name) ?? 0,
    })),
  };
};

const planAction = async (host: ConnectedSuperpowerHost, query: string): Promise<Record<string, unknown>> => {
  const plan = await host.gateway.planAction(query, { maxCandidates: 5 });
  const selected = plan.selected;
  const policy = selected
    ? host.gateway.evaluate(selected.tool.name, plan.arguments, selected.tool.description ?? '')
    : null;

  return {
    transport: host.transportKind,
    query: plan.query,
    selected: selected
      ? {
          name: selected.tool.name,
          description: selected.tool.description ?? '',
          inputSchema: selected.tool.inputSchema ?? {},
          score: selected.score,
          matchedTerms: selected.matchedTerms,
        }
      : null,
    candidates: plan.candidates.map(item => ({
      name: item.tool.name,
      description: item.tool.description ?? '',
      score: item.score,
      matchedTerms: item.matchedTerms,
    })),
    arguments: plan.arguments,
    draftedFields: plan.draftedFields,
    missingRequired: plan.missingRequired,
    confidence: plan.confidence,
    requiresReview: plan.requiresReview,
    policy: policy
      ? {
          decision: policy.decision,
          ...policyDetails(policy),
        }
      : null,
  };
};

const planWorkflow = async (host: ConnectedSuperpowerHost, query: string): Promise<Record<string, unknown>> => {
  const workflow = await host.gateway.planWorkflow(query, { maxCandidates: 5, maxSteps: 6 });

  return {
    transport: host.transportKind,
    query: workflow.query,
    confidence: workflow.confidence,
    requiresReview: workflow.requiresReview,
    autoExecutable: workflow.autoExecutable,
    unresolvedStepCount: workflow.unresolvedStepCount,
    reviewReasons: workflow.reviewReasons,
    steps: workflow.steps.map(step => {
      const action = step.action;
      const selected = action?.selected ?? null;
      const policy = selected
        ? host.gateway.evaluate(selected.tool.name, action?.arguments ?? {}, selected.tool.description ?? '')
        : null;

      return {
        id: step.id,
        index: step.index,
        instruction: step.instruction,
        kind: step.kind,
        dependsOn: step.dependsOn,
        needsPreviousOutput: step.needsPreviousOutput,
        confidence: step.confidence,
        requiresReview: step.requiresReview,
        transform: step.transform,
        action: action
          ? {
              query: action.query,
              selected: selected
                ? {
                    name: selected.tool.name,
                    description: selected.tool.description ?? '',
                    inputSchema: selected.tool.inputSchema ?? {},
                    score: selected.score,
                    matchedTerms: selected.matchedTerms,
                  }
                : null,
              candidates: action.candidates.map(item => ({
                name: item.tool.name,
                description: item.tool.description ?? '',
                score: item.score,
                matchedTerms: item.matchedTerms,
              })),
              arguments: action.arguments,
              draftedFields: action.draftedFields,
              missingRequired: action.missingRequired,
              confidence: action.confidence,
              requiresReview: action.requiresReview,
              policy: policy
                ? {
                    decision: policy.decision,
                    ...policyDetails(policy),
                  }
                : null,
            }
          : null,
      };
    }),
  };
};

const findToolDescription = async (host: ConnectedSuperpowerHost, toolName: string): Promise<string> => {
  const response = await host.client.listTools();
  return response.tools.find(tool => tool.name === toolName)?.description ?? '';
};

const callTool = async (
  host: ConnectedSuperpowerHost,
  toolName: string,
  args: Record<string, unknown>,
  approve: boolean,
  setApproval: (approved: boolean) => void,
): Promise<unknown> => {
  const description = await findToolDescription(host, toolName);
  setApproval(approve);

  try {
    return await host.gateway.callTool(toolName, args, description);
  } catch (error) {
    if (
      error instanceof McpGatewayConfirmationRequiredError ||
      (error instanceof McpGatewayRejectedError && !approve)
    ) {
      throw new BridgeRequestError(
        'confirmation_required',
        'This guarded MCP action requires desktop confirmation before execution.',
        policyDetails(error.policy),
      );
    }
    if (error instanceof McpGatewayRejectedError) {
      throw new BridgeRequestError('rejected', error.message, policyDetails(error.policy));
    }
    throw error;
  } finally {
    setApproval(false);
  }
};

/**
 * Long-lived, newline-delimited JSON bridge for native shells such as the Qt desktop client.
 *
 * The bridge deliberately keeps connection setup, routing, action/workflow planning,
 * policy evaluation and telemetry in the existing TypeScript host. Native clients
 * only own presentation and user interaction.
 */
export const runBridge = async (options: BridgeRunOptions): Promise<void> => {
  let approvalForCurrentCall = false;
  const host = await connectSuperpowerHost({
    connection: options.connection,
    taskFocus: options.taskFocus,
    policyMode: options.policyMode,
    confirm: async () => approvalForCurrentCall,
    clientName: 'superpower-desktop-bridge',
  });
  const conversationServer = await startConversationBridge();
  const readline = createInterface({ input: stdin, crlfDelay: Infinity });

  writeMessage({
    type: 'ready',
    protocol: 'superpower-desktop-bridge',
    version: 4,
    transport: host.transportKind,
    taskFocus: host.gateway.getTaskFocus(),
    policyMode: host.gateway.getPolicyMode(),
    conversationBridge: conversationServer ? { host: CONVERSATION_BRIDGE_HOST, port: CONVERSATION_BRIDGE_PORT } : null,
  });

  try {
    for await (const rawLine of readline) {
      const line = rawLine.trim();
      if (!line) continue;

      let request: BridgeRequest;
      try {
        request = JSON.parse(line) as BridgeRequest;
      } catch {
        writeMessage({
          id: null,
          ok: false,
          error: { code: 'invalid_json', message: 'Bridge requests must be one valid JSON object per line.' },
        });
        continue;
      }

      const id = request.id;
      if (id === undefined || id === null || (typeof id !== 'string' && typeof id !== 'number')) {
        writeMessage({
          id: null,
          ok: false,
          error: { code: 'invalid_request', message: 'Bridge requests require a string or numeric id.' },
        });
        continue;
      }

      const method = typeof request.method === 'string' ? request.method : '';
      const params = asRecord(request.params) ?? {};
      let shouldClose = false;

      try {
        let result: unknown;
        switch (method) {
          case 'ping':
            result = { pong: true };
            break;
          case 'tools':
            result = await listTools(host);
            break;
          case 'plan': {
            const query = params.query;
            if (typeof query !== 'string' || !query.trim()) {
              throw new BridgeRequestError('invalid_params', 'plan requires a non-empty query.');
            }
            if (query.length > MAX_ACTION_QUERY_LENGTH) {
              throw new BridgeRequestError('invalid_params', 'plan query is too long.');
            }
            result = await planAction(host, query.trim());
            break;
          }
          case 'workflowPlan': {
            const query = params.query;
            if (typeof query !== 'string' || !query.trim()) {
              throw new BridgeRequestError('invalid_params', 'workflowPlan requires a non-empty query.');
            }
            if (query.length > MAX_ACTION_QUERY_LENGTH) {
              throw new BridgeRequestError('invalid_params', 'workflowPlan query is too long.');
            }
            result = await planWorkflow(host, query.trim());
            break;
          }
          case 'focus': {
            const focus = params.focus;
            if (typeof focus !== 'string') throw new BridgeRequestError('invalid_params', 'focus requires a string.');
            host.gateway.setTaskFocus(focus);
            result = { taskFocus: host.gateway.getTaskFocus() };
            break;
          }
          case 'policy': {
            const mode = params.mode;
            if (mode !== 'audit' && mode !== 'guarded') {
              throw new BridgeRequestError('invalid_params', 'policy mode must be audit or guarded.');
            }
            host.gateway.setPolicyMode(mode);
            result = { policyMode: host.gateway.getPolicyMode() };
            break;
          }
          case 'call': {
            const toolName = params.toolName;
            if (typeof toolName !== 'string' || !toolName.trim()) {
              throw new BridgeRequestError('invalid_params', 'call requires a non-empty toolName.');
            }
            const args = params.args === undefined ? {} : asRecord(params.args);
            if (!args) throw new BridgeRequestError('invalid_params', 'call args must be a JSON object.');
            result = await callTool(host, toolName, args, params.approve === true, approved => {
              approvalForCurrentCall = approved;
            });
            break;
          }
          case 'stats':
            result = host.gateway.getTelemetrySummary();
            break;
          case 'clearStats':
            host.gateway.clearTelemetry();
            result = { cleared: true };
            break;
          case 'close':
            result = { closed: true };
            shouldClose = true;
            break;
          default:
            throw new BridgeRequestError('unknown_method', `Unknown bridge method: ${method || '(empty)'}.`);
        }

        writeMessage({ id, ok: true, result });
      } catch (error) {
        if (error instanceof BridgeRequestError) {
          writeMessage({
            id,
            ok: false,
            error: {
              code: error.code,
              message: error.message,
              ...(error.details ? { details: error.details } : {}),
            },
          });
        } else {
          writeMessage({
            id,
            ok: false,
            error: {
              code: 'request_failed',
              message: error instanceof Error ? error.message : String(error),
            },
          });
        }
      }

      if (shouldClose) break;
    }
  } finally {
    readline.close();
    await closeConversationBridge(conversationServer);
    await host.close();
  }
};
