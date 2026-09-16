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
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFont>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QToolBar>
#include <QUrl>

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
                         QStringLiteral("Planning ordered workflow steps, explicit bindings, and policy previews... Nothing has run."),
                         true);
                   });
  QObject::connect(&window, &MainWindow::workflowPlanPrepared, dashboard,
                   [dashboard, workflowReview](const QString &summary, const QJsonObject &plan) {
                     dashboard->setActionRouterStatus(
                         QStringLiteral("Workflow prepared for binding review: %1").arg(summary), false);
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
  QObject::connect(workflowReview, &WorkflowReviewDialog::startRunRequested, &window,
                   &MainWindow::startWorkflowRun);
  QObject::connect(workflowReview, &WorkflowReviewDialog::advanceRunRequested, &window,
                   &MainWindow::advanceWorkflowRun);
  QObject::connect(workflowReview, &WorkflowReviewDialog::provideOutputRequested, &window,
                   &MainWindow::provideWorkflowStepOutput);
  QObject::connect(&window, &MainWindow::workflowRunUpdated, workflowReview,
                   &WorkflowReviewDialog::setRunState);
  QObject::connect(&window, &MainWindow::workflowRunUpdated, dashboard,
                   [dashboard](const QJsonObject &state) {
                     const QString status = state.value(QStringLiteral("status")).toString(QStringLiteral("ready"));
                     const QJsonObject gate = state.value(QStringLiteral("gate")).toObject();
                     const QString gateType = gate.value(QStringLiteral("type")).toString(QStringLiteral("unknown"));
                     dashboard->setActionRouterStatus(
                         QStringLiteral("Workflow Runner · %1 · gate %2 · one-step advance only")
                             .arg(status.toUpper(), gateType.toUpper()),
                         false);
                   });
  QObject::connect(&window, &MainWindow::workflowRunFailed, workflowReview,
                   &WorkflowReviewDialog::setRunError);
  QObject::connect(&window, &MainWindow::workflowRunFailed, dashboard,
                   [dashboard](const QString &message) { dashboard->setActionRouterStatus(message, false); });

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
  QAction *desktopGuideAction = helpMenu->addAction(QStringLiteral("Desktop Guide..."));
  QObject::connect(desktopGuideAction, &QAction::triggered, &window, []() {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://github.com/stloendays/Superpower/blob/main/docs/usage/desktop-app.md")));
  });
  QObject::connect(dashboard, &WorkspaceDashboard::gettingStartedRequested, &window,
                   [onboarding, showActions]() {
                     showActions();
                     onboarding->showForCurrentState();
                   });

  auto *conversationRelay = new McpBridgeProcess(&window);
  QObject::connect(conversation, &ConversationWindow::promptSubmitted, &window,
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
  dashboard->setConversationRelayStatus(QStringLiteral("Starting local conversation relay..."), false);
  QObject::connect(conversationRelay, &McpBridgeProcess::bridgeReady, conversation,
                   [conversation, dashboard](const QString &transport) {
                     Q_UNUSED(transport);
                     const QString status =
                         QStringLiteral("Listening locally on 127.0.0.1:32148 · no MCP server connection required.");
                     conversation->setRelayStatus(status, true);
                     conversation->setPromptStatus(
                         QStringLiteral("Quick Ask ready - prompts are sent to the active ChatGPT tab."), true);
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
