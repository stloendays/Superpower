#!/usr/bin/env node

import { createInterface } from 'node:readline/promises';
import { stdin, stdout, stderr } from 'node:process';
import {
  McpGatewayConfirmationRequiredError,
  McpGatewayRejectedError,
  type ExecutionPolicyMode,
  type McpGatewayConfirmationHandler,
} from '@superpower/mcp-core';
import { runBridge } from './bridge.js';
import { connectSuperpowerHost, type ConnectedSuperpowerHost, type HostConnection } from './host.js';

type CliCommand = 'connect' | 'tools' | 'call' | 'bridge' | 'help';

type ReadlineInterface = ReturnType<typeof createInterface>;

interface ParsedCli {
  command: CliCommand;
  toolName?: string;
  connection?: HostConnection;
  taskFocus: string;
  policyMode: ExecutionPolicyMode;
  toolArgs: Record<string, unknown>;
  json: boolean;
  yes: boolean;
}

const HELP = `Superpower MCP Host v1.3

Usage:
  superpower connect --http <url> [options]
  superpower connect --stdio <command> [--server-arg <arg> ...] [options]
  superpower tools   --http <url> [options]
  superpower call <tool> --args <json> --http <url> [options]
  superpower bridge --http <url> [options]
  superpower bridge --stdio <command> [--server-arg <arg> ...] [options]

Connection options:
  --http <url>                 Connect with MCP Streamable HTTP
  --stdio <command>            Spawn an MCP server over stdio
  --server-arg <arg>           Add one stdio server argument (repeatable)
  --header-env <name=ENV>      Read an HTTP header value from ENV (repeatable)
  --server-env <name=ENV>      Read a child-process env value from ENV (repeatable)

Gateway options:
  --focus <text>               Task focus used by the local Tool Router
  --policy <audit|guarded>     Execution policy (default: audit)
  --yes                        Explicitly approve guarded high/critical calls
  --json                       Emit machine-readable JSON and skip interactive shell
  --args <json>                JSON object passed to a direct tool call
  -h, --help                   Show this help

Desktop bridge:
  bridge keeps one MCP session alive and reads newline-delimited JSON requests from stdin.
  It supports both Streamable HTTP and local stdio MCP servers for first-party native shells.

Interactive commands after 'connect':
  tools                        Show the currently routed tool catalog
  focus <text>                 Change task focus locally
  policy <audit|guarded>       Change execution policy
  call <tool> [json]           Call a tool (json defaults to {})
  stats                        Show privacy-safe session telemetry
  help                         Show interactive commands
  exit | quit                  Close the MCP connection

Examples:
  superpower connect --http http://localhost:3000/mcp --focus "find files"
  superpower connect --stdio node --server-arg server.js
  superpower call search --args '{"query":"MCP"}' --http http://localhost:3000/mcp
  superpower bridge --http http://localhost:3000/mcp --policy guarded
  superpower bridge --stdio npx --server-arg @modelcontextprotocol/server-filesystem --server-arg . --policy guarded
`;

const takeValue = (argv: string[], index: number, flag: string): string => {
  const value = argv[index + 1];
  if (value === undefined) throw new Error(`${flag} requires a value.`);
  return value;
};

const parseReference = (value: string, flag: string): [string, string] => {
  const separator = value.indexOf('=');
  if (separator <= 0 || separator === value.length - 1) {
    throw new Error(`${flag} expects name=ENV_VAR.`);
  }
  return [value.slice(0, separator), value.slice(separator + 1)];
};

const parseJsonObject = (value: string): Record<string, unknown> => {
  let parsed: unknown;
  try {
    parsed = JSON.parse(value);
  } catch {
    throw new Error('--args must be valid JSON.');
  }

  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    throw new Error('--args must be a JSON object.');
  }
  return parsed as Record<string, unknown>;
};

