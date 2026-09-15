#include "onboardingdialog.h"

#include "mainwindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

namespace {
QFrame *stepCard(const QString &number, const QString &title, const QString &body, QWidget *parent,
                 QLabel **statusLabel, QPushButton **actionButton) {
  auto *frame = new QFrame(parent);
  frame->setProperty("stepCard", true);

  auto *layout = new QVBoxLayout(frame);
  layout->setContentsMargins(16, 14, 16, 14);
  layout->setSpacing(8);

  auto *top = new QHBoxLayout();
  auto *badge = new QLabel(number, frame);
  badge->setProperty("stepBadge", true);
  badge->setAlignment(Qt::AlignCenter);
  badge->setFixedSize(28, 28);
  top->addWidget(badge);

  auto *heading = new QLabel(title, frame);
  heading->setProperty("stepTitle", true);
  top->addWidget(heading, 1);

  *statusLabel = new QLabel(QStringLiteral("Not started"), frame);
  (*statusLabel)->setProperty("stepStatus", true);
  top->addWidget(*statusLabel);
  layout->addLayout(top);

  auto *description = new QLabel(body, frame);
  description->setWordWrap(true);
  description->setProperty("muted", true);
  layout->addWidget(description);

  auto *actions = new QHBoxLayout();
  actions->addStretch(1);
  *actionButton = new QPushButton(frame);
  actions->addWidget(*actionButton);
  layout->addLayout(actions);
  return frame;
}
}  // namespace

OnboardingDialog::OnboardingDialog(MainWindow *workspace) : QDialog(workspace), workspace_(workspace) {
  setWindowTitle(QStringLiteral("Getting Started - Superpower"));
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, false);
  resize(720, 620);
  setMinimumSize(640, 560);

  buildUi();
  applyStyle();

  refreshTimer_ = new QTimer(this);
  refreshTimer_->setInterval(500);
  connect(refreshTimer_, &QTimer::timeout, this, &OnboardingDialog::refreshState);

  connect(serverActionButton_, &QPushButton::clicked, this, &OnboardingDialog::focusServerSetup);
  connect(connectionActionButton_, &QPushButton::clicked, this, &OnboardingDialog::connectCurrentServer);
  connect(toolsActionButton_, &QPushButton::clicked, this, &OnboardingDialog::discoverActions);
  connect(finishButton_, &QPushButton::clicked, this, [this]() {
    markSeen(true);
    close();
  });

  refreshState();
}

void OnboardingDialog::showForCurrentState() {
  refreshState();
  show();
  raise();
  activateWindow();
  refreshTimer_->start();
}

