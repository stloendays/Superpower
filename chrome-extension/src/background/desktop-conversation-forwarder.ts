import { createLogger } from '@extension/shared/lib/logger';

const logger = createLogger('DesktopConversationForwarder');
const DESKTOP_ENDPOINT = 'http://127.0.0.1:32148/v1/conversation';
const DESKTOP_PROMPT_NEXT_ENDPOINT = 'http://127.0.0.1:32148/v1/prompt/next';
const DESKTOP_PROMPT_ACK_ENDPOINT = 'http://127.0.0.1:32148/v1/prompt/ack';
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

interface DesktopPrompt {
  promptId: string;
  text: string;
  timestamp: number;
}

interface PromptAckPayload {
  promptId?: unknown;
  success?: unknown;
  message?: unknown;
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

const fetchWithTimeout = async (url: string, init: RequestInit): Promise<Response> => {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), FORWARD_TIMEOUT_MS);
  try {
    return await fetch(url, { ...init, signal: controller.signal });
  } finally {
    clearTimeout(timeout);
  }
};

const forwardConversationEvent = async (payload: ConversationPayload): Promise<boolean> => {
  if (!isConversationPayload(payload)) return false;

  try {
    const response = await fetchWithTimeout(DESKTOP_ENDPOINT, {
      method: 'POST',
      cache: 'no-store',
      headers: {
        'content-type': 'application/json',
        [BRIDGE_HEADER]: '1',
      },
      body: JSON.stringify(payload),
    });
    return response.ok;
  } catch (error) {
    // Desktop sync is optional and local-only. Avoid logging conversation contents.
    logger.debug('Desktop bridge unavailable:', error instanceof Error ? error.message : String(error));
    return false;
  }
};

const fetchNextDesktopPrompt = async (): Promise<DesktopPrompt | null> => {
  try {
    const response = await fetchWithTimeout(DESKTOP_PROMPT_NEXT_ENDPOINT, {
      method: 'GET',
      cache: 'no-store',
      headers: {
        [BRIDGE_HEADER]: '1',
      },
    });
    if (!response.ok) return null;

    const payload = (await response.json()) as { prompt?: unknown };
    if (!payload.prompt || typeof payload.prompt !== 'object') return null;

    const prompt = payload.prompt as Partial<DesktopPrompt>;
    if (typeof prompt.promptId !== 'string' || typeof prompt.text !== 'string') return null;
    return {
      promptId: prompt.promptId,
      text: prompt.text,
      timestamp: typeof prompt.timestamp === 'number' ? prompt.timestamp : Date.now(),
    };
  } catch (error) {
    logger.debug('Desktop prompt queue unavailable:', error instanceof Error ? error.message : String(error));
    return null;
  }
};

const acknowledgeDesktopPrompt = async (payload: PromptAckPayload): Promise<boolean> => {
  if (typeof payload.promptId !== 'string' || typeof payload.success !== 'boolean') return false;

  try {
    const response = await fetchWithTimeout(DESKTOP_PROMPT_ACK_ENDPOINT, {
      method: 'POST',
      cache: 'no-store',
      headers: {
        'content-type': 'application/json',
        [BRIDGE_HEADER]: '1',
      },
      body: JSON.stringify({
        promptId: payload.promptId,
        success: payload.success,
        message: typeof payload.message === 'string' ? payload.message : '',
      }),
    });
    return response.ok;
  } catch (error) {
    logger.debug('Desktop prompt acknowledgement failed:', error instanceof Error ? error.message : String(error));
    return false;
  }
};

const isServiceWorkerContext =
  typeof window === 'undefined' && typeof chrome !== 'undefined' && Boolean(chrome.runtime?.onMessage);

if (isServiceWorkerContext) {
  chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (!isChatGptSender(sender)) {
      if (message?.type?.startsWith?.('desktop:')) {
        sendResponse({ success: false, error: 'unsupported_sender' });
      }
      return false;
    }

    if (message?.type === 'desktop:conversation-upsert') {
      void forwardConversationEvent(message.payload || {}).then(success => {
        sendResponse({ success });
      });
      return true;
    }

    if (message?.type === 'desktop:prompt-poll') {
      // Only the active ChatGPT tab may claim a desktop prompt. This avoids sending
      // the same desktop request into an arbitrary background conversation.
      if (sender.tab && sender.tab.active === false) {
        sendResponse({ success: true, prompt: null });
        return false;
      }

      void fetchNextDesktopPrompt().then(prompt => {
        sendResponse({ success: true, prompt });
      });
      return true;
    }

    if (message?.type === 'desktop:prompt-ack') {
      void acknowledgeDesktopPrompt(message.payload || {}).then(success => {
        sendResponse({ success });
      });
      return true;
    }

    return false;
  });
}
