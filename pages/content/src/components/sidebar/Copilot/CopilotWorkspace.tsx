import React, { useEffect, useMemo, useState } from 'react';
import { Button, Typography } from '../ui';

const STORAGE_KEY = 'superpower_copilot_saved_insights';
const MAX_CONTEXT_CHARS = 12000;
const MAX_SAVED_ITEMS = 100;

interface SavedInsight {
  id: string;
  text: string;
  title: string;
  url: string;
  createdAt: number;
}

interface CopilotTool {
  name: string;
  description?: string;
}

interface CopilotWorkspaceProps {
  onRunPrompt: (prompt: string) => Promise<void>;
  tools: CopilotTool[];
  onOpenTools: () => void;
}

type QuickAction = 'summary' | 'explain' | 'rewrite' | 'translate' | 'actions' | 'notes';

const normalizeText = (value: string): string => value.replace(/\s+/g, ' ').trim();

const getSelectionText = (): string => {
  try {
    return normalizeText(window.getSelection()?.toString() || '');
  } catch {
    return '';
  }
};

const getPageContext = (): string => {
  try {
    const main = document.querySelector('main, [role="main"]') as HTMLElement | null;
    const raw = main?.innerText || document.body?.innerText || '';
    return normalizeText(raw).slice(0, MAX_CONTEXT_CHARS);
  } catch {
    return '';
  }
};

const readSavedInsights = (): Promise<SavedInsight[]> =>
  new Promise(resolve => {
    try {
      chrome.storage.local.get([STORAGE_KEY], result => {
        if (chrome.runtime.lastError) {
          resolve([]);
          return;
        }
        const items = result?.[STORAGE_KEY];
        resolve(Array.isArray(items) ? items : []);
      });
    } catch {
      resolve([]);
    }
  });

const writeSavedInsights = (items: SavedInsight[]): Promise<void> =>
  new Promise(resolve => {
    try {
      chrome.storage.local.set({ [STORAGE_KEY]: items.slice(0, MAX_SAVED_ITEMS) }, () => resolve());
    } catch {
      resolve();
    }
  });

const buildActionPrompt = (action: QuickAction, context: string): string => {
  const source = context || 'No usable page context was captured. Ask me for the text I want to work with.';

  switch (action) {
    case 'summary':
      return `Summarize the context below. Give me: (1) a concise overview, (2) five key points, and (3) important decisions, risks, or open questions. Do not invent details.\n\nContext:\n${source}`;
    case 'explain':
      return `Explain the text below clearly and simply. Preserve technical meaning, define specialized terms when needed, and end with a one-sentence takeaway.\n\nText:\n${source}`;
    case 'rewrite':
      return `Rewrite the text below to be clearer, more concise, and professional while preserving its meaning. Return only the improved version unless clarification is necessary.\n\nText:\n${source}`;
    case 'translate':
      return `Translate the text below into the language I am currently using in this conversation. Preserve meaning, names, numbers, formatting, and technical terminology.\n\nText:\n${source}`;
    case 'actions':
      return `Extract actionable next steps from the context below. Use a checklist. Include owners and deadlines only when they are explicitly stated; do not invent them.\n\nContext:\n${source}`;
    case 'notes':
      return `Turn the context below into compact reusable notes with: Key ideas, Evidence, Terms, Questions, and Follow-ups. Keep provenance-sensitive claims tied to the supplied context.\n\nContext:\n${source}`;
    default:
      return source;
  }
};

const formatDate = (timestamp: number): string => {
  try {
    return new Date(timestamp).toLocaleString();
  } catch {
    return '';
  }
};

