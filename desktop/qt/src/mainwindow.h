#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QStringList>

class QComboBox;
class QFormLayout;
class QIcon;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QWidget;
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
  void updateConnectionForm();
  void connectOrDisconnect();
  void refreshTools();
  void runSelectedTool();
  void showSelectedTool();
  void applyToolFilters();
  void populateTools(const QJsonObject &result);
  void rebuildArgumentForm(const QJsonObject &schema);
  bool collectFormArguments(QJsonObject *arguments, QString *errorMessage) const;
  QWidget *createFieldEditor(const QString &name, const QJsonObject &schema, bool required);
  void handleGlobalSearch(const QString &query);
  void executeGlobalCommand();
  void openSettingsDialog();
  void openLogsDialog();
  void openAboutDialog();
  void openSchemaDialog();
  void handleResponse(const QString &id, const QString &method, const QJsonValue &result);
  void handleRequestFailure(const QString &id, const QString &method, const QString &code,
                            const QString &message, const QJsonObject &details);
  void appendOutput(const QString &heading, const QJsonValue &value);
  void appendLog(const QString &line);
  void setStatus(const QString &text, bool connected);
  QString connectionSummary() const;
  QIcon iconForCategory(const QString &category) const;

  static QString findDefaultHostScript();
  static QString findDefaultNodeProgram();
  static QString toolCategory(const QString &name, const QString &description);
  static bool parseStdioArguments(const QString &text, QStringList *arguments, QString *errorMessage);

  McpBridgeProcess *bridge_ = nullptr;

  QLabel *headerConnectionLabel_ = nullptr;
  QLineEdit *globalSearchEdit_ = nullptr;
  QPushButton *settingsButton_ = nullptr;
  QPushButton *logsButton_ = nullptr;
  QPushButton *aboutButton_ = nullptr;

  QComboBox *transportCombo_ = nullptr;
  QWidget *httpConnectionWidget_ = nullptr;
  QWidget *stdioConnectionWidget_ = nullptr;
  QLineEdit *endpointEdit_ = nullptr;
  QLineEdit *stdioCommandEdit_ = nullptr;
  QLineEdit *stdioArgsEdit_ = nullptr;
  QPushButton *connectButton_ = nullptr;
  QLabel *statusLabel_ = nullptr;
  QLabel *connectionSummaryLabel_ = nullptr;

  QComboBox *categoryCombo_ = nullptr;
  QListWidget *toolList_ = nullptr;
  QLabel *toolCountLabel_ = nullptr;
  QPushButton *refreshButton_ = nullptr;

  QLabel *toolNameLabel_ = nullptr;
  QLabel *toolDescriptionLabel_ = nullptr;
  QScrollArea *formScrollArea_ = nullptr;
  QWidget *formWidget_ = nullptr;
  QFormLayout *formLayout_ = nullptr;
  QPushButton *resetFormButton_ = nullptr;
  QPushButton *runButton_ = nullptr;
  QPushButton *statsButton_ = nullptr;
  QPushButton *schemaButton_ = nullptr;
  QPlainTextEdit *outputView_ = nullptr;

  QString nodeProgram_;
  QString hostScript_;
  QString taskFocus_;
  QString policyMode_ = QStringLiteral("guarded");
  QStringList logLines_;

  QHash<QString, QJsonObject> toolsByName_;
  QHash<QString, PendingCall> pendingCalls_;
  QHash<QString, QWidget *> fieldEditors_;
  QHash<QString, QJsonObject> fieldSchemas_;
  QSet<QString> requiredFields_;
};