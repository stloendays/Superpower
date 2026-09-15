#include "mainwindow.h"

#include "mcpbridgeprocess.h"

#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSize>

namespace {
QString compactActionDescription(const QString &text, int maxLength = 84) {
  QString value = text.simplified();
  if (value.size() > maxLength) value = value.left(maxLength - 3) + QStringLiteral("...");
  return value;
}

QStringList stringArray(const QJsonValue &value) {
  QStringList values;
  for (const QJsonValue &item : value.toArray()) {
    if (item.isString() && !item.toString().trimmed().isEmpty()) values << item.toString();
  }
  return values;
}
}

void MainWindow::ensureActionRouterConnections() {
  if (actionRouterSignalsConnected_) return;
  actionRouterSignalsConnected_ = true;

  connect(bridge_, &McpBridgeProcess::responseReceived, this,
          [this](const QString &id, const QString &method, const QJsonValue &result) {
            if (method == QStringLiteral("plan")) {
              if (!pendingActionPlanIds_.contains(id)) return;
              pendingActionPlanIds_.remove(id);
              if (id != latestActionPlanId_) return;
              latestActionPlanId_.clear();
              applyActionPlan(result.toObject());
              return;
            }

            if (method == QStringLiteral("call") && !plannedToolName_.isEmpty()) {
              clearActionPlanReview();
            }
          });

  connect(bridge_, &McpBridgeProcess::requestFailed, this,
          [this](const QString &id, const QString &method, const QString &code,
                 const QString &message, const QJsonObject &) {
            if (method == QStringLiteral("plan")) {
              if (!pendingActionPlanIds_.contains(id)) return;
              pendingActionPlanIds_.remove(id);
              if (id != latestActionPlanId_) return;
              latestActionPlanId_.clear();
              clearActionPlanReview();
              const QString detail = QStringLiteral("Action Router failed · %1: %2").arg(code, message);
              appendLog(QStringLiteral("Action planning failed (%1).").arg(code));
              emit actionPlanFailed(detail);
              return;
            }

            if (method == QStringLiteral("call") && code != QStringLiteral("confirmation_required") &&
                !plannedToolName_.isEmpty()) {
              clearActionPlanReview();
            }
          });

  connect(bridge_, &McpBridgeProcess::processExited, this,
          [this](int, QProcess::ExitStatus) {
            const bool planningWasPending = !latestActionPlanId_.isEmpty();
            pendingActionPlanIds_.clear();
            latestActionPlanId_.clear();
            clearActionPlanReview();
            if (planningWasPending) {
              emit actionPlanFailed(QStringLiteral("Action Router stopped because the MCP connection closed."));
            }
          });

  connect(toolList_, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *current, QListWidgetItem *) {
            const QString selected = current ? current->data(Qt::UserRole).toString() : QString();
            if (!plannedToolName_.isEmpty() && selected != plannedToolName_) {
              clearActionPlanReview();
              return;
            }
            runButton_->setText(!plannedToolName_.isEmpty() && selected == plannedToolName_
                                    ? QStringLiteral("Run reviewed action")
                                    : QStringLiteral("Run tool"));
          });
}

void MainWindow::clearActionPlanReview() {
  plannedToolName_.clear();
  if (runButton_) runButton_->setText(QStringLiteral("Run tool"));

  for (QWidget *editor : fieldEditors_) {
    if (!editor) continue;
    editor->setStyleSheet(QString());
  }
}

void MainWindow::planAction(const QString &query) {
  const QString trimmed = query.trimmed();
  if (trimmed.isEmpty()) {
    emit actionPlanFailed(QStringLiteral("Describe the action you want to perform."));
    return;
  }
  if (!bridge_ || !bridge_->isRunning()) {
    emit actionPlanFailed(QStringLiteral("Connect an MCP server before routing an action."));
    return;
  }

  ensureActionRouterConnections();

  // Only the newest natural-language request may update the review workspace.
  // Older bridge responses are still allowed to finish, but they are ignored.
  pendingActionPlanIds_.clear();
  latestActionPlanId_.clear();
  clearActionPlanReview();

  const QString id = bridge_->sendRequest(QStringLiteral("plan"), {{QStringLiteral("query"), trimmed}});
  if (id.isEmpty()) {
    emit actionPlanFailed(QStringLiteral("The MCP bridge could not start action planning."));
    return;
  }

  latestActionPlanId_ = id;
  pendingActionPlanIds_.insert(id);
  appendLog(QStringLiteral("Action Router requested a review plan from MCP Core."));
  emit actionPlanStarted(trimmed);
}

