import React, { useEffect, useMemo, useState } from 'react';
import { Button, Typography } from '../ui';

const STORAGE_KEY = 'superpower_copilot_saved_insights';
const MAX_CONTEXT_CHARS = 12000;
const MAX_SAVED_ITEMS = 100;

type KnowledgeSource = 'web' | 'youtube' | 'ai-chat' | 'other';

interface SavedInsight {
  id: string;
  text: string;
  title: string;
  url: string;
  createdAt: number;
  tags?: string[];
  sourceType?: KnowledgeSource;
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

interface ConnectedActionDefinition {
  id: string;
  label: string;
  description: string;
  toolPattern: RegExp;
  instruction: string;
}

interface ConnectedAction extends ConnectedActionDefinition {
  tool: CopilotTool;
}

const CONNECTED_ACTIONS: ConnectedActionDefinition[] = [
  {
    id: 'notes',
    label: 'Save note',
    description: 'Capture the current context in your connected notes workspace.',
    toolPattern: /notion|note|readwise|obsidian/i,
    instruction:
      'Use the connected notes tool to save the useful content as a concise structured note. Preserve the source URL when available. If the destination, database, page, or required schema field is missing, ask for it before executing.',
  },
  {
    id: 'email',
    label: 'Draft email',
    description: 'Turn the current context into an email draft before sending.',
    toolPattern: /gmail|mail|email/i,
    instruction:
      'Use the connected mail tool to prepare an email draft from the current context. Do not send it automatically. Show the proposed recipient, subject, and body for review, and ask for any missing required recipient or intent.',
  },
  {
    id: 'calendar',
    label: 'Plan event',
    description: 'Prepare a calendar event from dates and decisions in context.',
    toolPattern: /calendar|gcal|event/i,
    instruction:
      'Use the connected calendar tool to prepare an event from the current context. Extract dates, time, title, attendees, and notes only when supported by the context. Ask for missing required scheduling details before creating the event.',
  },
  {
    id: 'task',
    label: 'Create task',
    description: 'Convert an explicit follow-up into a trackable task.',
    toolPattern: /task|todo|linear|asana|trello|clickup/i,
    instruction:
      'Use the connected task tool to prepare a task from the current context. Preserve explicit owner, due date, project, and dependencies when present. Do not invent missing fields; ask for required values before creation.',
  },
  {
    id: 'slack',
    label: 'Draft Slack',
    description: 'Prepare a concise team update without posting automatically.',
    toolPattern: /slack|teams|discord/i,
    instruction:
      'Use the connected messaging tool to draft a concise team update based on the current context. Do not post automatically. Show the channel or recipient and message for review, and ask if the destination is missing.',
  },
  {
    id: 'drive',
    label: 'Search files',
    description: 'Use connected storage to find related files or source material.',
    toolPattern: /drive|dropbox|onedrive|box|file|document/i,
    instruction:
      'Use the connected file or storage tool to search for files relevant to the current context. Start with a narrow query using the strongest names, project terms, or identifiers in the context and summarize the most relevant matches.',
  },
];

const normalizeText = (value: string): string => value.replace(/\s+/g, ' ').trim();

const getKnowledgeSource = (url: string): KnowledgeSource => {
  try {
    const hostname = new URL(url).hostname.toLowerCase();
    if (hostname === 'youtube.com' || hostname.endsWith('.youtube.com') || hostname === 'youtu.be') return 'youtube';
    if (
      /chatgpt\.com|chat\.openai\.com|gemini\.google\.com|perplexity\.ai|grok\.com|chat\.deepseek\.com|chat\.mistral\.ai|kimi\.com|chat\.qwen\.ai|chat\.z\.ai|t3\.chat|openrouter\.ai/.test(
        hostname,
      )
    ) {
      return 'ai-chat';
    }
    if (hostname) return 'web';
  } catch {
    // Keep old or non-URL items usable.
  }
  return 'other';
};

const getKnowledgeSourceLabel = (source: KnowledgeSource): string => {
  switch (source) {
    case 'youtube':
      return 'YouTube';
    case 'ai-chat':
      return 'AI chat';
    case 'web':
      return 'Web';
    default:
      return 'Other';
  }
};

const normalizeSavedInsight = (item: SavedInsight): SavedInsight => ({
  ...item,
  tags: Array.isArray(item.tags) ? item.tags.filter(Boolean) : [],
  sourceType: item.sourceType || getKnowledgeSource(item.url),
});

const formatKnowledgeContext = (items: SavedInsight[]): string =>
  items
    .map((item, index) =>
      [
        `Saved note ${index + 1}: ${item.title}`,
        `Source: ${item.url}`,
        item.tags?.length ? `Tags: ${item.tags.join(', ')}` : '',
        item.text,
      ]
        .filter(Boolean)
        .join('\n'),
    )
    .join('\n\n---\n\n')
    .slice(0, MAX_CONTEXT_CHARS);

const formatKnowledgeMarkdown = (items: SavedInsight[]): string =>
  [
    '# Superpower Knowledge',
    '',
    ...items.flatMap(item => [
      `## ${item.title}`,
      '',
      item.tags?.length ? `Tags: ${item.tags.join(', ')}` : '',
      `Type: ${getKnowledgeSourceLabel(item.sourceType || getKnowledgeSource(item.url))}`,
      '',
      item.text,
      '',
      `Source: ${item.url}`,
      `Saved: ${formatDate(item.createdAt)}`,
      '',
    ]),
  ]
    .filter(line => line !== undefined)
    .join('\n');

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
        resolve(Array.isArray(items) ? items.map(normalizeSavedInsight) : []);
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
  const [knowledgeQuery, setKnowledgeQuery] = useState('');
  const [knowledgeSourceFilter, setKnowledgeSourceFilter] = useState<'all' | KnowledgeSource>('all');
  const [selectedKnowledgeIds, setSelectedKnowledgeIds] = useState<string[]>([]);
  const [attachedKnowledgeIds, setAttachedKnowledgeIds] = useState<string[]>([]);
  const [tagDrafts, setTagDrafts] = useState<Record<string, string>>({});

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
        .slice(0, 8),
    [tools],
  );

  const connectedActions = useMemo(
    () =>
      CONNECTED_ACTIONS.map(definition => {
        const tool = tools.find(candidate => definition.toolPattern.test(candidate.name));
        return tool ? ({ ...definition, tool } as ConnectedAction) : null;
      }).filter((action): action is ConnectedAction => action !== null),
    [tools],
  );

  const filteredKnowledge = useMemo(() => {
    const query = normalizeText(knowledgeQuery).toLowerCase();

    return savedItems.filter(item => {
      const source = item.sourceType || getKnowledgeSource(item.url);
      if (knowledgeSourceFilter !== 'all' && source !== knowledgeSourceFilter) return false;
      if (!query) return true;

      const haystack = [item.title, item.text, item.url, ...(item.tags || [])].join(' ').toLowerCase();
      return haystack.includes(query);
    });
  }, [knowledgeQuery, knowledgeSourceFilter, savedItems]);

  const selectedKnowledge = useMemo(
    () => savedItems.filter(item => selectedKnowledgeIds.includes(item.id)),
    [savedItems, selectedKnowledgeIds],
  );

  const attachedKnowledge = useMemo(
    () => savedItems.filter(item => attachedKnowledgeIds.includes(item.id)),
    [attachedKnowledgeIds, savedItems],
  );

  const knowledgeDestinationTool = useMemo(
    () => tools.find(tool => /notion|note|readwise|obsidian|drive|dropbox|onedrive|document|file/i.test(tool.name)),
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

  const runConnectedAction = async (action: ConnectedAction) => {
    const context = activeContext();
    const contextBlock = context
      ? `\n\nCurrent context:\n${context}`
      : '\n\nThere is no captured page context. Ask me for the minimum information needed to continue.';

    setIsRunning(true);
    setStatus('');
    try {
      await onRunPrompt(
        [
          `Use the connected MCP capability associated with tool "${action.tool.name}".`,
          action.instruction,
          'Keep the workflow review-first: distinguish drafting/search from state-changing execution, and do not bypass required confirmation or missing schema fields.',
          contextBlock,
        ].join('\n\n'),
      );
      setStatus(`${action.label} request sent to the current AI conversation.`);
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Could not start the connected action.');
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
    const knowledgeContext = attachedKnowledge.length ? formatKnowledgeContext(attachedKnowledge) : '';

    const promptParts = [trimmed];
    if (context) {
      promptParts.push(
        `Use the following current-page context when it is relevant. If the context does not answer the question, say so rather than inventing information.\n\nPage context:\n${context}`,
      );
    }
    if (knowledgeContext) {
      promptParts.push(
        `Use the following saved Knowledge as additional source context. Keep claims attributable to these notes and their source URLs.\n\nKnowledge:\n${knowledgeContext}`,
      );
    }

    const prompt = promptParts.join('\n\n');

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
      tags: [],
      sourceType: getKnowledgeSource(window.location.href),
    };

    const next = [item, ...savedItems].slice(0, MAX_SAVED_ITEMS);
    setSavedItems(next);
    await writeSavedInsights(next);
    setStatus('Saved to Knowledge.');
  };

  const removeSavedItem = async (id: string) => {
    const next = savedItems.filter(item => item.id !== id);
    setSavedItems(next);
    setSelectedKnowledgeIds(ids => ids.filter(selectedId => selectedId !== id));
    setAttachedKnowledgeIds(ids => ids.filter(attachedId => attachedId !== id));
    await writeSavedInsights(next);
  };

  const toggleKnowledgeSelection = (id: string) => {
    setSelectedKnowledgeIds(ids => (ids.includes(id) ? ids.filter(itemId => itemId !== id) : [...ids, id]));
  };

  const attachKnowledge = (items: SavedInsight[]) => {
    const ids = items.map(item => item.id);
    setAttachedKnowledgeIds(current => Array.from(new Set([...current, ...ids])));
    setStatus(`${items.length} Knowledge ${items.length === 1 ? 'item' : 'items'} attached to Ask.`);
  };

  const addTag = async (item: SavedInsight) => {
    const raw = normalizeText(tagDrafts[item.id] || '').replace(/^#/, '');
    if (!raw) return;

    const tag = raw.slice(0, 32);
    const next = savedItems.map(saved =>
      saved.id === item.id
        ? {
            ...saved,
            tags: Array.from(new Set([...(saved.tags || []), tag])).slice(0, 8),
          }
        : saved,
    );
    setSavedItems(next);
    setTagDrafts(drafts => ({ ...drafts, [item.id]: '' }));
    await writeSavedInsights(next);
  };

  const removeTag = async (item: SavedInsight, tag: string) => {
    const next = savedItems.map(saved =>
      saved.id === item.id ? { ...saved, tags: (saved.tags || []).filter(existing => existing !== tag) } : saved,
    );
    setSavedItems(next);
    await writeSavedInsights(next);
  };

  const synthesizeKnowledge = async (items: SavedInsight[]) => {
    if (items.length === 0) {
      setStatus('Select at least one Knowledge item first.');
      return;
    }

    setIsRunning(true);
    setStatus('');
    try {
      await onRunPrompt(
        [
          'Synthesize the saved Knowledge below into one coherent research note.',
          'Separate: Shared themes, Important differences, Evidence and provenance, Open questions, and Recommended follow-ups.',
          'Do not merge conflicting claims into one claim. Preserve source URLs for provenance.',
          '',
          formatKnowledgeContext(items),
        ].join('\n'),
      );
      setStatus('Knowledge synthesis sent to the current AI conversation.');
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Could not synthesize Knowledge.');
    } finally {
      setIsRunning(false);
    }
  };

  const sendKnowledgeToConnectedTool = async (items: SavedInsight[]) => {
    if (items.length === 0) {
      setStatus('Select at least one Knowledge item first.');
      return;
    }
    if (!knowledgeDestinationTool) {
      setStatus('Connect a notes or file-storage MCP tool first.');
      return;
    }

    setIsRunning(true);
    setStatus('');
    try {
      await onRunPrompt(
        [
          `Use the connected MCP capability associated with tool "${knowledgeDestinationTool.name}".`,
          'Prepare these selected Knowledge items for saving into the connected notes or file workspace.',
          'Preserve each title, source URL, tags, and note text. If the destination page/folder/database or a required schema field is missing, ask for it before performing the state-changing save.',
          'Keep the workflow review-first and show the proposed destination/structure before execution when appropriate.',
          '',
          formatKnowledgeContext(items),
        ].join('\n'),
      );
      setStatus(`Knowledge handoff sent for ${knowledgeDestinationTool.name}.`);
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Could not send Knowledge to the connected tool.');
    } finally {
      setIsRunning(false);
    }
  };

  const copySavedItem = async (item: SavedInsight) => {
    try {
      await navigator.clipboard.writeText(item.text);
      setStatus('Copied saved insight.');
    } catch {
      setStatus('Clipboard access was not available.');
    }
  };

  const exportMarkdown = async (items: SavedInsight[] = savedItems) => {
    if (items.length === 0) {
      setStatus('There are no saved insights to export yet.');
      return;
    }

    try {
      await navigator.clipboard.writeText(formatKnowledgeMarkdown(items));
      setStatus(`${items.length} Knowledge ${items.length === 1 ? 'item' : 'items'} exported as Markdown.`);
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

        {attachedKnowledge.length > 0 && (
          <div className="mt-2 flex items-center justify-between gap-2 rounded-lg border border-indigo-100 dark:border-indigo-800 bg-indigo-50/70 dark:bg-indigo-950/30 px-2.5 py-2">
            <span className="text-[10px] font-medium text-indigo-600 dark:text-indigo-300">
              {attachedKnowledge.length} Knowledge {attachedKnowledge.length === 1 ? 'item' : 'items'} attached
            </span>
            <button
              type="button"
              onClick={() => setAttachedKnowledgeIds([])}
              className="text-[10px] text-slate-400 hover:text-indigo-600">
              Clear
            </button>
          </div>
        )}

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
        <div className="p-3 bg-slate-50 dark:bg-slate-800/70 flex items-start justify-between gap-2">
          <div>
            <Typography variant="subtitle" className="font-semibold text-slate-800 dark:text-slate-100">
              Knowledge
            </Typography>
            <p className="text-[11px] text-slate-500 dark:text-slate-400">
              Search, tag, combine and reuse saved sources.
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
          <>
            <div className="p-3 border-b border-slate-100 dark:border-slate-700 space-y-2">
              <input
                value={knowledgeQuery}
                onChange={event => setKnowledgeQuery(event.target.value)}
                placeholder="Search titles, text, URLs or tags..."
                className="w-full rounded-lg border border-slate-200 dark:border-slate-700 bg-white dark:bg-slate-900 px-2.5 py-2 text-xs text-slate-700 dark:text-slate-200 placeholder:text-slate-400 focus:outline-none focus:ring-2 focus:ring-indigo-400/30"
              />

              <div className="flex flex-wrap gap-1.5">
                {(['all', 'web', 'youtube', 'ai-chat', 'other'] as const).map(source => (
                  <button
                    key={source}
                    type="button"
                    onClick={() => setKnowledgeSourceFilter(source)}
                    className={`rounded-full border px-2 py-1 text-[10px] transition-colors ${
                      knowledgeSourceFilter === source
                        ? 'border-indigo-300 bg-indigo-50 text-indigo-600 dark:border-indigo-700 dark:bg-indigo-950/30 dark:text-indigo-300'
                        : 'border-slate-200 text-slate-500 dark:border-slate-700 dark:text-slate-400'
                    }`}>
                    {source === 'all' ? 'All' : getKnowledgeSourceLabel(source)}
                  </button>
                ))}
              </div>

              {selectedKnowledge.length > 0 && (
                <div className="rounded-lg border border-indigo-100 dark:border-indigo-800 bg-indigo-50/50 dark:bg-indigo-950/20 p-2">
                  <div className="flex items-center justify-between gap-2 mb-2">
                    <span className="text-[10px] font-semibold text-indigo-600 dark:text-indigo-300">
                      {selectedKnowledge.length} selected
                    </span>
                    <button
                      type="button"
                      onClick={() => setSelectedKnowledgeIds([])}
                      className="text-[10px] text-slate-400 hover:text-indigo-600">
                      Clear
                    </button>
                  </div>
                  <div className="grid grid-cols-2 gap-1.5">
                    <button
                      type="button"
                      onClick={() => attachKnowledge(selectedKnowledge)}
                      className="rounded-md border border-indigo-200 dark:border-indigo-700 px-2 py-1.5 text-[10px] font-medium text-indigo-600 dark:text-indigo-300">
                      Use in Ask
                    </button>
                    <button
                      type="button"
                      onClick={() => synthesizeKnowledge(selectedKnowledge)}
                      disabled={isRunning}
                      className="rounded-md border border-indigo-200 dark:border-indigo-700 px-2 py-1.5 text-[10px] font-medium text-indigo-600 dark:text-indigo-300 disabled:opacity-50">
                      Synthesize
                    </button>
                    <button
                      type="button"
                      onClick={() => exportMarkdown(selectedKnowledge)}
                      className="rounded-md border border-slate-200 dark:border-slate-700 px-2 py-1.5 text-[10px] text-slate-600 dark:text-slate-300">
                      Copy Markdown
                    </button>
                    <button
                      type="button"
                      onClick={() => sendKnowledgeToConnectedTool(selectedKnowledge)}
                      disabled={isRunning || !knowledgeDestinationTool}
                      title={
                        knowledgeDestinationTool
                          ? `Uses ${knowledgeDestinationTool.name}`
                          : 'Connect Notion, notes, Drive or another storage MCP tool'
                      }
                      className="rounded-md border border-slate-200 dark:border-slate-700 px-2 py-1.5 text-[10px] text-slate-600 dark:text-slate-300 disabled:opacity-40">
                      Send to MCP
                    </button>
                  </div>
                </div>
              )}
            </div>

            <div className="max-h-96 overflow-y-auto divide-y divide-slate-100 dark:divide-slate-700">
              {filteredKnowledge.length === 0 ? (
                <div className="p-4 text-center text-xs text-slate-500 dark:text-slate-400">
                  No Knowledge matches this search or source filter.
                </div>
              ) : (
                filteredKnowledge.map(item => {
                  const source = item.sourceType || getKnowledgeSource(item.url);
                  const isSelected = selectedKnowledgeIds.includes(item.id);

                  return (
                    <div key={item.id} className={`p-3 ${isSelected ? 'bg-indigo-50/40 dark:bg-indigo-950/20' : ''}`}>
                      <div className="flex items-start gap-2">
                        <input
                          type="checkbox"
                          checked={isSelected}
                          onChange={() => toggleKnowledgeSelection(item.id)}
                          className="mt-0.5 rounded border-slate-300 text-indigo-600 focus:ring-indigo-500"
                          aria-label={`Select ${item.title}`}
                        />

                        <div className="min-w-0 flex-1">
                          <div className="flex items-center gap-1.5 min-w-0">
                            <span className="rounded-full bg-slate-100 dark:bg-slate-800 px-1.5 py-0.5 text-[9px] font-medium text-slate-500 dark:text-slate-400 shrink-0">
                              {getKnowledgeSourceLabel(source)}
                            </span>
                            <p className="text-xs font-semibold text-slate-700 dark:text-slate-200 truncate">
                              {item.title}
                            </p>
                          </div>

                          <p className="mt-1 text-xs leading-5 text-slate-500 dark:text-slate-400 line-clamp-3">
                            {item.text}
                          </p>

                          {(item.tags || []).length > 0 && (
                            <div className="mt-1.5 flex flex-wrap gap-1">
                              {(item.tags || []).map(tag => (
                                <button
                                  key={tag}
                                  type="button"
                                  onClick={() => removeTag(item, tag)}
                                  title="Remove tag"
                                  className="rounded-full bg-indigo-50 dark:bg-indigo-950/30 px-1.5 py-0.5 text-[9px] text-indigo-600 dark:text-indigo-300">
                                  #{tag} ×
                                </button>
                              ))}
                            </div>
                          )}

                          <div className="mt-2 flex items-center gap-1.5">
                            <input
                              value={tagDrafts[item.id] || ''}
                              onChange={event => setTagDrafts(drafts => ({ ...drafts, [item.id]: event.target.value }))}
                              onKeyDown={event => {
                                if (event.key === 'Enter') {
                                  event.preventDefault();
                                  addTag(item);
                                }
                              }}
                              placeholder="Add tag"
                              className="min-w-0 flex-1 rounded-md border border-slate-200 dark:border-slate-700 bg-white dark:bg-slate-900 px-2 py-1 text-[10px] text-slate-600 dark:text-slate-300 focus:outline-none focus:ring-1 focus:ring-indigo-400/40"
                            />
                            <button
                              type="button"
                              onClick={() => addTag(item)}
                              className="rounded-md border border-slate-200 dark:border-slate-700 px-2 py-1 text-[10px] text-slate-500 hover:text-indigo-600">
                              Tag
                            </button>
                          </div>

                          <div className="mt-2 flex items-center justify-between gap-2">
                            <p className="text-[9px] text-slate-400 truncate">{formatDate(item.createdAt)}</p>
                            <div className="flex gap-1">
                              <button
                                type="button"
                                onClick={() => attachKnowledge([item])}
                                className="text-[10px] px-2 py-1 rounded-md border border-slate-200 dark:border-slate-700 text-indigo-500 hover:text-indigo-700">
                                Use in Ask
                              </button>
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
                      </div>
                    </div>
                  );
                })
              )}
            </div>

            <div className="p-2 border-t border-slate-100 dark:border-slate-700 flex items-center justify-between gap-2">
              <span className="text-[10px] text-slate-400">
                {filteredKnowledge.length} of {savedItems.length} items
              </span>
              <Button size="sm" variant="ghost" onClick={() => exportMarkdown(filteredKnowledge)}>
                Copy visible Markdown
              </Button>
            </div>
          </>
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

        {connectedActions.length > 0 ? (
          <>
            <div className="grid grid-cols-2 gap-2">
              {connectedActions.map(action => (
                <button
                  key={action.id}
                  type="button"
                  disabled={isRunning}
                  onClick={() => runConnectedAction(action)}
                  title={`Uses ${action.tool.name}`}
                  className="rounded-xl border border-slate-200 dark:border-slate-700 p-2.5 text-left hover:border-indigo-300 dark:hover:border-indigo-600 hover:bg-indigo-50/40 dark:hover:bg-indigo-950/20 transition-all disabled:opacity-50">
                  <div className="text-xs font-semibold text-slate-800 dark:text-slate-100">{action.label}</div>
                  <div className="mt-1 text-[10px] leading-4 text-slate-500 dark:text-slate-400">
                    {action.description}
                  </div>
                  <div className="mt-1.5 text-[9px] text-indigo-500 dark:text-indigo-300 truncate">
                    {action.tool.name}
                  </div>
                </button>
              ))}
            </div>
            <div className="mt-2 flex flex-wrap gap-1.5">
              {suggestedTools.map(tool => (
                <span
                  key={tool.name}
                  title={tool.description || tool.name}
                  className="max-w-full truncate rounded-full bg-slate-100 dark:bg-slate-800 px-2 py-1 text-[10px] text-slate-600 dark:text-slate-300 border border-slate-200 dark:border-slate-700">
                  {tool.name}
                </span>
              ))}
            </div>
          </>
        ) : suggestedTools.length > 0 ? (
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
