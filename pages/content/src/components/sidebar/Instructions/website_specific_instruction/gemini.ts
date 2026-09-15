/**
 * Gemini-specific additions to the shared SuperAssistant instructions.
 * Keep this file intentionally small: the cross-platform JSONL protocol lives
 * in instructionGeneratorJson.ts so it is not repeated for every website.
 */
export const geminiInstructions = `
Gemini platform notes:
- Emit MCP calls directly as JSONL text in the assistant response so Superpower can capture them.
- Do not translate MCP calls into Python wrappers or pseudo-code; only the shared JSONL call format is executable.
`;
