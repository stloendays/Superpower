#include "conversationwindow.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

ConversationWindow::ConversationWindow(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Superpower Conversation"));
  resize(760, 680);
  setMinimumSize(560, 420);
  setModal(false);
  setAttribute(Qt::WA_QuitOnClose, false);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(10);

  auto *title = new QLabel(QStringLiteral("Conversation"), this);
  title->setStyleSheet(QStringLiteral("font-size:18px;font-weight:750;"));
  layout->addWidget(title);

  auto *note = new QLabel(
      QStringLiteral("Live ChatGPT browser text · local loopback only · session-only memory. Ctrl+Shift+C reopens this window."),
      this);
  note->setWordWrap(true);
  note->setStyleSheet(QStringLiteral("color:#737373;"));
  layout->addWidget(note);

  statusLabel_ = new QLabel(QStringLiteral("Waiting for ChatGPT browser activity. Connect Superpower Desktop to activate browser sync."), this);
  statusLabel_->setWordWrap(true);
  statusLabel_->setStyleSheet(
      QStringLiteral("background:#f3f3f3;border:1px solid #dfdfdf;border-radius:8px;padding:8px 10px;color:#444444;"));
  layout->addWidget(statusLabel_);

  conversationView_ = new QPlainTextEdit(this);
  conversationView_->setReadOnly(true);
  conversationView_->setPlaceholderText(QStringLiteral("ChatGPT messages will appear here as the webpage updates."));
  conversationView_->setStyleSheet(
      QStringLiteral("QPlainTextEdit{background:#ffffff;border:1px solid #dddddd;border-radius:10px;padding:12px;font-family:'Segoe UI',sans-serif;font-size:13px;}"));
  layout->addWidget(conversationView_, 1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  auto *clearButton = buttons->addButton(QStringLiteral("Clear session"), QDialogButtonBox::ResetRole);
  connect(clearButton, &QPushButton::clicked, this, &ConversationWindow::clearConversation);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
  layout->addWidget(buttons);
}

void ConversationWindow::ingestEvent(const QJsonObject &event) {
  const QString eventId = event.value(QStringLiteral("eventId")).toString().trimmed();
  const QString sessionId = event.value(QStringLiteral("sessionId")).toString().trimmed();
  const QString role = event.value(QStringLiteral("role")).toString();
  const QString text = event.value(QStringLiteral("text")).toString();
  const QString phase = event.value(QStringLiteral("phase")).toString();

  if (eventId.isEmpty() || sessionId.isEmpty() || text.trimmed().isEmpty()) return;
  if (role != QStringLiteral("user") && role != QStringLiteral("assistant")) return;
  if (phase != QStringLiteral("streaming") && phase != QStringLiteral("completed")) return;

  int existingIndex = -1;
  for (int i = 0; i < records_.size(); ++i) {
    if (records_.at(i).eventId == eventId) {
      existingIndex = i;
      break;
    }
  }

  if (existingIndex >= 0) {
    ConversationRecord &record = records_[existingIndex];
    record.text = text;
    record.phase = phase;
  } else {
    ConversationRecord record;
    record.eventId = eventId;
    record.sessionId = sessionId;
    record.role = role;
    record.text = text;
    record.phase = phase;
    const double timestamp = event.value(QStringLiteral("timestamp")).toDouble();
    record.receivedAt = timestamp > 0
                            ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestamp)).toLocalTime()
                            : QDateTime::currentDateTime();
    records_.append(record);
    while (records_.size() > 200) records_.removeFirst();
  }

  statusLabel_->setText(role == QStringLiteral("assistant") && phase == QStringLiteral("streaming")
                            ? QStringLiteral("Live · ChatGPT is responding")
                            : QStringLiteral("Live · browser conversation synchronized locally"));
  renderConversation();

  if (!autoShown_) {
    autoShown_ = true;
    showConversation();
  }
}

void ConversationWindow::showConversation() {
  show();
  raise();
}

void ConversationWindow::renderConversation() {
  const bool followBottom = conversationView_->verticalScrollBar()->value() >=
                            conversationView_->verticalScrollBar()->maximum() - 24;

  QStringList lines;
  QString previousSession;
  for (const ConversationRecord &record : records_) {
    if (record.sessionId != previousSession) {
      if (!lines.isEmpty()) lines << QString();
      lines << QStringLiteral("──────── Browser conversation ────────") << QString();
      previousSession = record.sessionId;
    }

    const QString speaker = record.role == QStringLiteral("user") ? QStringLiteral("You") : QStringLiteral("ChatGPT");
    const QString streaming = record.role == QStringLiteral("assistant") && record.phase == QStringLiteral("streaming")
                                  ? QStringLiteral(" · streaming")
                                  : QString();
    lines << QStringLiteral("[%1] %2%3")
                 .arg(record.receivedAt.toString(QStringLiteral("HH:mm:ss")), speaker, streaming)
          << record.text << QString();
  }

  conversationView_->setPlainText(lines.join(QLatin1Char('\n')));
  if (followBottom || records_.size() <= 2) {
    conversationView_->verticalScrollBar()->setValue(conversationView_->verticalScrollBar()->maximum());
  }
}

void ConversationWindow::clearConversation() {
  records_.clear();
  conversationView_->clear();
  statusLabel_->setText(QStringLiteral("Conversation cleared for this desktop session. New browser updates will appear here."));
}