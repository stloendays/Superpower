#include "workflowreviewdialog.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QStringList stringArray(const QJsonValue &value) {
  QStringList values;
  for (const QJsonValue &item : value.toArray()) {
    if (item.isString() && !item.toString().trimmed().isEmpty()) values << item.toString();
  }
  return values;
}

QString stepLabel(const QJsonObject &step) {
  const int index = step.value(QStringLiteral("index")).toInt();
  const QString kind = step.value(QStringLiteral("kind")).toString(QStringLiteral("unresolved")).toUpper();
  const QString instruction = step.value(QStringLiteral("instruction")).toString().simplified();
  const QJsonObject action = step.value(QStringLiteral("action")).toObject();
  const QString tool = action.value(QStringLiteral("selected")).toObject().value(QStringLiteral("name")).toString();

  QString title = QStringLiteral("Step %1 · %2").arg(index).arg(kind);
  if (!tool.isEmpty()) title += QStringLiteral(" · %1").arg(tool);
  if (!instruction.isEmpty()) title += QStringLiteral("\n%1").arg(instruction.left(110));
  return title;
}
}  // namespace

WorkflowReviewDialog::WorkflowReviewDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Superpower Workflow Review"));
  setModal(false);
  resize(860, 640);
  setMinimumSize(700, 520);
  buildUi();
  applyStyle();
}

void WorkflowReviewDialog::buildUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(22, 20, 22, 20);
  root->setSpacing(14);

  auto *eyebrow = new QLabel(QStringLiteral("WORKFLOW PLANNER"), this);
  eyebrow->setProperty("workflowEyebrow", true);
  root->addWidget(eyebrow);

  auto *title = new QLabel(QStringLiteral("Review the plan before any execution"), this);
  title->setProperty("workflowTitle", true);
  root->addWidget(title);

  summaryLabel_ = new QLabel(
      QStringLiteral("No workflow plan yet. Planning is review-only and never executes MCP tools automatically."), this);
  summaryLabel_->setWordWrap(true);
  summaryLabel_->setProperty("workflowMuted", true);
  root->addWidget(summaryLabel_);

  auto *contentRow = new QHBoxLayout();
  contentRow->setSpacing(12);

  stepList_ = new QListWidget(this);
  stepList_->setMinimumWidth(310);
  stepList_->setMaximumWidth(390);
  contentRow->addWidget(stepList_);

  detailView_ = new QPlainTextEdit(this);
  detailView_->setReadOnly(true);
  detailView_->setPlaceholderText(QStringLiteral("Select a workflow step to inspect its routing, risk, and handoff requirements."));
  contentRow->addWidget(detailView_, 1);
  root->addLayout(contentRow, 1);

  auto *safety = new QLabel(
      QStringLiteral("Opening an Action step only loads the existing Actions review form. Running it still goes through the normal guarded execution policy. Transform and unresolved steps are not executable in this V1.5 slice."),
      this);
  safety->setWordWrap(true);
  safety->setProperty("workflowMuted", true);
  root->addWidget(safety);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch(1);
  auto *closeButton = new QPushButton(QStringLiteral("Close"), this);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::hide);
  buttons->addWidget(closeButton);

  reviewButton_ = new QPushButton(QStringLiteral("Review selected action"), this);
  reviewButton_->setProperty("workflowPrimary", true);
  reviewButton_->setEnabled(false);
  connect(reviewButton_, &QPushButton::clicked, this, &WorkflowReviewDialog::reviewSelectedAction);
  buttons->addWidget(reviewButton_);
  root->addLayout(buttons);

  connect(stepList_, &QListWidget::currentRowChanged, this, &WorkflowReviewDialog::updateSelectedStep);
}

