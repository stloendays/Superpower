#include "mainwindow.h"

#include "mcpbridgeprocess.h"

#include <QJsonArray>
#include <QPlainTextEdit>
#include <QProcess>

namespace {
bool isWorkflowRunMethod(const QString &method) {
  return method == QStringLiteral("workflowStart") || method == QStringLiteral("workflowStatus") ||
         method == QStringLiteral("workflowAdvance") || method == QStringLiteral("workflowProvideOutput");
}
}  // namespace

void MainWindow::ensureWorkflowPlannerConnections() {
  if (workflowPlannerSignalsConnected_) return;
  workflowPlannerSignalsConnected_ = true;

  connect(bridge_, &McpBridgeProcess::responseReceived, this,
          [this](const QString &id, const QString &method, const QJsonValue &result) {
            if (method == QStringLiteral("workflowPlan")) {
              if (!pendingWorkflowPlanIds_.contains(id)) return;
              pendingWorkflowPlanIds_.remove(id);
              if (id != latestWorkflowPlanId_) return;
              latestWorkflowPlanId_.clear();
              applyWorkflowPlan(result.toObject());
              return;
            }

            if (!isWorkflowRunMethod(method) || !pendingWorkflowRunIds_.contains(id)) return;
            pendingWorkflowRunIds_.remove(id);
            const QJsonObject state = result.toObject();
            if (method == QStringLiteral("workflowStart")) {
              activeWorkflowRunId_ = state.value(QStringLiteral("runId")).toString();
            }
            emit workflowRunUpdated(state);
          });

  connect(bridge_, &McpBridgeProcess::requestFailed, this,
          [this](const QString &id, const QString &method, const QString &code,
                 const QString &message, const QJsonObject &) {
            if (method == QStringLiteral("workflowPlan")) {
              if (!pendingWorkflowPlanIds_.contains(id)) return;
              pendingWorkflowPlanIds_.remove(id);
              if (id != latestWorkflowPlanId_) return;
              latestWorkflowPlanId_.clear();
              const QString detail = QStringLiteral("Workflow Planner failed · %1: %2").arg(code, message);
              appendLog(QStringLiteral("Workflow planning failed (%1).").arg(code));
              emit workflowPlanFailed(detail);
              return;
            }

            if (!isWorkflowRunMethod(method) || !pendingWorkflowRunIds_.contains(id)) return;
            pendingWorkflowRunIds_.remove(id);
            const QString detail = QStringLiteral("Workflow Runner failed · %1: %2").arg(code, message);
            appendLog(QStringLiteral("Workflow Runner request failed (%1).").arg(code));
            emit workflowRunFailed(detail);
          });

  connect(bridge_, &McpBridgeProcess::processExited, this,
          [this](int, QProcess::ExitStatus) {
            const bool planningWasPending = !latestWorkflowPlanId_.isEmpty();
            const bool runWasActive = !activeWorkflowRunId_.isEmpty() || !pendingWorkflowRunIds_.isEmpty();
            pendingWorkflowPlanIds_.clear();
            pendingWorkflowRunIds_.clear();
            latestWorkflowPlanId_.clear();
            activeWorkflowRunId_.clear();
            if (planningWasPending) {
              emit workflowPlanFailed(QStringLiteral("Workflow Planner stopped because the MCP connection closed."));
            }
            if (runWasActive) {
              emit workflowRunFailed(
                  QStringLiteral("Workflow Runner stopped because the MCP connection closed. Start a new reviewed run after reconnecting."));
            }
          });
}

void MainWindow::planWorkflow(const QString &query) {
  const QString trimmed = query.trimmed();
  if (trimmed.isEmpty()) {
    emit workflowPlanFailed(QStringLiteral("Describe the multi-step outcome you want Superpower to plan."));
    return;
  }
  if (!bridge_ || !bridge_->isRunning()) {
    emit workflowPlanFailed(QStringLiteral("Connect an MCP server before planning a workflow."));
    return;
  }

  ensureWorkflowPlannerConnections();
  pendingWorkflowPlanIds_.clear();
  pendingWorkflowRunIds_.clear();
  latestWorkflowPlanId_.clear();
  activeWorkflowRunId_.clear();
  clearActionPlanReview();

  const QString id =
      bridge_->sendRequest(QStringLiteral("workflowPlan"), {{QStringLiteral("query"), trimmed}});
  if (id.isEmpty()) {
    emit workflowPlanFailed(QStringLiteral("The MCP bridge could not start workflow planning."));
    return;
  }

  latestWorkflowPlanId_ = id;
  pendingWorkflowPlanIds_.insert(id);
  appendLog(QStringLiteral("Workflow Planner requested a review-only plan from MCP Core."));
  emit workflowPlanStarted(trimmed);
}

