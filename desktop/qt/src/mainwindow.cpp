#include "mainwindow.h"

#include "mcpbridgeprocess.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QLabel *sectionLabel(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setProperty("sectionTitle", true);
  return label;
}

QFrame *card(QWidget *parent) {
  auto *frame = new QFrame(parent);
  frame->setProperty("card", true);
  return frame;
}

QString jsonText(const QJsonValue &value) {
  if (value.isObject()) return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented));
  if (value.isArray()) return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented));
  if (value.isString()) return value.toString();
  if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  if (value.isDouble()) return QString::number(value.toDouble());
  if (value.isNull()) return QStringLiteral("null");
  return QStringLiteral("undefined");
}
}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), bridge_(new McpBridgeProcess(this)) {
  setWindowTitle(QStringLiteral("Superpower Desktop"));
  resize(1220, 780);
  setMinimumSize(980, 660);

  buildUi();
  applyStyle();
  connectSignals();
  setConnectedUi(false);
}

void MainWindow::buildUi() {
  auto *root = new QWidget(this);
  auto *rootLayout = new QHBoxLayout(root);
  rootLayout->setContentsMargins(18, 18, 18, 18);
  rootLayout->setSpacing(14);

  auto *connectionCard = card(root);
  connectionCard->setFixedWidth(300);
  auto *connectionLayout = new QVBoxLayout(connectionCard);
  connectionLayout->setContentsMargins(22, 22, 22, 22);
  connectionLayout->setSpacing(12);

  auto *brand = new QLabel(QStringLiteral("SUPERPOWER"), connectionCard);
  brand->setProperty("brand", true);
  auto *subtitle = new QLabel(QStringLiteral("Native MCP workspace"), connectionCard);
  subtitle->setProperty("muted", true);
  subtitle->setWordWrap(true);
  connectionLayout->addWidget(brand);
  connectionLayout->addWidget(subtitle);
  connectionLayout->addSpacing(12);

  connectionLayout->addWidget(sectionLabel(QStringLiteral("Connection"), connectionCard));
  endpointEdit_ = new QLineEdit(QStringLiteral("http://localhost:3000/mcp"), connectionCard);
  endpointEdit_->setPlaceholderText(QStringLiteral("https://server.example/mcp"));
  endpointEdit_->setClearButtonEnabled(true);
  connectionLayout->addWidget(new QLabel(QStringLiteral("MCP endpoint"), connectionCard));
  connectionLayout->addWidget(endpointEdit_);

  nodeEdit_ = new QLineEdit(QStringLiteral("node"), connectionCard);
  hostScriptEdit_ = new QLineEdit(findDefaultHostScript(), connectionCard);
  connectionLayout->addWidget(new QLabel(QStringLiteral("Node executable"), connectionCard));
  connectionLayout->addWidget(nodeEdit_);
  connectionLayout->addWidget(new QLabel(QStringLiteral("MCP host script"), connectionCard));
  connectionLayout->addWidget(hostScriptEdit_);

  connectionLayout->addSpacing(8);
  connectionLayout->addWidget(sectionLabel(QStringLiteral("Routing"), connectionCard));
  focusEdit_ = new QLineEdit(connectionCard);
  focusEdit_->setPlaceholderText(QStringLiteral("e.g. find files and summarize"));
  connectionLayout->addWidget(new QLabel(QStringLiteral("Task focus"), connectionCard));
  connectionLayout->addWidget(focusEdit_);

  policyCombo_ = new QComboBox(connectionCard);
  policyCombo_->addItems({QStringLiteral("audit"), QStringLiteral("guarded")});
  policyCombo_->setCurrentText(QStringLiteral("guarded"));
  connectionLayout->addWidget(new QLabel(QStringLiteral("Execution policy"), connectionCard));
  connectionLayout->addWidget(policyCombo_);

  connectionLayout->addStretch(1);
  auto *safety = new QLabel(
      QStringLiteral("Guarded mode asks before high-risk or destructive MCP actions. Credentials are not stored by this desktop shell."),
      connectionCard);
  safety->setProperty("muted", true);
  safety->setWordWrap(true);
  connectionLayout->addWidget(safety);

  statusLabel_ = new QLabel(QStringLiteral("Disconnected"), connectionCard);
  statusLabel_->setProperty("status", true);
  connectionLayout->addWidget(statusLabel_);

  connectButton_ = new QPushButton(QStringLiteral("Connect"), connectionCard);
  connectButton_->setProperty("primary", true);
  connectionLayout->addWidget(connectButton_);

  auto *splitter = new QSplitter(Qt::Horizontal, root);
  splitter->setChildrenCollapsible(false);

  auto *catalogCard = card(splitter);
  auto *catalogLayout = new QVBoxLayout(catalogCard);
  catalogLayout->setContentsMargins(18, 18, 18, 18);
  catalogLayout->setSpacing(10);
  auto *catalogHeader = new QHBoxLayout();
  catalogHeader->addWidget(sectionLabel(QStringLiteral("Tools"), catalogCard));
  catalogHeader->addStretch(1);
  toolCountLabel_ = new QLabel(QStringLiteral("0"), catalogCard);
  toolCountLabel_->setProperty("muted", true);
  catalogHeader->addWidget(toolCountLabel_);
  catalogLayout->addLayout(catalogHeader);

  toolSearchEdit_ = new QLineEdit(catalogCard);
  toolSearchEdit_->setPlaceholderText(QStringLiteral("Filter routed tools"));
  toolSearchEdit_->setClearButtonEnabled(true);
  catalogLayout->addWidget(toolSearchEdit_);

  toolList_ = new QListWidget(catalogCard);
  toolList_->setAlternatingRowColors(false);
  catalogLayout->addWidget(toolList_, 1);

  refreshButton_ = new QPushButton(QStringLiteral("Refresh tools"), catalogCard);
  catalogLayout->addWidget(refreshButton_);

  auto *workCard = card(splitter);
  auto *workLayout = new QVBoxLayout(workCard);
  workLayout->setContentsMargins(22, 18, 22, 18);
  workLayout->setSpacing(10);

  auto *workHeader = new QHBoxLayout();
  auto *detailTitle = sectionLabel(QStringLiteral("Tool workspace"), workCard);
  workHeader->addWidget(detailTitle);
  workHeader->addStretch(1);
  statsButton_ = new QPushButton(QStringLiteral("Session stats"), workCard);
  workHeader->addWidget(statsButton_);
  workLayout->addLayout(workHeader);

  toolNameLabel_ = new QLabel(QStringLiteral("Select a tool"), workCard);
  toolNameLabel_->setProperty("toolName", true);
  workLayout->addWidget(toolNameLabel_);

  toolDescriptionLabel_ = new QLabel(QStringLiteral("Connect to an MCP server to load the routed catalog."), workCard);
  toolDescriptionLabel_->setWordWrap(true);
  toolDescriptionLabel_->setProperty("muted", true);
  workLayout->addWidget(toolDescriptionLabel_);

  workLayout->addWidget(new QLabel(QStringLiteral("Input schema"), workCard));
  schemaView_ = new QPlainTextEdit(workCard);
  schemaView_->setReadOnly(true);
  schemaView_->setMaximumHeight(150);
  schemaView_->setPlainText(QStringLiteral("{}"));
  workLayout->addWidget(schemaView_);

  workLayout->addWidget(new QLabel(QStringLiteral("Arguments (JSON)"), workCard));
  argumentsEdit_ = new QPlainTextEdit(workCard);
  argumentsEdit_->setPlaceholderText(QStringLiteral("{}"));
  argumentsEdit_->setPlainText(QStringLiteral("{}"));
  argumentsEdit_->setMinimumHeight(120);
  workLayout->addWidget(argumentsEdit_);

  runButton_ = new QPushButton(QStringLiteral("Run tool"), workCard);
  runButton_->setProperty("primary", true);
  workLayout->addWidget(runButton_);

  workLayout->addWidget(new QLabel(QStringLiteral("Activity"), workCard));
  outputView_ = new QPlainTextEdit(workCard);
  outputView_->setReadOnly(true);
  outputView_->setPlaceholderText(QStringLiteral("Tool results and bridge messages appear here."));
  workLayout->addWidget(outputView_, 1);

  splitter->addWidget(catalogCard);
  splitter->addWidget(workCard);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({320, 600});

  rootLayout->addWidget(connectionCard);
  rootLayout->addWidget(splitter, 1);
  setCentralWidget(root);
}

