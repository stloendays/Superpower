#include "mainwindow.h"

#include "mcpbridgeprocess.h"

#include <QJsonArray>
#include <QProcess>

void MainWindow::ensureWorkflowPlannerConnections() {
  if (workflowPlannerSignalsConnected_) return;
  workflowPlannerSignalsConnected_ = true;

  connect(bridge_, &McpBridgeProcess::responseReceived, this,
          [this](const QString &id, const QString &method, const QJsonValue &result) {
            if (method != QStringLiteral("workflowPlan") || !pendingWorkflowPlanIds_.contains(id)) return;
            pendingWorkflowPlanIds_.remove(id);
            if (id != latestWorkflowPlanId_) return;
            latestWorkflowPlanId_.clear();
            applyWorkflowPlan(result.toObject());
          });

  connect(bridge_, &McpBridgeProcess::requestFailed, this,
          [this](const QString &id, const QString &method, const QString &code,
                 const QString &message, const QJsonObject &) {
            if (method != QStringLiteral("workflowPlan") || !pendingWorkflowPlanIds_.contains(id)) return;
            pendingWorkflowPlanIds_.remove(id);
            if (id != latestWorkflowPlanId_) return;
            latestWorkflowPlanId_.clear();
            const QString detail = QStringLiteral("Workflow Planner failed · %1: %2").arg(code, message);
            appendLog(QStringLiteral("Workflow planning failed (%1).").arg(code));
            emit workflowPlanFailed(detail);
          });

  connect(bridge_, &McpBridgeProcess::processExited, this,
          [this](int, QProcess::ExitStatus) {
            const bool planningWasPending = !latestWorkflowPlanId_.isEmpty();
            pendingWorkflowPlanIds_.clear();
            latestWorkflowPlanId_.clear();
            if (planningWasPending) {
              emit workflowPlanFailed(QStringLiteral("Workflow Planner stopped because the MCP connection closed."));
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
  latestWorkflowPlanId_.clear();
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
  QStringList outline;
  outline << QStringLiteral("Workflow Planner proposal — review every step before execution") << QString();
  outline << QStringLiteral("Intent: %1").arg(plan.value(QStringLiteral("query")).toString());
  outline << QStringLiteral("Confidence: %1")
                 .arg(plan.value(QStringLiteral("confidence")).toString(QStringLiteral("low")).toUpper());
  outline << QStringLiteral("Auto execution: DISABLED") << QString();

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

  outline << QString();
  for (const QJsonValue &reason : plan.value(QStringLiteral("reviewReasons")).toArray()) {
    if (reason.isString()) outline << QStringLiteral("• %1").arg(reason.toString());
  }
  outputView_->setPlainText(outline.join(QLatin1Char('\n')));

  const QString summary =
      QStringLiteral("%1 steps · %2 actions · %3 transforms · %4 unresolved · %5 handoffs · %6 confidence")
          .arg(steps.size())
          .arg(actions)
          .arg(transforms)
          .arg(unresolved)
          .arg(handoffs)
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
