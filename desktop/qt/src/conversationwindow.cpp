#include "conversationwindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace {
QString providerDisplayName(const QString &provider) {
  if (provider == QStringLiteral("chatgpt")) return QStringLiteral("ChatGPT");
  if (provider == QStringLiteral("gemini")) return QStringLiteral("Gemini");
  if (provider == QStringLiteral("grok")) return QStringLiteral("Grok");
  if (provider == QStringLiteral("perplexity")) return QStringLiteral("Perplexity");
  if (provider == QStringLiteral("auto")) return QStringLiteral("Auto");
  return provider.isEmpty() ? QStringLiteral("Unknown") : provider;
}
}  // namespace

ConversationWindow::ConversationWindow(QWidget *parent) : QWidget(parent) {
  setMinimumWidth(380);

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
      QStringLiteral("Desktop Quick Ask supports ChatGPT, Gemini, Grok, and Perplexity through the local browser relay. Live transcript mirroring is currently available for ChatGPT."),
      this);
  note->setWordWrap(true);
  note->setStyleSheet(QStringLiteral("color:#737373;"));
  layout->addWidget(note);

  statusLabel_ = new QLabel(QStringLiteral("Starting local conversation relay..."), this);
  statusLabel_->setWordWrap(true);
  layout->addWidget(statusLabel_);
  setRelayStatus(QStringLiteral("Starting local conversation relay..."), false);

  auto *providerBar = new QHBoxLayout();
  providerBar->setSpacing(8);
  auto *providerLabel = new QLabel(QStringLiteral("AI Provider"), this);
  providerLabel->setStyleSheet(QStringLiteral("font-size:12px;font-weight:700;color:#333333;"));
  providerBar->addWidget(providerLabel);

  providerCombo_ = new QComboBox(this);
  providerCombo_->addItem(QStringLiteral("Auto · active supported tab"), QStringLiteral("auto"));
  providerCombo_->addItem(QStringLiteral("ChatGPT"), QStringLiteral("chatgpt"));
  providerCombo_->addItem(QStringLiteral("Gemini"), QStringLiteral("gemini"));
  providerCombo_->addItem(QStringLiteral("Grok"), QStringLiteral("grok"));
  providerCombo_->addItem(QStringLiteral("Perplexity"), QStringLiteral("perplexity"));
  providerCombo_->setEnabled(false);
  providerCombo_->setMinimumHeight(32);
  providerCombo_->setStyleSheet(
      QStringLiteral("QComboBox{background:#ffffff;border:1px solid #cfcfcf;border-radius:8px;padding:0 9px;min-width:190px;}"
                     "QComboBox:disabled{background:#f6f6f6;color:#888888;}"));
  providerBar->addWidget(providerCombo_, 1);
  layout->addLayout(providerBar);

  providerStatusLabel_ = new QLabel(QStringLiteral("Active browser provider: waiting for a supported tab..."), this);
  providerStatusLabel_->setWordWrap(true);
  providerStatusLabel_->setStyleSheet(QStringLiteral("color:#737373;font-size:11px;"));
  layout->addWidget(providerStatusLabel_);

  conversationView_ = new QPlainTextEdit(this);
  conversationView_->setReadOnly(true);
  conversationView_->setPlaceholderText(
      QStringLiteral("ChatGPT transcript messages will appear here as the webpage updates."));
  conversationView_->setStyleSheet(
      QStringLiteral("QPlainTextEdit{background:#ffffff;border:1px solid #dddddd;border-radius:10px;padding:12px;font-family:'Segoe UI',sans-serif;font-size:13px;}"));
  layout->addWidget(conversationView_, 1);

  auto *askLabel = new QLabel(QStringLiteral("Quick Ask"), this);
  askLabel->setStyleSheet(QStringLiteral("font-size:12px;font-weight:700;color:#333333;"));
  layout->addWidget(askLabel);

  auto *promptBar = new QHBoxLayout();
  promptBar->setSpacing(8);
  promptInput_ = new QLineEdit(this);
  promptInput_->setPlaceholderText(QStringLiteral("Ask in the selected browser AI conversation..."));
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
      QStringLiteral("Open a supported AI site with the Superpower extension enabled. Auto routes only to the active supported browser tab."),
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
  const QString source = event.value(QStringLiteral("source")).toString(QStringLiteral("chatgpt"));
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
    record.source = source;
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

  setProviderStatus(source, true);
  const QString providerName = providerDisplayName(source);
  setRelayStatus(role == QStringLiteral("assistant") && phase == QStringLiteral("streaming")
                     ? QStringLiteral("Live · %1 is responding").arg(providerName)
                     : QStringLiteral("Live · %1 browser conversation synchronized locally").arg(providerName),
                 true);
  renderConversation();
}

