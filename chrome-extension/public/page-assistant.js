(function () {
  'use strict';

  const STORAGE_KEY = 'superpower_copilot_saved_insights';
  const MAX_SAVED_ITEMS = 100;
  const MAX_TEXT_CHARS = 18000;
  const ROOT_ID = 'superpower-page-assistant-root';

  if (document.getElementById(ROOT_ID)) {
    return;
  }

  const normalizeText = value =>
    String(value || '')
      .replace(/\s+/g, ' ')
      .trim();
  const truncate = (value, limit = MAX_TEXT_CHARS) => normalizeText(value).slice(0, limit);

  const getKnowledgeSource = () => {
    const hostname = window.location.hostname.toLowerCase();
    if (hostname === 'youtube.com' || hostname.endsWith('.youtube.com') || hostname === 'youtu.be') {
      return 'youtube';
    }
    return hostname ? 'web' : 'other';
  };
  const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

  const getSelectionText = () => {
    try {
      return truncate(window.getSelection()?.toString() || '', 10000);
    } catch {
      return '';
    }
  };

  const getPageText = () => {
    const candidate =
      document.querySelector('article') || document.querySelector('main, [role="main"]') || document.body;
    return truncate(candidate?.innerText || '');
  };

  const getSourceHeader = () =>
    [`Page title: ${document.title || 'Untitled page'}`, `Source URL: ${window.location.href}`].join('\n');

  const buildPrompt = (action, context) => {
    const source = context || getPageText();
    const header = getSourceHeader();

    switch (action) {
      case 'summary':
        return `Summarize this source. Give me a concise overview, five key points, and any important decisions, risks, caveats, or open questions. Do not invent details.\n\n${header}\n\nSource content:\n${source}`;
      case 'explain':
        return `Explain the selected text clearly and precisely. Preserve technical meaning, define specialized terms when useful, and end with a one-sentence takeaway.\n\n${header}\n\nSelected text:\n${source}`;
      case 'actions':
        return `Extract actionable next steps from this source. Use a checklist. Include owners, dates, and dependencies only when explicitly stated; do not invent them.\n\n${header}\n\nSource content:\n${source}`;
      case 'ask':
        return `Help me work with the source below. First identify what the selected passage is saying and what is worth paying attention to. Then ask me what I want to do next if my intent is not clear.\n\n${header}\n\nSelected text:\n${source}`;
      default:
        return source;
    }
  };

  const readKnowledge = () =>
    new Promise(resolve => {
      try {
        chrome.storage.local.get([STORAGE_KEY], result => {
          if (chrome.runtime.lastError) {
            resolve([]);
            return;
          }
          resolve(Array.isArray(result?.[STORAGE_KEY]) ? result[STORAGE_KEY] : []);
        });
      } catch {
        resolve([]);
      }
    });

  const saveKnowledge = async text => {
    const cleaned = truncate(text, 12000);
    if (!cleaned) {
      return false;
    }

    const items = await readKnowledge();
    const item = {
      id: `insight_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`,
      text: cleaned,
      title: document.title || 'Untitled page',
      url: window.location.href,
      createdAt: Date.now(),
      tags: [],
      sourceType: getKnowledgeSource(),
    };

    return new Promise(resolve => {
      try {
        chrome.storage.local.set({ [STORAGE_KEY]: [item, ...items].slice(0, MAX_SAVED_ITEMS) }, () =>
          resolve(!chrome.runtime.lastError),
        );
      } catch {
        resolve(false);
      }
    });
  };

  const routePrompt = prompt =>
    new Promise(resolve => {
      try {
        chrome.runtime.sendMessage(
          {
            command: 'superpower:route-prompt',
            prompt,
            source: {
              title: document.title,
              url: window.location.href,
            },
          },
          response => {
            if (chrome.runtime.lastError) {
              resolve({ success: false, error: chrome.runtime.lastError.message });
              return;
            }
            resolve(response || { success: false, error: 'No response from Superpower.' });
          },
        );
      } catch (error) {
        resolve({ success: false, error: error instanceof Error ? error.message : String(error) });
      }
    });

  const isYouTubeWatchPage = () =>
    /(^|\.)youtube\.com$/i.test(window.location.hostname) && window.location.pathname === '/watch';

  const collectYouTubeTranscript = () => {
    const segments = Array.from(
      document.querySelectorAll('ytd-transcript-segment-renderer, yt-formatted-string.segment-text, .segment-text'),
    )
      .map(node => normalizeText(node.textContent || ''))
      .filter(Boolean);

    return truncate(segments.join(' '), MAX_TEXT_CHARS);
  };

  const tryOpenYouTubeTranscript = async () => {
    let transcript = collectYouTubeTranscript();
    if (transcript) {
      return transcript;
    }

    const transcriptButton =
      document.querySelector('ytd-video-description-transcript-section-renderer button') ||
      Array.from(document.querySelectorAll('button')).find(button =>
        /transcript/i.test(`${button.getAttribute('aria-label') || ''} ${button.textContent || ''}`),
      );

    if (transcriptButton instanceof HTMLElement) {
      transcriptButton.click();

      for (let attempt = 0; attempt < 10; attempt += 1) {
        await delay(250);
        transcript = collectYouTubeTranscript();
        if (transcript) {
          return transcript;
        }
      }
    }

    return '';
  };

  const buildYouTubePrompt = async () => {
    const transcript = await tryOpenYouTubeTranscript();
    const title = normalizeText(document.querySelector('h1 yt-formatted-string, h1')?.textContent || document.title);
    const channel = normalizeText(
      document.querySelector('ytd-channel-name a, #channel-name a, #owner-name a')?.textContent || '',
    );
    const description = truncate(
      document.querySelector('#description-inline-expander, #description')?.textContent || '',
      5000,
    );

    const transcriptBlock = transcript
      ? `Transcript:\n${transcript}`
      : 'Transcript: not available from the loaded page. Base the summary only on the supplied metadata and clearly state that the transcript was unavailable.';

    return [
      'Summarize this YouTube video for me.',
      'Return: (1) a short overview, (2) the main arguments or lessons, (3) notable examples or evidence, (4) practical takeaways, and (5) questions or claims worth verifying.',
      'Do not invent content that is not supported by the supplied video metadata/transcript.',
      '',
      `Video title: ${title}`,
      channel ? `Channel: ${channel}` : '',
      `URL: ${window.location.href}`,
      description ? `Description: ${description}` : '',
      '',
      transcriptBlock,
    ]
      .filter(Boolean)
      .join('\n');
  };

  const host = document.createElement('div');
  host.id = ROOT_ID;
  host.style.all = 'initial';
  host.style.position = 'fixed';
  host.style.right = '18px';
  host.style.bottom = '18px';
  host.style.zIndex = '2147483646';
  document.documentElement.appendChild(host);

  const shadow = host.attachShadow({ mode: 'open' });
  shadow.innerHTML = `
    <style>
      :host { all: initial; }
      * { box-sizing: border-box; }
      .sp-shell {
        font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
        color: #0f172a;
      }
      .sp-launcher {
        width: 46px;
        height: 46px;
        border-radius: 15px;
        border: 1px solid rgba(148, 163, 184, .34);
        background: rgba(255, 255, 255, .96);
        box-shadow: 0 12px 32px rgba(15, 23, 42, .18);
        display: grid;
        place-items: center;
        cursor: pointer;
        transition: transform .16s ease, box-shadow .16s ease;
      }
      .sp-launcher:hover {
        transform: translateY(-2px);
        box-shadow: 0 16px 38px rgba(15, 23, 42, .23);
      }
      .sp-mark {
        width: 28px;
        height: 28px;
        border-radius: 9px;
        background: linear-gradient(135deg, #4f46e5, #7c3aed);
        color: #fff;
        display: grid;
        place-items: center;
        font-weight: 800;
        font-size: 14px;
        letter-spacing: -.02em;
      }
      .sp-panel {
        position: absolute;
        right: 0;
        bottom: 56px;
        width: 320px;
        max-height: min(570px, calc(100vh - 96px));
        overflow: auto;
        border-radius: 18px;
        border: 1px solid rgba(148, 163, 184, .28);
        background: rgba(255, 255, 255, .98);
        box-shadow: 0 24px 70px rgba(15, 23, 42, .24);
        padding: 14px;
        display: none;
      }
      .sp-panel[data-open="true"] { display: block; }
      .sp-heading {
        display: flex;
        align-items: flex-start;
        justify-content: space-between;
        gap: 10px;
        margin-bottom: 12px;
      }
      .sp-title { font-size: 14px; font-weight: 750; }
      .sp-subtitle { margin-top: 3px; color: #64748b; font-size: 11px; line-height: 1.45; }
      .sp-section-label {
        margin: 12px 0 7px;
        color: #64748b;
        font-size: 10px;
        font-weight: 750;
        letter-spacing: .08em;
        text-transform: uppercase;
      }
      .sp-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 7px; }
      .sp-button {
        appearance: none;
        border: 1px solid #e2e8f0;
        background: #fff;
        border-radius: 11px;
        padding: 9px 10px;
        color: #1e293b;
        font: inherit;
        font-size: 11px;
        font-weight: 650;
        text-align: left;
        cursor: pointer;
        transition: border-color .14s ease, background .14s ease, transform .14s ease;
      }
      .sp-button:hover { border-color: #a5b4fc; background: #f8fafc; transform: translateY(-1px); }
      .sp-button-primary {
        background: linear-gradient(135deg, #eef2ff, #f5f3ff);
        border-color: #c7d2fe;
        color: #4338ca;
      }
      .sp-button-wide { grid-column: 1 / -1; }
      .sp-selection {
        margin-top: 8px;
        padding: 9px 10px;
        border-radius: 10px;
        background: #f8fafc;
        color: #475569;
        font-size: 10px;
        line-height: 1.5;
        display: none;
        max-height: 72px;
        overflow: hidden;
      }
      .sp-selection[data-visible="true"] { display: block; }
      .sp-status {
        margin-top: 10px;
        border-radius: 9px;
        padding: 8px 9px;
        background: #f1f5f9;
        color: #475569;
        font-size: 10px;
        line-height: 1.4;
        display: none;
      }
      .sp-status[data-visible="true"] { display: block; }
      .sp-bubble {
        position: fixed;
        display: none;
        align-items: center;
        gap: 4px;
        padding: 4px;
        border-radius: 11px;
        border: 1px solid rgba(148, 163, 184, .35);
        background: rgba(255,255,255,.98);
        box-shadow: 0 10px 28px rgba(15, 23, 42, .2);
        z-index: 2147483647;
      }
      .sp-bubble[data-visible="true"] { display: flex; }
      .sp-mini {
        border: 0;
        border-radius: 8px;
        background: transparent;
        padding: 6px 8px;
        color: #334155;
        font: inherit;
        font-size: 10px;
        font-weight: 700;
        cursor: pointer;
      }
      .sp-mini:hover { background: #f1f5f9; color: #4338ca; }
      @media (prefers-color-scheme: dark) {
        .sp-shell { color: #e2e8f0; }
        .sp-launcher, .sp-panel, .sp-bubble {
          background: rgba(15, 23, 42, .97);
          border-color: rgba(71, 85, 105, .7);
        }
        .sp-title, .sp-button { color: #e2e8f0; }
        .sp-subtitle, .sp-section-label { color: #94a3b8; }
        .sp-button { background: #111827; border-color: #334155; }
        .sp-button:hover { background: #1e293b; border-color: #6366f1; }
        .sp-button-primary { background: rgba(67, 56, 202, .2); color: #c7d2fe; border-color: #4f46e5; }
        .sp-selection, .sp-status { background: #1e293b; color: #cbd5e1; }
        .sp-mini { color: #cbd5e1; }
        .sp-mini:hover { background: #1e293b; color: #c7d2fe; }
      }
    </style>
    <div class="sp-shell">
      <div class="sp-panel" id="panel" data-open="false">
        <div class="sp-heading">
          <div>
            <div class="sp-title">Superpower</div>
            <div class="sp-subtitle">Capture this page, send it to your AI workspace, or keep it in Knowledge.</div>
          </div>
          <div class="sp-mark">S</div>
        </div>

        <div class="sp-section-label">Selection</div>
        <div class="sp-grid">
          <button class="sp-button sp-button-primary" data-action="selection-summary">Summarize</button>
          <button class="sp-button" data-action="selection-explain">Explain</button>
          <button class="sp-button" data-action="selection-ask">Ask AI</button>
          <button class="sp-button" data-action="selection-save">Save</button>
        </div>
        <div class="sp-selection" id="selectionPreview"></div>

        <div class="sp-section-label">Page</div>
        <div class="sp-grid">
          <button class="sp-button" data-action="page-summary">Summarize page</button>
          <button class="sp-button" data-action="page-actions">Action items</button>
          <button class="sp-button sp-button-wide" data-action="page-save">Save page excerpt to Knowledge</button>
        </div>

        <div id="youtubeBlock" style="display:none">
          <div class="sp-section-label">YouTube</div>
          <div class="sp-grid">
            <button class="sp-button sp-button-primary sp-button-wide" data-action="youtube-summary">Summarize video</button>
          </div>
        </div>

        <div class="sp-status" id="status"></div>
      </div>

      <button class="sp-launcher" id="launcher" aria-label="Open Superpower page assistant">
        <span class="sp-mark">S</span>
      </button>

      <div class="sp-bubble" id="selectionBubble" data-visible="false">
        <button class="sp-mini" data-bubble-action="summary">Summarize</button>
        <button class="sp-mini" data-bubble-action="explain">Explain</button>
        <button class="sp-mini" data-bubble-action="save">Save</button>
      </div>
    </div>
  `;

  const panel = shadow.getElementById('panel');
  const launcher = shadow.getElementById('launcher');
  const selectionPreview = shadow.getElementById('selectionPreview');
  const selectionBubble = shadow.getElementById('selectionBubble');
  const statusNode = shadow.getElementById('status');
  const youtubeBlock = shadow.getElementById('youtubeBlock');

  const setStatus = message => {
    statusNode.textContent = message || '';
    statusNode.dataset.visible = message ? 'true' : 'false';
  };

  const refreshSelectionPreview = () => {
    const selected = getSelectionText();
    selectionPreview.textContent = selected;
    selectionPreview.dataset.visible = selected ? 'true' : 'false';
    return selected;
  };

  const openPanel = () => {
    panel.dataset.open = panel.dataset.open === 'true' ? 'false' : 'true';
    if (panel.dataset.open === 'true') {
      refreshSelectionPreview();
      youtubeBlock.style.display = isYouTubeWatchPage() ? 'block' : 'none';
    }
  };

  launcher.addEventListener('click', openPanel);

  const runPromptAction = async (action, text) => {
    const context = truncate(text || '');
    if (!context) {
      setStatus('Select text first, or use a page-level action.');
      return;
    }

    setStatus('Sending to your AI workspace…');
    const response = await routePrompt(buildPrompt(action, context));
    setStatus(
      response?.success
        ? `Sent to ${response.provider || 'your AI workspace'}.`
        : response?.error || 'Could not route the prompt.',
    );
  };

  shadow.addEventListener('click', async event => {
    const target = event.target;
    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.dataset.action;
    if (!action) {
      return;
    }

    const selection = refreshSelectionPreview();

    if (action === 'selection-summary') {
      await runPromptAction('summary', selection);
    } else if (action === 'selection-explain') {
      await runPromptAction('explain', selection);
    } else if (action === 'selection-ask') {
      await runPromptAction('ask', selection);
    } else if (action === 'selection-save') {
      setStatus((await saveKnowledge(selection)) ? 'Saved selection to Knowledge.' : 'Select text first.');
    } else if (action === 'page-summary') {
      setStatus('Sending page summary request…');
      const response = await routePrompt(buildPrompt('summary', getPageText()));
      setStatus(
        response?.success
          ? `Sent to ${response.provider || 'your AI workspace'}.`
          : response?.error || 'Could not route the prompt.',
      );
    } else if (action === 'page-actions') {
      setStatus('Extracting page actions with your AI…');
      const response = await routePrompt(buildPrompt('actions', getPageText()));
      setStatus(
        response?.success
          ? `Sent to ${response.provider || 'your AI workspace'}.`
          : response?.error || 'Could not route the prompt.',
      );
    } else if (action === 'page-save') {
      setStatus(
        (await saveKnowledge(getPageText())) ? 'Saved page excerpt to Knowledge.' : 'No readable page text found.',
      );
    } else if (action === 'youtube-summary') {
      setStatus('Collecting video context and transcript…');
      const prompt = await buildYouTubePrompt();
      const response = await routePrompt(prompt);
      setStatus(
        response?.success
          ? `Video sent to ${response.provider || 'your AI workspace'}.`
          : response?.error || 'Could not route the video summary.',
      );
    }
  });

  const hideSelectionBubble = () => {
    selectionBubble.dataset.visible = 'false';
  };

  const positionSelectionBubble = () => {
    const selection = window.getSelection();
    const text = getSelectionText();

    if (!selection || selection.rangeCount === 0 || text.length < 2) {
      hideSelectionBubble();
      return;
    }

    const rect = selection.getRangeAt(0).getBoundingClientRect();
    if (!rect || (!rect.width && !rect.height)) {
      hideSelectionBubble();
      return;
    }

    selectionBubble.style.left = `${Math.min(window.innerWidth - 250, Math.max(8, rect.left))}px`;
    selectionBubble.style.top = `${Math.max(8, rect.top - 44)}px`;
    selectionBubble.dataset.visible = 'true';
  };

  document.addEventListener('mouseup', () => window.setTimeout(positionSelectionBubble, 20), true);
  document.addEventListener('keyup', () => window.setTimeout(positionSelectionBubble, 20), true);
  document.addEventListener('scroll', hideSelectionBubble, true);

  selectionBubble.addEventListener('mousedown', event => event.preventDefault());
  selectionBubble.addEventListener('click', async event => {
    const target = event.target;
    if (!(target instanceof HTMLElement)) {
      return;
    }

    const action = target.dataset.bubbleAction;
    const selection = getSelectionText();
    if (!action || !selection) {
      hideSelectionBubble();
      return;
    }

    if (action === 'save') {
      const saved = await saveKnowledge(selection);
      setStatus(saved ? 'Saved selection to Knowledge.' : 'Could not save selection.');
      panel.dataset.open = 'true';
    } else {
      panel.dataset.open = 'true';
      await runPromptAction(action, selection);
    }

    hideSelectionBubble();
  });

  let lastUrl = window.location.href;
  const observer = new MutationObserver(() => {
    if (window.location.href !== lastUrl) {
      lastUrl = window.location.href;
      youtubeBlock.style.display = isYouTubeWatchPage() ? 'block' : 'none';
      setStatus('');
    }
  });
  observer.observe(document.documentElement, { childList: true, subtree: true });
})();