void OnboardingDialog::buildUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 22, 24, 22);
  layout->setSpacing(14);

  auto *eyebrow = new QLabel(QStringLiteral("SUPERPOWER V1.5"), this);
  eyebrow->setProperty("eyebrow", true);
  layout->addWidget(eyebrow);

  auto *title = new QLabel(QStringLiteral("From install to your first MCP action"), this);
  title->setProperty("heroTitle", true);
  layout->addWidget(title);

  auto *subtitle = new QLabel(
      QStringLiteral("Conversation sync already runs independently. These three steps connect one MCP server, verify the connection, and load the first routed Apps / Actions into the workspace."),
      this);
  subtitle->setWordWrap(true);
  subtitle->setProperty("muted", true);
  layout->addWidget(subtitle);

  progressLabel_ = new QLabel(this);
  progressLabel_->setProperty("progress", true);
  layout->addWidget(progressLabel_);

  layout->addWidget(stepCard(
      QStringLiteral("1"), QStringLiteral("Add a server profile"),
      QStringLiteral("Create a named Streamable HTTP or local stdio connection. Superpower stores only non-secret profile metadata; stdio arguments and sensitive URL data remain session-only."),
      this, &serverStatusLabel_, &serverActionButton_));

  layout->addWidget(stepCard(
      QStringLiteral("2"), QStringLiteral("Test the connection"),
      QStringLiteral("Use the existing guarded MCP host bridge to connect. This does not create a second connection path or bypass the current routing and policy layer."),
      this, &connectionStatusLabel_, &connectionActionButton_));

  layout->addWidget(stepCard(
      QStringLiteral("3"), QStringLiteral("Discover your first action"),
      QStringLiteral("Load the routed tool catalog, then choose an App / Action in the main workspace. Destructive actions still require the existing guarded confirmation flow."),
      this, &toolsStatusLabel_, &toolsActionButton_));

  auto *privacy = new QLabel(
      QStringLiteral("Privacy boundary: onboarding state is local UI metadata only. It does not persist conversation text, tool arguments, credentials, or execution results."),
      this);
  privacy->setWordWrap(true);
  privacy->setProperty("privacy", true);
  layout->addWidget(privacy);

  auto *bottom = new QHBoxLayout();
  auto *openWorkspaceButton = new QPushButton(QStringLiteral("Open workspace"), this);
  auto *skipButton = new QPushButton(QStringLiteral("Skip for now"), this);
  finishButton_ = new QPushButton(QStringLiteral("Finish setup"), this);
  finishButton_->setProperty("primary", true);
  bottom->addWidget(openWorkspaceButton);
  bottom->addStretch(1);
  bottom->addWidget(skipButton);
  bottom->addWidget(finishButton_);
  layout->addLayout(bottom);

  connect(openWorkspaceButton, &QPushButton::clicked, this, &OnboardingDialog::bringWorkspaceForward);
  connect(skipButton, &QPushButton::clicked, this, [this]() {
    markSeen(false);
    close();
  });
}

