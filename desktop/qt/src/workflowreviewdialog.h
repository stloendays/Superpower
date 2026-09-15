#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class WorkflowReviewDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit WorkflowReviewDialog(QWidget *parent = nullptr);
  void setPlan(const QJsonObject &plan);

 signals:
  void reviewActionRequested(const QJsonObject &actionPlan);

 private:
  void buildUi();
  void applyStyle();
  void updateSelectedStep();
  void reviewSelectedAction();

  QJsonObject plan_;
  QJsonArray steps_;
  QLabel *summaryLabel_ = nullptr;
  QListWidget *stepList_ = nullptr;
  QPlainTextEdit *detailView_ = nullptr;
  QPushButton *reviewButton_ = nullptr;
};