const CopilotWorkspace: React.FC<CopilotWorkspaceProps> = ({ onRunPrompt, tools, onOpenTools }) => {
  const [question, setQuestion] = useState('');
  const [selection, setSelection] = useState('');
  const [savedItems, setSavedItems] = useState<SavedInsight[]>([]);
  const [status, setStatus] = useState('');
  const [isRunning, setIsRunning] = useState(false);
  const [includePageContext, setIncludePageContext] = useState(true);

  useEffect(() => {
    readSavedInsights().then(setSavedItems);

    let timer: number | undefined;
    const refreshSelection = () => {
      if (timer) {
        window.clearTimeout(timer);
      }
      timer = window.setTimeout(() => {
        setSelection(getSelectionText());
      }, 80);
    };

    document.addEventListener('selectionchange', refreshSelection);
    refreshSelection();

    return () => {
      document.removeEventListener('selectionchange', refreshSelection);
      if (timer) {
        window.clearTimeout(timer);
      }
    };
  }, []);

  const suggestedTools = useMemo(
    () =>
      tools
        .filter(tool => /notion|gmail|mail|slack|calendar|drive|search|task|todo|note|readwise|linear/i.test(tool.name))
        .slice(0, 6),
    [tools],
  );

  const activeContext = (): string => {
    const selected = getSelectionText();
    if (selected) {
      setSelection(selected);
      return selected.slice(0, MAX_CONTEXT_CHARS);
    }
    return includePageContext ? getPageContext() : '';
  };

  const runAction = async (action: QuickAction) => {
    const context = activeContext();
    if (!context && ['explain', 'rewrite', 'translate'].includes(action)) {
      setStatus('Select text on the page first, then run this action.');
      return;
    }

    setIsRunning(true);
    setStatus('');
    try {
      await onRunPrompt(buildActionPrompt(action, context));
      setStatus('Sent to the current AI conversation.');
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Could not send the prompt.');
    } finally {
      setIsRunning(false);
    }
  };

  const handleAsk = async () => {
    const trimmed = question.trim();
    if (!trimmed) {
      setStatus('Type a question first.');
      return;
    }

    const context = activeContext();
    const prompt = context
      ? `${trimmed}\n\nUse the following current-page context when it is relevant. If the context does not answer the question, say so rather than inventing information.\n\nContext:\n${context}`
      : trimmed;

    setIsRunning(true);
    setStatus('');
    try {
      await onRunPrompt(prompt);
      setQuestion('');
      setStatus('Sent to the current AI conversation.');
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Could not send the prompt.');
    } finally {
      setIsRunning(false);
    }
  };

  const saveSelection = async () => {
    const text = getSelectionText();
    if (!text) {
      setStatus('Select text on the page before saving an insight.');
      return;
    }

    const item: SavedInsight = {
      id: `insight_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`,
      text,
      title: document.title || 'Untitled page',
      url: window.location.href,
      createdAt: Date.now(),
    };

    const next = [item, ...savedItems].slice(0, MAX_SAVED_ITEMS);
    setSavedItems(next);
    await writeSavedInsights(next);
    setStatus('Saved to Knowledge.');
  };

  const removeSavedItem = async (id: string) => {
    const next = savedItems.filter(item => item.id !== id);
    setSavedItems(next);
    await writeSavedInsights(next);
  };

  const copySavedItem = async (item: SavedInsight) => {
    try {
      await navigator.clipboard.writeText(item.text);
      setStatus('Copied saved insight.');
    } catch {
      setStatus('Clipboard access was not available.');
    }
  };

  const exportMarkdown = async () => {
    if (savedItems.length === 0) {
      setStatus('There are no saved insights to export yet.');
      return;
    }

    const markdown = [
      '# Superpower Knowledge',
      '',
      ...savedItems.flatMap(item => [
        `## ${item.title}`,
        '',
        item.text,
        '',
        `Source: ${item.url}`,
        `Saved: ${formatDate(item.createdAt)}`,
        '',
      ]),
    ].join('\n');

    try {
      await navigator.clipboard.writeText(markdown);
      setStatus('Knowledge exported as Markdown to the clipboard.');
    } catch {
      setStatus('Could not copy the Markdown export.');
    }
  };

  const actionButton = (label: string, description: string, action: QuickAction, accent: string) => (
    <button
      key={action}
      type="button"
      disabled={isRunning}
      onClick={() => runAction(action)}
      className="text-left rounded-xl border border-slate-200 dark:border-slate-700 bg-white dark:bg-slate-800 p-3 hover:border-indigo-300 dark:hover:border-indigo-600 hover:shadow-sm transition-all disabled:opacity-50">
      <div className="flex items-center gap-2 mb-1">
        <span className={`h-2.5 w-2.5 rounded-full ${accent}`} />
        <span className="text-sm font-semibold text-slate-800 dark:text-slate-100">{label}</span>
      </div>
      <span className="text-xs leading-5 text-slate-500 dark:text-slate-400">{description}</span>
    </button>
  );

  return (
    <div className="space-y-4 pb-2">
      <div className="rounded-2xl border border-indigo-100 dark:border-indigo-900/60 bg-gradient-to-br from-indigo-50 via-white to-violet-50 dark:from-indigo-950/40 dark:via-slate-900 dark:to-violet-950/30 p-4">
        <div className="flex items-start justify-between gap-3 mb-3">
          <div>
            <Typography variant="subtitle" className="font-semibold text-slate-900 dark:text-white">
              Copilot
            </Typography>
            <p className="text-xs text-slate-500 dark:text-slate-400 mt-1">
              Understand, write, capture, and act without leaving the current conversation.
            </p>
          </div>
          <span className="text-[10px] font-medium rounded-full bg-white/80 dark:bg-slate-800 px-2 py-1 text-indigo-600 dark:text-indigo-300 border border-indigo-100 dark:border-indigo-800">
            Ctrl/⌘ + Shift + K
          </span>
        </div>

        <textarea
          value={question}
          onChange={event => setQuestion(event.target.value)}
          onKeyDown={event => {
            if ((event.metaKey || event.ctrlKey) && event.key === 'Enter') {
              event.preventDefault();
              handleAsk();
            }
          }}
          placeholder="Ask about this page, the current conversation, or your selection..."
          className="w-full min-h-[84px] resize-y rounded-xl border border-slate-200 dark:border-slate-700 bg-white dark:bg-slate-900 px-3 py-2 text-sm text-slate-800 dark:text-slate-100 placeholder:text-slate-400 focus:outline-none focus:ring-2 focus:ring-indigo-400/40"
        />

        <div className="mt-2 flex items-center justify-between gap-2">
          <label className="flex items-center gap-2 text-xs text-slate-500 dark:text-slate-400 cursor-pointer">
            <input
              type="checkbox"
              checked={includePageContext}
              onChange={event => setIncludePageContext(event.target.checked)}
              className="rounded border-slate-300 text-indigo-600 focus:ring-indigo-500"
            />
            Include current page context
          </label>
          <Button size="sm" onClick={handleAsk} disabled={isRunning || !question.trim()}>
            {isRunning ? 'Working…' : 'Ask'}
          </Button>
        </div>

        {selection && (
          <div className="mt-3 rounded-lg bg-white/80 dark:bg-slate-800/80 border border-indigo-100 dark:border-indigo-800 px-3 py-2">
            <p className="text-[10px] uppercase tracking-wide font-semibold text-indigo-500 mb-1">Current selection</p>
            <p className="text-xs text-slate-600 dark:text-slate-300 line-clamp-3">{selection}</p>
          </div>
        )}
      </div>

      <div>
        <div className="flex items-center justify-between mb-2">
          <Typography variant="subtitle" className="font-semibold text-slate-800 dark:text-slate-100">
            Quick actions
          </Typography>
          <span className="text-[10px] text-slate-400">One click → current AI</span>
        </div>
        <div className="grid grid-cols-2 gap-2">
          {actionButton('Summarize', 'Overview, key points, risks and open questions.', 'summary', 'bg-indigo-500')}
          {actionButton('Explain', 'Explain selected text clearly without losing meaning.', 'explain', 'bg-sky-500')}
          {actionButton(
            'Improve writing',
            'Rewrite selected text to be clearer and more concise.',
            'rewrite',
            'bg-violet-500',
          )}
          {actionButton(
            'Translate',
            'Translate selection into your conversation language.',
            'translate',
            'bg-fuchsia-500',
          )}
          {actionButton(
            'Action items',
            'Extract explicit next steps without inventing owners.',
            'actions',
            'bg-emerald-500',
          )}
          {actionButton('Study notes', 'Turn context into reusable structured notes.', 'notes', 'bg-amber-500')}
        </div>
      </div>

      <div className="rounded-xl border border-slate-200 dark:border-slate-700 overflow-hidden">
        <div className="p-3 bg-slate-50 dark:bg-slate-800/70 flex items-center justify-between gap-2">
          <div>
            <Typography variant="subtitle" className="font-semibold text-slate-800 dark:text-slate-100">
              Knowledge
            </Typography>
            <p className="text-[11px] text-slate-500 dark:text-slate-400">
              Save selected text with its source, then reuse or export it.
            </p>
          </div>
          <Button size="sm" variant="outline" onClick={saveSelection}>
            Save selection
          </Button>
        </div>

        {savedItems.length === 0 ? (
          <div className="p-4 text-center text-xs text-slate-500 dark:text-slate-400">
            Select useful text on the page and save it here.
          </div>
        ) : (
          <div className="max-h-64 overflow-y-auto divide-y divide-slate-100 dark:divide-slate-700">
            {savedItems.slice(0, 12).map(item => (
              <div key={item.id} className="p-3">
                <div className="flex items-start justify-between gap-2">
                  <div className="min-w-0">
                    <p className="text-xs font-semibold text-slate-700 dark:text-slate-200 truncate">{item.title}</p>
                    <p className="mt-1 text-xs leading-5 text-slate-500 dark:text-slate-400 line-clamp-3">
                      {item.text}
                    </p>
                    <p className="mt-1 text-[10px] text-slate-400">{formatDate(item.createdAt)}</p>
                  </div>
                  <div className="flex flex-col gap-1 shrink-0">
                    <button
                      type="button"
                      onClick={() => copySavedItem(item)}
                      className="text-[10px] px-2 py-1 rounded-md border border-slate-200 dark:border-slate-700 text-slate-500 hover:text-indigo-600">
                      Copy
                    </button>
                    <button
                      type="button"
                      onClick={() => removeSavedItem(item.id)}
                      className="text-[10px] px-2 py-1 rounded-md border border-slate-200 dark:border-slate-700 text-slate-400 hover:text-red-500">
                      Remove
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}

        {savedItems.length > 0 && (
          <div className="p-2 border-t border-slate-100 dark:border-slate-700 flex justify-end">
            <Button size="sm" variant="ghost" onClick={exportMarkdown}>
              Copy Markdown export
            </Button>
          </div>
        )}
      </div>

      <div className="rounded-xl border border-slate-200 dark:border-slate-700 p-3">
        <div className="flex items-center justify-between gap-2 mb-2">
          <div>
            <Typography variant="subtitle" className="font-semibold text-slate-800 dark:text-slate-100">
              MCP actions
            </Typography>
            <p className="text-[11px] text-slate-500 dark:text-slate-400">
              Your connected tools stay available for real actions.
            </p>
          </div>
          <Button size="sm" variant="outline" onClick={onOpenTools}>
            Open tools
          </Button>
        </div>

        {suggestedTools.length > 0 ? (
          <div className="flex flex-wrap gap-1.5">
            {suggestedTools.map(tool => (
              <span
                key={tool.name}
                title={tool.description || tool.name}
                className="max-w-full truncate rounded-full bg-slate-100 dark:bg-slate-800 px-2 py-1 text-[10px] text-slate-600 dark:text-slate-300 border border-slate-200 dark:border-slate-700">
                {tool.name}
              </span>
            ))}
          </div>
        ) : (
          <p className="text-xs text-slate-500 dark:text-slate-400">
            Connect an MCP server to surface task, notes, search, mail, calendar and other actions here.
          </p>
        )}
      </div>

      {status && (
        <div className="rounded-lg bg-slate-100 dark:bg-slate-800 px-3 py-2 text-xs text-slate-600 dark:text-slate-300">
          {status}
        </div>
      )}
    </div>
  );
};

export default CopilotWorkspace;
