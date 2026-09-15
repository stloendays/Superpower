#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QMainWindow>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class McpBridgeProcess;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = nullptr);

 private:
  struct PendingCall {
    QString toolName;
    QJsonObject arguments;
  };

  void buildUi();
  void applyStyle();
  void connectSignals();
  void setConnectedUi(bool connected);
  void connectOrDisconnect();
  void refreshTools();
  void applyFocus();
  void runSelectedTool();
  void showSelectedTool();
  void filterTools(const QString &query);
  void populateTools(const QJsonObject &result);
  void handleResponse(const QString &id, const QString &method, const QJsonValue &result);
  void handleRequestFailure(const QString &id, const QString &method, const QString &code,
                            const QString &message, const QJsonObject &details);
  void appendOutput(const QString &heading, const QJsonValue &value);
  void appendLog(const QString &line);
  void setStatus(const QString &text, bool connected);

  static QString findDefaultHostScript();
  static QJsonValue sampleValueForSchema(const QJsonObject &schema);
  static QJsonObject argumentTemplate(const QJsonObject &schema);

  McpBridgeProcess *bridge_ = nullptr;

  QLineEdit *endpointEdit_ = nullptr;
  QLineEdit *nodeEdit_ = nullptr;
  QLineEdit *hostScriptEdit_ = nullptr;
  QLineEdit *focusEdit_ = nullptr;
  QComboBox *policyCombo_ = nullptr;
  QPushButton *connectButton_ = nullptr;
  QLabel *statusLabel_ = nullptr;

  QLineEdit *toolSearchEdit_ = nullptr;
  QListWidget *toolList_ = nullptr;
  QLabel *toolCountLabel_ = nullptr;

  QLabel *toolNameLabel_ = nullptr;
  QLabel *toolDescriptionLabel_ = nullptr;
  QPlainTextEdit *schemaView_ = nullptr;
  QPlainTextEdit *argumentsEdit_ = nullptr;
  QPushButton *runButton_ = nullptr;
  QPushButton *refreshButton_ = nullptr;
  QPushButton *statsButton_ = nullptr;
  QPlainTextEdit *outputView_ = nullptr;

  QHash<QString, QJsonObject> toolsByName_;
  QHash<QString, PendingCall> pendingCalls_;
};
