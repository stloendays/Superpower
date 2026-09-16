#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class ConversationWindow final : public QWidget {
  Q_OBJECT

 public:
  explicit ConversationWindow(QWidget *parent = nullptr);

 public slots:
  void ingestEvent(const QJsonObject &event);
  void setRelayStatus(const QString &text, bool online);
  void setPromptStatus(const QString &text, bool success);
  void clearConversation();

 signals:
  void promptSubmitted(const QString &text);

 private slots:
  void submitPrompt();

 private:
  struct ConversationRecord {
    QString eventId;
    QString sessionId;
    QString role;
    QString text;
    QString phase;
    QDateTime receivedAt;
  };

  void renderConversation();

  QLabel *statusLabel_ = nullptr;
  QLabel *promptStatusLabel_ = nullptr;
  QPlainTextEdit *conversationView_ = nullptr;
  QLineEdit *promptInput_ = nullptr;
  QPushButton *sendButton_ = nullptr;
  QList<ConversationRecord> records_;
};