const parseCli = (argv: string[]): ParsedCli => {
  if (argv.length === 0 || argv[0] === '-h' || argv[0] === '--help') {
    return {
      command: 'help',
      taskFocus: '',
      policyMode: 'audit',
      toolArgs: {},
      json: false,
      yes: false,
    };
  }

  const rawCommand = argv[0];
  if (!['connect', 'tools', 'call', 'bridge'].includes(rawCommand)) throw new Error(`Unknown command: ${rawCommand}`);
  const command = rawCommand as Exclude<CliCommand, 'help'>;
  let index = 1;
  let toolName: string | undefined;

  if (command === 'call' && argv[index] && !argv[index].startsWith('-')) {
    toolName = argv[index];
    index += 1;
  }

  let httpUrl: string | undefined;
  let stdioCommand: string | undefined;
  const serverArgs: string[] = [];
  const headerFrom: Record<string, string> = {};
  const envFrom: Record<string, string> = {};
  let taskFocus = '';
  let policyMode: ExecutionPolicyMode = 'audit';
  let toolArgs: Record<string, unknown> = {};
  let json = false;
  let yes = false;

  while (index < argv.length) {
    const flag = argv[index];
    switch (flag) {
      case '--http':
        httpUrl = takeValue(argv, index, flag);
        index += 2;
        break;
      case '--stdio':
        stdioCommand = takeValue(argv, index, flag);
        index += 2;
        break;
      case '--server-arg':
        serverArgs.push(takeValue(argv, index, flag));
        index += 2;
        break;
      case '--header-env': {
        const [header, envName] = parseReference(takeValue(argv, index, flag), flag);
        headerFrom[header] = envName;
        index += 2;
        break;
      }
      case '--server-env': {
        const [name, envName] = parseReference(takeValue(argv, index, flag), flag);
        envFrom[name] = envName;
        index += 2;
        break;
      }
      case '--focus':
        taskFocus = takeValue(argv, index, flag);
        index += 2;
        break;
      case '--policy': {
        const value = takeValue(argv, index, flag);
        if (value !== 'audit' && value !== 'guarded') throw new Error('--policy must be audit or guarded.');
        policyMode = value;
        index += 2;
        break;
      }
      case '--args':
        toolArgs = parseJsonObject(takeValue(argv, index, flag));
        index += 2;
        break;
      case '--json':
        json = true;
        index += 1;
        break;
      case '--yes':
        yes = true;
        index += 1;
        break;
      case '-h':
      case '--help':
        return {
          command: 'help',
          taskFocus,
          policyMode,
          toolArgs,
          json,
          yes,
        };
      default:
        throw new Error(`Unknown option: ${flag}`);
    }
  }

  if (httpUrl && stdioCommand) throw new Error('Choose either --http or --stdio, not both.');
  if (!httpUrl && !stdioCommand) throw new Error('A connection is required: use --http or --stdio.');
  if (command === 'call' && !toolName) throw new Error('call requires a tool name.');

  const connection: HostConnection = httpUrl
    ? {
        kind: 'http',
        url: httpUrl,
        ...(Object.keys(headerFrom).length > 0 ? { headerFrom } : {}),
      }
    : {
        kind: 'stdio',
        command: stdioCommand!,
        args: serverArgs,
        ...(Object.keys(envFrom).length > 0 ? { envFrom } : {}),
      };

  return {
    command,
    toolName,
    connection,
    taskFocus,
    policyMode,
    toolArgs,
    json,
    yes,
  };
};

const print = (value: unknown, json = false): void => {
  if (json) {
    stdout.write(`${JSON.stringify(value)}\n`);
    return;
  }
  stdout.write(`${JSON.stringify(value, null, 2)}\n`);
};

const createConfirmationHandler = (
  cli: ParsedCli,
  readline: ReadlineInterface | null,
): McpGatewayConfirmationHandler | undefined => {
  if (cli.yes) return async () => true;
  if (cli.policyMode !== 'guarded') return undefined;
  if (!readline) return undefined;

  return async context => {
    const reasons = context.policy.reasons.length > 0 ? `\nReasons: ${context.policy.reasons.join('; ')}` : '';
    const answer = await readline.question(
      `Allow ${context.policy.risk}-risk tool ${context.toolName}?${reasons}\nType yes to continue: `,
    );
    return answer.trim().toLowerCase() === 'yes';
  };
};

