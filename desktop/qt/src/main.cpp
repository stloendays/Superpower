#include "conversationwindow.h"
#include "mainwindow.h"
#include "mcpbridgeprocess.h"
#include "onboardingdialog.h"
#include "workflowreviewdialog.h"
#include "workspacedashboard.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFont>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QToolBar>

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

  QWidget *actionsWorkspace = window.takeCentralWidget();
  auto *workspaceStack = new QStackedWidget(&window);
  auto *dashboard = new WorkspaceDashboard(&window, workspaceStack);
  workspaceStack->addWidget(dashboard);
  workspaceStack->addWidget(actionsWorkspace);
  window.setCentralWidget(workspaceStack);

  auto *workflowReview = new WorkflowReviewDialog(&window);

  auto *workspaceToolbar = window.addToolBar(QStringLiteral("Workspace"));
  workspaceToolbar->setObjectName(QStringLiteral("workspaceToolbar"));
  workspaceToolbar->setMovable(false);
  workspaceToolbar->setFloatable(false);
  workspaceToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

  auto *workspaceGroup = new QActionGroup(workspaceToolbar);
  workspaceGroup->setExclusive(true);
  QAction *homeAction = workspaceToolbar->addAction(QStringLiteral("Home"));
  homeAction->setCheckable(true);
  homeAction->setChecked(true);
  homeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
  workspaceGroup->addAction(homeAction);

  QAction *actionsAction = workspaceToolbar->addAction(QStringLiteral("Actions"));
  actionsAction->setCheckable(true);
  actionsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+2")));
  workspaceGroup->addAction(actionsAction);

  const auto showHome = [workspaceStack, homeAction]() {
    workspaceStack->setCurrentIndex(0);
    homeAction->setChecked(true);
  };
  const auto showActions = [workspaceStack, actionsAction]() {
    workspaceStack->setCurrentIndex(1);
    actionsAction->setChecked(true);
  };
  QObject::connect(homeAction, &QAction::triggered, &window, showHome);
  QObject::connect(actionsAction, &QAction::triggered, &window, showActions);
  QObject::connect(dashboard, &WorkspaceDashboard::browseActionsRequested, &window, showActions);
  QObject::connect(dashboard, &WorkspaceDashboard::actionQueryRequested, &window, &MainWindow::planAction);
  QObject::connect(dashboard, &WorkspaceDashboard::workflowQueryRequested, &window, &MainWindow::planWorkflow);

  QObject::connect(&window, &MainWindow::actionPlanStarted, dashboard,
                   [dashboard](const QString &) {
                     dashboard->setActionRouterStatus(
                         QStringLiteral("Routing against the full MCP catalog and drafting parameters... Nothing has run."),
                         true);
                   });
  QObject::connect(&window, &MainWindow::actionPlanPrepared, dashboard,
                   [dashboard, showActions](const QString &summary) {
                     dashboard->setActionRouterStatus(
                         QStringLiteral("Prepared for review: %1").arg(summary), false);
                     showActions();
                   });
  QObject::connect(&window, &MainWindow::actionPlanFailed, dashboard,
                   [dashboard](const QString &message) { dashboard->setActionRouterStatus(message, false); });

  QObject::connect(&window, &MainWindow::workflowPlanStarted, dashboard,
                   [dashboard](const QString &) {
                     dashboard->setActionRouterStatus(
                         QStringLiteral("Planning ordered workflow steps and policy previews... Nothing has run."), true);
                   });
  QObject::connect(&window, &MainWindow::workflowPlanPrepared, dashboard,
                   [dashboard, workflowReview](const QString &summary, const QJsonObject &plan) {
                     dashboard->setActionRouterStatus(
                         QStringLiteral("Workflow prepared for review: %1").arg(summary), false);
                     workflowReview->setPlan(plan);
                     workflowReview->show();
                     workflowReview->raise();
                     workflowReview->activateWindow();
                   });
  QObject::connect(&window, &MainWindow::workflowPlanFailed, dashboard,
                   [dashboard](const QString &message) { dashboard->setActionRouterStatus(message, false); });
  QObject::connect(workflowReview, &WorkflowReviewDialog::reviewActionRequested, &window,
                   [&window, showActions](const QJsonObject &actionPlan) {
                     showActions();
                     window.reviewWorkflowStep(actionPlan);
                   });

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
  viewMenu->addAction(homeAction);
  viewMenu->addAction(actionsAction);
  QAction *workflowReviewAction = viewMenu->addAction(QStringLiteral("Workflow Review..."));
  QObject::connect(workflowReviewAction, &QAction::triggered, workflowReview,
                   [workflowReview]() {
                     workflowReview->show();
                     workflowReview->raise();
                   });
  viewMenu->addSeparator();
  QAction *conversationAction = conversationDock->toggleViewAction();
  conversationAction->setText(QStringLiteral("Conversation"));
  conversationAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
  viewMenu->addAction(conversationAction);

  QObject::connect(dashboard, &WorkspaceDashboard::openConversationRequested, &window,
                   [conversationDock]() {
                     conversationDock->show();
                     conversationDock->raise();
                   });

  auto *onboarding = new OnboardingDialog(&window);
  QMenu *helpMenu = window.menuBar()->addMenu(QStringLiteral("Help"));
  QAction *gettingStartedAction = helpMenu->addAction(QStringLiteral("Getting Started..."));
  gettingStartedAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
  QObject::connect(gettingStartedAction, &QAction::triggered, &window,
                   [onboarding, showActions]() {
                     showActions();
                     onboarding->showForCurrentState();
                   });
  QObject::connect(dashboard, &WorkspaceDashboard::gettingStartedRequested, &window,
                   [onboarding, showActions]() {
                     showActions();
                     onboarding->showForCurrentState();
                   });

  auto *conversationRelay = new McpBridgeProcess(&window);
  dashboard->setConversationRelayStatus(QStringLiteral("Starting local conversation relay..."), false);
  QObject::connect(conversationRelay, &McpBridgeProcess::bridgeReady, conversation,
                   [conversation, dashboard](const QString &transport) {
                     Q_UNUSED(transport);
                     const QString status =
                         QStringLiteral("Listening locally on 127.0.0.1:32148 · no MCP server connection required.");
                     conversation->setRelayStatus(status, true);
                     dashboard->setConversationRelayStatus(status, true);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::conversationEvent, &window,
                   [conversation, conversationDock, dashboard](const QJsonObject &event) {
                     conversation->ingestEvent(event);
                     dashboard->noteConversationActivity();
                     if (!conversationDock->isVisible()) conversationDock->show();
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::processError, conversation,
                   [conversation, dashboard](const QString &message) {
                     const QString status = QStringLiteral("Conversation relay unavailable · %1").arg(message);
                     conversation->setRelayStatus(status, false);
                     dashboard->setConversationRelayStatus(status, false);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::processExited, conversation,
                   [conversation, dashboard](int exitCode, QProcess::ExitStatus) {
                     const QString status =
                         QStringLiteral("Conversation relay stopped (exit %1). Restart Superpower Desktop to resume sync.")
                             .arg(exitCode);
                     conversation->setRelayStatus(status, false);
                     dashboard->setConversationRelayStatus(status, false);
                   });
  QObject::connect(conversationRelay, &McpBridgeProcess::logLine, conversation,
                   [conversation](const QString &line) {
                     if (!line.trimmed().isEmpty()) {
                       conversation->setRelayStatus(QStringLiteral("Conversation relay · %1").arg(line.trimmed()), false);
                     }
                   });

  window.show();
  conversationRelay->startConversation(findDefaultNodeProgram(), findDefaultHostScript());

  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  if (!settings.value(QStringLiteral("onboarding/v1_5_seen"), false).toBool()) {
    QTimer::singleShot(350, onboarding, [onboarding, showActions]() {
      showActions();
      onboarding->showForCurrentState();
    });
  } else {
    showHome();
  }

  return app.exec();
}
