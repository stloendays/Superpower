#include "workspacedashboard.h"

#include "mainwindow.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QFrame *dashboardCard(QWidget *parent) {
  auto *frame = new QFrame(parent);
  frame->setProperty("dashboardCard", true);
  return frame;
}

QString providerDisplayName(const QString &provider) {
  if (provider == QStringLiteral("chatgpt")) return QStringLiteral("ChatGPT");
  if (provider == QStringLiteral("gemini")) return QStringLiteral("Gemini");
  if (provider == QStringLiteral("grok")) return QStringLiteral("Grok");
  if (provider == QStringLiteral("perplexity")) return QStringLiteral("Perplexity");
  return provider;
}
}

WorkspaceDashboard::WorkspaceDashboard(MainWindow *workspace, QWidget *parent)
    : QWidget(parent), workspace_(workspace) {
  setObjectName(QStringLiteral("workspaceDashboard"));
  buildUi();
  applyStyle();

  refreshTimer_ = new QTimer(this);
  refreshTimer_->setInterval(500);
  connect(refreshTimer_, &QTimer::timeout, this, &WorkspaceDashboard::refreshFromWorkspace);
  refreshTimer_->start();
  refreshFromWorkspace();
}

QWidget *WorkspaceDashboard::createStatusCard(const QString &title, QLabel **valueLabel,
                                              QLabel **detailLabel) {
  auto *frame = dashboardCard(this);
  auto *layout = new QVBoxLayout(frame);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(7);

  auto *titleLabel = new QLabel(title, frame);
  titleLabel->setProperty("dashboardEyebrow", true);
  layout->addWidget(titleLabel);

  *valueLabel = new QLabel(QStringLiteral("Checking..."), frame);
  (*valueLabel)->setProperty("dashboardStatus", true);
  layout->addWidget(*valueLabel);

  *detailLabel = new QLabel(frame);
  (*detailLabel)->setProperty("dashboardMuted", true);
  (*detailLabel)->setWordWrap(true);
  layout->addWidget(*detailLabel);
  layout->addStretch(1);
  return frame;
}

QWidget *WorkspaceDashboard::createMetricCard(const QString &title, QLabel **valueLabel,
                                              const QString &detail) {
  auto *frame = dashboardCard(this);
  auto *layout = new QVBoxLayout(frame);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(6);

  auto *titleLabel = new QLabel(title, frame);
  titleLabel->setProperty("dashboardEyebrow", true);
  layout->addWidget(titleLabel);

  *valueLabel = new QLabel(QStringLiteral("0"), frame);
  (*valueLabel)->setProperty("dashboardMetric", true);
  layout->addWidget(*valueLabel);

  auto *detailLabel = new QLabel(detail, frame);
  detailLabel->setProperty("dashboardMuted", true);
  detailLabel->setWordWrap(true);
  layout->addWidget(detailLabel);
  layout->addStretch(1);
  return frame;
}

void WorkspaceDashboard::submitActionQuery() {
  const QString query = actionQueryEdit_->text().trimmed();
  if (query.isEmpty()) {
    setActionRouterStatus(QStringLiteral("Describe the action you want Superpower to route."), false);
    actionQueryEdit_->setFocus();
    return;
  }
  emit actionQueryRequested(query);
}

void WorkspaceDashboard::submitWorkflowQuery() {
  const QString query = actionQueryEdit_->text().trimmed();
  if (query.isEmpty()) {
    setActionRouterStatus(QStringLiteral("Describe the multi-step outcome you want Superpower to plan."), false);
    actionQueryEdit_->setFocus();
    return;
  }
  emit workflowQueryRequested(query);
}

void WorkspaceDashboard::submitWebChatGptPrompt() {
  if (!webChatGptInput_) return;
  const QString prompt = webChatGptInput_->text().trimmed();
  if (prompt.isEmpty()) {
    setWebChatGptStatus(QStringLiteral("Type a question for web ChatGPT first."), false);
    webChatGptInput_->setFocus();
    return;
  }
  if (!conversationRelayOnline_) {
    setWebChatGptStatus(
        QStringLiteral("The local browser relay is not ready yet. Open ChatGPT with the Superpower extension enabled."),
        false);
    return;
  }

  webChatGptInput_->clear();
  setWebChatGptStatus(QStringLiteral("Sending to the active ChatGPT browser tab..."), false);
  emit askWebChatGptRequested(prompt);
}