void ConversationWindow::setRelayStatus(const QString &text, bool online) {
  statusLabel_->setText(text);
  statusLabel_->setStyleSheet(
      online
          ? QStringLiteral("background:#111111;border:1px solid #111111;border-radius:8px;padding:8px 10px;color:#ffffff;font-weight:650;")
          : QStringLiteral("background:#f3f3f3;border:1px solid #dfdfdf;border-radius:8px;padding:8px 10px;color:#444444;font-weight:650;"));
  if (providerCombo_) providerCombo_->setEnabled(online);
  if (promptInput_) promptInput_->setEnabled(online);
  if (sendButton_) sendButton_->setEnabled(online);
}

void ConversationWindow::setPromptStatus(const QString &text, bool success) {
  promptStatusLabel_->setText(text);
  promptStatusLabel_->setStyleSheet(
      success ? QStringLiteral("color:#166534;font-size:11px;font-weight:600;")
              : QStringLiteral("color:#737373;font-size:11px;"));
}

void ConversationWindow::setProviderStatus(const QString &provider, bool online) {
  if (!providerStatusLabel_) return;
  if (!online || provider.trimmed().isEmpty()) {
    providerStatusLabel_->setText(QStringLiteral("Active browser provider: no supported active tab detected yet."));
    providerStatusLabel_->setStyleSheet(QStringLiteral("color:#737373;font-size:11px;"));
    return;
  }

  providerStatusLabel_->setText(
      QStringLiteral("Active browser provider: %1 · local relay detected").arg(providerDisplayName(provider)));
  providerStatusLabel_->setStyleSheet(QStringLiteral("color:#166534;font-size:11px;font-weight:600;"));
}

void ConversationWindow::submitPrompt() {
  if (!promptInput_ || !promptInput_->isEnabled()) return;
  const QString text = promptInput_->text().trimmed();
  if (text.isEmpty()) return;

  const QString provider = providerCombo_ ? providerCombo_->currentData().toString() : QStringLiteral("auto");
  promptInput_->clear();
  setPromptStatus(
      provider == QStringLiteral("auto")
          ? QStringLiteral("Sending to the active supported AI tab through the local conversation relay...")
          : QStringLiteral("Sending to the active %1 tab through the local conversation relay...")
                .arg(providerDisplayName(provider)),
      false);
  emit promptSubmitted(text, provider);
}

void ConversationWindow::renderConversation() {
  const bool followBottom = conversationView_->verticalScrollBar()->value() >=
                            conversationView_->verticalScrollBar()->maximum() - 24;

  QStringList lines;
  QString previousSession;
  QString previousSource;
  for (const ConversationRecord &record : records_) {
    if (record.sessionId != previousSession || record.source != previousSource) {
      if (!lines.isEmpty()) lines << QString();
      lines << QStringLiteral("──────── %1 browser conversation ────────")
                   .arg(providerDisplayName(record.source))
            << QString();
      previousSession = record.sessionId;
      previousSource = record.source;
    }

    const QString speaker = record.role == QStringLiteral("user") ? QStringLiteral("You")
                                                                    : providerDisplayName(record.source);
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
