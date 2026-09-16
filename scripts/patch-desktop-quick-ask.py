from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    target = Path(path)
    text = target.read_text(encoding="utf-8")
    if old not in text:
        raise SystemExit(f"Expected patch anchor not found in {path}: {old[:120]!r}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


background = "chrome-extension/src/background/index.ts"
replace_once(
    background,
    "import 'webextension-polyfill';\n",
    "import 'webextension-polyfill';\nimport './desktop-conversation-forwarder';\n",
)
replace_once(
    background,
    "    connectionType = DEFAULT_CONNECTION_TYPE;\n    serverUrl = DEFAULT_SSE_URL;\n    isInitialized = true;\n",
    "    connectionType = DEFAULT_CONNECTION_TYPE;\n    serverUrl = DEFAULT_STREAMABLE_HTTP_URL;\n    isInitialized = true;\n",
)
replace_once(
    background,
    "            const url = new URL(config.uri);\n            newType = (url.protocol === 'ws:' || url.protocol === 'wss:') ? 'websocket' : 'sse';\n",
    "            const url = new URL(config.uri);\n            if (url.protocol === 'ws:' || url.protocol === 'wss:') {\n              newType = 'websocket';\n            } else if (/\\/sse\\/?$/i.test(url.pathname)) {\n              newType = 'sse';\n            } else {\n              newType = 'streamable-http';\n            }\n",
)

content_index = "pages/content/src/index.ts"
replace_once(
    content_index,
    "import './tailwind-input.css';\n",
    "import './tailwind-input.css';\nimport './utils/desktop-conversation-sync';\n",
)

main_cpp = "desktop/qt/src/main.cpp"
replace_once(
    main_cpp,
    "#include <QCoreApplication>\n#include <QDir>\n",
    "#include <QCoreApplication>\n#include <QDesktopServices>\n#include <QDir>\n",
)
replace_once(
    main_cpp,
    "#include <QKeySequence>\n#include <QMenu>\n",
    "#include <QJsonObject>\n#include <QKeySequence>\n#include <QMenu>\n",
)
replace_once(
    main_cpp,
    "#include <QToolBar>\n",
    "#include <QToolBar>\n#include <QUrl>\n",
)

help_anchor = """  QObject::connect(gettingStartedAction, &QAction::triggered, &window,
                   [onboarding, showActions]() {
                     showActions();
                     onboarding->showForCurrentState();
                   });
"""
help_replacement = help_anchor + """  QAction *desktopGuideAction = helpMenu->addAction(QStringLiteral("Desktop Guide..."));
  QObject::connect(desktopGuideAction, &QAction::triggered, &window, []() {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://github.com/stloendays/Superpower/blob/main/docs/usage/desktop-app.md")));
  });
"""
replace_once(main_cpp, help_anchor, help_replacement)

relay_anchor = "  auto *conversationRelay = new McpBridgeProcess(&window);\n"
relay_replacement = relay_anchor + """  QObject::connect(conversation, &ConversationWindow::promptSubmitted, &window,
                   [conversation, conversationDock, conversationRelay](const QString &text) {
                     conversationDock->show();
                     conversationDock->raise();
                     const QString requestId = conversationRelay->sendRequest(
                         QStringLiteral("submit_prompt"), QJsonObject{{QStringLiteral("text"), text}});
                     if (requestId.isEmpty()) {
                       conversation->setPromptStatus(
                           QStringLiteral("Quick Ask unavailable - the local conversation relay is not running."), false);
                       return;
                     }
                     conversation->setPromptStatus(
                         QStringLiteral("Waiting for the active ChatGPT tab to accept the prompt..."), false);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::responseReceived, conversation,
                   [conversation](const QString &, const QString &method, const QJsonValue &result) {
                     if (method != QStringLiteral("submit_prompt")) return;
                     const QString message = result.toObject().value(QStringLiteral("message")).toString();
                     conversation->setPromptStatus(
                         message.isEmpty() ? QStringLiteral("Quick Ask sent to ChatGPT.") : message, true);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::requestFailed, conversation,
                   [conversation](const QString &, const QString &method, const QString &, const QString &message,
                                  const QJsonObject &) {
                     if (method != QStringLiteral("submit_prompt")) return;
                     conversation->setPromptStatus(
                         QStringLiteral("Quick Ask failed - %1").arg(message), false);
                   });
"""
replace_once(main_cpp, relay_anchor, relay_replacement)

ready_anchor = """                     conversation->setRelayStatus(status, true);
                     dashboard->setConversationRelayStatus(status, true);
"""
replace_once(
    main_cpp,
    ready_anchor,
    """                     conversation->setRelayStatus(status, true);
                     conversation->setPromptStatus(
                         QStringLiteral("Quick Ask ready - prompts are sent to the active ChatGPT tab."), true);
                     dashboard->setConversationRelayStatus(status, true);
""",
)

desktop_readme = Path("desktop/qt/README.md")
desktop_text = desktop_readme.read_text(encoding="utf-8")
guide_section = """

## Desktop Quick Ask and user guide

The Conversation dock now includes a **Quick Ask** bar. With the Superpower browser extension active on ChatGPT, a desktop prompt is routed through the loopback conversation relay (`127.0.0.1:32148`) into the active ChatGPT tab and the resulting conversation is mirrored back into Desktop.

This conversation relay is independent of the MCP endpoint. The standard local MCP proxy remains `http://localhost:3006/mcp`.

- [Desktop guide](../../docs/usage/desktop-app.md)
- [Chinese desktop guide](../../docs/usage/desktop-app.zh-CN.md)
"""
if "## Desktop Quick Ask and user guide" not in desktop_text:
    desktop_readme.write_text(desktop_text.rstrip() + guide_section + "\n", encoding="utf-8")

for readme_path, marker, addition in [
    (
        "README.md",
        "### Desktop App\n",
        "**Guide:** [Desktop usage and Quick Ask](docs/usage/desktop-app.md)\n\n",
    ),
    (
        "README.zh-CN.md",
        "### 桌面应用\n",
        "**指南：** [桌面端使用与 Quick Ask](docs/usage/desktop-app.zh-CN.md)\n\n",
    ),
]:
    target = Path(readme_path)
    text = target.read_text(encoding="utf-8")
    if addition.strip() not in text:
        if marker not in text:
            raise SystemExit(f"Readme marker not found: {readme_path} {marker!r}")
        target.write_text(text.replace(marker, marker + "\n" + addition, 1), encoding="utf-8")
