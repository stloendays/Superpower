/**
 * ChatGPT-specific additions to the shared SuperAssistant instructions.
 * Keep this file intentionally small: the cross-platform JSONL protocol lives
 * in instructionGeneratorJson.ts so it is not repeated for every website.
 */
export const chatgptInstructions = `
ChatGPT platform notes:
- Emit MCP JSONL calls as visible assistant text so Superpower's DOM observer can capture them.
- Do not place MCP calls in Canvas/can mode; use the normal chat response.
`;
