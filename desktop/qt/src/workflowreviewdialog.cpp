#include "workflowreviewdialog.h"

#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
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

QString stepLabel(const QJsonObject &step, const QString &status = QString()) {
  const int index = step.value(QStringLiteral("index")).toInt();
  const QString kind = step.value(QStringLiteral("kind")).toString(QStringLiteral("unresolved")).toUpper();
  const QString instruction = step.value(QStringLiteral("instruction")).toString().simplified();
  const QJsonObject action = step.value(QStringLiteral("action")).toObject();
  const QString tool = action.value(QStringLiteral("selected")).toObject().value(QStringLiteral("name")).toString();

  QString title;
  if (!status.isEmpty()) title += QStringLiteral("[%1] ").arg(status.toUpper());
  title += QStringLiteral("Step %1 · %2").arg(index).arg(kind);
  if (!tool.isEmpty()) title += QStringLiteral(" · %1").arg(tool);
  if (!instruction.isEmpty()) title += QStringLiteral("\n%1").arg(instruction.left(110));
  return title;
}

QString bindingLabel(const QJsonObject &binding) {
  return QStringLiteral("%1%2  →  %3.%4  ·  %5")
      .arg(binding.value(QStringLiteral("sourceStepId")).toString(),
           binding.value(QStringLiteral("sourcePath")).toString(),
           binding.value(QStringLiteral("targetStepId")).toString(),
           binding.value(QStringLiteral("targetArgument")).toString(),
           binding.value(QStringLiteral("coercion")).toString(QStringLiteral("raw")).toUpper());
}
}  // namespace

WorkflowReviewDialog::WorkflowReviewDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Superpower Workflow Review"));
  setModal(false);
  resize(920, 760);
  setMinimumSize(760, 600);
  buildUi();
  applyStyle();
}

