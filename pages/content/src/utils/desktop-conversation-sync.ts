import { createLogger } from '@extension/shared/lib/logger';
import {
  insertTextToChatInput as insertChatGptText,
  submitChatInput as submitChatGptInput,
} from '../components/websites/chatgpt/chatInputHandler';
import {
  insertTextToChatInput as insertGeminiText,
  submitChatInput as submitGeminiInput,
} from '../components/websites/gemini/chatInputHandler';
import {
  insertTextToChatInput as insertGrokText,
  submitChatInput as submitGrokInput,
} from '../components/websites/grok/chatInputHandler';
import {
  insertTextToChatInput as insertPerplexityText,
  submitChatInput as submitPerplexityInput,
} from '../components/websites/perplexity/chatInputHandler';

const logger = createLogger('DesktopConversationSync');

const CHATGPT_MESSAGE_SELECTOR = '[data-message-author-role="user"], [data-message-author-role="assistant"]';
const MAX_MESSAGE_LENGTH = 64 * 1024;
const MAX_DESKTOP_PROMPT_LENGTH = 32 * 1024;
const SCAN_DELAY_MS = 120;
const PROMPT_POLL_INTERVAL_MS = 900;

const PROVIDERS = ['chatgpt', 'gemini', 'grok', 'perplexity'] as const;
type ProviderId = (typeof PROVIDERS)[number];
type PromptTarget = ProviderId | 'auto';

interface ConversationSnapshot {
  eventId: string;
  sessionId: string;
  source: ProviderId;
  role: 'user' | 'assistant';
  text: string;
  phase: 'streaming' | 'completed';
  url: string;
  title: string;
  timestamp: number;
}

interface DesktopPrompt {
  promptId: string;
  text: string;
  provider?: PromptTarget;
  claimedBy?: ProviderId;
  timestamp: number;
}

const providerForHost = (host: string): ProviderId | null => {
  const normalized = host.toLowerCase();
  if (normalized === 'chatgpt.com' || normalized.endsWith('.chatgpt.com') || normalized === 'chat.openai.com') {
    return 'chatgpt';
  }
  if (normalized === 'gemini.google.com' || normalized.endsWith('.gemini.google.com')) return 'gemini';
  if (normalized === 'grok.com' || normalized.endsWith('.grok.com')) return 'grok';
  if (normalized === 'perplexity.ai' || normalized.endsWith('.perplexity.ai')) return 'perplexity';
  return null;
};

const providerDisplayName = (provider: ProviderId): string => {
  if (provider === 'chatgpt') return 'ChatGPT';
  if (provider === 'gemini') return 'Gemini';
  if (provider === 'grok') return 'Grok';
  return 'Perplexity';
};

class DesktopConversationSync {
  private observer: MutationObserver | null = null;
  private scanTimer: ReturnType<typeof setTimeout> | null = null;
  private routeTimer: ReturnType<typeof setInterval> | null = null;
  private promptTimer: ReturnType<typeof setInterval> | null = null;
  private promptPolling = false;
  private lastRoute = '';
  private lastSnapshots = new Map<string, string>();
  private provider: ProviderId | null = null;

  start(): void {
    if (this.promptTimer || !document.body) return;

    this.provider = providerForHost(window.location.hostname);
    if (!this.provider) return;

    this.lastRoute = this.sessionId();

    // Transcript mirroring remains intentionally conservative. ChatGPT exposes
    // stable role markers; other providers are Quick Ask send targets for now.
    if (this.provider === 'chatgpt') {
      this.observer = new MutationObserver(() => this.scheduleScan());
      this.observer.observe(document.body, {
        childList: true,
        characterData: true,
        subtree: true,
      });

      this.routeTimer = setInterval(() => {
        const route = this.sessionId();
        if (route !== this.lastRoute) {
          this.lastRoute = route;
          this.lastSnapshots.clear();
          this.scheduleScan();
        }
      }, 750);
      this.scheduleScan();
    }

    this.promptTimer = setInterval(() => {
      void this.pollDesktopPrompt();
    }, PROMPT_POLL_INTERVAL_MS);

    void this.pollDesktopPrompt();
    window.addEventListener('beforeunload', this.stop, { once: true });
    logger.debug(`Desktop relay started for ${providerDisplayName(this.provider)}`);
  }

  stop = (): void => {
    this.observer?.disconnect();
    this.observer = null;
    if (this.scanTimer) clearTimeout(this.scanTimer);
    this.scanTimer = null;
    if (this.routeTimer) clearInterval(this.routeTimer);
    this.routeTimer = null;
    if (this.promptTimer) clearInterval(this.promptTimer);
    this.promptTimer = null;
    this.promptPolling = false;
    this.lastSnapshots.clear();
    this.provider = null;
  };

  private sessionId(): string {
    return `${this.provider ?? 'unknown'}:${window.location.hostname}${window.location.pathname}`;
  }

  private isGenerating(): boolean {
    if (!this.provider) return false;

    if (this.provider === 'chatgpt') {
      return Boolean(
        document.querySelector(
          'button[data-testid="stop-button"], button[aria-label*="Stop generating"], button[aria-label*="Stop streaming"]',
        ),
      );
    }

    if (this.provider === 'gemini') {
      return Boolean(
        document.querySelector(
          'button[aria-label*="Stop" i], button[mattooltip*="Stop" i], .stop-button, button.stop-response-button',
        ),
      );
    }

    return Boolean(
      document.querySelector(
        'button[data-testid="stop-button"], button[aria-label*="Stop" i], button[title*="Stop" i]',
      ),
    );
  }