void WorkspaceDashboard::buildUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(28, 26, 28, 26);
  root->setSpacing(18);

  auto *headerRow = new QHBoxLayout();
  auto *headerBlock = new QWidget(this);
  auto *headerLayout = new QVBoxLayout(headerBlock);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(4);

  auto *title = new QLabel(QStringLiteral("Workspace Home"), headerBlock);
  title->setProperty("dashboardTitle", true);
  headerLayout->addWidget(title);

  auto *subtitle = new QLabel(
      QStringLiteral("Ask web ChatGPT directly, route one MCP Action, or plan a review-first workflow."),
      headerBlock);
  subtitle->setProperty("dashboardMuted", true);
  subtitle->setWordWrap(true);
  headerLayout->addWidget(subtitle);
  headerRow->addWidget(headerBlock, 1);

  auto *browseButton = new QPushButton(QStringLiteral("Browse actions"), this);
  browseButton->setProperty("dashboardPrimary", true);
  connect(browseButton, &QPushButton::clicked, this, &WorkspaceDashboard::browseActionsRequested);
  headerRow->addWidget(browseButton);
  root->addLayout(headerRow);

  auto *chatGptCard = dashboardCard(this);
  chatGptCard->setProperty("webChatGptCard", true);
  auto *chatGptLayout = new QVBoxLayout(chatGptCard);
  chatGptLayout->setContentsMargins(20, 18, 20, 18);
  chatGptLayout->setSpacing(9);

  auto *chatGptEyebrow = new QLabel(QStringLiteral("WEB CHATGPT"), chatGptCard);
  chatGptEyebrow->setProperty("dashboardEyebrow", true);
  chatGptLayout->addWidget(chatGptEyebrow);

  auto *chatGptTitle = new QLabel(QStringLiteral("Ask the web version of ChatGPT"), chatGptCard);
  chatGptTitle->setProperty("actionRouterTitle", true);
  chatGptLayout->addWidget(chatGptTitle);

  auto *chatGptHint = new QLabel(
      QStringLiteral("Uses your existing signed-in ChatGPT browser session through the Superpower extension. No OpenAI API key is required. Keep a ChatGPT tab open and active."),
      chatGptCard);
  chatGptHint->setProperty("dashboardMuted", true);
  chatGptHint->setWordWrap(true);
  chatGptLayout->addWidget(chatGptHint);

  auto *chatGptInputRow = new QHBoxLayout();
  chatGptInputRow->setSpacing(8);
  webChatGptInput_ = new QLineEdit(chatGptCard);
  webChatGptInput_->setProperty("actionRouterInput", true);
  webChatGptInput_->setClearButtonEnabled(true);
  webChatGptInput_->setPlaceholderText(QStringLiteral("Ask ChatGPT from Superpower Desktop..."));
  webChatGptInput_->setEnabled(false);
  connect(webChatGptInput_, &QLineEdit::returnPressed, this, &WorkspaceDashboard::submitWebChatGptPrompt);
  chatGptInputRow->addWidget(webChatGptInput_, 1);

  webChatGptButton_ = new QPushButton(QStringLiteral("Ask ChatGPT"), chatGptCard);
  webChatGptButton_->setProperty("dashboardPrimary", true);
  webChatGptButton_->setEnabled(false);
  connect(webChatGptButton_, &QPushButton::clicked, this, &WorkspaceDashboard::submitWebChatGptPrompt);
  chatGptInputRow->addWidget(webChatGptButton_);
  chatGptLayout->addLayout(chatGptInputRow);

  webChatGptStatusLabel_ = new QLabel(
      QStringLiteral("Starting the local relay. Open ChatGPT in your browser with the Superpower extension enabled."),
      chatGptCard);
  webChatGptStatusLabel_->setProperty("webChatGptStatus", true);
  webChatGptStatusLabel_->setWordWrap(true);
  chatGptLayout->addWidget(webChatGptStatusLabel_);
  root->addWidget(chatGptCard);

  auto *routerCard = dashboardCard(this);
  routerCard->setProperty("actionRouterCard", true);
  auto *routerLayout = new QVBoxLayout(routerCard);
  routerLayout->setContentsMargins(20, 18, 20, 18);
  routerLayout->setSpacing(9);

  auto *routerEyebrow = new QLabel(QStringLiteral("ACTION ROUTER · WORKFLOW PLANNER"), routerCard);
  routerEyebrow->setProperty("dashboardEyebrow", true);
  routerLayout->addWidget(routerEyebrow);

  auto *routerTitle = new QLabel(QStringLiteral("Route an action or plan a workflow"), routerCard);
  routerTitle->setProperty("actionRouterTitle", true);
  routerLayout->addWidget(routerTitle);

  auto *routerHint = new QLabel(
      QStringLiteral("Use Route action for one MCP tool. Use Plan workflow for explicit sequences such as search → summarize → write. Workflow planning separates MCP Actions, local transforms, unresolved steps, and cross-step handoffs; nothing auto-runs."),
      routerCard);
  routerHint->setProperty("dashboardMuted", true);
  routerHint->setWordWrap(true);
  routerLayout->addWidget(routerHint);

  auto *routerInputRow = new QHBoxLayout();
  routerInputRow->setSpacing(8);
  actionQueryEdit_ = new QLineEdit(routerCard);
  actionQueryEdit_->setProperty("actionRouterInput", true);
  actionQueryEdit_->setClearButtonEnabled(true);
  actionQueryEdit_->setPlaceholderText(
      QStringLiteral("e.g. Find GitHub bug issues, summarize them, then write the result to Notion"));
  connect(actionQueryEdit_, &QLineEdit::returnPressed, this, &WorkspaceDashboard::submitActionQuery);
  routerInputRow->addWidget(actionQueryEdit_, 1);

  actionRouteButton_ = new QPushButton(QStringLiteral("Route action"), routerCard);
  actionRouteButton_->setProperty("dashboardPrimary", true);
  connect(actionRouteButton_, &QPushButton::clicked, this, &WorkspaceDashboard::submitActionQuery);
  routerInputRow->addWidget(actionRouteButton_);

  workflowPlanButton_ = new QPushButton(QStringLiteral("Plan workflow"), routerCard);
  connect(workflowPlanButton_, &QPushButton::clicked, this, &WorkspaceDashboard::submitWorkflowQuery);
  routerInputRow->addWidget(workflowPlanButton_);
  routerLayout->addLayout(routerInputRow);

  actionRouterStatusLabel_ = new QLabel(
      QStringLiteral("Nothing runs from Home. Actions open for parameter review; workflows open in Workflow Review."),
      routerCard);
  actionRouterStatusLabel_->setProperty("actionRouterStatus", true);
  actionRouterStatusLabel_->setWordWrap(true);
  routerLayout->addWidget(actionRouterStatusLabel_);
  root->addWidget(routerCard);

  auto *healthTitle = new QLabel(QStringLiteral("System health"), this);
  healthTitle->setProperty("dashboardSection", true);
  root->addWidget(healthTitle);

  auto *healthRow = new QHBoxLayout();
  healthRow->setSpacing(12);
  healthRow->addWidget(createStatusCard(QStringLiteral("Browser AI"), &providerValueLabel_,
                                        &providerDetailLabel_),
                       1);
  healthRow->addWidget(createStatusCard(QStringLiteral("Conversation Relay"), &conversationValueLabel_,
                                        &conversationDetailLabel_),
                       1);
  healthRow->addWidget(createStatusCard(QStringLiteral("MCP Server"), &mcpValueLabel_, &mcpDetailLabel_), 1);
  root->addLayout(healthRow);

  auto *metricRow = new QHBoxLayout();
  metricRow->setSpacing(12);
  metricRow->addWidget(createMetricCard(QStringLiteral("Apps"), &appsValueLabel_,
                                        QStringLiteral("Distinct app groups in the routed catalog.")),
                       1);
  metricRow->addWidget(createMetricCard(QStringLiteral("Actions"), &actionsValueLabel_,
                                        QStringLiteral("MCP tools currently available to this workspace.")),
                       1);
  metricRow->addWidget(createMetricCard(QStringLiteral("Runs"), &runsValueLabel_,
                                        QStringLiteral("Tool executions recorded in this session.")),
                       1);
  metricRow->addWidget(createMetricCard(QStringLiteral("Conversation events"), &activityValueLabel_,
                                        QStringLiteral("Browser message events observed this session.")),
                       1);
  root->addLayout(metricRow);

  auto *activityHeader = new QHBoxLayout();
  auto *activityTitle = new QLabel(QStringLiteral("Recent runs"), this);
  activityTitle->setProperty("dashboardSection", true);
  activityHeader->addWidget(activityTitle);
  activityHeader->addStretch(1);

  auto *conversationButton = new QPushButton(QStringLiteral("Open Conversation"), this);
  connect(conversationButton, &QPushButton::clicked, this, &WorkspaceDashboard::openConversationRequested);
  activityHeader->addWidget(conversationButton);

  auto *gettingStartedButton = new QPushButton(QStringLiteral("Getting Started"), this);
  connect(gettingStartedButton, &QPushButton::clicked, this, &WorkspaceDashboard::gettingStartedRequested);
  activityHeader->addWidget(gettingStartedButton);
  root->addLayout(activityHeader);

  recentRunsList_ = new QListWidget(this);
  recentRunsList_->setMinimumHeight(150);
  recentRunsList_->setMaximumHeight(230);
  recentRunsList_->setFocusPolicy(Qt::NoFocus);
  root->addWidget(recentRunsList_, 1);

  auto *privacy = new QLabel(
      QStringLiteral("Home is session-oriented. Web ChatGPT prompts use the local browser relay; Action/workflow queries and drafted parameters are not persisted by the dashboard."),
      this);
  privacy->setProperty("dashboardMuted", true);
  privacy->setWordWrap(true);
  root->addWidget(privacy);
}