void MainWindow::applyStyle() {
  qApp->setStyleSheet(QStringLiteral(R"(
    QMainWindow, QWidget {
      background: #f6f6f6;
      color: #111111;
      font-size: 13px;
    }
    QFrame[card="true"] {
      background: #ffffff;
      border: 1px solid #dddddd;
      border-radius: 12px;
    }
    QLabel[brand="true"] {
      font-size: 22px;
      font-weight: 800;
      letter-spacing: 2px;
      color: #000000;
    }
    QLabel[sectionTitle="true"] {
      font-size: 12px;
      font-weight: 700;
      color: #303030;
    }
    QLabel[toolName="true"] {
      font-size: 22px;
      font-weight: 700;
      padding-top: 4px;
      color: #000000;
    }
    QLabel[muted="true"] {
      color: #737373;
    }
    QLabel[status="true"] {
      background: #f2f2f2;
      border: 1px solid #d9d9d9;
      border-radius: 8px;
      padding: 8px 10px;
      font-weight: 600;
      color: #202020;
    }
    QLineEdit, QComboBox, QPlainTextEdit, QListWidget {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d6d6d6;
      border-radius: 8px;
      padding: 7px 9px;
      selection-background-color: #111111;
      selection-color: #ffffff;
    }
    QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus, QListWidget:focus {
      border: 1px solid #111111;
    }
    QLineEdit:disabled, QComboBox:disabled, QPlainTextEdit:disabled {
      background: #f5f5f5;
      color: #8a8a8a;
      border-color: #e2e2e2;
    }
    QComboBox::drop-down {
      border: none;
      width: 24px;
    }
    QPlainTextEdit {
      font-family: "Cascadia Code", "Consolas", monospace;
      font-size: 12px;
      background: #fcfcfc;
    }
    QListWidget {
      padding: 4px;
      outline: none;
    }
    QListWidget::item {
      border-radius: 7px;
      padding: 9px 8px;
      margin: 2px 0;
      color: #111111;
    }
    QListWidget::item:hover {
      background: #f1f1f1;
    }
    QListWidget::item:selected {
      background: #111111;
      color: #ffffff;
    }
    QPushButton {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d4d4d4;
      border-radius: 8px;
      padding: 8px 12px;
      font-weight: 600;
    }
    QPushButton:hover {
      background: #f0f0f0;
      border-color: #bbbbbb;
    }
    QPushButton:pressed {
      background: #e7e7e7;
    }
    QPushButton:disabled {
      color: #9a9a9a;
      background: #f3f3f3;
      border-color: #e2e2e2;
    }
    QPushButton[primary="true"] {
      background: #000000;
      color: #ffffff;
      border: 1px solid #000000;
      padding: 10px 12px;
    }
    QPushButton[primary="true"]:hover {
      background: #202020;
      border-color: #202020;
    }
    QPushButton[primary="true"]:pressed {
      background: #333333;
      border-color: #333333;
    }
    QPushButton[primary="true"]:disabled {
      background: #aaaaaa;
      border-color: #aaaaaa;
      color: #ffffff;
    }
    QSplitter::handle {
      background: transparent;
      width: 10px;
    }
    QScrollBar:vertical {
      width: 10px;
      background: transparent;
      margin: 0;
    }
    QScrollBar::handle:vertical {
      background: #c8c8c8;
      border-radius: 5px;
      min-height: 24px;
    }
    QScrollBar::handle:vertical:hover {
      background: #a8a8a8;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0;
    }
  )"));
}

void MainWindow::connectSignals() {
  connect(connectButton_, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
  connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshTools);
  connect(runButton_, &QPushButton::clicked, this, &MainWindow::runSelectedTool);
  connect(toolList_, &QListWidget::currentItemChanged, this, [this]() { showSelectedTool(); });
  connect(toolSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::filterTools);
  connect(focusEdit_, &QLineEdit::editingFinished, this, &MainWindow::applyFocus);
  connect(policyCombo_, &QComboBox::currentTextChanged, this, [this](const QString &mode) {
    if (bridge_->isRunning()) bridge_->sendRequest(QStringLiteral("policy"), {{QStringLiteral("mode"), mode}});
  });
  connect(statsButton_, &QPushButton::clicked, this, [this]() {
    if (bridge_->isRunning()) bridge_->sendRequest(QStringLiteral("stats"));
  });

  connect(bridge_, &McpBridgeProcess::bridgeReady, this, [this](const QString &transport) {
    setConnectedUi(true);
    setStatus(QStringLiteral("Connected via %1").arg(transport), true);
    appendLog(QStringLiteral("MCP bridge ready."));
    refreshTools();
  });
  connect(bridge_, &McpBridgeProcess::responseReceived, this, &MainWindow::handleResponse);
  connect(bridge_, &McpBridgeProcess::requestFailed, this, &MainWindow::handleRequestFailure);
  connect(bridge_, &McpBridgeProcess::logLine, this, &MainWindow::appendLog);
  connect(bridge_, &McpBridgeProcess::processError, this, [this](const QString &message) {
    appendLog(QStringLiteral("Bridge error: %1").arg(message));
    setStatus(QStringLiteral("Connection error"), false);
  });
  connect(bridge_, &McpBridgeProcess::processExited, this, [this](int exitCode, QProcess::ExitStatus) {
    setConnectedUi(false);
    setStatus(QStringLiteral("Disconnected"), false);
    appendLog(QStringLiteral("Bridge exited with code %1.").arg(exitCode));
  });
}

void MainWindow::setConnectedUi(bool connected) {
  connectButton_->setText(connected ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
  endpointEdit_->setEnabled(!connected);
  nodeEdit_->setEnabled(!connected);
  hostScriptEdit_->setEnabled(!connected);
  refreshButton_->setEnabled(connected);
  statsButton_->setEnabled(connected);
  focusEdit_->setEnabled(connected);
  policyCombo_->setEnabled(connected);
  runButton_->setEnabled(connected && toolList_->currentItem() != nullptr);
}

void MainWindow::connectOrDisconnect() {
  if (bridge_->isRunning()) {
    setStatus(QStringLiteral("Disconnecting..."), false);
    bridge_->stop();
    return;
  }

  const QString endpoint = endpointEdit_->text().trimmed();
  const QString node = nodeEdit_->text().trimmed();
  const QString hostScript = hostScriptEdit_->text().trimmed();
  if (endpoint.isEmpty() || node.isEmpty() || hostScript.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Missing connection settings"),
                         QStringLiteral("Endpoint, Node executable, and MCP host script are required."));
    return;
  }
  if (!endpoint.startsWith(QStringLiteral("http://")) && !endpoint.startsWith(QStringLiteral("https://"))) {
    QMessageBox::warning(this, QStringLiteral("Unsupported endpoint"),
                         QStringLiteral("The first desktop build supports Streamable HTTP endpoints (http/https)."));
    return;
  }
  if (!QFileInfo::exists(hostScript)) {
    QMessageBox::warning(this, QStringLiteral("MCP host not built"),
                         QStringLiteral("The host script was not found. Build it first with:\n\npnpm -F @superpower/mcp-host build"));
    return;
  }

  outputView_->clear();
  setStatus(QStringLiteral("Connecting..."), false);
  connectButton_->setEnabled(false);
  bridge_->start(node, hostScript, endpoint, focusEdit_->text(), policyCombo_->currentText());
  connectButton_->setEnabled(true);
}

void MainWindow::refreshTools() {
  if (!bridge_->isRunning()) return;
  bridge_->sendRequest(QStringLiteral("tools"));
}

void MainWindow::applyFocus() {
  if (!bridge_->isRunning()) return;
  bridge_->sendRequest(QStringLiteral("focus"), {{QStringLiteral("focus"), focusEdit_->text()}});
}

void MainWindow::runSelectedTool() {
  auto *item = toolList_->currentItem();
  if (!item || !bridge_->isRunning()) return;

  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(argumentsEdit_->toPlainText().toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    QMessageBox::warning(this, QStringLiteral("Invalid arguments"),
                         QStringLiteral("Tool arguments must be one valid JSON object."));
    return;
  }

  const QString toolName = item->data(Qt::UserRole).toString();
  const QJsonObject arguments = document.object();
  const QString id = bridge_->sendRequest(
      QStringLiteral("call"), {{QStringLiteral("toolName"), toolName},
                                {QStringLiteral("args"), arguments},
                                {QStringLiteral("approve"), false}});
  if (id.isEmpty()) return;

  pendingCalls_.insert(id, {toolName, arguments});
  runButton_->setEnabled(false);
  appendLog(QStringLiteral("Calling %1...").arg(toolName));
}

void MainWindow::showSelectedTool() {
  auto *item = toolList_->currentItem();
  if (!item) {
    toolNameLabel_->setText(QStringLiteral("Select a tool"));
    toolDescriptionLabel_->clear();
    schemaView_->setPlainText(QStringLiteral("{}"));
    argumentsEdit_->setPlainText(QStringLiteral("{}"));
    runButton_->setEnabled(false);
    return;
  }

  const QString name = item->data(Qt::UserRole).toString();
  const QJsonObject tool = toolsByName_.value(name);
  const QJsonObject schema = tool.value(QStringLiteral("inputSchema")).toObject();
  toolNameLabel_->setText(name);
  toolDescriptionLabel_->setText(tool.value(QStringLiteral("description")).toString());
  schemaView_->setPlainText(QString::fromUtf8(QJsonDocument(schema).toJson(QJsonDocument::Indented)));
  argumentsEdit_->setPlainText(
      QString::fromUtf8(QJsonDocument(argumentTemplate(schema)).toJson(QJsonDocument::Indented)));
  runButton_->setEnabled(bridge_->isRunning());
}

void MainWindow::filterTools(const QString &query) {
  const QString needle = query.trimmed();
  for (int i = 0; i < toolList_->count(); ++i) {
    auto *item = toolList_->item(i);
    const QString name = item->data(Qt::UserRole).toString();
    const QJsonObject tool = toolsByName_.value(name);
    const QString haystack = name + QLatin1Char(' ') + tool.value(QStringLiteral("description")).toString();
    item->setHidden(!needle.isEmpty() && !haystack.contains(needle, Qt::CaseInsensitive));
  }
}

void MainWindow::populateTools(const QJsonObject &result) {
  toolList_->clear();
  toolsByName_.clear();

  const QJsonArray tools = result.value(QStringLiteral("tools")).toArray();
  for (const QJsonValue &value : tools) {
    const QJsonObject tool = value.toObject();
    const QString name = tool.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) continue;
    toolsByName_.insert(name, tool);
    auto *item = new QListWidgetItem(name, toolList_);
    item->setData(Qt::UserRole, name);
    item->setToolTip(tool.value(QStringLiteral("description")).toString());
  }

  const int omitted = result.value(QStringLiteral("omitted")).toInt();
  toolCountLabel_->setText(omitted > 0 ? QStringLiteral("%1 shown / %2 omitted").arg(tools.size()).arg(omitted)
                                       : QStringLiteral("%1 shown").arg(tools.size()));
  filterTools(toolSearchEdit_->text());
  if (toolList_->count() > 0) toolList_->setCurrentRow(0);
  appendLog(QStringLiteral("Loaded %1 routed tool(s).").arg(tools.size()));
}