void MainWindow::startWorkflowRun(const QString &planId, const QStringList &approvedBindingIds) {
  const QString trimmedPlanId = planId.trimmed();
  if (trimmedPlanId.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("This workflow plan has no session plan ID. Re-plan it before starting a run."));
    return;
  }
  if (!bridge_ || !bridge_->isRunning()) {
    emit workflowRunFailed(QStringLiteral("Connect an MCP server before starting a workflow run."));
    return;
  }
  if (!pendingWorkflowRunIds_.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("A Workflow Runner request is already in progress."));
    return;
  }

  ensureWorkflowPlannerConnections();
  QJsonArray approved;
  for (const QString &bindingId : approvedBindingIds) {
    const QString value = bindingId.trimmed();
    if (!value.isEmpty()) approved.append(value);
  }

  const QString id = bridge_->sendRequest(
      QStringLiteral("workflowStart"),
      {{QStringLiteral("planId"), trimmedPlanId}, {QStringLiteral("approvedBindingIds"), approved}});
  if (id.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("The MCP bridge could not start the reviewed workflow run."));
    return;
  }

  pendingWorkflowRunIds_.insert(id);
  appendLog(QStringLiteral("Workflow Runner starting with %1 explicitly approved binding(s).").arg(approved.size()));
}

void MainWindow::advanceWorkflowRun(bool approve) {
  if (!bridge_ || !bridge_->isRunning()) {
    emit workflowRunFailed(QStringLiteral("Connect an MCP server before advancing the workflow."));
    return;
  }
  if (activeWorkflowRunId_.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("Start a reviewed workflow run before advancing it."));
    return;
  }
  if (!pendingWorkflowRunIds_.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("A Workflow Runner request is already in progress."));
    return;
  }

  ensureWorkflowPlannerConnections();
  const QString id = bridge_->sendRequest(
      QStringLiteral("workflowAdvance"),
      {{QStringLiteral("runId"), activeWorkflowRunId_}, {QStringLiteral("approve"), approve}});
  if (id.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("The MCP bridge could not advance the workflow."));
    return;
  }

  pendingWorkflowRunIds_.insert(id);
  appendLog(approve ? QStringLiteral("Workflow Runner advancing one explicitly approved guarded step.")
                    : QStringLiteral("Workflow Runner advancing at most one reviewed step."));
}

void MainWindow::provideWorkflowStepOutput(const QString &stepId, const QString &output) {
  const QString trimmedStepId = stepId.trimmed();
  if (!bridge_ || !bridge_->isRunning()) {
    emit workflowRunFailed(QStringLiteral("Connect an MCP server before providing workflow output."));
    return;
  }
  if (activeWorkflowRunId_.isEmpty() || trimmedStepId.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("The Workflow Runner has no current step that can accept manual output."));
    return;
  }
  if (!pendingWorkflowRunIds_.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("A Workflow Runner request is already in progress."));
    return;
  }

  ensureWorkflowPlannerConnections();
  const QString id = bridge_->sendRequest(
      QStringLiteral("workflowProvideOutput"),
      {{QStringLiteral("runId"), activeWorkflowRunId_},
       {QStringLiteral("stepId"), trimmedStepId},
       {QStringLiteral("output"), output}});
  if (id.isEmpty()) {
    emit workflowRunFailed(QStringLiteral("The MCP bridge could not record the manual workflow output."));
    return;
  }

  pendingWorkflowRunIds_.insert(id);
  appendLog(QStringLiteral("Workflow Runner received explicit manual output for %1.").arg(trimmedStepId));
}

