#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QPlainTextEdit;

class ConversationWindow final : public QWidget {
  Q_OBJECT

 public:
  explicit ConversationWindow(QWidget *parent = nullptr);

 public slots:
  void ingestEvent(const QJsonObject &event);
  void setRelayStatus(const QString &text, bool online);
  void clearConversation();

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
  QPlainTextEdit *conversationView_ = nullptr;
  QList<ConversationRecord> records_;
};
