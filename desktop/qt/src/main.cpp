#include "conversationwindow.h"
#include "mainwindow.h"
#include "mcpbridgeprocess.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QKeySequence>
#include <QObject>
#include <QShortcut>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("Superpower Desktop"));
  QCoreApplication::setOrganizationName(QStringLiteral("Superpower"));
  QCoreApplication::setApplicationVersion(QStringLiteral(SUPERPOWER_DESKTOP_VERSION));

  QFont font = app.font();
  font.setPointSize(10);
  app.setFont(font);

  MainWindow window;
  auto *conversationWindow = new ConversationWindow(&window);
  if (auto *bridge = window.findChild<McpBridgeProcess *>()) {
    QObject::connect(bridge, &McpBridgeProcess::conversationEvent,
                     conversationWindow, &ConversationWindow::ingestEvent);
  }

  auto *conversationShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")), &window);
  QObject::connect(conversationShortcut, &QShortcut::activated,
                   conversationWindow, &ConversationWindow::showConversation);

  window.show();
  return app.exec();
}