import { createLogger } from '@extension/shared/lib/logger';
import { insertTextToChatInput, submitChatInput } from '../components/websites/chatgpt/chatInputHandler';

const logger = createLogger('DesktopConversationSync');

const MESSAGE_SELECTOR = '[data-message-author-role="user"], [data-message-author-role="assistant"]';
const MAX_MESSAGE_LENGTH = 64 * 1024;
const MAX_DESKTOP_PROMPT_LENGTH = 32 * 1024;
const SCAN_DELAY_MS = 120;
const PROMPT_POLL_INTERVAL_MS = 900;

interface ConversationSnapshot {
  eventId: string;
  sessionId: string;
  source: 'chatgpt';
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
  timestamp: number;
}

class DesktopConversationSync {
  private observer: MutationObserver | null = null;
  private scanTimer: ReturnType<typeof setTimeout> | null = null;
  private routeTimer: ReturnType<typeof setInterval> | null = null;
  private promptTimer: ReturnType<typeof setInterval> | null = null;
  private promptPolling = false;
  private lastRoute = '';
  private lastSnapshots = new Map<string, string>();

  start(): void {
    if (this.observer || !this.isSupportedHost() || !document.body) return;

    this.lastRoute = this.sessionId();
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

    this.promptTimer = setInterval(() => {
      void this.pollDesktopPrompt();
    }, PROMPT_POLL_INTERVAL_MS);

    this.scheduleScan();
    void this.pollDesktopPrompt();
    window.addEventListener('beforeunload', this.stop, { once: true });
    logger.debug('ChatGPT conversation observer started');
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
  };

  private isSupportedHost(): boolean {
    const host = window.location.hostname.toLowerCase();
    return host === 'chatgpt.com' || host.endsWith('.chatgpt.com') || host === 'chat.openai.com';
  }

  private sessionId(): string {
    return `${window.location.hostname}${window.location.pathname}`;
  }

  private isGenerating(): boolean {
    return Boolean(
      document.querySelector(
        'button[data-testid="stop-button"], button[aria-label*="Stop generating"], button[aria-label*="Stop streaming"]',
      ),
    );
  }

  private scheduleScan(): void {
    if (this.scanTimer) return;
    this.scanTimer = setTimeout(() => {
      this.scanTimer = null;
      this.scan();
    }, SCAN_DELAY_MS);
  }

  private scan(): void {
    const elements = Array.from(document.querySelectorAll<HTMLElement>(MESSAGE_SELECTOR));
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

  private async pollDesktopPrompt(): Promise<void> {
    if (this.promptPolling || this.isGenerating()) return;
    if (typeof chrome === 'undefined' || !chrome.runtime?.sendMessage) return;

    this.promptPolling = true;
    try {
      const response = (await chrome.runtime.sendMessage({ type: 'desktop:prompt-poll' })) as
        | { success?: boolean; prompt?: DesktopPrompt | null }
        | undefined;
      const prompt = response?.prompt;
      if (!response?.success || !prompt) return;
      if (typeof prompt.promptId !== 'string' || typeof prompt.text !== 'string') return;

      const text = prompt.text.trim().slice(0, MAX_DESKTOP_PROMPT_LENGTH);
      if (!text) {
        await this.acknowledgePrompt(prompt.promptId, false, 'Desktop prompt was empty.');
        return;
      }

      const inserted = insertTextToChatInput(text);
      if (!inserted) {
        await this.acknowledgePrompt(prompt.promptId, false, 'ChatGPT input was not available.');
        return;
      }

      const submitted = await submitChatInput(5000);
      await this.acknowledgePrompt(
        prompt.promptId,
        submitted,
        submitted
          ? 'Prompt submitted to the active ChatGPT conversation.'
          : 'Prompt was inserted, but automatic submission failed. Check the browser composer.',
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
