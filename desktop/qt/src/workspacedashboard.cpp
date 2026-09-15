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
      QStringLiteral("Route natural-language requests into MCP Actions, then review every parameter before execution."),
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

  auto *routerCard = dashboardCard(this);
  routerCard->setProperty("actionRouterCard", true);
  auto *routerLayout = new QVBoxLayout(routerCard);
  routerLayout->setContentsMargins(20, 18, 20, 18);
  routerLayout->setSpacing(9);

  auto *routerEyebrow = new QLabel(QStringLiteral("ACTION ROUTER"), routerCard);
  routerEyebrow->setProperty("dashboardEyebrow", true);
  routerLayout->addWidget(routerEyebrow);

  auto *routerTitle = new QLabel(QStringLiteral("Search or run an action"), routerCard);
  routerTitle->setProperty("actionRouterTitle", true);
  routerLayout->addWidget(routerTitle);

  auto *routerHint = new QLabel(
      QStringLiteral("Describe the outcome in natural language. Superpower ranks the full MCP catalog, drafts schema-backed parameters, previews execution risk, and opens the proposal for review. It never auto-runs the result."),
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
      QStringLiteral("e.g. Find issue #29 in stloendays/Superpower-V1, or 给客户发一封邮件"));
  connect(actionQueryEdit_, &QLineEdit::returnPressed, this, &WorkspaceDashboard::submitActionQuery);
  routerInputRow->addWidget(actionQueryEdit_, 1);

  actionRouteButton_ = new QPushButton(QStringLiteral("Route action"), routerCard);
  actionRouteButton_->setProperty("dashboardPrimary", true);
  connect(actionRouteButton_, &QPushButton::clicked, this, &WorkspaceDashboard::submitActionQuery);
  routerInputRow->addWidget(actionRouteButton_);
  routerLayout->addLayout(routerInputRow);

  actionRouterStatusLabel_ = new QLabel(
      QStringLiteral("Nothing runs from Home. A routed proposal always opens in Actions for review."), routerCard);
  actionRouterStatusLabel_->setProperty("actionRouterStatus", true);
  actionRouterStatusLabel_->setWordWrap(true);
  routerLayout->addWidget(actionRouterStatusLabel_);
  root->addWidget(routerCard);

  auto *healthTitle = new QLabel(QStringLiteral("System health"), this);
  healthTitle->setProperty("dashboardSection", true);
  root->addWidget(healthTitle);

  auto *healthRow = new QHBoxLayout();
  healthRow->setSpacing(12);
  healthRow->addWidget(createStatusCard(QStringLiteral("Browser Conversation"), &conversationValueLabel_,
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
      QStringLiteral("Home is session-oriented. Action queries and drafted parameters are not persisted by the dashboard; execution still goes through the existing MCP guarded policy."),
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
    QLabel[actionRouterStatus="true"] {
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
  refreshFromWorkspace();
}

void WorkspaceDashboard::setActionRouterStatus(const QString &text, bool busy) {
  if (actionRouterStatusLabel_) actionRouterStatusLabel_->setText(text);
  if (actionRouteButton_) actionRouteButton_->setEnabled(!busy);
  if (actionQueryEdit_) actionQueryEdit_->setEnabled(!busy);
}

void WorkspaceDashboard::noteConversationActivity() {
  ++conversationEventCount_;
  refreshFromWorkspace();
}

void WorkspaceDashboard::refreshFromWorkspace() {
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
