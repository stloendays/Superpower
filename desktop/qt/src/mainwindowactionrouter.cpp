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
            if (method != QStringLiteral("plan") || !pendingActionPlanIds_.contains(id)) return;
            pendingActionPlanIds_.remove(id);
            applyActionPlan(result.toObject());
          });

  connect(bridge_, &McpBridgeProcess::requestFailed, this,
          [this](const QString &id, const QString &method, const QString &code,
                 const QString &message, const QJsonObject &) {
            if (method != QStringLiteral("plan") || !pendingActionPlanIds_.contains(id)) return;
            pendingActionPlanIds_.remove(id);
            const QString detail = QStringLiteral("Action Router failed · %1: %2").arg(code, message);
            appendLog(QStringLiteral("Action planning failed (%1).").arg(code));
            emit actionPlanFailed(detail);
          });

  connect(toolList_, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *current, QListWidgetItem *) {
            const QString selected = current ? current->data(Qt::UserRole).toString() : QString();
            runButton_->setText(!plannedToolName_.isEmpty() && selected == plannedToolName_
                                    ? QStringLiteral("Run reviewed action")
                                    : QStringLiteral("Run tool"));
          });
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
  const QString id = bridge_->sendRequest(QStringLiteral("plan"), {{QStringLiteral("query"), trimmed}});
  if (id.isEmpty()) {
    emit actionPlanFailed(QStringLiteral("The MCP bridge could not start action planning."));
    return;
  }

  pendingActionPlanIds_.insert(id);
  appendLog(QStringLiteral("Action Router requested a review plan from MCP Core."));
  emit actionPlanStarted(trimmed);
}

void MainWindow::applyActionPlan(const QJsonObject &plan) {
  const QJsonObject selected = plan.value(QStringLiteral("selected")).toObject();
  const QString toolName = selected.value(QStringLiteral("name")).toString().trimmed();
  if (toolName.isEmpty()) {
    plannedToolName_.clear();
    runButton_->setText(QStringLiteral("Run tool"));
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
  const QStringList draftedFields = stringArray(plan.value(QStringLiteral("draftedFields")));
  const QStringList missingRequired = stringArray(plan.value(QStringLiteral("missingRequired")));
  const QJsonObject policy = plan.value(QStringLiteral("policy")).toObject();
  const QString policyDecision = policy.value(QStringLiteral("decision")).toString(QStringLiteral("unknown"));
  const QString risk = policy.value(QStringLiteral("risk")).toString(QStringLiteral("unknown"));
  const QStringList reasons = stringArray(policy.value(QStringLiteral("reasons")));
  const QStringList sensitiveFields = stringArray(policy.value(QStringLiteral("sensitiveArgumentKeys")));

  QStringList alternatives;
  for (const QJsonValue &candidateValue : plan.value(QStringLiteral("candidates")).toArray()) {
    const QJsonObject candidate = candidateValue.toObject();
    const QString candidateName = candidate.value(QStringLiteral("name")).toString();
    if (!candidateName.isEmpty() && candidateName != toolName) alternatives << candidateName;
    if (alternatives.size() >= 3) break;
  }

  QStringList review;
  review << QStringLiteral("Action Router proposal — review before execution") << QString();
  review << QStringLiteral("Intent: %1").arg(query);
  review << QStringLiteral("Matched: %1 · %2").arg(appName, displayName);
  review << QStringLiteral("Raw MCP tool: %1").arg(toolName);
  review << QStringLiteral("Confidence: %1").arg(confidence.toUpper());
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
  review << QString()
         << QStringLiteral("Review the parameters above. Nothing has run yet. Press “Run reviewed action” to continue through the existing guarded execution policy.");

  outputView_->setPlainText(review.join(QLatin1Char('\n')));
  outputView_->verticalScrollBar()->setValue(0);
  appendLog(QStringLiteral("Action Router prepared %1 for review; no execution occurred.").arg(toolName));
  emit actionPlanPrepared(QStringLiteral("%1 · %2 · %3 confidence").arg(appName, displayName, confidence));
}