  private scheduleScan(): void {
    if (this.provider !== 'chatgpt' || this.scanTimer) return;
    this.scanTimer = setTimeout(() => {
      this.scanTimer = null;
      this.scanChatGpt();
    }, SCAN_DELAY_MS);
  }

  private scanChatGpt(): void {
    if (this.provider !== 'chatgpt') return;

    const elements = Array.from(document.querySelectorAll<HTMLElement>(CHATGPT_MESSAGE_SELECTOR));
    if (elements.length === 0) return;

    const generating = this.isGenerating();
    const lastAssistantIndex = elements.reduce(
      (last, element, index) => (element.getAttribute('data-message-author-role') === 'assistant' ? index : last),
      -1,
    );

    elements.forEach((element, index) => {
      const roleValue = element.getAttribute('data-message-author-role');
      if (roleValue !== 'user' && roleValue !== 'assistant') return;

      const text = this.extractMessageText(element);
      if (!text) return;

      const phase: ConversationSnapshot['phase'] =
        roleValue === 'assistant' && generating && index === lastAssistantIndex ? 'streaming' : 'completed';
      const eventId = `${this.sessionId()}:${roleValue}:${index}`;
      const fingerprint = `${phase}\n${text}`;
      if (this.lastSnapshots.get(eventId) === fingerprint) return;
      this.lastSnapshots.set(eventId, fingerprint);

      const snapshot: ConversationSnapshot = {
        eventId,
        sessionId: this.sessionId(),
        source: 'chatgpt',
        role: roleValue,
        text: text.slice(0, MAX_MESSAGE_LENGTH),
        phase,
        url: window.location.href.slice(0, 2048),
        title: document.title.slice(0, 256),
        timestamp: Date.now(),
      };
      this.forward(snapshot);
    });
  }

  private extractMessageText(element: HTMLElement): string {
    const content = element.querySelector<HTMLElement>('.markdown, [data-message-content]') ?? element;
    return (content.innerText || content.textContent || '').replace(/\u00a0/g, ' ').trim();
  }

  private forward(snapshot: ConversationSnapshot): void {
    if (typeof chrome === 'undefined' || !chrome.runtime?.sendMessage) return;
    chrome.runtime
      .sendMessage({
        type: 'desktop:conversation-upsert',
        payload: snapshot,
      })
      .catch(error => {
        // Desktop is optional. Do not surface noisy errors when it is closed or disconnected.
        logger.debug(
          'Desktop conversation bridge unavailable:',
          error instanceof Error ? error.message : String(error),
        );
      });
  }

  private insertPrompt(text: string): boolean {
    if (this.provider === 'chatgpt') return insertChatGptText(text);
    if (this.provider === 'gemini') return insertGeminiText(text);
    if (this.provider === 'grok') return insertGrokText(text);
    if (this.provider === 'perplexity') return insertPerplexityText(text);
    return false;
  }

  private async submitPrompt(): Promise<boolean> {
    if (this.provider === 'chatgpt') return await submitChatGptInput(5000);
    if (this.provider === 'gemini') return submitGeminiInput();
    if (this.provider === 'grok') return await submitGrokInput(5000);
    if (this.provider === 'perplexity') return await submitPerplexityInput(5000);
    return false;
  }

  private async pollDesktopPrompt(): Promise<void> {
    if (this.promptPolling || this.isGenerating() || !this.provider) return;
    if (typeof chrome === 'undefined' || !chrome.runtime?.sendMessage) return;

    this.promptPolling = true;
    try {
      const response = (await chrome.runtime.sendMessage({
        type: 'desktop:prompt-poll',
        provider: this.provider,
      })) as { success?: boolean; prompt?: DesktopPrompt | null } | undefined;
      const prompt = response?.prompt;
      if (!response?.success || !prompt) return;
      if (typeof prompt.promptId !== 'string' || typeof prompt.text !== 'string') return;
      if (prompt.claimedBy && prompt.claimedBy !== this.provider) return;

      const text = prompt.text.trim().slice(0, MAX_DESKTOP_PROMPT_LENGTH);
      if (!text) {
        await this.acknowledgePrompt(prompt.promptId, false, 'Desktop prompt was empty.');
        return;
      }

      const inserted = this.insertPrompt(text);
      if (!inserted) {
        await this.acknowledgePrompt(
          prompt.promptId,
          false,
          `${providerDisplayName(this.provider)} input was not available.`,
        );
        return;
      }

      const submitted = await this.submitPrompt();
      await this.acknowledgePrompt(
        prompt.promptId,
        submitted,
        submitted
          ? `Prompt submitted to the active ${providerDisplayName(this.provider)} conversation.`
          : `Prompt was inserted into ${providerDisplayName(this.provider)}, but automatic submission failed. Check the browser composer.`,
      );
    } catch (error) {
      logger.debug('Desktop prompt polling unavailable:', error instanceof Error ? error.message : String(error));
    } finally {
      this.promptPolling = false;
    }
  }

  private async acknowledgePrompt(promptId: string, success: boolean, message: string): Promise<void> {
    try {
      await chrome.runtime.sendMessage({
        type: 'desktop:prompt-ack',
        payload: { promptId, success, message },
      });
    } catch (error) {
      logger.debug(
        'Desktop prompt acknowledgement unavailable:',
        error instanceof Error ? error.message : String(error),
      );
    }
  }
}

const sync = new DesktopConversationSync();

const startWhenReady = (): void => {
  if (document.body) {
    sync.start();
    return;
  }
  document.addEventListener('DOMContentLoaded', () => sync.start(), { once: true });
};

if (typeof window !== 'undefined') startWhenReady();
