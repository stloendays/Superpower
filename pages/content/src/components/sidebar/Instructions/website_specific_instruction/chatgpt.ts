/**
 * ChatGPT-specific additions to the shared SuperAssistant instructions.
 * Keep this intentionally small: the cross-platform JSONL protocol is already
 * defined by instructionGeneratorJson.ts and should not be paid for twice.
 */
export const chatgptInstructions = `
ChatGPT platform notes:
- Emit MCP JSONL calls as visible assistant text so Superpower's DOM observer can capture them.
- Keep MCP calls in the normal chat response rather than Canvas/can mode.
`;