void WorkspaceDashboard::applyStyle() {
  setStyleSheet(QStringLiteral(R"(
    QWidget#workspaceDashboard {
      background: #f6f6f6;
      color: #111111;
    }
    QFrame[dashboardCard="true"] {
      background: #ffffff;
      border: 1px solid #dddddd;
      border-radius: 12px;
    }
    QFrame[webChatGptCard="true"] {
      border: 1px solid #b8b8b8;
    }
    QFrame[actionRouterCard="true"] {
      border: 1px solid #c9c9c9;
    }
    QLabel[dashboardTitle="true"] {
      font-size: 28px;
      font-weight: 750;
      color: #000000;
    }
    QLabel[actionRouterTitle="true"] {
      font-size: 20px;
      font-weight: 750;
      color: #000000;
    }
    QLabel[actionRouterStatus="true"], QLabel[webChatGptStatus="true"] {
      background: #f7f7f7;
      border: 1px solid #e2e2e2;
      border-radius: 8px;
      color: #4e4e4e;
      padding: 8px 10px;
    }
    QLabel[dashboardSection="true"] {
      font-size: 15px;
      font-weight: 700;
      color: #111111;
    }
    QLabel[dashboardEyebrow="true"] {
      font-size: 11px;
      font-weight: 700;
      color: #6f6f6f;
      text-transform: uppercase;
    }
    QLabel[dashboardStatus="true"] {
      font-size: 20px;
      font-weight: 750;
      color: #111111;
    }
    QLabel[dashboardMetric="true"] {
      font-size: 30px;
      font-weight: 800;
      color: #000000;
    }
    QLabel[dashboardMuted="true"] {
      color: #737373;
    }
    QLineEdit[actionRouterInput="true"] {
      background: #ffffff;
      color: #111111;
      border: 1px solid #bfbfbf;
      border-radius: 9px;
      padding: 10px 12px;
      font-size: 14px;
      selection-background-color: #111111;
      selection-color: #ffffff;
    }
    QLineEdit[actionRouterInput="true"]:focus { border: 1px solid #111111; }
    QPushButton {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d4d4d4;
      border-radius: 8px;
      padding: 8px 12px;
      font-weight: 650;
    }
    QPushButton:hover { background: #f0f0f0; }
    QPushButton:disabled {
      background: #eeeeee;
      color: #999999;
      border-color: #dddddd;
    }
    QPushButton[dashboardPrimary="true"] {
      background: #000000;
      color: #ffffff;
      border-color: #000000;
      padding: 10px 14px;
    }
    QPushButton[dashboardPrimary="true"]:hover { background: #202020; }
    QPushButton[dashboardPrimary="true"]:disabled {
      background: #9a9a9a;
      color: #ffffff;
      border-color: #9a9a9a;
    }
    QListWidget {
      background: #ffffff;
      color: #111111;
      border: 1px solid #dddddd;
      border-radius: 12px;
      padding: 8px;
      outline: none;
    }
    QListWidget::item {
      border-bottom: 1px solid #eeeeee;
      padding: 10px 8px;
    }
    QListWidget::item:last { border-bottom: none; }
  )"));
}

void WorkspaceDashboard::setConversationRelayStatus(const QString &text, bool online) {
  conversationRelayText_ = text;
  conversationRelayOnline_ = online;
  if (webChatGptInput_) webChatGptInput_->setEnabled(online);
  if (webChatGptButton_) webChatGptButton_->setEnabled(online);
  if (online && webChatGptStatusLabel_) {
    setWebChatGptStatus(
        activeProvider_ == QStringLiteral("chatgpt")
            ? QStringLiteral("Ready. Questions will be sent to the active web ChatGPT conversation.")
            : QStringLiteral("Relay ready. Open ChatGPT in your browser and keep that tab active before asking."),
        activeProvider_ == QStringLiteral("chatgpt"));
  }
  refreshFromWorkspace();
}

void WorkspaceDashboard::setActiveProvider(const QString &provider) {
  activeProvider_ = provider.trimmed().toLower();
  if (webChatGptStatusLabel_ && activeProvider_ == QStringLiteral("chatgpt")) {
    setWebChatGptStatus(QStringLiteral("ChatGPT detected. Ready to ask the web conversation."), true);
  }
  refreshFromWorkspace();
}

void WorkspaceDashboard::setActionRouterStatus(const QString &text, bool busy) {
  if (actionRouterStatusLabel_) actionRouterStatusLabel_->setText(text);
  if (actionRouteButton_) actionRouteButton_->setEnabled(!busy);
  if (workflowPlanButton_) workflowPlanButton_->setEnabled(!busy);
  if (actionQueryEdit_) actionQueryEdit_->setEnabled(!busy);
}

void WorkspaceDashboard::setWebChatGptStatus(const QString &text, bool success) {
  if (!webChatGptStatusLabel_) return;
  webChatGptStatusLabel_->setText(text);
  webChatGptStatusLabel_->setStyleSheet(
      success ? QStringLiteral("background:#f0fdf4;border:1px solid #bbf7d0;border-radius:8px;padding:8px 10px;color:#166534;font-weight:600;")
              : QStringLiteral("background:#f7f7f7;border:1px solid #e2e2e2;border-radius:8px;padding:8px 10px;color:#4e4e4e;"));
}

void WorkspaceDashboard::noteConversationActivity() {
  ++conversationEventCount_;
  refreshFromWorkspace();
}

void WorkspaceDashboard::refreshFromWorkspace() {
  if (webChatGptInput_) webChatGptInput_->setEnabled(conversationRelayOnline_);
  if (webChatGptButton_) webChatGptButton_->setEnabled(conversationRelayOnline_);

  if (activeProvider_.isEmpty()) {
    providerValueLabel_->setText(QStringLiteral("Waiting"));
    providerDetailLabel_->setText(
        QStringLiteral("Open ChatGPT, Gemini, Grok, or Perplexity and keep the target browser tab active."));
  } else {
    providerValueLabel_->setText(providerDisplayName(activeProvider_));
    providerDetailLabel_->setText(
        QStringLiteral("Active supported browser provider detected through the local Quick Ask relay."));
  }

  conversationValueLabel_->setText(conversationRelayOnline_ ? QStringLiteral("Ready")
                                                            : QStringLiteral("Needs attention"));
  conversationDetailLabel_->setText(conversationRelayText_);
  activityValueLabel_->setText(QString::number(conversationEventCount_));

  if (!workspace_) return;

  mcpValueLabel_->setText(workspace_->isMcpConnected() ? QStringLiteral("Connected")
                                                       : QStringLiteral("Disconnected"));
  mcpDetailLabel_->setText(workspace_->mcpStatusText());
  appsValueLabel_->setText(QString::number(workspace_->discoveredAppCount()));
  actionsValueLabel_->setText(QString::number(workspace_->discoveredActionCount()));
  runsValueLabel_->setText(QString::number(workspace_->runCount()));

  const QStringList summaries = workspace_->recentRunSummaries(6);
  recentRunsList_->clear();
  if (summaries.isEmpty()) {
    recentRunsList_->addItem(QStringLiteral("No tool runs yet. Connect a server and route or browse an Action to start."));
    return;
  }
  for (const QString &summary : summaries) recentRunsList_->addItem(summary);
}
