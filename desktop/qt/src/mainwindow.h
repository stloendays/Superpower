#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
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

  [[nodiscard]] bool isMcpConnected() const;
  [[nodiscard]] QString mcpStatusText() const;
  [[nodiscard]] int discoveredAppCount() const;
  [[nodiscard]] int discoveredActionCount() const;
  [[nodiscard]] int runCount() const;
  [[nodiscard]] QStringList recentRunSummaries(int limit = 5) const;
  void planAction(const QString &query);
  void planWorkflow(const QString &query);
  void reviewWorkflowStep(const QJsonObject &actionPlan);
  void startWorkflowRun(const QString &planId, const QStringList &approvedBindingIds);
  void advanceWorkflowRun(bool approve);
  void provideWorkflowStepOutput(const QString &stepId, const QString &output);

  struct ServerProfile {
    QString id;
    QString name;
    QString transport;
    QString endpoint;
    QString command;
  };

  struct PendingCall {
    QString toolName;
    QString appName;
    QString serverName;
    QString risk;
    QJsonObject arguments;
    QDateTime startedAt;
  };

  struct RunRecord {
    QString toolName;
    QString appName;
    QString serverName;
    QString status;
    QString risk;
    QString summary;
    QJsonObject arguments;
    QDateTime startedAt;
    qint64 durationMs = 0;
  };

 signals:
  void actionPlanStarted(const QString &query);
  void actionPlanPrepared(const QString &summary);
  void actionPlanFailed(const QString &message);
  void workflowPlanStarted(const QString &query);
  void workflowPlanPrepared(const QString &summary, const QJsonObject &plan);
  void workflowPlanFailed(const QString &message);
  void workflowRunUpdated(const QJsonObject &state);
  void workflowRunFailed(const QString &message);

 private:
  void buildUi();
  void applyStyle();
  void connectSignals();
  void setConnectedUi(bool connected);
  void updateConnectionForm();
  void connectOrDisconnect();

  void loadServerProfiles();
  void persistServerProfiles() const;
  void refreshServerList();
  void newServerProfile();
  void saveCurrentServerProfile();
  void deleteCurrentServerProfile();
  void selectServerProfile();
  void setServerStatus(const QString &profileId, const QString &status);
  int serverProfileIndex(const QString &profileId) const;
  QString currentServerName() const;
  QString currentServerProfileId() const;

  void refreshTools();
  void runSelectedTool();
  void showSelectedTool();
  void applyToolFilters();
  void populateTools(const QJsonObject &result);
  void rebuildAppFilter();
  void rebuildArgumentForm(const QJsonObject &schema);
  void applyArgumentsToForm(const QJsonObject &arguments);
  bool collectFormArguments(QJsonObject *arguments, QString *errorMessage) const;
  QWidget *createFieldEditor(const QString &name, const QJsonObject &schema, bool required);

  void ensureActionRouterConnections();
  void clearActionPlanReview();
  void applyActionPlan(const QJsonObject &plan);
  void ensureWorkflowPlannerConnections();
  void applyWorkflowPlan(const QJsonObject &plan);
  void handleGlobalSearch(const QString &query);
  void executeGlobalCommand();
  void openSettingsDialog();
  void openRunsDialog();
  void openLogsDialog();
  void openAboutDialog();
  void openSchemaDialog();
  void recordRun(const PendingCall &pending, const QString &status, const QString &summary);
  void loadRunIntoWorkspace(int historyIndex);

  void handleResponse(const QString &id, const QString &method, const QJsonValue &result);
  void handleRequestFailure(const QString &id, const QString &method, const QString &code,
                            const QString &message, const QJsonObject &details);
  void appendOutput(const QString &heading, const QJsonValue &value);
  void appendLog(const QString &line);
  void setStatus(const QString &text, bool connected);
  QString connectionSummary() const;
  QIcon iconForCategory(const QString &category) const;
  QIcon iconForServerStatus(const QString &status) const;

  static QString findDefaultHostScript();
  static QString findDefaultNodeProgram();
  static QString toolCategory(const QString &name, const QString &description);
  static QString toolApp(const QString &name, const QString &description);
  static QString toolDisplayName(const QString &name, const QString &appName);
  static bool parseStdioArguments(const QString &text, QStringList *arguments, QString *errorMessage);
  static bool endpointIsSafeToPersist(const QString &endpoint);

  McpBridgeProcess *bridge_ = nullptr;

  QLabel *headerConnectionLabel_ = nullptr;
  QLineEdit *globalSearchEdit_ = nullptr;
  QPushButton *settingsButton_ = nullptr;
  QPushButton *runsButton_ = nullptr;
  QPushButton *logsButton_ = nullptr;
  QPushButton *aboutButton_ = nullptr;

  QListWidget *serverList_ = nullptr;
  QLineEdit *serverNameEdit_ = nullptr;
  QPushButton *newServerButton_ = nullptr;
  QPushButton *saveServerButton_ = nullptr;
  QPushButton *deleteServerButton_ = nullptr;
  QComboBox *transportCombo_ = nullptr;
  QWidget *httpConnectionWidget_ = nullptr;
  QWidget *stdioConnectionWidget_ = nullptr;
  QLineEdit *endpointEdit_ = nullptr;
  QLineEdit *stdioCommandEdit_ = nullptr;
  QLineEdit *stdioArgsEdit_ = nullptr;
  QPushButton *connectButton_ = nullptr;
  QLabel *statusLabel_ = nullptr;
  QLabel *connectionSummaryLabel_ = nullptr;

  QComboBox *appCombo_ = nullptr;
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
  QString activeProfileId_;
  QString activeConnectionName_;
  QString plannedToolName_;
  QString latestActionPlanId_;
  QString latestWorkflowPlanId_;
  QString activeWorkflowRunId_;
  QStringList logLines_;
  QList<ServerProfile> serverProfiles_;
  QList<RunRecord> runHistory_;
  QHash<QString, QString> serverStatuses_;

  QHash<QString, QJsonObject> toolsByName_;
  QHash<QString, PendingCall> pendingCalls_;
  QHash<QString, QWidget *> fieldEditors_;
  QHash<QString, QJsonObject> fieldSchemas_;
  QSet<QString> requiredFields_;
  QSet<QString> pendingActionPlanIds_;
  QSet<QString> pendingWorkflowPlanIds_;
  QSet<QString> pendingWorkflowRunIds_;
  bool actionRouterSignalsConnected_ = false;
  bool workflowPlannerSignalsConnected_ = false;
};