void MainWindow::applyWorkflowPlan(const QJsonObject &plan) {
  const QJsonArray steps = plan.value(QStringLiteral("steps")).toArray();
  if (steps.isEmpty()) {
    emit workflowPlanFailed(
        QStringLiteral("No workflow steps were produced. Add explicit sequencing such as “then” or “然后”."));
    return;
  }

  int actions = 0;
  int transforms = 0;
  int unresolved = 0;
  int handoffs = 0;
  const QJsonArray bindings = plan.value(QStringLiteral("bindings")).toArray();
  QStringList outline;
  outline << QStringLiteral("Workflow Planner proposal — review every step before execution") << QString();
  outline << QStringLiteral("Intent: %1").arg(plan.value(QStringLiteral("query")).toString());
  outline << QStringLiteral("Confidence: %1")
                 .arg(plan.value(QStringLiteral("confidence")).toString(QStringLiteral("low")).toUpper());
  outline << QStringLiteral("Auto execution: DISABLED")
          << QStringLiteral("Bindings: %1 proposed · explicit approval required").arg(bindings.size()) << QString();

  for (const QJsonValue &value : steps) {
    const QJsonObject step = value.toObject();
    const QString kind = step.value(QStringLiteral("kind")).toString(QStringLiteral("unresolved"));
    const QString instruction = step.value(QStringLiteral("instruction")).toString();
    if (kind == QStringLiteral("action")) ++actions;
    else if (kind == QStringLiteral("transform")) ++transforms;
    else ++unresolved;
    if (step.value(QStringLiteral("needsPreviousOutput")).toBool()) ++handoffs;

    QString detail;
    if (kind == QStringLiteral("action")) {
      detail = step.value(QStringLiteral("action"))
                   .toObject()
                   .value(QStringLiteral("selected"))
                   .toObject()
                   .value(QStringLiteral("name"))
                   .toString(QStringLiteral("unresolved action"));
    } else if (kind == QStringLiteral("transform")) {
      detail = step.value(QStringLiteral("transform"))
                   .toObject()
                   .value(QStringLiteral("operation"))
                   .toString(QStringLiteral("transform"));
    } else {
      detail = QStringLiteral("needs manual resolution");
    }

    outline << QStringLiteral("%1. [%2] %3")
                   .arg(step.value(QStringLiteral("index")).toInt())
                   .arg(kind.toUpper(), detail);
    outline << QStringLiteral("   %1").arg(instruction);
    if (step.value(QStringLiteral("needsPreviousOutput")).toBool()) {
      outline << QStringLiteral("   ↳ previous-output binding requires review");
    }
  }

  if (!bindings.isEmpty()) {
    outline << QString() << QStringLiteral("Proposed bindings");
    for (const QJsonValue &value : bindings) {
      const QJsonObject binding = value.toObject();
      outline << QStringLiteral("• %1%2 → %3.%4 [%5]")
                     .arg(binding.value(QStringLiteral("sourceStepId")).toString(),
                          binding.value(QStringLiteral("sourcePath")).toString(),
                          binding.value(QStringLiteral("targetStepId")).toString(),
                          binding.value(QStringLiteral("targetArgument")).toString(),
                          binding.value(QStringLiteral("coercion")).toString());
    }
  }

  outline << QString();
  for (const QJsonValue &reason : plan.value(QStringLiteral("reviewReasons")).toArray()) {
    if (reason.isString()) outline << QStringLiteral("• %1").arg(reason.toString());
  }
  outputView_->setPlainText(outline.join(QLatin1Char('\n')));

  const QString summary =
      QStringLiteral("%1 steps · %2 actions · %3 transforms · %4 unresolved · %5 handoffs · %6 bindings · %7 confidence")
          .arg(steps.size())
          .arg(actions)
          .arg(transforms)
          .arg(unresolved)
          .arg(handoffs)
          .arg(bindings.size())
          .arg(plan.value(QStringLiteral("confidence")).toString(QStringLiteral("low")));
  appendLog(QStringLiteral("Workflow Planner prepared %1; no execution occurred.").arg(summary));
  emit workflowPlanPrepared(summary, plan);
}

void MainWindow::reviewWorkflowStep(const QJsonObject &actionPlan) {
  if (actionPlan.value(QStringLiteral("selected")).toObject().isEmpty()) {
    emit actionPlanFailed(QStringLiteral("This workflow step does not contain a reviewable MCP action."));
    return;
  }
  applyActionPlan(actionPlan);
}