void OnboardingDialog::applyStyle() {
  setStyleSheet(QStringLiteral(R"(
    QDialog {
      background: #f6f6f6;
      color: #111111;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      font-size: 13px;
    }
    QLabel[eyebrow="true"] {
      color: #666666;
      font-size: 11px;
      font-weight: 750;
      letter-spacing: 1.2px;
    }
    QLabel[heroTitle="true"] {
      color: #000000;
      font-size: 24px;
      font-weight: 780;
    }
    QLabel[muted="true"] { color: #666666; }
    QLabel[progress="true"] {
      background: #111111;
      color: #ffffff;
      border-radius: 9px;
      padding: 9px 12px;
      font-weight: 700;
    }
    QFrame[stepCard="true"] {
      background: #ffffff;
      border: 1px solid #dddddd;
      border-radius: 12px;
    }
    QLabel[stepBadge="true"] {
      background: #111111;
      color: #ffffff;
      border-radius: 14px;
      font-weight: 800;
    }
    QLabel[stepTitle="true"] {
      color: #111111;
      font-size: 15px;
      font-weight: 750;
    }
    QLabel[stepStatus="true"] {
      background: #f2f2f2;
      border: 1px solid #dddddd;
      border-radius: 8px;
      color: #444444;
      padding: 5px 8px;
      font-weight: 650;
    }
    QLabel[privacy="true"] {
      background: #ededed;
      border-radius: 9px;
      color: #555555;
      padding: 10px 12px;
    }
    QPushButton {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d0d0d0;
      border-radius: 8px;
      padding: 8px 12px;
      font-weight: 650;
    }
    QPushButton:hover { background: #eeeeee; }
    QPushButton:disabled { color: #999999; background: #f3f3f3; border-color: #e2e2e2; }
    QPushButton[primary="true"] {
      background: #000000;
      color: #ffffff;
      border-color: #000000;
    }
    QPushButton[primary="true"]:disabled {
      background: #aaaaaa;
      border-color: #aaaaaa;
      color: #ffffff;
    }
  )"));
}

void OnboardingDialog::refreshState() {
  const int servers = savedServerCount();
  const bool connected = isWorkspaceConnected();
  const int tools = discoveredToolCount();

  int completed = 0;
  if (servers > 0) ++completed;
  if (connected) ++completed;
  if (tools > 0) ++completed;

  progressLabel_->setText(QStringLiteral("Setup progress  %1 / 3").arg(completed));

  serverStatusLabel_->setText(servers > 0 ? QStringLiteral("Ready · %1 saved").arg(servers)
                                          : QStringLiteral("Not started"));
  serverActionButton_->setText(servers > 0 ? QStringLiteral("Add another server")
                                            : QStringLiteral("Add server profile"));
  serverActionButton_->setEnabled(!connected);

  QPushButton *connectButton = findWorkspaceButton(QStringLiteral("Connect"));
  const bool connecting = connectButton && !connectButton->isEnabled() && !connected;
  connectionStatusLabel_->setText(connected ? QStringLiteral("Connected")
                                            : connecting ? QStringLiteral("Connecting...")
                                                         : QStringLiteral("Not connected"));
  connectionActionButton_->setText(connected ? QStringLiteral("Connected") : QStringLiteral("Test connection"));
  connectionActionButton_->setEnabled(!connected && !connecting && servers > 0);

  toolsStatusLabel_->setText(tools > 0 ? QStringLiteral("Ready · %1 actions").arg(tools)
                                       : QStringLiteral("No actions loaded"));
  toolsActionButton_->setText(tools > 0 ? QStringLiteral("Refresh actions")
                                        : QStringLiteral("Discover actions"));
  toolsActionButton_->setEnabled(connected);

  finishButton_->setEnabled(tools > 0);
}

void OnboardingDialog::focusServerSetup() {
  if (!workspace_ || isWorkspaceConnected()) return;
  if (QPushButton *button = findWorkspaceButton(QStringLiteral("New"))) button->click();
  bringWorkspaceForward();

  const auto fields = workspace_->findChildren<QLineEdit *>();
  for (QLineEdit *field : fields) {
    if (field->placeholderText().contains(QStringLiteral("GitHub MCP"), Qt::CaseInsensitive)) {
      field->setFocus();
      break;
    }
  }
}

void OnboardingDialog::connectCurrentServer() {
  if (!workspace_ || isWorkspaceConnected()) return;
  if (QPushButton *button = findWorkspaceButton(QStringLiteral("Connect")); button && button->isEnabled()) {
    button->click();
  }
  bringWorkspaceForward();
}

void OnboardingDialog::discoverActions() {
  if (!workspace_ || !isWorkspaceConnected()) return;
  if (QPushButton *button = findWorkspaceButton(QStringLiteral("Refresh tools")); button && button->isEnabled()) {
    button->click();
  }
  bringWorkspaceForward();
}

void OnboardingDialog::bringWorkspaceForward() {
  if (!workspace_) return;
  workspace_->showNormal();
  workspace_->raise();
  workspace_->activateWindow();
}

void OnboardingDialog::markSeen(bool completed) {
  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  settings.setValue(QStringLiteral("onboarding/v1_5_seen"), true);
  if (completed) {
    settings.setValue(QStringLiteral("onboarding/v1_5_completed"), true);
    settings.setValue(QStringLiteral("onboarding/v1_5_completed_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  }
  settings.sync();
}

QPushButton *OnboardingDialog::findWorkspaceButton(const QString &text) const {
  if (!workspace_) return nullptr;
  const auto buttons = workspace_->findChildren<QPushButton *>();
  for (QPushButton *button : buttons) {
    if (button->text() == text) return button;
  }
  return nullptr;
}

int OnboardingDialog::savedServerCount() const {
  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  const int count = settings.beginReadArray(QStringLiteral("servers"));
  settings.endArray();
  return count;
}

int OnboardingDialog::discoveredToolCount() const {
  if (!workspace_) return 0;
  const auto lists = workspace_->findChildren<QListWidget *>();
  for (QListWidget *list : lists) {
    int actionCount = 0;
    for (int i = 0; i < list->count(); ++i) {
      if (list->item(i)->data(Qt::UserRole + 4).isValid()) ++actionCount;
    }
    if (actionCount > 0) return actionCount;
  }
  return 0;
}

bool OnboardingDialog::isWorkspaceConnected() const {
  return findWorkspaceButton(QStringLiteral("Disconnect")) != nullptr;
}

void OnboardingDialog::closeEvent(QCloseEvent *event) {
  refreshTimer_->stop();
  markSeen(false);
  QDialog::closeEvent(event);
}
