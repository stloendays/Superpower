import { createLogger } from '@extension/shared/lib/logger';

const logger = createLogger('DesktopConversationForwarder');
const DESKTOP_ENDPOINT = 'http://127.0.0.1:32148/v1/conversation';
const DESKTOP_PROMPT_NEXT_ENDPOINT = 'http://127.0.0.1:32148/v1/prompt/next';
const DESKTOP_PROMPT_ACK_ENDPOINT = 'http://127.0.0.1:32148/v1/prompt/ack';
const BRIDGE_HEADER = 'x-superpower-conversation-bridge';
const FORWARD_TIMEOUT_MS = 1800;

const PROVIDERS = ['chatgpt', 'gemini', 'grok', 'perplexity'] as const;
type ProviderId = (typeof PROVIDERS)[number];
type PromptTarget = ProviderId | 'auto';

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
  provider: PromptTarget;
  claimedBy: ProviderId;
  timestamp: number;
}

interface PromptAckPayload {
  promptId?: unknown;
  success?: unknown;
  message?: unknown;
}

const providerFromSender = (sender: chrome.runtime.MessageSender): ProviderId | null => {
  const rawUrl = sender.url || sender.tab?.url || '';
  try {
    const host = new URL(rawUrl).hostname.toLowerCase();
    if (host === 'chatgpt.com' || host.endsWith('.chatgpt.com') || host === 'chat.openai.com') return 'chatgpt';
    if (host === 'gemini.google.com' || host.endsWith('.gemini.google.com')) return 'gemini';
    if (host === 'grok.com' || host.endsWith('.grok.com')) return 'grok';
    if (host === 'perplexity.ai' || host.endsWith('.perplexity.ai')) return 'perplexity';
    return null;
  } catch {
    return null;
  }
};

const isPromptTarget = (value: unknown): value is PromptTarget =>
  value === 'auto' || (typeof value === 'string' && (PROVIDERS as readonly string[]).includes(value));

const isProvider = (value: unknown): value is ProviderId =>
  typeof value === 'string' && (PROVIDERS as readonly string[]).includes(value);

const isConversationPayload = (payload: ConversationPayload, provider: ProviderId): boolean =>
  typeof payload.eventId === 'string' &&
  typeof payload.sessionId === 'string' &&
  payload.source === provider &&
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

const forwardConversationEvent = async (payload: ConversationPayload, provider: ProviderId): Promise<boolean> => {
  if (!isConversationPayload(payload, provider)) return false;

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

const fetchNextDesktopPrompt = async (provider: ProviderId): Promise<DesktopPrompt | null> => {
  try {
    const response = await fetchWithTimeout(
      `${DESKTOP_PROMPT_NEXT_ENDPOINT}?provider=${encodeURIComponent(provider)}`,
      {
        method: 'GET',
        cache: 'no-store',
        headers: {
          [BRIDGE_HEADER]: '1',
        },
      },
    );
    if (!response.ok) return null;

    const payload = (await response.json()) as { prompt?: unknown };
    if (!payload.prompt || typeof payload.prompt !== 'object') return null;

    const prompt = payload.prompt as Partial<DesktopPrompt>;
    if (typeof prompt.promptId !== 'string' || typeof prompt.text !== 'string') return null;
    return {
      promptId: prompt.promptId,
      text: prompt.text,
      provider: isPromptTarget(prompt.provider) ? prompt.provider : 'auto',
      claimedBy: isProvider(prompt.claimedBy) ? prompt.claimedBy : provider,
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
    const provider = providerFromSender(sender);
    if (!provider) {
      if (message?.type?.startsWith?.('desktop:')) {
        sendResponse({ success: false, error: 'unsupported_sender' });
      }
      return false;
    }

    if (message?.type === 'desktop:conversation-upsert') {
      void forwardConversationEvent(message.payload || {}, provider).then(success => {
        sendResponse({ success });
      });
      return true;
    }

    if (message?.type === 'desktop:prompt-poll') {
      // Only the active supported AI tab may claim a desktop prompt. This keeps
      // Desktop routing aligned with the tab the user is currently viewing.
      if (sender.tab && sender.tab.active === false) {
        sendResponse({ success: true, prompt: null });
        return false;
      }
      if (message.provider && message.provider !== provider) {
        sendResponse({ success: false, prompt: null, error: 'provider_mismatch' });
        return false;
      }

      void fetchNextDesktopPrompt(provider).then(prompt => {
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
