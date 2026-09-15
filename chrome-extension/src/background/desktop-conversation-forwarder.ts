import { createLogger } from '@extension/shared/lib/logger';

const logger = createLogger('DesktopConversationForwarder');
const DESKTOP_ENDPOINT = 'http://127.0.0.1:32147/v1/conversation';
const BRIDGE_HEADER = 'x-superpower-conversation-bridge';
const FORWARD_TIMEOUT_MS = 1800;

interface ConversationPayload {
  eventId?: unknown;
  sessionId?: unknown;
  source?: unknown;
  role?: unknown;
  text?: unknown;
  phase?: unknown;
  url?: unknown;
  title?: unknown;
  timestamp?: unknown;
}

const isChatGptSender = (sender: chrome.runtime.MessageSender): boolean => {
  const rawUrl = sender.url || sender.tab?.url || '';
  try {
    const host = new URL(rawUrl).hostname.toLowerCase();
    return host === 'chatgpt.com' || host.endsWith('.chatgpt.com') || host === 'chat.openai.com';
  } catch {
    return false;
  }
};

const isConversationPayload = (payload: ConversationPayload): boolean =>
  typeof payload.eventId === 'string' &&
  typeof payload.sessionId === 'string' &&
  payload.source === 'chatgpt' &&
  (payload.role === 'user' || payload.role === 'assistant') &&
  typeof payload.text === 'string' &&
  (payload.phase === 'streaming' || payload.phase === 'completed');

const forwardConversationEvent = async (payload: ConversationPayload): Promise<boolean> => {
  if (!isConversationPayload(payload)) return false;

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), FORWARD_TIMEOUT_MS);
  try {
    const response = await fetch(DESKTOP_ENDPOINT, {
      method: 'POST',
      cache: 'no-store',
      headers: {
        'content-type': 'application/json',
        [BRIDGE_HEADER]: '1',
      },
      body: JSON.stringify(payload),
      signal: controller.signal,
    });
    return response.ok;
  } catch (error) {
    // Desktop sync is optional and local-only. Avoid logging conversation contents.
    logger.debug('Desktop bridge unavailable:', error instanceof Error ? error.message : String(error));
    return false;
  } finally {
    clearTimeout(timeout);
  }
};

const isServiceWorkerContext =
  typeof window === 'undefined' && typeof chrome !== 'undefined' && Boolean(chrome.runtime?.onMessage);

if (isServiceWorkerContext) {
  chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message?.type !== 'desktop:conversation-upsert') return false;
    if (!isChatGptSender(sender)) {
      sendResponse({ success: false, error: 'unsupported_sender' });
      return false;
    }

    void forwardConversationEvent(message.payload || {}).then(success => {
      sendResponse({ success });
    });
    return true;
  });
}
