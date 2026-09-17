const LOCAL_MCP_URL = 'http://localhost:3006/mcp';

const statusDot = document.getElementById('status-dot');
const statusTitle = document.getElementById('status-title');
const statusDetail = document.getElementById('status-detail');
const connectLocalButton = document.getElementById('connect-local');
const versionLabel = document.getElementById('version');

const requestId = () => `popup-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
const wait = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));

const sendMcpMessage = async (type, payload = {}) => {
  return await chrome.runtime.sendMessage({
    type,
    payload,
    origin: 'popup',
    timestamp: Date.now(),
    id: requestId(),
  });
};

const setStatus = (state, title, detail) => {
  statusDot.className = `status-dot ${state}`;
  statusTitle.textContent = title;
  statusDetail.textContent = detail;
};

const readConnectionStatus = async () => {
  const response = await sendMcpMessage('mcp:get-connection-status');
  return Boolean(response?.success && response?.payload?.isConnected);
};

const waitForConnection = async (attempts = 6, delayMs = 450) => {
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    if (await readConnectionStatus()) return true;
    if (attempt < attempts - 1) await wait(delayMs);
  }
  return false;
};

const refreshStatus = async () => {
  setStatus('checking', 'Checking MCP…', 'Reading the current local connection.');

  try {
    const connected = await readConnectionStatus();

    if (!connected) {
      setStatus(
        'error',
        'MCP not connected',
        'Connect the local Superpower host or configure another server in the sidebar.',
      );
      connectLocalButton.textContent = 'Connect local MCP';
      return;
    }

    let toolCount = 0;
    try {
      const toolsResponse = await sendMcpMessage('mcp:get-tools');
      if (toolsResponse?.success && Array.isArray(toolsResponse.payload)) {
        toolCount = toolsResponse.payload.length;
      }
    } catch {
      toolCount = 0;
    }

    setStatus(
      'connected',
      'MCP ready',
      toolCount > 0
        ? `${toolCount} tool${toolCount === 1 ? '' : 's'} available.`
        : 'Connected. Tools can be refreshed from the sidebar.',
    );
    connectLocalButton.textContent = 'Reconnect local MCP';
  } catch (error) {
    setStatus(
      'error',
      'MCP status unavailable',
      error instanceof Error ? error.message : 'Open a supported AI page and try again.',
    );
  }
};

const connectLocal = async () => {
  connectLocalButton.disabled = true;
  connectLocalButton.textContent = 'Connecting…';
  setStatus('checking', 'Connecting…', 'Using the local Superpower MCP endpoint.');

  try {
    const configResponse = await sendMcpMessage('mcp:update-server-config', {
      config: {
        uri: LOCAL_MCP_URL,
        connectionType: 'streamable-http',
      },
    });

    if (!configResponse?.success) {
      throw new Error(configResponse?.error || 'Could not save the local MCP configuration.');
    }

    let connected = await waitForConnection();
    if (!connected) {
      const reconnectResponse = await sendMcpMessage('mcp:force-reconnect');
      connected = Boolean(reconnectResponse?.success && reconnectResponse?.payload?.isConnected);
      if (!connected) {
        throw new Error(
          reconnectResponse?.payload?.error || reconnectResponse?.error || 'Local MCP server did not respond.',
        );
      }
    }

    await refreshStatus();
  } catch (error) {
    setStatus(
      'error',
      'Local MCP unavailable',
      error instanceof Error ? error.message : 'Start Superpower Host, then try again.',
    );
    connectLocalButton.textContent = 'Try local MCP again';
  } finally {
    connectLocalButton.disabled = false;
  }
};

connectLocalButton.addEventListener('click', () => {
  void connectLocal();
});

document.querySelectorAll('[data-url]').forEach(button => {
  button.addEventListener('click', () => {
    const url = button.dataset.url;
    if (url) void chrome.tabs.create({ url });
  });
});

document.getElementById('open-project').addEventListener('click', () => {
  void chrome.tabs.create({ url: 'https://github.com/stloendays/Superpower' });
});

const manifest = chrome.runtime.getManifest();
versionLabel.textContent = `v${manifest.version}`;

void refreshStatus();