void WorkflowReviewDialog::buildUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(22, 20, 22, 20);
  root->setSpacing(12);

  auto *eyebrow = new QLabel(QStringLiteral("WORKFLOW RUNNER"), this);
  eyebrow->setProperty("workflowEyebrow", true);
  root->addWidget(eyebrow);

  auto *title = new QLabel(QStringLiteral("Review bindings, then advance one step at a time"), this);
  title->setProperty("workflowTitle", true);
  root->addWidget(title);

  summaryLabel_ = new QLabel(
      QStringLiteral("No workflow plan yet. Planning and execution are review-gated and session-only."), this);
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
  detailView_->setPlaceholderText(
      QStringLiteral("Select a workflow step to inspect routing, policy, bindings, and runner state."));
  contentRow->addWidget(detailView_, 1);
  root->addLayout(contentRow, 1);

  auto *bindingTitle = new QLabel(QStringLiteral("OUTPUT BINDINGS · UNCHECKED BY DEFAULT"), this);
  bindingTitle->setProperty("workflowEyebrow", true);
  root->addWidget(bindingTitle);

  bindingList_ = new QListWidget(this);
  bindingList_->setMaximumHeight(132);
  bindingList_->setToolTip(
      QStringLiteral("Checked bindings are approved only for the next run you start. Running never adds hidden bindings."));
  root->addWidget(bindingList_);

  runStatusLabel_ = new QLabel(
      QStringLiteral("No run started. Check only the cross-step bindings you explicitly approve, then start a reviewed run."),
      this);
  runStatusLabel_->setWordWrap(true);
  runStatusLabel_->setProperty("workflowRunStatus", true);
  root->addWidget(runStatusLabel_);

  auto *safety = new QLabel(
      QStringLiteral("Each Advance click can execute at most one MCP Action through the existing guarded policy. Confirm-risk steps pause for explicit approval. Transform or unresolved steps require manual output; Superpower does not fabricate their result."),
      this);
  safety->setWordWrap(true);
  safety->setProperty("workflowMuted", true);
  root->addWidget(safety);

  auto *runButtons = new QHBoxLayout();
  startRunButton_ = new QPushButton(QStringLiteral("Start reviewed run"), this);
  startRunButton_->setEnabled(false);
  connect(startRunButton_, &QPushButton::clicked, this, &WorkflowReviewDialog::startReviewedRun);
  runButtons->addWidget(startRunButton_);

  manualOutputButton_ = new QPushButton(QStringLiteral("Provide step output..."), this);
  manualOutputButton_->setEnabled(false);
  connect(manualOutputButton_, &QPushButton::clicked, this, &WorkflowReviewDialog::provideManualOutput);
  runButtons->addWidget(manualOutputButton_);

  runButtons->addStretch(1);

  advanceButton_ = new QPushButton(QStringLiteral("Run next step"), this);
  advanceButton_->setProperty("workflowPrimary", true);
  advanceButton_->setEnabled(false);
  connect(advanceButton_, &QPushButton::clicked, this, &WorkflowReviewDialog::advanceReviewedRun);
  runButtons->addWidget(advanceButton_);
  root->addLayout(runButtons);

  auto *buttons = new QHBoxLayout();
  reviewButton_ = new QPushButton(QStringLiteral("Open selected action review"), this);
  reviewButton_->setEnabled(false);
  connect(reviewButton_, &QPushButton::clicked, this, &WorkflowReviewDialog::reviewSelectedAction);
  buttons->addWidget(reviewButton_);
  buttons->addStretch(1);

  auto *closeButton = new QPushButton(QStringLiteral("Close"), this);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::hide);
  buttons->addWidget(closeButton);
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
    QLabel[workflowRunStatus="true"] {
      background: #ffffff;
      color: #222222;
      border: 1px solid #d7d7d7;
      border-radius: 8px;
      padding: 9px 11px;
      font-weight: 650;
    }
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
      padding: 9px 8px;
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
  runState_ = QJsonObject();
  planId_ = plan.value(QStringLiteral("planId")).toString();
  steps_ = plan.value(QStringLiteral("steps")).toArray();
  bindings_ = plan.value(QStringLiteral("bindings")).toArray();
  stepList_->clear();
  bindingList_->clear();

  for (const QJsonValue &value : steps_) {
    const QJsonObject step = value.toObject();
    auto *item = new QListWidgetItem(stepLabel(step), stepList_);
    item->setToolTip(step.value(QStringLiteral("instruction")).toString());
  }

  for (const QJsonValue &value : bindings_) {
    const QJsonObject binding = value.toObject();
    auto *item = new QListWidgetItem(bindingLabel(binding), bindingList_);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    item->setData(Qt::UserRole, binding.value(QStringLiteral("id")).toString());
    item->setToolTip(QStringLiteral("Approve this exact binding for the next run only."));
  }
  if (bindings_.isEmpty()) {
    auto *item = new QListWidgetItem(QStringLiteral("No cross-step output bindings were proposed."), bindingList_);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  }

  const QString confidence = plan.value(QStringLiteral("confidence")).toString(QStringLiteral("low")).toUpper();
  const int unresolved = plan.value(QStringLiteral("unresolvedStepCount")).toInt();
  summaryLabel_->setText(
      QStringLiteral("%1 steps · %2 confidence · %3 unresolved · %4 proposed bindings · auto execution disabled")
          .arg(steps_.size())
          .arg(confidence)
          .arg(unresolved)
          .arg(bindings_.size()));
  runStatusLabel_->setText(
      QStringLiteral("No run started. Bindings are unchecked by default; only checked bindings are approved for a new session-only run."));

  if (!steps_.isEmpty()) {
    stepList_->setCurrentRow(0);
  } else {
    detailView_->setPlainText(QStringLiteral("No workflow steps were produced."));
    reviewButton_->setEnabled(false);
  }
  updateRunControls();
}

void WorkflowReviewDialog::setRunState(const QJsonObject &state) {
  runState_ = state;

  QHash<QString, QString> statusByStep;
  for (const QJsonValue &value : state.value(QStringLiteral("steps")).toArray()) {
    const QJsonObject stepState = value.toObject();
    statusByStep.insert(stepState.value(QStringLiteral("stepId")).toString(),
                        stepState.value(QStringLiteral("status")).toString());
  }

  const int selectedRow = stepList_->currentRow();
  for (int row = 0; row < steps_.size() && row < stepList_->count(); ++row) {
    const QJsonObject step = steps_.at(row).toObject();
    stepList_->item(row)->setText(stepLabel(step, statusByStep.value(step.value(QStringLiteral("id")).toString())));
  }
  if (selectedRow >= 0 && selectedRow < stepList_->count()) stepList_->setCurrentRow(selectedRow);

  updateRunControls();
  updateSelectedStep();
}

void WorkflowReviewDialog::setRunError(const QString &message) {
  runStatusLabel_->setText(message);
}