void MainWindow::applyActionPlan(const QJsonObject &plan) {
  const QJsonObject selected = plan.value(QStringLiteral("selected")).toObject();
  const QString toolName = selected.value(QStringLiteral("name")).toString().trimmed();
  if (toolName.isEmpty()) {
    clearActionPlanReview();
    emit actionPlanFailed(
        QStringLiteral("No confident MCP action matched this request. Try naming the app or action more explicitly."));
    return;
  }

  const QString description = selected.value(QStringLiteral("description")).toString();
  const QJsonObject inputSchema = selected.value(QStringLiteral("inputSchema")).toObject();
  const QString appName = toolApp(toolName, description);
  const QString category = toolCategory(toolName, description);
  const QString displayName = toolDisplayName(toolName, appName);
  const QString summary = compactActionDescription(description);
  const QString subtitle = summary.isEmpty()
                               ? QStringLiteral("%1 · %2").arg(appName.toUpper(), category.toUpper())
                               : QStringLiteral("%1 · %2 · %3").arg(appName.toUpper(), category.toUpper(), summary);

  QJsonObject tool;
  tool.insert(QStringLiteral("name"), toolName);
  tool.insert(QStringLiteral("description"), description);
  tool.insert(QStringLiteral("inputSchema"), inputSchema);
  toolsByName_.insert(toolName, tool);

  QListWidgetItem *targetItem = nullptr;
  for (int row = 0; row < toolList_->count(); ++row) {
    auto *item = toolList_->item(row);
    if (item->data(Qt::UserRole).toString() == toolName) {
      targetItem = item;
      break;
    }
  }

  if (!targetItem) {
    targetItem = new QListWidgetItem(iconForCategory(category),
                                     QStringLiteral("%1\n%2").arg(displayName, subtitle), toolList_);
    targetItem->setData(Qt::UserRole, toolName);
    targetItem->setData(Qt::UserRole + 1, category);
    targetItem->setData(Qt::UserRole + 2,
                        displayName + QLatin1Char(' ') + toolName + QLatin1Char(' ') + description +
                            QLatin1Char(' ') + category + QLatin1Char(' ') + appName);
    targetItem->setData(Qt::UserRole + 3, appName);
    targetItem->setData(Qt::UserRole + 4, displayName);
    targetItem->setToolTip(
        QStringLiteral("%1 · %2\n%3\n\nRaw MCP tool: %4").arg(appName, category, description, toolName));
    targetItem->setSizeHint(QSize(0, 60));
  }

  clearActionPlanReview();
  plannedToolName_ = toolName;
  globalSearchEdit_->clear();
  rebuildAppFilter();
  appCombo_->setCurrentIndex(0);
  categoryCombo_->setCurrentIndex(0);
  applyToolFilters();
  toolList_->setCurrentItem(targetItem);
  showSelectedTool();
  applyArgumentsToForm(plan.value(QStringLiteral("arguments")).toObject());
  runButton_->setText(QStringLiteral("Run reviewed action"));

  const QString confidence = plan.value(QStringLiteral("confidence")).toString(QStringLiteral("low"));
  const QString query = plan.value(QStringLiteral("query")).toString();
  const QStringList matchedTerms = stringArray(selected.value(QStringLiteral("matchedTerms")));
  const QStringList draftedFields = stringArray(plan.value(QStringLiteral("draftedFields")));
  const QStringList missingRequired = stringArray(plan.value(QStringLiteral("missingRequired")));
  const QJsonObject policy = plan.value(QStringLiteral("policy")).toObject();
  const QString policyDecision = policy.value(QStringLiteral("decision")).toString(QStringLiteral("unknown"));
  const QString risk = policy.value(QStringLiteral("risk")).toString(QStringLiteral("unknown"));
  const QStringList reasons = stringArray(policy.value(QStringLiteral("reasons")));
  const QStringList sensitiveFields = stringArray(policy.value(QStringLiteral("sensitiveArgumentKeys")));

  QWidget *firstMissingEditor = nullptr;
  for (const QString &field : missingRequired) {
    QWidget *editor = fieldEditors_.value(field);
    if (!editor) continue;
    editor->setStyleSheet(QStringLiteral("border:1px solid #777777;background:#f4f4f4;"));
    const QString previousTip = editor->toolTip();
    editor->setToolTip(previousTip.isEmpty()
                           ? QStringLiteral("Action Router could not infer this required field. Review and complete it.")
                           : previousTip + QStringLiteral("\n\nAction Router could not infer this required field. Review and complete it."));
    if (!firstMissingEditor) firstMissingEditor = editor;
  }

  QStringList alternatives;
  for (const QJsonValue &candidateValue : plan.value(QStringLiteral("candidates")).toArray()) {
    const QJsonObject candidate = candidateValue.toObject();
    const QString candidateName = candidate.value(QStringLiteral("name")).toString();
    if (!candidateName.isEmpty() && candidateName != toolName) {
      alternatives << QStringLiteral("%1 (score %2)")
                          .arg(candidateName)
                          .arg(candidate.value(QStringLiteral("score")).toDouble(), 0, 'f', 0);
    }
    if (alternatives.size() >= 3) break;
  }

  QStringList review;
  review << QStringLiteral("Action Router proposal — review before execution") << QString();
  review << QStringLiteral("Intent: %1").arg(query);
  review << QStringLiteral("Matched: %1 · %2").arg(appName, displayName);
  review << QStringLiteral("Raw MCP tool: %1").arg(toolName);
  review << QStringLiteral("Confidence: %1").arg(confidence.toUpper());
  if (!matchedTerms.isEmpty()) {
    review << QStringLiteral("Matched terms: %1").arg(matchedTerms.join(QStringLiteral(", ")));
  }
  review << QStringLiteral("Drafted fields: %1")
                .arg(draftedFields.isEmpty() ? QStringLiteral("none") : draftedFields.join(QStringLiteral(", ")));
  review << QStringLiteral("Missing required: %1")
                .arg(missingRequired.isEmpty() ? QStringLiteral("none")
                                               : missingRequired.join(QStringLiteral(", ")));
  review << QStringLiteral("Policy preview: %1 · risk %2").arg(policyDecision.toUpper(), risk.toUpper());
  if (!reasons.isEmpty()) review << QStringLiteral("Policy reasons: %1").arg(reasons.join(QStringLiteral(" · ")));
  if (!sensitiveFields.isEmpty()) {
    review << QStringLiteral("Sensitive fields detected: %1").arg(sensitiveFields.join(QStringLiteral(", ")));
  }
  if (!alternatives.isEmpty()) {
    review << QStringLiteral("Other candidates: %1").arg(alternatives.join(QStringLiteral(", ")));
  }
  if (confidence == QStringLiteral("low")) {
    review << QStringLiteral("Router note: LOW confidence. Verify the selected action and alternatives carefully.");
  } else if (confidence == QStringLiteral("medium")) {
    review << QStringLiteral("Router note: MEDIUM confidence. Confirm the selected action before running it.");
  }
  if (!missingRequired.isEmpty()) {
    review << QStringLiteral("Required fields that could not be inferred are outlined in grey in the parameter form.");
  }
  if (policyDecision == QStringLiteral("confirm")) {
    review << QStringLiteral("Guarded policy will ask for explicit confirmation again before the MCP call executes.");
  }
  review << QString()
         << QStringLiteral("Review and edit the parameters above. Nothing has run yet. Press “Run reviewed action” to continue through the existing guarded execution policy.");

  outputView_->setPlainText(review.join(QLatin1Char('\n')));
  outputView_->verticalScrollBar()->setValue(0);
  if (firstMissingEditor) firstMissingEditor->setFocus();

  appendLog(QStringLiteral("Action Router prepared %1 for review; no execution occurred.").arg(toolName));
  emit actionPlanPrepared(
      QStringLiteral("%1 · %2 · %3 confidence · %4 risk").arg(appName, displayName, confidence, risk));
}
