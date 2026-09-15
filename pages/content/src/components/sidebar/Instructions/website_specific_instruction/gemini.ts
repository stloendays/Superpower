/**
 * Gemini-specific additions to the shared SuperAssistant instructions.
 * Keep this intentionally small: the cross-platform JSONL protocol is already
 * defined by instructionGeneratorJson.ts and should not be paid for twice.
 */
export const geminiInstructions = `
Gemini platform notes:
- Emit MCP calls directly as visible JSONL text so Superpower can capture them.
- Do not translate MCP calls into Python wrappers or pseudo-code; only the shared JSONL format is executable.
`;
