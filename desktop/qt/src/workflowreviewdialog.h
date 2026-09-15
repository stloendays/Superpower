#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class WorkflowReviewDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit WorkflowReviewDialog(QWidget *parent = nullptr);
  void setPlan(const QJsonObject &plan);
  void setRunState(const QJsonObject &state);
  void setRunError(const QString &message);

 signals:
  void reviewActionRequested(const QJsonObject &actionPlan);
  void startRunRequested(const QString &planId, const QStringList &approvedBindingIds);
  void advanceRunRequested(bool approve);
  void provideOutputRequested(const QString &stepId, const QString &output);

 private:
  void buildUi();
  void applyStyle();
  void updateSelectedStep();
  void updateRunControls();
  void reviewSelectedAction();
  void startReviewedRun();
  void advanceReviewedRun();
  void provideManualOutput();
  QStringList approvedBindingIds() const;

  QJsonObject plan_;
  QJsonObject runState_;
  QJsonArray steps_;
  QJsonArray bindings_;
  QString planId_;
  bool advanceRequiresApproval_ = false;

  QLabel *summaryLabel_ = nullptr;
  QLabel *runStatusLabel_ = nullptr;
  QListWidget *stepList_ = nullptr;
  QListWidget *bindingList_ = nullptr;
  QPlainTextEdit *detailView_ = nullptr;
  QPushButton *reviewButton_ = nullptr;
  QPushButton *startRunButton_ = nullptr;
  QPushButton *advanceButton_ = nullptr;
  QPushButton *manualOutputButton_ = nullptr;
};
