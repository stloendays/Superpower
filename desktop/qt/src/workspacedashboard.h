#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class MainWindow;
class QPushButton;
class QTimer;

class WorkspaceDashboard final : public QWidget {
  Q_OBJECT

 public:
  explicit WorkspaceDashboard(MainWindow *workspace, QWidget *parent = nullptr);

 public slots:
  void setConversationRelayStatus(const QString &text, bool online);
  void setActiveProvider(const QString &provider);
  void setActionRouterStatus(const QString &text, bool busy);
  void noteConversationActivity();
  void refreshFromWorkspace();

 signals:
  void actionQueryRequested(const QString &query);
  void workflowQueryRequested(const QString &query);
  void browseActionsRequested();
  void gettingStartedRequested();
  void openConversationRequested();

 private:
  QWidget *createStatusCard(const QString &title, QLabel **valueLabel, QLabel **detailLabel);
  QWidget *createMetricCard(const QString &title, QLabel **valueLabel, const QString &detail);
  void submitActionQuery();
  void submitWorkflowQuery();
  void buildUi();
  void applyStyle();

  MainWindow *workspace_ = nullptr;
  QLineEdit *actionQueryEdit_ = nullptr;
  QPushButton *actionRouteButton_ = nullptr;
  QPushButton *workflowPlanButton_ = nullptr;
  QLabel *actionRouterStatusLabel_ = nullptr;
  QLabel *providerValueLabel_ = nullptr;
  QLabel *providerDetailLabel_ = nullptr;
  QLabel *conversationValueLabel_ = nullptr;
  QLabel *conversationDetailLabel_ = nullptr;
  QLabel *mcpValueLabel_ = nullptr;
  QLabel *mcpDetailLabel_ = nullptr;
  QLabel *appsValueLabel_ = nullptr;
  QLabel *actionsValueLabel_ = nullptr;
  QLabel *runsValueLabel_ = nullptr;
  QLabel *activityValueLabel_ = nullptr;
  QListWidget *recentRunsList_ = nullptr;
  QTimer *refreshTimer_ = nullptr;

  QString activeProvider_;
  QString conversationRelayText_ = QStringLiteral("Starting local relay...");
  bool conversationRelayOnline_ = false;
  int conversationEventCount_ = 0;
};