void WorkflowReviewDialog::updateRunControls() {
  startRunButton_->setEnabled(!planId_.isEmpty());
  manualOutputButton_->setEnabled(false);
  advanceButton_->setEnabled(false);
  advanceButton_->setText(QStringLiteral("Run next step"));
  advanceRequiresApproval_ = false;

  if (runState_.isEmpty()) return;

  const QString status = runState_.value(QStringLiteral("status")).toString(QStringLiteral("ready"));
  const QJsonObject gate = runState_.value(QStringLiteral("gate")).toObject();
  const QJsonObject event = runState_.value(QStringLiteral("event")).toObject();
  const QString gateType = gate.value(QStringLiteral("type")).toString();
  const QString reason = gate.value(QStringLiteral("reason")).toString();
  const QString eventType = event.value(QStringLiteral("type")).toString();

  if (status == QStringLiteral("completed") || gateType == QStringLiteral("completed")) {
    runStatusLabel_->setText(QStringLiteral("Workflow run complete. Every action advanced through a reviewed single-step boundary."));
    return;
  }
  if (status == QStringLiteral("failed") || gateType == QStringLiteral("failed")) {
    runStatusLabel_->setText(QStringLiteral("Workflow run failed · %1").arg(reason));
    return;
  }

  if (eventType == QStringLiteral("confirmation_required")) {
    const QJsonObject policy = event.value(QStringLiteral("policy")).toObject();
    runStatusLabel_->setText(
        QStringLiteral("Guarded confirmation required · risk %1. Inspect this step, then explicitly approve it to execute once.")
            .arg(policy.value(QStringLiteral("risk")).toString(QStringLiteral("unknown")).toUpper()));
    advanceRequiresApproval_ = true;
    advanceButton_->setText(QStringLiteral("Approve & run current step"));
    advanceButton_->setEnabled(true);
    return;
  }

  if (gateType == QStringLiteral("ready")) {
    const QJsonObject policy = gate.value(QStringLiteral("policy")).toObject();
    const QString decision = policy.value(QStringLiteral("decision")).toString(QStringLiteral("allow"));
    runStatusLabel_->setText(
        QStringLiteral("Ready · %1. One click can advance only this step; it will stop again after the result.").arg(reason));
    advanceButton_->setText(decision == QStringLiteral("confirm") ? QStringLiteral("Review guarded step")
                                                                  : QStringLiteral("Run next step"));
    advanceButton_->setEnabled(true);
    return;
  }

  if (gateType == QStringLiteral("manual_input_required") || gateType == QStringLiteral("unresolved_step")) {
    runStatusLabel_->setText(QStringLiteral("Manual boundary · %1").arg(reason));
    manualOutputButton_->setEnabled(true);
    return;
  }

  if (gateType == QStringLiteral("binding_review_required")) {
    runStatusLabel_->setText(
        QStringLiteral("Binding approval required · %1 Check the intended bindings above and start a new reviewed run; approvals are immutable within a run.")
            .arg(reason));
    return;
  }

  if (gateType == QStringLiteral("missing_required")) {
    runStatusLabel_->setText(
        QStringLiteral("Missing reviewed arguments · %1 Re-plan with explicit values or open the Action review to inspect what is missing.")
            .arg(reason));
    return;
  }

  runStatusLabel_->setText(QStringLiteral("Runner paused · %1").arg(reason));
}

