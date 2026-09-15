#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;
class MainWindow;
class QTimer;

class WorkspaceDashboard final : public QWidget {
  Q_OBJECT

 public:
  explicit WorkspaceDashboard(MainWindow *workspace, QWidget *parent = nullptr);

 public slots:
  void setConversationRelayStatus(const QString &text, bool online);
  void noteConversationActivity();
  void refreshFromWorkspace();

 signals:
  void browseActionsRequested();
  void gettingStartedRequested();
  void openConversationRequested();

 private:
  QWidget *createStatusCard(const QString &title, QLabel **valueLabel, QLabel **detailLabel);
  QWidget *createMetricCard(const QString &title, QLabel **valueLabel, const QString &detail);
  void buildUi();
  void applyStyle();

  MainWindow *workspace_ = nullptr;
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

  QString conversationRelayText_ = QStringLiteral("Starting local relay...");
  bool conversationRelayOnline_ = false;
  int conversationEventCount_ = 0;
};