void WorkflowReviewDialog::applyStyle() {
  setStyleSheet(QStringLiteral(R"(
    QDialog {
      background: #f6f6f6;
      color: #111111;
    }
    QLabel[workflowEyebrow="true"] {
      color: #707070;
      font-size: 11px;
      font-weight: 700;
    }
    QLabel[workflowTitle="true"] {
      color: #000000;
      font-size: 22px;
      font-weight: 760;
    }
    QLabel[workflowMuted="true"] { color: #686868; }
    QListWidget, QPlainTextEdit {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d7d7d7;
      border-radius: 10px;
      padding: 8px;
      selection-background-color: #111111;
      selection-color: #ffffff;
    }
    QListWidget::item {
      border-bottom: 1px solid #eeeeee;
      padding: 10px 8px;
    }
    QPushButton {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d0d0d0;
      border-radius: 8px;
      padding: 8px 12px;
      font-weight: 650;
    }
    QPushButton:hover { background: #eeeeee; }
    QPushButton:disabled { color: #999999; background: #eeeeee; }
    QPushButton[workflowPrimary="true"] {
      background: #000000;
      color: #ffffff;
      border-color: #000000;
    }
    QPushButton[workflowPrimary="true"]:hover { background: #202020; }
    QPushButton[workflowPrimary="true"]:disabled {
      background: #999999;
      color: #ffffff;
      border-color: #999999;
    }
  )"));
}

void WorkflowReviewDialog::setPlan(const QJsonObject &plan) {
  plan_ = plan;
  steps_ = plan.value(QStringLiteral("steps")).toArray();
  stepList_->clear();

  for (const QJsonValue &value : steps_) {
    const QJsonObject step = value.toObject();
    auto *item = new QListWidgetItem(stepLabel(step), stepList_);
    item->setToolTip(step.value(QStringLiteral("instruction")).toString());
  }

  const QString confidence = plan.value(QStringLiteral("confidence")).toString(QStringLiteral("low")).toUpper();
  const int unresolved = plan.value(QStringLiteral("unresolvedStepCount")).toInt();
  summaryLabel_->setText(
      QStringLiteral("%1 steps · %2 confidence · %3 unresolved · auto execution disabled")
          .arg(steps_.size())
          .arg(confidence)
          .arg(unresolved));

  if (!steps_.isEmpty()) {
    stepList_->setCurrentRow(0);
  } else {
    detailView_->setPlainText(QStringLiteral("No workflow steps were produced."));
    reviewButton_->setEnabled(false);
  }
}

void WorkflowReviewDialog::updateSelectedStep() {
  const int row = stepList_->currentRow();
  if (row < 0 || row >= steps_.size()) {
    reviewButton_->setEnabled(false);
    detailView_->clear();
    return;
  }

  const QJsonObject step = steps_.at(row).toObject();
  const QString kind = step.value(QStringLiteral("kind")).toString(QStringLiteral("unresolved"));
  const QString confidence = step.value(QStringLiteral("confidence")).toString(QStringLiteral("low")).toUpper();
  const QString instruction = step.value(QStringLiteral("instruction")).toString();
  const QStringList dependencies = stringArray(step.value(QStringLiteral("dependsOn")));
  const bool needsPrevious = step.value(QStringLiteral("needsPreviousOutput")).toBool();

  QStringList lines;
  lines << QStringLiteral("Step %1 · %2").arg(step.value(QStringLiteral("index")).toInt()).arg(kind.toUpper());
  lines << QStringLiteral("Instruction: %1").arg(instruction);
  lines << QStringLiteral("Confidence: %1").arg(confidence);
  lines << QStringLiteral("Depends on: %1")
               .arg(dependencies.isEmpty() ? QStringLiteral("none") : dependencies.join(QStringLiteral(", ")));
  lines << QStringLiteral("Previous-output handoff: %1")
               .arg(needsPrevious ? QStringLiteral("REVIEW REQUIRED") : QStringLiteral("none detected"));

  const QJsonObject action = step.value(QStringLiteral("action")).toObject();
  const QJsonObject selected = action.value(QStringLiteral("selected")).toObject();
  const bool reviewableAction = kind == QStringLiteral("action") && !selected.isEmpty();
  reviewButton_->setEnabled(reviewableAction);

  if (kind == QStringLiteral("action")) {
    const QJsonObject policy = action.value(QStringLiteral("policy")).toObject();
    const QStringList drafted = stringArray(action.value(QStringLiteral("draftedFields")));
    const QStringList missing = stringArray(action.value(QStringLiteral("missingRequired")));
    const QStringList matched = stringArray(selected.value(QStringLiteral("matchedTerms")));

    lines << QString();
    lines << QStringLiteral("MCP action: %1").arg(selected.value(QStringLiteral("name")).toString());
    lines << QStringLiteral("Matched terms: %1")
                 .arg(matched.isEmpty() ? QStringLiteral("none") : matched.join(QStringLiteral(", ")));
    lines << QStringLiteral("Drafted fields: %1")
                 .arg(drafted.isEmpty() ? QStringLiteral("none") : drafted.join(QStringLiteral(", ")));
    lines << QStringLiteral("Missing required: %1")
                 .arg(missing.isEmpty() ? QStringLiteral("none") : missing.join(QStringLiteral(", ")));
    lines << QStringLiteral("Policy: %1 · risk %2")
                 .arg(policy.value(QStringLiteral("decision")).toString(QStringLiteral("unknown")).toUpper(),
                      policy.value(QStringLiteral("risk")).toString(QStringLiteral("unknown")).toUpper());
    lines << QString();
    lines << QStringLiteral("Draft arguments:");
    lines << QString::fromUtf8(
        QJsonDocument(action.value(QStringLiteral("arguments")).toObject()).toJson(QJsonDocument::Indented));
    lines << QStringLiteral("Opening this step does not execute it.");
  } else if (kind == QStringLiteral("transform")) {
    const QJsonObject transform = step.value(QStringLiteral("transform")).toObject();
    lines << QString();
    lines << QStringLiteral("Transform operation: %1")
                 .arg(transform.value(QStringLiteral("operation")).toString(QStringLiteral("unknown")).toUpper());
    lines << QStringLiteral("This is a descriptive local transform step. V1.5 does not auto-run it or fabricate an MCP tool for it.");
  } else {
    const QJsonArray candidates = action.value(QStringLiteral("candidates")).toArray();
    QStringList names;
    for (const QJsonValue &candidateValue : candidates) {
      const QJsonObject candidate = candidateValue.toObject();
      const QString name = candidate.value(QStringLiteral("name")).toString();
      if (!name.isEmpty()) names << name;
      if (names.size() >= 3) break;
    }
    lines << QString();
    lines << QStringLiteral("No MCP action met the deterministic routing threshold.");
    if (!names.isEmpty()) lines << QStringLiteral("Closest candidates: %1").arg(names.join(QStringLiteral(", ")));
  }

  detailView_->setPlainText(lines.join(QLatin1Char('\n')));
}

void WorkflowReviewDialog::reviewSelectedAction() {
  const int row = stepList_->currentRow();
  if (row < 0 || row >= steps_.size()) return;
  const QJsonObject step = steps_.at(row).toObject();
  const QJsonObject action = step.value(QStringLiteral("action")).toObject();
  if (step.value(QStringLiteral("kind")).toString() != QStringLiteral("action") ||
      action.value(QStringLiteral("selected")).toObject().isEmpty()) {
    return;
  }
  emit reviewActionRequested(action);
}