void WorkflowReviewDialog::updateSelectedStep() {
  const int row = stepList_->currentRow();
  if (row < 0 || row >= steps_.size()) {
    reviewButton_->setEnabled(false);
    detailView_->clear();
    return;
  }

  const QJsonObject step = steps_.at(row).toObject();
  const QString stepId = step.value(QStringLiteral("id")).toString();
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
               .arg(needsPrevious ? QStringLiteral("EXPLICIT BINDING REVIEW") : QStringLiteral("none detected"));

  QStringList targetBindings;
  for (const QJsonValue &value : bindings_) {
    const QJsonObject binding = value.toObject();
    if (binding.value(QStringLiteral("targetStepId")).toString() == stepId) targetBindings << bindingLabel(binding);
  }
  lines << QStringLiteral("Bindings into this step: %1")
               .arg(targetBindings.isEmpty() ? QStringLiteral("none") : targetBindings.join(QStringLiteral("; ")));

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
    lines << QStringLiteral("Missing required before bindings: %1")
                 .arg(missing.isEmpty() ? QStringLiteral("none") : missing.join(QStringLiteral(", ")));
    lines << QStringLiteral("Policy preview: %1 · risk %2")
                 .arg(policy.value(QStringLiteral("decision")).toString(QStringLiteral("unknown")).toUpper(),
                      policy.value(QStringLiteral("risk")).toString(QStringLiteral("unknown")).toUpper());
    lines << QString();
    lines << QStringLiteral("Draft arguments:");
    lines << QString::fromUtf8(
        QJsonDocument(action.value(QStringLiteral("arguments")).toObject()).toJson(QJsonDocument::Indented));
  } else if (kind == QStringLiteral("transform")) {
    const QJsonObject transform = step.value(QStringLiteral("transform")).toObject();
    lines << QString();
    lines << QStringLiteral("Transform operation: %1")
                 .arg(transform.value(QStringLiteral("operation")).toString(QStringLiteral("unknown")).toUpper());
    lines << QStringLiteral("The Runner will pause here for explicit user-provided output. It does not invent a local model result or MCP tool.");
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
    lines << QStringLiteral("The Runner can continue only if you explicitly provide this step's output.");
  }

  if (!runState_.isEmpty()) {
    for (const QJsonValue &value : runState_.value(QStringLiteral("steps")).toArray()) {
      const QJsonObject stepState = value.toObject();
      if (stepState.value(QStringLiteral("stepId")).toString() != stepId) continue;
      lines << QString();
      lines << QStringLiteral("Run state: %1")
                   .arg(stepState.value(QStringLiteral("status")).toString(QStringLiteral("pending")).toUpper());
      const QString error = stepState.value(QStringLiteral("error")).toString();
      if (!error.isEmpty()) lines << QStringLiteral("Run error: %1").arg(error);
      break;
    }

    const QJsonObject gate = runState_.value(QStringLiteral("gate")).toObject();
    if (gate.value(QStringLiteral("stepId")).toString() == stepId) {
      lines << QStringLiteral("Current gate: %1")
                   .arg(gate.value(QStringLiteral("type")).toString(QStringLiteral("unknown")).toUpper());
      lines << QStringLiteral("Gate reason: %1").arg(gate.value(QStringLiteral("reason")).toString());
      const QStringList missing = stringArray(gate.value(QStringLiteral("missingRequired")));
      if (!missing.isEmpty()) lines << QStringLiteral("Still missing: %1").arg(missing.join(QStringLiteral(", ")));
      const QJsonObject resolved = gate.value(QStringLiteral("arguments")).toObject();
      if (!resolved.isEmpty()) {
        lines << QStringLiteral("Resolved run arguments:");
        lines << QString::fromUtf8(QJsonDocument(resolved).toJson(QJsonDocument::Indented));
      }
    }
  }

  lines << QString();
  lines << QStringLiteral("Opening the Action review never executes the step. Workflow execution happens only through the Runner controls below.");
  detailView_->setPlainText(lines.join(QLatin1Char('\n')));
}

QStringList WorkflowReviewDialog::approvedBindingIds() const {
  QStringList approved;
  for (int index = 0; index < bindingList_->count(); ++index) {
    const QListWidgetItem *item = bindingList_->item(index);
    if (!item || item->checkState() != Qt::Checked) continue;
    const QString id = item->data(Qt::UserRole).toString().trimmed();
    if (!id.isEmpty()) approved << id;
  }
  return approved;
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

void WorkflowReviewDialog::startReviewedRun() {
  if (planId_.isEmpty()) return;
  emit startRunRequested(planId_, approvedBindingIds());
}

void WorkflowReviewDialog::advanceReviewedRun() {
  if (runState_.isEmpty()) return;
  emit advanceRunRequested(advanceRequiresApproval_);
}

void WorkflowReviewDialog::provideManualOutput() {
  if (runState_.isEmpty()) return;
  const QJsonObject gate = runState_.value(QStringLiteral("gate")).toObject();
  const QString stepId = gate.value(QStringLiteral("stepId")).toString();
  if (stepId.isEmpty()) return;

  bool accepted = false;
  const QString output = QInputDialog::getMultiLineText(
      this, QStringLiteral("Provide workflow step output"),
      QStringLiteral("Enter the explicit output for %1. This value stays in the current MCP session and can feed only bindings you approved when the run started.")
          .arg(stepId),
      QString(), &accepted);
  if (!accepted) return;
  emit provideOutputRequested(stepId, output);
}
