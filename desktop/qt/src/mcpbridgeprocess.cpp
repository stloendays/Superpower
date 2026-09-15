#include "mcpbridgeprocess.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QTimer>

McpBridgeProcess::McpBridgeProcess(QObject *parent) : QObject(parent) {
  process_.setProcessChannelMode(QProcess::SeparateChannels);

  connect(&process_, &QProcess::readyReadStandardOutput, this, &McpBridgeProcess::handleStdout);
  connect(&process_, &QProcess::readyReadStandardError, this, &McpBridgeProcess::handleStderr);
  connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    Q_UNUSED(error);
    emit processError(process_.errorString());
  });
  connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int exitCode, QProcess::ExitStatus status) {
            stopping_ = false;
            pendingMethods_.clear();
            emit processExited(exitCode, status);
          });
}

McpBridgeProcess::~McpBridgeProcess() {
  if (!isRunning()) return;
  process_.kill();
  process_.waitForFinished(500);
}

bool McpBridgeProcess::isRunning() const { return process_.state() != QProcess::NotRunning; }

void McpBridgeProcess::startHttp(const QString &nodeProgram, const QString &hostScript,
                                 const QString &serverUrl, const QString &taskFocus,
                                 const QString &policyMode) {
  startWithConnectionArguments(nodeProgram, hostScript,
                               {QStringLiteral("--http"), serverUrl}, taskFocus, policyMode);
}

void McpBridgeProcess::startStdio(const QString &nodeProgram, const QString &hostScript,
                                  const QString &serverCommand, const QStringList &serverArgs,
                                  const QString &taskFocus, const QString &policyMode) {
  QStringList connectionArguments{QStringLiteral("--stdio"), serverCommand};
  for (const QString &argument : serverArgs) {
    connectionArguments << QStringLiteral("--server-arg") << argument;
  }
  startWithConnectionArguments(nodeProgram, hostScript, connectionArguments, taskFocus, policyMode);
}

void McpBridgeProcess::startWithConnectionArguments(const QString &nodeProgram, const QString &hostScript,
                                                    const QStringList &connectionArguments,
                                                    const QString &taskFocus,
                                                    const QString &policyMode) {
  if (isRunning()) {
    emit processError(QStringLiteral("The MCP desktop bridge is already running."));
    return;
  }

  stdoutBuffer_.clear();
  pendingMethods_.clear();
  stopping_ = false;

  QStringList arguments{hostScript, QStringLiteral("bridge")};
  arguments.append(connectionArguments);
  arguments << QStringLiteral("--policy") << policyMode;
  if (!taskFocus.trimmed().isEmpty()) {
    arguments << QStringLiteral("--focus") << taskFocus.trimmed();
  }

  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  const QFileInfo nodeInfo(nodeProgram);
  if (nodeInfo.isAbsolute() && nodeInfo.exists()) {
    const QString runtimeDir = nodeInfo.absolutePath();
    const QString existingPath = environment.value(QStringLiteral("PATH"));
    environment.insert(QStringLiteral("PATH"),
                       existingPath.isEmpty()
                           ? runtimeDir
                           : runtimeDir + QDir::listSeparator() + existingPath);
  }

  process_.setProcessEnvironment(environment);
  process_.setProgram(nodeProgram);
  process_.setArguments(arguments);
  process_.start();
}

QString McpBridgeProcess::sendRequest(const QString &method, const QJsonObject &params) {
  if (!isRunning()) return {};

  const QString id = QString::number(nextRequestId_++);
  pendingMethods_.insert(id, method);

  QJsonObject request{{QStringLiteral("id"), id}, {QStringLiteral("method"), method}};
  if (!params.isEmpty()) request.insert(QStringLiteral("params"), params);

  QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
  payload.append('\n');
  process_.write(payload);
  return id;
}

void McpBridgeProcess::stop() {
  if (!isRunning() || stopping_) return;
  stopping_ = true;
  sendRequest(QStringLiteral("close"));
  scheduleForcedStop();
}

void McpBridgeProcess::scheduleForcedStop() {
  QTimer::singleShot(1500, this, [this]() {
    if (!isRunning()) return;
    process_.terminate();
    QTimer::singleShot(750, this, [this]() {
      if (isRunning()) process_.kill();
    });
  });
}

void McpBridgeProcess::handleStdout() {
  stdoutBuffer_.append(process_.readAllStandardOutput());

  while (true) {
    const qsizetype newline = stdoutBuffer_.indexOf('\n');
    if (newline < 0) break;

    const QByteArray line = stdoutBuffer_.left(newline).trimmed();
    stdoutBuffer_.remove(0, newline + 1);
    if (!line.isEmpty()) handleProtocolLine(line);
  }
}

void McpBridgeProcess::handleStderr() {
  const QString text = QString::fromUtf8(process_.readAllStandardError()).trimmed();
  if (!text.isEmpty()) emit logLine(text);
}

void McpBridgeProcess::handleProtocolLine(const QByteArray &line) {
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    emit logLine(QStringLiteral("Unrecognized bridge output: %1").arg(QString::fromUtf8(line)));
    return;
  }

  const QJsonObject object = document.object();
  if (object.value(QStringLiteral("type")).toString() == QStringLiteral("ready")) {
    emit bridgeReady(object.value(QStringLiteral("transport")).toString());
    return;
  }

  const QString id = object.value(QStringLiteral("id")).toVariant().toString();
  const QString method = pendingMethods_.take(id);
  if (object.value(QStringLiteral("ok")).toBool()) {
    emit responseReceived(id, method, object.value(QStringLiteral("result")));
    return;
  }

  const QJsonObject error = object.value(QStringLiteral("error")).toObject();
  emit requestFailed(id, method, error.value(QStringLiteral("code")).toString(),
                     error.value(QStringLiteral("message")).toString(),
                     error.value(QStringLiteral("details")).toObject());
}