void MainWindow::handleResponse(const QString &id, const QString &method, const QJsonValue &result) {
  if (method == QStringLiteral("tools")) {
    populateTools(result.toObject());
    return;
  }
  if (method == QStringLiteral("focus")) {
    appendLog(QStringLiteral("Task focus updated; refreshing routed tools."));
    refreshTools();
    return;
  }
  if (method == QStringLiteral("policy")) {
    appendLog(QStringLiteral("Execution policy updated."));
    return;
  }
  if (method == QStringLiteral("stats")) {
    appendOutput(QStringLiteral("Session telemetry"), result);
    return;
  }
  if (method == QStringLiteral("call")) {
    const PendingCall pending = pendingCalls_.take(id);
    appendOutput(QStringLiteral("Result: %1").arg(pending.toolName), result);
    runButton_->setEnabled(bridge_->isRunning() && toolList_->currentItem() != nullptr);
  }
}

void MainWindow::handleRequestFailure(const QString &id, const QString &method, const QString &code,
                                      const QString &message, const QJsonObject &details) {
  if (method == QStringLiteral("call") && code == QStringLiteral("confirmation_required")) {
    const PendingCall pending = pendingCalls_.take(id);
    const QString risk = details.value(QStringLiteral("risk")).toString();
    const QJsonArray reasons = details.value(QStringLiteral("reasons")).toArray();
    QStringList reasonText;
    for (const QJsonValue &reason : reasons) reasonText << reason.toString();

    const QString body = QStringLiteral("Tool: %1\nRisk: %2\n\n%3\n\nAllow this action?")
                             .arg(pending.toolName, risk, reasonText.join(QStringLiteral("\n")));
    const auto decision = QMessageBox::warning(this, QStringLiteral("Guarded MCP action"), body,
                                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (decision == QMessageBox::Yes) {
      const QString retryId = bridge_->sendRequest(
          QStringLiteral("call"), {{QStringLiteral("toolName"), pending.toolName},
                                    {QStringLiteral("args"), pending.arguments},
                                    {QStringLiteral("approve"), true}});
      if (!retryId.isEmpty()) pendingCalls_.insert(retryId, pending);
    } else {
      appendLog(QStringLiteral("Action cancelled by user."));
      runButton_->setEnabled(true);
    }
    return;
  }

  if (method == QStringLiteral("call")) {
    pendingCalls_.remove(id);
    runButton_->setEnabled(bridge_->isRunning() && toolList_->currentItem() != nullptr);
  }
  appendLog(QStringLiteral("%1: %2").arg(code, message));
}

void MainWindow::appendOutput(const QString &heading, const QJsonValue &value) {
  outputView_->appendPlainText(QStringLiteral("\n%1\n%2").arg(heading, jsonText(value)));
  outputView_->verticalScrollBar()->setValue(outputView_->verticalScrollBar()->maximum());
}

void MainWindow::appendLog(const QString &line) {
  if (line.trimmed().isEmpty()) return;
  outputView_->appendPlainText(QStringLiteral("[desktop] %1").arg(line.trimmed()));
  outputView_->verticalScrollBar()->setValue(outputView_->verticalScrollBar()->maximum());
}

void MainWindow::setStatus(const QString &text, bool connected) {
  statusLabel_->setText(text);
  statusLabel_->setStyleSheet(
      connected ? QStringLiteral("background:#111111;border:1px solid #111111;color:#ffffff;")
                : QStringLiteral("background:#f2f2f2;border:1px solid #d9d9d9;color:#202020;"));
}

QString MainWindow::findDefaultHostScript() {
  const QStringList startingPoints{QDir::currentPath(), QCoreApplication::applicationDirPath()};
  for (const QString &start : startingPoints) {
    QDir dir(start);
    for (int level = 0; level < 7; ++level) {
      const QString candidate = dir.filePath(QStringLiteral("packages/mcp-host/dist/cli.js"));
      if (QFileInfo::exists(candidate)) return QDir::toNativeSeparators(QFileInfo(candidate).absoluteFilePath());
      if (!dir.cdUp()) break;
    }
  }
  return QDir::toNativeSeparators(QStringLiteral("packages/mcp-host/dist/cli.js"));
}

QJsonValue MainWindow::sampleValueForSchema(const QJsonObject &schema) {
  const QString type = schema.value(QStringLiteral("type")).toString();
  if (type == QStringLiteral("string")) return QString();
  if (type == QStringLiteral("number") || type == QStringLiteral("integer")) return 0;
  if (type == QStringLiteral("boolean")) return false;
  if (type == QStringLiteral("array")) return QJsonArray();
  if (type == QStringLiteral("object")) return argumentTemplate(schema);

  const QJsonArray enumValues = schema.value(QStringLiteral("enum")).toArray();
  if (!enumValues.isEmpty()) return enumValues.first();
  return QJsonValue();
}

QJsonObject MainWindow::argumentTemplate(const QJsonObject &schema) {
  QJsonObject result;
  const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
  const QJsonArray required = schema.value(QStringLiteral("required")).toArray();
  for (const QJsonValue &requiredName : required) {
    const QString name = requiredName.toString();
    if (name.isEmpty()) continue;
    result.insert(name, sampleValueForSchema(properties.value(name).toObject()));
  }
  return result;
}