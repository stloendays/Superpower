#pragma once

#include <QDateTime>
#include <QDialog>
#include <QJsonObject>
#include <QList>
#include <QString>

class QLabel;
class QPlainTextEdit;

class ConversationWindow final : public QDialog {
  Q_OBJECT

 public:
  explicit ConversationWindow(QWidget *parent = nullptr);

 public slots:
  void ingestEvent(const QJsonObject &event);
  void showConversation();

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
  void clearConversation();

  QLabel *statusLabel_ = nullptr;
  QPlainTextEdit *conversationView_ = nullptr;
  QList<ConversationRecord> records_;
  bool autoShown_ = false;
};