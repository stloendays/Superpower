#include "mainwindow.h"

#include "mcpbridgeprocess.h"

#include <QComboBox>
#include <QLabel>

bool MainWindow::isMcpConnected() const {
  return bridge_ && bridge_->isRunning();
}

QString MainWindow::mcpStatusText() const {
  if (headerConnectionLabel_ && !headerConnectionLabel_->text().trimmed().isEmpty()) {
    return headerConnectionLabel_->text();
  }
  return isMcpConnected() ? QStringLiteral("Active MCP connection")
                          : QStringLiteral("No active MCP connection");
}

int MainWindow::discoveredAppCount() const {
  return appCombo_ ? qMax(0, appCombo_->count() - 1) : 0;
}

int MainWindow::discoveredActionCount() const {
  return toolsByName_.size();
}

int MainWindow::runCount() const {
  return runHistory_.size();
}

QStringList MainWindow::recentRunSummaries(int limit) const {
  QStringList summaries;
  if (limit <= 0) return summaries;

  const int count = qMin(limit, runHistory_.size());
  summaries.reserve(count);
  for (int i = 0; i < count; ++i) {
    const RunRecord &record = runHistory_.at(i);
    const QString when = record.startedAt.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    const QString displayName = toolDisplayName(record.toolName, record.appName);
    summaries.append(QStringLiteral("%1   %2   %3 · %4\n%5 · %6 ms")
                         .arg(when, record.status.toUpper(), record.appName, displayName, record.serverName)
                         .arg(record.durationMs));
  }
  return summaries;
}
