#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class McpBridgeProcess final : public QObject {
  Q_OBJECT

 public:
  explicit McpBridgeProcess(QObject *parent = nullptr);
  ~McpBridgeProcess() override;

  [[nodiscard]] bool isRunning() const;
  void startHttp(const QString &nodeProgram, const QString &hostScript, const QString &serverUrl,
                 const QString &taskFocus, const QString &policyMode);
  void startStdio(const QString &nodeProgram, const QString &hostScript, const QString &serverCommand,
                  const QStringList &serverArgs, const QString &taskFocus, const QString &policyMode);
  void startConversation(const QString &nodeProgram, const QString &hostScript);
  QString sendRequest(const QString &method, const QJsonObject &params = {});
  void stop();

 signals:
  void bridgeReady(const QString &transport);
  void conversationEvent(const QJsonObject &event);
  void responseReceived(const QString &id, const QString &method, const QJsonValue &result);
  void requestFailed(const QString &id, const QString &method, const QString &code,
                     const QString &message, const QJsonObject &details);
  void processExited(int exitCode, QProcess::ExitStatus status);
  void processError(const QString &message);
  void logLine(const QString &line);

 private slots:
  void handleStdout();
  void handleStderr();

 private:
  void startWithConnectionArguments(const QString &nodeProgram, const QString &hostScript,
                                    const QStringList &connectionArguments, const QString &taskFocus,
                                    const QString &policyMode);
  void prepareProcess(const QString &nodeProgram);
  void handleProtocolLine(const QByteArray &line);
  void scheduleForcedStop();

  QProcess process_;
  QByteArray stdoutBuffer_;
  quint64 nextRequestId_ = 1;
  QHash<QString, QString> pendingMethods_;
  bool stopping_ = false;
};