const describeRoute = async (host: ConnectedSuperpowerHost, json = false): Promise<void> => {
  const routed = await host.gateway.listTools();
  const payload = {
    transport: host.transportKind,
    taskFocus: host.gateway.getTaskFocus(),
    policyMode: host.gateway.getPolicyMode(),
    contextBudget: host.gateway.getContextBudget(),
    omitted: routed.omitted,
    queryUsed: routed.queryUsed,
    tools: routed.tools.map(tool => ({
      name: tool.name,
      description: tool.description ?? '',
      schema: tool.schema ?? '{}',
    })),
  };
  print(payload, json);
};

const findToolDescription = async (host: ConnectedSuperpowerHost, toolName: string): Promise<string> => {
  const response = await host.client.listTools();
  return response.tools.find(tool => tool.name === toolName)?.description ?? '';
};

const callTool = async (
  host: ConnectedSuperpowerHost,
  toolName: string,
  args: Record<string, unknown>,
): Promise<unknown> => {
  const description = await findToolDescription(host, toolName);
  return host.gateway.callTool(toolName, args, description);
};

const runInteractiveShell = async (host: ConnectedSuperpowerHost, readline: ReadlineInterface): Promise<void> => {
  stdout.write('Interactive MCP shell. Type help for commands.\n');
  while (true) {
    const input = (await readline.question('superpower> ')).trim();
    if (!input) continue;
    if (input === 'exit' || input === 'quit') return;
    if (input === 'help') {
      stdout.write('tools | focus <text> | policy <audit|guarded> | call <tool> [json] | stats | exit\n');
      continue;
    }

    try {
      if (input === 'tools') {
        await describeRoute(host);
        continue;
      }
      if (input === 'stats') {
        print(host.gateway.getTelemetrySummary());
        continue;
      }
      if (input.startsWith('focus ')) {
        host.gateway.setTaskFocus(input.slice('focus '.length));
        await describeRoute(host);
        continue;
      }
      if (input.startsWith('policy ')) {
        const policy = input.slice('policy '.length).trim();
        if (policy !== 'audit' && policy !== 'guarded') throw new Error('Policy must be audit or guarded.');
        host.gateway.setPolicyMode(policy);
        stdout.write(`Execution policy: ${policy}.\n`);
        continue;
      }
      if (input.startsWith('call ')) {
        const body = input.slice('call '.length).trim();
        const firstSpace = body.indexOf(' ');
        const toolName = firstSpace === -1 ? body : body.slice(0, firstSpace);
        const argsText = firstSpace === -1 ? '{}' : body.slice(firstSpace + 1).trim() || '{}';
        const args = parseJsonObject(argsText);
        print(await callTool(host, toolName, args));
        continue;
      }

      stdout.write('Unknown command. Type help.\n');
    } catch (error) {
      stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
    }
  }
};

const main = async (): Promise<void> => {
  const cli = parseCli(process.argv.slice(2));
  if (cli.command === 'help') {
    stdout.write(HELP);
    return;
  }

  if (cli.command === 'bridge') {
    await runBridge({
      connection: cli.connection!,
      taskFocus: cli.taskFocus,
      policyMode: cli.policyMode,
    });
    return;
  }

  const interactive = Boolean(stdin.isTTY && stdout.isTTY && !cli.json);
  const readline = interactive ? createInterface({ input: stdin, output: stdout }) : null;
  const confirm = createConfirmationHandler(cli, readline);
  const host = await connectSuperpowerHost({
    connection: cli.connection!,
    taskFocus: cli.taskFocus,
    policyMode: cli.policyMode,
    confirm,
  });

  try {
    if (cli.command === 'tools') {
      await describeRoute(host, cli.json);
      return;
    }

    if (cli.command === 'call') {
      const result = await callTool(host, cli.toolName!, cli.toolArgs);
      print(result, cli.json);
      return;
    }

    await describeRoute(host, cli.json);
    if (readline) await runInteractiveShell(host, readline);
  } finally {
    readline?.close();
    await host.close();
  }
};

void main().catch(error => {
  if (error instanceof McpGatewayConfirmationRequiredError) {
    stderr.write('Guarded mode blocked a high/critical action because no interactive confirmation was available.\n');
    process.exitCode = 3;
    return;
  }
  if (error instanceof McpGatewayRejectedError) {
    stderr.write('The guarded MCP action was rejected.\n');
    process.exitCode = 4;
    return;
  }

  stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
  process.exitCode = 1;
});
