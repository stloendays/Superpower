import { jsonSchemaToCsn } from './schema_converter';
import { chatgptInstructions } from './website_specific_instruction/chatgpt';
import { geminiInstructions } from './website_specific_instruction/gemini';
import { createLogger } from '@extension/shared/lib/logger';

interface ToolDefinition {
  name: string;
  schema: string;
  description: string;
}

type JsonSchema = {
  properties?: Record<string, { description?: string }>;
  [key: string]: unknown;
};

const logger = createLogger('InstructionGeneratorJSON');

const BASE_INSTRUCTIONS = `[SuperAssistant Operational Instructions][IMPORTANT]

<system>
You can invoke only the MCP tools listed below. When a tool is needed, emit a JSON Lines call that Superpower can capture and execute.

Tool-call protocol:
- Put every tool call in a fenced \`\`\`jsonl block.
- Each fenced block contains exactly one complete call.
- Start with {"type":"function_call_start","name":"FUNCTION_NAME","call_id":1}.
- Add one {"type":"parameter","key":"PARAMETER_NAME","value":PARAMETER_VALUE} line per argument.
- End with {"type":"function_call_end","call_id":1}.
- A short {"type":"description","text":"..."} line after function_call_start is optional.
- call_id must be unique and increase for each new call.
- Include every required parameter. Include optional parameters only when useful.
- Preserve JSON value types: strings, numbers, booleans, arrays, objects, and null must be valid JSON values.
- Never invent tool names, required values, or function results.
- If a required value is missing and cannot be inferred safely, ask the user for it.
- You may emit up to five independent calls in one response, each in its own fenced block.
- If a call depends on an earlier result, stop at that dependency and wait for the returned <function_results> before continuing.
- After emitting the current tool-call batch, stop and wait for the results.
- Do not wrap MCP calls in Python or other pseudo-tool syntax.

Example:
\`\`\`jsonl
{"type":"function_call_start","name":"function_name","call_id":1}
{"type":"description","text":"Short description of the action"}
{"type":"parameter","key":"parameter_1","value":"value_1"}
{"type":"function_call_end","call_id":1}
\`\`\`
`;

const CSN_LEGEND = `Schema notation: s=string; i=integer; n=number; b=boolean; a[T]=array; o {p {...}}=object; r=required; e[...]=enum; u[...]=union; lit[...]=literal; ap f=additional properties forbidden.`;

const compactWhitespace = (value: string): string => value.replace(/\s+/g, ' ').trim();

const getParameterDescriptions = (schema: JsonSchema): string => {
  if (!schema.properties) {
    return '';
  }

  return Object.entries(schema.properties)
    .map(([name, details]) => {
      const description = typeof details?.description === 'string' ? compactWhitespace(details.description) : '';
      return description ? `${name}: ${description}` : '';
    })
    .filter(Boolean)
    .join('; ');
};

const formatTool = (tool: ToolDefinition): string => {
  const description = compactWhitespace(tool.description || '');

  try {
    const schema = JSON.parse(tool.schema) as JsonSchema;
    const compressedSchema = jsonSchemaToCsn(schema);

    if (!compressedSchema || compressedSchema.includes('undefined')) {
      throw new Error('Compressed schema is incomplete');
    }

    const parameterDescriptions = getParameterDescriptions(schema);
    const lines = [`- ${tool.name}${description ? ` — ${description}` : ''}`, `  schema: ${compressedSchema}`];

    if (parameterDescriptions) {
      lines.push(`  params: ${parameterDescriptions}`);
    }

    return lines.join('\n');
  } catch (error) {
    logger.warn(`Unable to compress schema for ${tool.name}; using minified JSON schema instead.`, error);

    try {
      const minifiedSchema = JSON.stringify(JSON.parse(tool.schema));
      return `- ${tool.name}${description ? ` — ${description}` : ''}\n  schema-json: ${minifiedSchema}`;
    } catch (schemaError) {
      logger.error(`Unable to parse schema for ${tool.name}.`, schemaError);
      return `- ${tool.name}${description ? ` — ${description}` : ''}\n  schema: unavailable`;
    }
  }
};

const getWebsiteSpecificInstructions = (): string => {
  if (typeof window === 'undefined') {
    return '';
  }

  const currentHost = window.location.hostname;

  if (currentHost.includes('gemini')) {
    return geminiInstructions;
  }

  if (currentHost.includes('chatgpt')) {
    return chatgptInstructions;
  }

  return '';
};

/**
 * Generate compact MCP instructions for the enabled tools.
 *
 * The tool schemas use the existing compressed schema notation (CSN) so the
 * prompt preserves types and required fields without expanding every schema
 * into verbose Markdown. Parameter descriptions are retained in one compact
 * line because they often carry semantics that are not represented by types.
 */
export const generateInstructionsJson = (
  tools: ToolDefinition[],
  customInstructions?: string,
  customInstructionsEnabled?: boolean,
): string => {
  if (!tools || tools.length === 0) {
    return '# No tools available\n\nConnect to the MCP server to see available tools.';
  }

  const sections = [BASE_INSTRUCTIONS];
  const websiteSpecificInstructions = getWebsiteSpecificInstructions();

  if (websiteSpecificInstructions) {
    sections.push(websiteSpecificInstructions.trim());
  }

  sections.push('## AVAILABLE MCP TOOLS', CSN_LEGEND, tools.map(formatTool).join('\n\n'));

  if (customInstructionsEnabled && customInstructions?.trim()) {
    sections.push(`<custom_instructions>\n${customInstructions.trim()}\n</custom_instructions>`);
  }

  sections.push('</system>', 'User Interaction Starts here:');

  return `${sections.join('\n\n')}\n`;
};
