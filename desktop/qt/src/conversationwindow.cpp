#include "conversationwindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

ConversationWindow::ConversationWindow(QWidget *parent) : QWidget(parent) {
  setMinimumWidth(360);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(10);

  auto *header = new QHBoxLayout();
  auto *title = new QLabel(QStringLiteral("Conversation"), this);
  title->setStyleSheet(QStringLiteral("font-size:18px;font-weight:750;"));
  header->addWidget(title);
  header->addStretch(1);
  auto *clearButton = new QPushButton(QStringLiteral("Clear session"), this);
  connect(clearButton, &QPushButton::clicked, this, &ConversationWindow::clearConversation);
  header->addWidget(clearButton);
  layout->addLayout(header);

  auto *note = new QLabel(
      QStringLiteral("Live ChatGPT browser text · local loopback only · session-only memory · no MCP server connection required."),
      this);
  note->setWordWrap(true);
  note->setStyleSheet(QStringLiteral("color:#737373;"));
  layout->addWidget(note);

  statusLabel_ = new QLabel(QStringLiteral("Starting local conversation relay..."), this);
  statusLabel_->setWordWrap(true);
  layout->addWidget(statusLabel_);
  setRelayStatus(QStringLiteral("Starting local conversation relay..."), false);

  conversationView_ = new QPlainTextEdit(this);
  conversationView_->setReadOnly(true);
  conversationView_->setPlaceholderText(QStringLiteral("ChatGPT messages will appear here as the webpage updates."));
  conversationView_->setStyleSheet(
      QStringLiteral("QPlainTextEdit{background:#ffffff;border:1px solid #dddddd;border-radius:10px;padding:12px;font-family:'Segoe UI',sans-serif;font-size:13px;}"));
  layout->addWidget(conversationView_, 1);

  auto *askLabel = new QLabel(QStringLiteral("Quick Ask"), this);
  askLabel->setStyleSheet(QStringLiteral("font-size:12px;font-weight:700;color:#333333;"));
  layout->addWidget(askLabel);

  auto *promptBar = new QHBoxLayout();
  promptBar->setSpacing(8);
  promptInput_ = new QLineEdit(this);
  promptInput_->setPlaceholderText(QStringLiteral("Ask in the active ChatGPT conversation..."));
  promptInput_->setClearButtonEnabled(true);
  promptInput_->setEnabled(false);
  promptInput_->setStyleSheet(
      QStringLiteral("QLineEdit{min-height:34px;background:#ffffff;border:1px solid #cfcfcf;border-radius:9px;padding:0 10px;font-size:13px;}"
                     "QLineEdit:focus{border:1px solid #111111;}"
                     "QLineEdit:disabled{background:#f6f6f6;color:#888888;}"));
  promptBar->addWidget(promptInput_, 1);

  sendButton_ = new QPushButton(QStringLiteral("Send"), this);
  sendButton_->setEnabled(false);
  sendButton_->setMinimumHeight(34);
  sendButton_->setStyleSheet(
      QStringLiteral("QPushButton{background:#111111;color:#ffffff;border:1px solid #111111;border-radius:9px;padding:0 14px;font-weight:650;}"
                     "QPushButton:hover{background:#292929;}"
                     "QPushButton:disabled{background:#d7d7d7;border-color:#d7d7d7;color:#777777;}"));
  promptBar->addWidget(sendButton_);
  layout->addLayout(promptBar);

  promptStatusLabel_ = new QLabel(
      QStringLiteral("Open ChatGPT in the browser with the Superpower extension enabled. Enter sends from Desktop to that active conversation."),
      this);
  promptStatusLabel_->setWordWrap(true);
  promptStatusLabel_->setStyleSheet(QStringLiteral("color:#737373;font-size:11px;"));
  layout->addWidget(promptStatusLabel_);

  connect(promptInput_, &QLineEdit::returnPressed, this, &ConversationWindow::submitPrompt);
  connect(sendButton_, &QPushButton::clicked, this, &ConversationWindow::submitPrompt);
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

  setRelayStatus(role == QStringLiteral("assistant") && phase == QStringLiteral("streaming")
                     ? QStringLiteral("Live · ChatGPT is responding")
                     : QStringLiteral("Live · browser conversation synchronized locally"),
                 true);
  renderConversation();
}

void ConversationWindow::setRelayStatus(const QString &text, bool online) {
  statusLabel_->setText(text);
  statusLabel_->setStyleSheet(
      online
          ? QStringLiteral("background:#111111;border:1px solid #111111;border-radius:8px;padding:8px 10px;color:#ffffff;font-weight:650;")
          : QStringLiteral("background:#f3f3f3;border:1px solid #dfdfdf;border-radius:8px;padding:8px 10px;color:#444444;font-weight:650;"));
  if (promptInput_) promptInput_->setEnabled(online);
  if (sendButton_) sendButton_->setEnabled(online);
}

void ConversationWindow::setPromptStatus(const QString &text, bool success) {
  promptStatusLabel_->setText(text);
  promptStatusLabel_->setStyleSheet(
      success ? QStringLiteral("color:#166534;font-size:11px;font-weight:600;")
              : QStringLiteral("color:#737373;font-size:11px;"));
}

void ConversationWindow::submitPrompt() {
  if (!promptInput_ || !promptInput_->isEnabled()) return;
  const QString text = promptInput_->text().trimmed();
  if (text.isEmpty()) return;

  promptInput_->clear();
  setPromptStatus(QStringLiteral("Sending to the active ChatGPT tab through the local conversation relay..."), false);
  emit promptSubmitted(text);
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
  setRelayStatus(QStringLiteral("Listening locally · conversation cleared for this desktop session."), true);
}
