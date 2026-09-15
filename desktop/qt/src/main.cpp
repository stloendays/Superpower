#include "conversationwindow.h"
#include "mainwindow.h"
#include "mcpbridgeprocess.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFont>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QStandardPaths>

namespace {
QString findDefaultNodeProgram() {
  const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
  const QString packagedNode = QDir(appDir).filePath(QStringLiteral("runtime/node.exe"));
#else
  const QString packagedNode = QDir(appDir).filePath(QStringLiteral("runtime/node"));
#endif
  if (QFileInfo::exists(packagedNode)) return QDir::toNativeSeparators(QFileInfo(packagedNode).absoluteFilePath());

  const QString discovered = QStandardPaths::findExecutable(QStringLiteral("node"));
  return discovered.isEmpty() ? QStringLiteral("node") : QDir::toNativeSeparators(discovered);
}

QString findDefaultHostScript() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString packagedBridge = QDir(appDir).filePath(QStringLiteral("bridge/superpower-host.mjs"));
  if (QFileInfo::exists(packagedBridge)) {
    return QDir::toNativeSeparators(QFileInfo(packagedBridge).absoluteFilePath());
  }

  const QStringList startingPoints{QDir::currentPath(), appDir};
  for (const QString &start : startingPoints) {
    QDir dir(start);
    for (int level = 0; level < 7; ++level) {
      const QString candidate = dir.filePath(QStringLiteral("packages/mcp-host/dist/cli.js"));
      if (QFileInfo::exists(candidate)) return QDir::toNativeSeparators(QFileInfo(candidate).absoluteFilePath());
      if (!dir.cdUp()) break;
    }
  }
  return QDir::toNativeSeparators(QStringLiteral("packages/mcp-host/dist/cli.js"));
}
}  // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("Superpower Desktop"));
  QCoreApplication::setOrganizationName(QStringLiteral("Superpower"));
  QCoreApplication::setApplicationVersion(QStringLiteral(SUPERPOWER_DESKTOP_VERSION));

  QFont font = app.font();
  font.setPointSize(10);
  app.setFont(font);

  MainWindow window;

  auto *conversationDock = new QDockWidget(QStringLiteral("Conversation"), &window);
  conversationDock->setObjectName(QStringLiteral("conversationDock"));
  conversationDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
  conversationDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
                                QDockWidget::DockWidgetFloatable);
  auto *conversation = new ConversationWindow(conversationDock);
  conversationDock->setWidget(conversation);
  window.addDockWidget(Qt::RightDockWidgetArea, conversationDock);
  window.resizeDocks({conversationDock}, {430}, Qt::Horizontal);

  QMenu *viewMenu = window.menuBar()->addMenu(QStringLiteral("View"));
  QAction *conversationAction = conversationDock->toggleViewAction();
  conversationAction->setText(QStringLiteral("Conversation"));
  conversationAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
  viewMenu->addAction(conversationAction);

  auto *conversationRelay = new McpBridgeProcess(&window);
  QObject::connect(conversationRelay, &McpBridgeProcess::bridgeReady, conversation,
                   [conversation](const QString &transport) {
                     Q_UNUSED(transport);
                     conversation->setRelayStatus(
                         QStringLiteral("Listening locally on 127.0.0.1:32148 · no MCP server connection required."),
                         true);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::conversationEvent, &window,
                   [conversation, conversationDock](const QJsonObject &event) {
                     conversation->ingestEvent(event);
                     if (!conversationDock->isVisible()) conversationDock->show();
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::processError, conversation,
                   [conversation](const QString &message) {
                     conversation->setRelayStatus(QStringLiteral("Conversation relay unavailable · %1").arg(message), false);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::processExited, conversation,
                   [conversation](int exitCode, QProcess::ExitStatus) {
                     conversation->setRelayStatus(
                         QStringLiteral("Conversation relay stopped (exit %1). Restart Superpower Desktop to resume sync.")
                             .arg(exitCode),
                         false);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::logLine, conversation,
                   [conversation](const QString &line) {
                     if (!line.trimmed().isEmpty()) {
                       conversation->setRelayStatus(QStringLiteral("Conversation relay · %1").arg(line.trimmed()), false);
                     }
                   });

  window.show();
  conversationRelay->startConversation(findDefaultNodeProgram(), findDefaultHostScript());
  return app.exec();
}
