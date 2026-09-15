import { createInterface } from 'node:readline';
import { stdin, stdout } from 'node:process';
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
 * The bridge deliberately keeps connection setup, routing, policy evaluation and telemetry in
 * the existing TypeScript host. Native clients only own presentation and user interaction.
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
  const readline = createInterface({ input: stdin, crlfDelay: Infinity });

  writeMessage({
    type: 'ready',
    protocol: 'superpower-desktop-bridge',
    version: 1,
    transport: host.transportKind,
    taskFocus: host.gateway.getTaskFocus(),
    policyMode: host.gateway.getPolicyMode(),
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
    await host.close();
  }
};
