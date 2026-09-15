#include "mainwindow.h"

#include "mcpbridgeprocess.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSize>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariant>
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
  return QString();
}

QString compactDescription(const QString &text) {
  QString value = text.simplified();
  if (value.size() > 78) value = value.left(75) + QStringLiteral("...");
  return value;
}
}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), bridge_(new McpBridgeProcess(this)) {
  setWindowTitle(QStringLiteral("Superpower Desktop"));
  resize(1420, 880);
  setMinimumSize(1120, 720);

  nodeProgram_ = findDefaultNodeProgram();
  hostScript_ = findDefaultHostScript();

  buildUi();
  applyStyle();
  connectSignals();
  updateConnectionForm();
  setConnectedUi(false);
}

void MainWindow::buildUi() {
  auto *root = new QWidget(this);
  auto *rootLayout = new QVBoxLayout(root);
  rootLayout->setContentsMargins(16, 16, 16, 16);
  rootLayout->setSpacing(12);

  auto *topBar = card(root);
  topBar->setProperty("topbar", true);
  auto *topLayout = new QHBoxLayout(topBar);
  topLayout->setContentsMargins(16, 12, 16, 12);
  topLayout->setSpacing(10);

  auto *logoMark = new QLabel(QStringLiteral("S"), topBar);
  logoMark->setProperty("logoMark", true);
  logoMark->setAlignment(Qt::AlignCenter);
  logoMark->setFixedSize(34, 34);
  topLayout->addWidget(logoMark);

  auto *brandBlock = new QWidget(topBar);
  auto *brandLayout = new QVBoxLayout(brandBlock);
  brandLayout->setContentsMargins(0, 0, 0, 0);
  brandLayout->setSpacing(0);
  auto *brand = new QLabel(QStringLiteral("SUPERPOWER"), brandBlock);
  brand->setProperty("brand", true);
  auto *desktopLabel = new QLabel(QStringLiteral("Desktop workspace"), brandBlock);
  desktopLabel->setProperty("muted", true);
  brandLayout->addWidget(brand);
  brandLayout->addWidget(desktopLabel);
  topLayout->addWidget(brandBlock);

  headerConnectionLabel_ = new QLabel(QStringLiteral("No active connection"), topBar);
  headerConnectionLabel_->setProperty("headerConnection", true);
  topLayout->addWidget(headerConnectionLabel_);
  topLayout->addStretch(1);

  globalSearchEdit_ = new QLineEdit(topBar);
  globalSearchEdit_->setPlaceholderText(QStringLiteral("Search tools or type >settings, >logs, >about  ·  Ctrl+K"));
  globalSearchEdit_->setClearButtonEnabled(true);
  globalSearchEdit_->setMinimumWidth(330);
  globalSearchEdit_->setMaximumWidth(520);
  topLayout->addWidget(globalSearchEdit_, 1);

  settingsButton_ = new QPushButton(QStringLiteral("Settings"), topBar);
  logsButton_ = new QPushButton(QStringLiteral("Logs"), topBar);
  aboutButton_ = new QPushButton(QStringLiteral("About"), topBar);
  topLayout->addWidget(settingsButton_);
  topLayout->addWidget(logsButton_);
  topLayout->addWidget(aboutButton_);
  rootLayout->addWidget(topBar);

  auto *bodyLayout = new QHBoxLayout();
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(12);

  auto *connectionsCard = card(root);
  connectionsCard->setFixedWidth(300);
  auto *connectionLayout = new QVBoxLayout(connectionsCard);
  connectionLayout->setContentsMargins(20, 20, 20, 20);
  connectionLayout->setSpacing(10);

  auto *connectionsTitle = new QLabel(QStringLiteral("Servers / Connections"), connectionsCard);
  connectionsTitle->setProperty("panelTitle", true);
  connectionLayout->addWidget(connectionsTitle);
  auto *connectionsSubtitle = new QLabel(
      QStringLiteral("Connect a remote MCP endpoint or launch a local stdio server."), connectionsCard);
  connectionsSubtitle->setProperty("muted", true);
  connectionsSubtitle->setWordWrap(true);
  connectionLayout->addWidget(connectionsSubtitle);
  connectionLayout->addSpacing(4);

  connectionSummaryLabel_ = new QLabel(QStringLiteral("No server selected"), connectionsCard);
  connectionSummaryLabel_->setProperty("connectionSummary", true);
  connectionSummaryLabel_->setWordWrap(true);
  connectionLayout->addWidget(connectionSummaryLabel_);

  connectionLayout->addWidget(sectionLabel(QStringLiteral("New connection"), connectionsCard));
  transportCombo_ = new QComboBox(connectionsCard);
  transportCombo_->addItem(QStringLiteral("Streamable HTTP"), QStringLiteral("http"));
  transportCombo_->addItem(QStringLiteral("Local stdio"), QStringLiteral("stdio"));
  connectionLayout->addWidget(new QLabel(QStringLiteral("Transport"), connectionsCard));
  connectionLayout->addWidget(transportCombo_);

  httpConnectionWidget_ = new QWidget(connectionsCard);
  auto *httpLayout = new QVBoxLayout(httpConnectionWidget_);
  httpLayout->setContentsMargins(0, 0, 0, 0);
  httpLayout->setSpacing(6);
  endpointEdit_ = new QLineEdit(QStringLiteral("http://localhost:3000/mcp"), httpConnectionWidget_);
  endpointEdit_->setPlaceholderText(QStringLiteral("https://server.example/mcp"));
  endpointEdit_->setClearButtonEnabled(true);
  httpLayout->addWidget(new QLabel(QStringLiteral("MCP endpoint"), httpConnectionWidget_));
  httpLayout->addWidget(endpointEdit_);
  connectionLayout->addWidget(httpConnectionWidget_);

  stdioConnectionWidget_ = new QWidget(connectionsCard);
  auto *stdioLayout = new QVBoxLayout(stdioConnectionWidget_);
  stdioLayout->setContentsMargins(0, 0, 0, 0);
  stdioLayout->setSpacing(6);
  stdioCommandEdit_ = new QLineEdit(stdioConnectionWidget_);
  stdioCommandEdit_->setPlaceholderText(QStringLiteral("npx, node, python, or server executable"));
  stdioArgsEdit_ = new QLineEdit(QStringLiteral("[]"), stdioConnectionWidget_);
  stdioArgsEdit_->setPlaceholderText(QStringLiteral("[\"server.js\", \"--port\", \"3000\"]"));
  stdioLayout->addWidget(new QLabel(QStringLiteral("Server command"), stdioConnectionWidget_));
  stdioLayout->addWidget(stdioCommandEdit_);
  stdioLayout->addWidget(new QLabel(QStringLiteral("Arguments"), stdioConnectionWidget_));
  stdioLayout->addWidget(stdioArgsEdit_);
  connectionLayout->addWidget(stdioConnectionWidget_);

  connectionLayout->addStretch(1);
  auto *settingsHint = new QLabel(
      QStringLiteral("Runtime, routing, and guarded-execution settings are available from Settings."), connectionsCard);
  settingsHint->setProperty("muted", true);
  settingsHint->setWordWrap(true);
  connectionLayout->addWidget(settingsHint);

  statusLabel_ = new QLabel(QStringLiteral("Disconnected"), connectionsCard);
  statusLabel_->setProperty("status", true);
  connectionLayout->addWidget(statusLabel_);

  connectButton_ = new QPushButton(QStringLiteral("Connect"), connectionsCard);
  connectButton_->setProperty("primary", true);
  connectionLayout->addWidget(connectButton_);
  bodyLayout->addWidget(connectionsCard);

  auto *splitter = new QSplitter(Qt::Horizontal, root);
  splitter->setChildrenCollapsible(false);

  auto *catalogCard = card(splitter);
  catalogCard->setMinimumWidth(300);
  auto *catalogLayout = new QVBoxLayout(catalogCard);
  catalogLayout->setContentsMargins(18, 18, 18, 18);
  catalogLayout->setSpacing(10);

  auto *catalogHeader = new QHBoxLayout();
  auto *catalogTitle = new QLabel(QStringLiteral("Tools / Apps"), catalogCard);
  catalogTitle->setProperty("panelTitle", true);
  catalogHeader->addWidget(catalogTitle);
  catalogHeader->addStretch(1);
  toolCountLabel_ = new QLabel(QStringLiteral("0"), catalogCard);
  toolCountLabel_->setProperty("muted", true);
  catalogHeader->addWidget(toolCountLabel_);
  catalogLayout->addLayout(catalogHeader);

  categoryCombo_ = new QComboBox(catalogCard);
  categoryCombo_->addItem(QStringLiteral("All categories"), QStringLiteral("All"));
  categoryCombo_->addItem(QStringLiteral("Developer"), QStringLiteral("Developer"));
  categoryCombo_->addItem(QStringLiteral("Files"), QStringLiteral("Files"));
  categoryCombo_->addItem(QStringLiteral("Web"), QStringLiteral("Web"));
  categoryCombo_->addItem(QStringLiteral("Data"), QStringLiteral("Data"));
  categoryCombo_->addItem(QStringLiteral("Communication"), QStringLiteral("Communication"));
  categoryCombo_->addItem(QStringLiteral("General"), QStringLiteral("General"));
  catalogLayout->addWidget(categoryCombo_);

  toolList_ = new QListWidget(catalogCard);
  toolList_->setAlternatingRowColors(false);
  toolList_->setWordWrap(true);
  catalogLayout->addWidget(toolList_, 1);

  refreshButton_ = new QPushButton(QStringLiteral("Refresh tools"), catalogCard);
  catalogLayout->addWidget(refreshButton_);

  auto *workCard = card(splitter);
  auto *workLayout = new QVBoxLayout(workCard);
  workLayout->setContentsMargins(22, 18, 22, 18);
  workLayout->setSpacing(10);

  auto *workHeader = new QHBoxLayout();
  auto *workTitle = new QLabel(QStringLiteral("Tool Workspace"), workCard);
  workTitle->setProperty("panelTitle", true);
  workHeader->addWidget(workTitle);
  workHeader->addStretch(1);
  schemaButton_ = new QPushButton(QStringLiteral("Schema"), workCard);
  statsButton_ = new QPushButton(QStringLiteral("Session stats"), workCard);
  workHeader->addWidget(schemaButton_);
  workHeader->addWidget(statsButton_);
  workLayout->addLayout(workHeader);

  toolNameLabel_ = new QLabel(QStringLiteral("Select a tool"), workCard);
  toolNameLabel_->setProperty("toolName", true);
  workLayout->addWidget(toolNameLabel_);

  toolDescriptionLabel_ = new QLabel(QStringLiteral("Connect to an MCP server to load the routed catalog."), workCard);
  toolDescriptionLabel_->setWordWrap(true);
  toolDescriptionLabel_->setProperty("muted", true);
  workLayout->addWidget(toolDescriptionLabel_);

  auto *formHeader = new QHBoxLayout();
  formHeader->addWidget(sectionLabel(QStringLiteral("Parameters"), workCard));
  formHeader->addStretch(1);
  resetFormButton_ = new QPushButton(QStringLiteral("Reset form"), workCard);
  formHeader->addWidget(resetFormButton_);
  workLayout->addLayout(formHeader);

  formScrollArea_ = new QScrollArea(workCard);
  formScrollArea_->setWidgetResizable(true);
  formScrollArea_->setFrameShape(QFrame::NoFrame);
  formScrollArea_->setMinimumHeight(190);
  formScrollArea_->setMaximumHeight(300);
  formWidget_ = new QWidget(formScrollArea_);
  formWidget_->setProperty("formSurface", true);
  formLayout_ = new QFormLayout(formWidget_);
  formLayout_->setContentsMargins(14, 12, 14, 12);
  formLayout_->setHorizontalSpacing(16);
  formLayout_->setVerticalSpacing(10);
  formLayout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  formScrollArea_->setWidget(formWidget_);
  workLayout->addWidget(formScrollArea_);

  runButton_ = new QPushButton(QStringLiteral("Run tool"), workCard);
  runButton_->setProperty("primary", true);
  workLayout->addWidget(runButton_);

  workLayout->addWidget(sectionLabel(QStringLiteral("Execution Result"), workCard));
  outputView_ = new QPlainTextEdit(workCard);
  outputView_->setReadOnly(true);
  outputView_->setPlaceholderText(QStringLiteral("The latest tool result appears here."));
  outputView_->setPlainText(QStringLiteral("Select a tool to begin."));
  workLayout->addWidget(outputView_, 1);

  splitter->addWidget(catalogCard);
  splitter->addWidget(workCard);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({350, 760});
  bodyLayout->addWidget(splitter, 1);

  rootLayout->addLayout(bodyLayout, 1);
  setCentralWidget(root);
}

void MainWindow::applyStyle() {
  qApp->setStyleSheet(QStringLiteral(R"(
    QMainWindow, QWidget {
      background: #f6f6f6;
      color: #111111;
      font-size: 13px;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    QFrame[card="true"] {
      background: #ffffff;
      border: 1px solid #dddddd;
      border-radius: 12px;
    }
    QLabel[logoMark="true"] {
      background: #000000;
      color: #ffffff;
      border-radius: 9px;
      font-size: 18px;
      font-weight: 800;
    }
    QLabel[brand="true"] {
      font-size: 16px;
      font-weight: 800;
      letter-spacing: 1.4px;
      color: #000000;
    }
    QLabel[panelTitle="true"] {
      font-size: 17px;
      font-weight: 750;
      color: #0f0f0f;
    }
    QLabel[sectionTitle="true"] {
      font-size: 12px;
      font-weight: 700;
      color: #333333;
    }
    QLabel[toolName="true"] {
      font-size: 22px;
      font-weight: 750;
      color: #000000;
      padding-top: 2px;
    }
    QLabel[muted="true"] {
      color: #737373;
    }
    QLabel[headerConnection="true"] {
      background: #f3f3f3;
      border: 1px solid #dfdfdf;
      border-radius: 9px;
      padding: 7px 10px;
      color: #444444;
      font-weight: 600;
    }
    QLabel[connectionSummary="true"] {
      background: #fafafa;
      border: 1px solid #e2e2e2;
      border-radius: 9px;
      padding: 10px;
      color: #3a3a3a;
      font-weight: 600;
    }
    QLabel[status="true"] {
      background: #f2f2f2;
      border: 1px solid #d9d9d9;
      border-radius: 8px;
      padding: 8px 10px;
      font-weight: 650;
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
    QScrollArea {
      background: #ffffff;
      border: 1px solid #e0e0e0;
      border-radius: 9px;
    }
    QWidget[formSurface="true"] {
      background: #ffffff;
    }
    QListWidget {
      padding: 4px;
      outline: none;
    }
    QListWidget::item {
      border-radius: 8px;
      padding: 8px;
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
      font-weight: 650;
    }
    QPushButton:hover {
      background: #f0f0f0;
      border-color: #bdbdbd;
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
  connect(categoryCombo_, &QComboBox::currentIndexChanged, this, [this]() { applyToolFilters(); });
  connect(transportCombo_, &QComboBox::currentIndexChanged, this, [this]() { updateConnectionForm(); });
  connect(endpointEdit_, &QLineEdit::textChanged, this, [this]() { updateConnectionForm(); });
  connect(stdioCommandEdit_, &QLineEdit::textChanged, this, [this]() { updateConnectionForm(); });
  connect(globalSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::handleGlobalSearch);
  connect(globalSearchEdit_, &QLineEdit::returnPressed, this, &MainWindow::executeGlobalCommand);
  connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
  connect(logsButton_, &QPushButton::clicked, this, &MainWindow::openLogsDialog);
  connect(aboutButton_, &QPushButton::clicked, this, &MainWindow::openAboutDialog);
  connect(schemaButton_, &QPushButton::clicked, this, &MainWindow::openSchemaDialog);
  connect(resetFormButton_, &QPushButton::clicked, this, [this]() {
    auto *item = toolList_->currentItem();
    if (!item) return;
    const QJsonObject tool = toolsByName_.value(item->data(Qt::UserRole).toString());
    rebuildArgumentForm(tool.value(QStringLiteral("inputSchema")).toObject());
  });
  connect(statsButton_, &QPushButton::clicked, this, [this]() {
    if (bridge_->isRunning()) bridge_->sendRequest(QStringLiteral("stats"));
  });

  auto *commandShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
  connect(commandShortcut, &QShortcut::activated, this, [this]() {
    globalSearchEdit_->setFocus();
    globalSearchEdit_->selectAll();
  });

  connect(bridge_, &McpBridgeProcess::bridgeReady, this, [this](const QString &transport) {
    setConnectedUi(true);
    setStatus(QStringLiteral("Connected via %1").arg(transport), true);
    appendLog(QStringLiteral("MCP bridge ready via %1.").arg(transport));
    refreshTools();
  });
  connect(bridge_, &McpBridgeProcess::responseReceived, this, &MainWindow::handleResponse);
  connect(bridge_, &McpBridgeProcess::requestFailed, this, &MainWindow::handleRequestFailure);
  connect(bridge_, &McpBridgeProcess::logLine, this, &MainWindow::appendLog);
  connect(bridge_, &McpBridgeProcess::processError, this, [this](const QString &message) {
    appendLog(QStringLiteral("Bridge error: %1").arg(message));
    setStatus(QStringLiteral("Connection error"), false);
    connectButton_->setEnabled(true);
  });
  connect(bridge_, &McpBridgeProcess::processExited, this, [this](int exitCode, QProcess::ExitStatus) {
    setConnectedUi(false);
    setStatus(QStringLiteral("Disconnected"), false);
    appendLog(QStringLiteral("Bridge exited with code %1.").arg(exitCode));
  });
}

void MainWindow::setConnectedUi(bool connected) {
  connectButton_->setEnabled(true);
  connectButton_->setText(connected ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
  transportCombo_->setEnabled(!connected);
  endpointEdit_->setEnabled(!connected);
  stdioCommandEdit_->setEnabled(!connected);
  stdioArgsEdit_->setEnabled(!connected);
  refreshButton_->setEnabled(connected);
  statsButton_->setEnabled(connected);
  runButton_->setEnabled(connected && toolList_->currentItem() != nullptr);
  schemaButton_->setEnabled(toolList_->currentItem() != nullptr);
  resetFormButton_->setEnabled(toolList_->currentItem() != nullptr);
  updateConnectionForm();
}

void MainWindow::updateConnectionForm() {
  const bool useStdio = transportCombo_->currentData().toString() == QStringLiteral("stdio");
  httpConnectionWidget_->setVisible(!useStdio);
  stdioConnectionWidget_->setVisible(useStdio);
  connectionSummaryLabel_->setText(connectionSummary());
}

void MainWindow::connectOrDisconnect() {
  if (bridge_->isRunning()) {
    setStatus(QStringLiteral("Disconnecting..."), false);
    connectButton_->setEnabled(false);
    bridge_->stop();
    return;
  }

  if (nodeProgram_.trimmed().isEmpty() || hostScript_.trimmed().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Missing runtime settings"),
                         QStringLiteral("Open Settings and configure the Node executable and MCP host bridge."));
    return;
  }
  if (!QFileInfo::exists(hostScript_)) {
    QMessageBox::warning(this, QStringLiteral("MCP host not found"),
                         QStringLiteral("The host bridge was not found. Build it with:\n\npnpm -F @superpower/mcp-host build\n\nor use the packaged Windows build."));
    return;
  }

  outputView_->setPlainText(QStringLiteral("Connecting..."));
  setStatus(QStringLiteral("Connecting..."), false);
  connectButton_->setEnabled(false);

  const QString transport = transportCombo_->currentData().toString();
  if (transport == QStringLiteral("stdio")) {
    const QString command = stdioCommandEdit_->text().trimmed();
    if (command.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("Missing stdio command"),
                           QStringLiteral("Local stdio requires a server command."));
      connectButton_->setEnabled(true);
      return;
    }

    QStringList arguments;
    QString argumentError;
    if (!parseStdioArguments(stdioArgsEdit_->text(), &arguments, &argumentError)) {
      QMessageBox::warning(this, QStringLiteral("Invalid stdio arguments"), argumentError);
      connectButton_->setEnabled(true);
      return;
    }
    bridge_->startStdio(nodeProgram_, hostScript_, command, arguments, taskFocus_, policyMode_);
    return;
  }

  const QString endpoint = endpointEdit_->text().trimmed();
  if (endpoint.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Missing endpoint"),
                         QStringLiteral("Streamable HTTP requires an MCP endpoint."));
    connectButton_->setEnabled(true);
    return;
  }
  if (!endpoint.startsWith(QStringLiteral("http://")) && !endpoint.startsWith(QStringLiteral("https://"))) {
    QMessageBox::warning(this, QStringLiteral("Unsupported endpoint"),
                         QStringLiteral("Streamable HTTP endpoints must start with http:// or https://."));
    connectButton_->setEnabled(true);
    return;
  }

  bridge_->startHttp(nodeProgram_, hostScript_, endpoint, taskFocus_, policyMode_);
}

void MainWindow::refreshTools() {
  if (!bridge_->isRunning()) return;
  bridge_->sendRequest(QStringLiteral("tools"));
}

void MainWindow::runSelectedTool() {
  auto *item = toolList_->currentItem();
  if (!item || !bridge_->isRunning()) return;

  QJsonObject arguments;
  QString errorMessage;
  if (!collectFormArguments(&arguments, &errorMessage)) {
    QMessageBox::warning(this, QStringLiteral("Invalid parameters"), errorMessage);
    return;
  }

  const QString toolName = item->data(Qt::UserRole).toString();
  const QString id = bridge_->sendRequest(
      QStringLiteral("call"), {{QStringLiteral("toolName"), toolName},
                                {QStringLiteral("args"), arguments},
                                {QStringLiteral("approve"), false}});
  if (id.isEmpty()) return;

  pendingCalls_.insert(id, {toolName, arguments});
  runButton_->setEnabled(false);
  outputView_->setPlainText(QStringLiteral("Running %1...").arg(toolName));
  appendLog(QStringLiteral("Calling %1.").arg(toolName));
}

void MainWindow::showSelectedTool() {
  auto *item = toolList_->currentItem();
  if (!item) {
    toolNameLabel_->setText(QStringLiteral("Select a tool"));
    toolDescriptionLabel_->setText(QStringLiteral("Choose a tool or app from the catalog."));
    rebuildArgumentForm(QJsonObject());
    runButton_->setEnabled(false);
    schemaButton_->setEnabled(false);
    resetFormButton_->setEnabled(false);
    outputView_->setPlainText(QStringLiteral("Select a tool to begin."));
    return;
  }

  const QString name = item->data(Qt::UserRole).toString();
  const QJsonObject tool = toolsByName_.value(name);
  const QJsonObject schema = tool.value(QStringLiteral("inputSchema")).toObject();
  toolNameLabel_->setText(name);
  toolDescriptionLabel_->setText(tool.value(QStringLiteral("description")).toString());
  rebuildArgumentForm(schema);
  runButton_->setEnabled(bridge_->isRunning());
  schemaButton_->setEnabled(true);
  resetFormButton_->setEnabled(true);
  outputView_->setPlainText(QStringLiteral("Ready to run %1.").arg(name));
}

void MainWindow::applyToolFilters() {
  QString needle = globalSearchEdit_->text().trimmed();
  const bool commandMode = needle.startsWith(QLatin1Char('>'));
  if (commandMode) needle.clear();
  const QString selectedCategory = categoryCombo_->currentData().toString();

  int visible = 0;
  for (int i = 0; i < toolList_->count(); ++i) {
    auto *item = toolList_->item(i);
    const QString category = item->data(Qt::UserRole + 1).toString();
    const QString searchable = item->data(Qt::UserRole + 2).toString();
    const bool categoryMatch = selectedCategory == QStringLiteral("All") || category == selectedCategory;
    const bool textMatch = needle.isEmpty() || searchable.contains(needle, Qt::CaseInsensitive);
    const bool show = categoryMatch && textMatch;
    item->setHidden(!show);
    if (show) ++visible;
  }

  if (commandMode) {
    toolCountLabel_->setText(QStringLiteral("Command mode"));
  } else {
    toolCountLabel_->setText(QStringLiteral("%1 / %2").arg(visible).arg(toolList_->count()));
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

    const QString description = tool.value(QStringLiteral("description")).toString();
    const QString category = toolCategory(name, description);
    const QString summary = compactDescription(description);
    const QString subtitle = summary.isEmpty() ? category.toUpper()
                                                : QStringLiteral("%1 · %2").arg(category.toUpper(), summary);

    toolsByName_.insert(name, tool);
    auto *item = new QListWidgetItem(iconForCategory(category), QStringLiteral("%1\n%2").arg(name, subtitle), toolList_);
    item->setData(Qt::UserRole, name);
    item->setData(Qt::UserRole + 1, category);
    item->setData(Qt::UserRole + 2, name + QLatin1Char(' ') + description + QLatin1Char(' ') + category);
    item->setToolTip(description);
    item->setSizeHint(QSize(0, 58));
  }

  applyToolFilters();
  for (int i = 0; i < toolList_->count(); ++i) {
    if (!toolList_->item(i)->isHidden()) {
      toolList_->setCurrentRow(i);
      break;
    }
  }

  const int omitted = result.value(QStringLiteral("omitted")).toInt();
  appendLog(omitted > 0 ? QStringLiteral("Loaded %1 routed tools; %2 omitted by context budget.").arg(tools.size()).arg(omitted)
                        : QStringLiteral("Loaded %1 routed tools.").arg(tools.size()));
}

void MainWindow::rebuildArgumentForm(const QJsonObject &schema) {
  while (formLayout_->count() > 0) {
    QLayoutItem *item = formLayout_->takeAt(0);
    if (item->widget()) delete item->widget();
    delete item;
  }
  fieldEditors_.clear();
  fieldSchemas_.clear();
  requiredFields_.clear();

  const QJsonArray required = schema.value(QStringLiteral("required")).toArray();
  for (const QJsonValue &value : required) {
    if (value.isString()) requiredFields_.insert(value.toString());
  }

  const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
  if (properties.isEmpty()) {
    auto *emptyLabel = new QLabel(QStringLiteral("This tool does not require parameters."), formWidget_);
    emptyLabel->setProperty("muted", true);
    formLayout_->addRow(emptyLabel);
    return;
  }

  for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
    const QString name = it.key();
    const QJsonObject fieldSchema = it.value().toObject();
    const bool isRequired = requiredFields_.contains(name);

    auto *label = new QLabel(isRequired ? name + QStringLiteral(" *") : name, formWidget_);
    const QString description = fieldSchema.value(QStringLiteral("description")).toString();
    if (!description.isEmpty()) label->setToolTip(description);

    QWidget *editor = createFieldEditor(name, fieldSchema, isRequired);
    fieldEditors_.insert(name, editor);
    fieldSchemas_.insert(name, fieldSchema);
    formLayout_->addRow(label, editor);
  }

  formScrollArea_->verticalScrollBar()->setValue(0);
}

QWidget *MainWindow::createFieldEditor(const QString &name, const QJsonObject &schema, bool required) {
  const QString type = schema.value(QStringLiteral("type")).toString();
  const QString description = schema.value(QStringLiteral("description")).toString();
  const QJsonArray enumValues = schema.value(QStringLiteral("enum")).toArray();
  const QJsonValue defaultValue = schema.value(QStringLiteral("default"));

  if (!enumValues.isEmpty()) {
    auto *combo = new QComboBox(formWidget_);
    if (!required) combo->addItem(QStringLiteral("Not set"), QVariant());
    for (const QJsonValue &value : enumValues) combo->addItem(jsonText(value), value.toVariant());
    if (!defaultValue.isUndefined()) {
      const int index = combo->findData(defaultValue.toVariant());
      if (index >= 0) combo->setCurrentIndex(index);
    }
    combo->setToolTip(description);
    return combo;
  }

  if (type == QStringLiteral("boolean")) {
    auto *combo = new QComboBox(formWidget_);
    if (!required) combo->addItem(QStringLiteral("Not set"), QVariant());
    combo->addItem(QStringLiteral("true"), true);
    combo->addItem(QStringLiteral("false"), false);
    if (!defaultValue.isUndefined()) {
      const int index = combo->findData(defaultValue.toBool());
      if (index >= 0) combo->setCurrentIndex(index);
    }
    combo->setToolTip(description);
    return combo;
  }

  if (type == QStringLiteral("object") || type == QStringLiteral("array")) {
    auto *editor = new QPlainTextEdit(formWidget_);
    editor->setMaximumHeight(92);
    editor->setPlaceholderText(type == QStringLiteral("array") ? QStringLiteral("[]") : QStringLiteral("{}"));
    if (!defaultValue.isUndefined()) {
      editor->setPlainText(jsonText(defaultValue));
    } else if (required) {
      editor->setPlainText(type == QStringLiteral("array") ? QStringLiteral("[]") : QStringLiteral("{}"));
    }
    editor->setToolTip(description);
    return editor;
  }

  auto *editor = new QLineEdit(formWidget_);
  if (!defaultValue.isUndefined()) editor->setText(jsonText(defaultValue));
  if (schema.contains(QStringLiteral("example"))) {
    editor->setPlaceholderText(jsonText(schema.value(QStringLiteral("example"))));
  } else if (required) {
    editor->setPlaceholderText(QStringLiteral("Required"));
  } else {
    editor->setPlaceholderText(QStringLiteral("Optional"));
  }
  editor->setToolTip(description);
  editor->setProperty("fieldName", name);
  return editor;
}

bool MainWindow::collectFormArguments(QJsonObject *arguments, QString *errorMessage) const {
  *arguments = QJsonObject();

  for (auto it = fieldSchemas_.constBegin(); it != fieldSchemas_.constEnd(); ++it) {
    const QString name = it.key();
    const QJsonObject schema = it.value();
    QWidget *editor = fieldEditors_.value(name);
    const bool required = requiredFields_.contains(name);
    const QString type = schema.value(QStringLiteral("type")).toString();

    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
      const QVariant value = combo->currentData();
      if (!value.isValid()) {
        if (required) {
          *errorMessage = QStringLiteral("%1 is required.").arg(name);
          return false;
        }
        continue;
      }
      arguments->insert(name, QJsonValue::fromVariant(value));
      continue;
    }

    if (auto *plainText = qobject_cast<QPlainTextEdit *>(editor)) {
      const QString text = plainText->toPlainText().trimmed();
      if (text.isEmpty()) {
        if (required) {
          *errorMessage = QStringLiteral("%1 is required.").arg(name);
          return false;
        }
        continue;
      }

      QJsonParseError parseError;
      const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
      if (parseError.error != QJsonParseError::NoError) {
        *errorMessage = QStringLiteral("%1 must contain valid JSON: %2").arg(name, parseError.errorString());
        return false;
      }
      if (type == QStringLiteral("array") && !document.isArray()) {
        *errorMessage = QStringLiteral("%1 must be a JSON array.").arg(name);
        return false;
      }
      if (type == QStringLiteral("object") && !document.isObject()) {
        *errorMessage = QStringLiteral("%1 must be a JSON object.").arg(name);
        return false;
      }
      arguments->insert(name, document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object()));
      continue;
    }

    auto *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (!lineEdit) continue;
    const QString rawText = lineEdit->text();
    const QString trimmed = rawText.trimmed();
    if (trimmed.isEmpty()) {
      if (required) {
        *errorMessage = QStringLiteral("%1 is required.").arg(name);
        return false;
      }
      continue;
    }

    if (type == QStringLiteral("integer")) {
      bool ok = false;
      const qlonglong value = trimmed.toLongLong(&ok);
      if (!ok) {
        *errorMessage = QStringLiteral("%1 must be an integer.").arg(name);
        return false;
      }
      arguments->insert(name, QJsonValue(static_cast<double>(value)));
    } else if (type == QStringLiteral("number")) {
      bool ok = false;
      const double value = trimmed.toDouble(&ok);
      if (!ok) {
        *errorMessage = QStringLiteral("%1 must be a number.").arg(name);
        return false;
      }
      arguments->insert(name, value);
    } else {
      arguments->insert(name, rawText);
    }
  }

  return true;
}

void MainWindow::handleGlobalSearch(const QString &) {
  applyToolFilters();
}

void MainWindow::executeGlobalCommand() {
  const QString text = globalSearchEdit_->text().trimmed();
  if (text.startsWith(QLatin1Char('>'))) {
    const QString command = text.mid(1).trimmed().toLower();
    if (command.startsWith(QStringLiteral("settings"))) {
      openSettingsDialog();
    } else if (command.startsWith(QStringLiteral("logs"))) {
      openLogsDialog();
    } else if (command.startsWith(QStringLiteral("about"))) {
      openAboutDialog();
    } else if (command.startsWith(QStringLiteral("refresh"))) {
      refreshTools();
    } else if (command.startsWith(QStringLiteral("connect"))) {
      connectOrDisconnect();
    } else {
      QMessageBox::information(this, QStringLiteral("Command palette"),
                               QStringLiteral("Available commands:\n>settings\n>logs\n>about\n>refresh\n>connect"));
    }
    globalSearchEdit_->clear();
    return;
  }

  for (int i = 0; i < toolList_->count(); ++i) {
    if (!toolList_->item(i)->isHidden()) {
      toolList_->setCurrentRow(i);
      toolList_->setFocus();
      return;
    }
  }
}

void MainWindow::openSettingsDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Superpower Settings"));
  dialog.resize(620, 360);

  auto *layout = new QVBoxLayout(&dialog);
  auto *title = new QLabel(QStringLiteral("Settings"), &dialog);
  title->setProperty("panelTitle", true);
  layout->addWidget(title);
  auto *note = new QLabel(
      QStringLiteral("Runtime paths are normally auto-detected. Routing focus and execution policy can be adjusted here without exposing them in the main workspace."),
      &dialog);
  note->setProperty("muted", true);
  note->setWordWrap(true);
  layout->addWidget(note);

  auto *form = new QFormLayout();
  auto *nodeEdit = new QLineEdit(nodeProgram_, &dialog);
  auto *hostEdit = new QLineEdit(hostScript_, &dialog);
  auto *focusEdit = new QLineEdit(taskFocus_, &dialog);
  auto *policyCombo = new QComboBox(&dialog);
  policyCombo->addItems({QStringLiteral("audit"), QStringLiteral("guarded")});
  policyCombo->setCurrentText(policyMode_);
  nodeEdit->setEnabled(!bridge_->isRunning());
  hostEdit->setEnabled(!bridge_->isRunning());
  form->addRow(QStringLiteral("Node executable"), nodeEdit);
  form->addRow(QStringLiteral("MCP host bridge"), hostEdit);
  form->addRow(QStringLiteral("Task focus"), focusEdit);
  form->addRow(QStringLiteral("Execution policy"), policyCombo);
  layout->addLayout(form);
  layout->addStretch(1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) return;

  if (!bridge_->isRunning()) {
    nodeProgram_ = nodeEdit->text().trimmed();
    hostScript_ = hostEdit->text().trimmed();
  }
  taskFocus_ = focusEdit->text();
  policyMode_ = policyCombo->currentText();
  appendLog(QStringLiteral("Settings updated."));

  if (bridge_->isRunning()) {
    bridge_->sendRequest(QStringLiteral("focus"), {{QStringLiteral("focus"), taskFocus_}});
    bridge_->sendRequest(QStringLiteral("policy"), {{QStringLiteral("mode"), policyMode_}});
  }
}

void MainWindow::openLogsDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Superpower Logs"));
  dialog.resize(780, 500);
  auto *layout = new QVBoxLayout(&dialog);
  auto *view = new QPlainTextEdit(&dialog);
  view->setReadOnly(true);
  view->setPlainText(logLines_.isEmpty() ? QStringLiteral("No desktop log entries yet.") : logLines_.join(QLatin1Char('\n')));
  view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
  layout->addWidget(view, 1);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
}

void MainWindow::openAboutDialog() {
  QMessageBox::about(
      this, QStringLiteral("About Superpower"),
      QStringLiteral("Superpower Desktop v1.3\n\nNative Qt 6 / C++20 workspace for the Superpower MCP stack.\n\nThe desktop shell reuses the existing MCP Host, Tool Router, guarded execution policy, context budgeting, and privacy-safe telemetry."));
}

void MainWindow::openSchemaDialog() {
  auto *item = toolList_->currentItem();
  if (!item) return;
  const QJsonObject tool = toolsByName_.value(item->data(Qt::UserRole).toString());
  const QJsonObject schema = tool.value(QStringLiteral("inputSchema")).toObject();

  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Input Schema - %1").arg(item->data(Qt::UserRole).toString()));
  dialog.resize(720, 520);
  auto *layout = new QVBoxLayout(&dialog);
  auto *view = new QPlainTextEdit(&dialog);
  view->setReadOnly(true);
  view->setPlainText(QString::fromUtf8(QJsonDocument(schema).toJson(QJsonDocument::Indented)));
  layout->addWidget(view, 1);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
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
    appendLog(QStringLiteral("Execution policy updated to %1.").arg(policyMode_));
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
    appendLog(QStringLiteral("Completed %1.").arg(pending.toolName));
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
      appendLog(QStringLiteral("Action cancelled by user: %1.").arg(pending.toolName));
      outputView_->setPlainText(QStringLiteral("Action cancelled."));
      runButton_->setEnabled(true);
    }
    return;
  }

  if (method == QStringLiteral("call")) {
    const PendingCall pending = pendingCalls_.take(id);
    runButton_->setEnabled(bridge_->isRunning() && toolList_->currentItem() != nullptr);
    outputView_->setPlainText(QStringLiteral("Execution failed\n\n%1: %2").arg(code, message));
    appendLog(QStringLiteral("Tool failure %1 (%2): %3").arg(pending.toolName, code, message));
    return;
  }
  appendLog(QStringLiteral("%1: %2").arg(code, message));
}

void MainWindow::appendOutput(const QString &heading, const QJsonValue &value) {
  outputView_->setPlainText(QStringLiteral("%1\n\n%2").arg(heading, jsonText(value)));
  outputView_->verticalScrollBar()->setValue(0);
}

void MainWindow::appendLog(const QString &line) {
  const QString cleaned = line.trimmed();
  if (cleaned.isEmpty()) return;
  logLines_.append(QStringLiteral("[desktop] %1").arg(cleaned));
  while (logLines_.size() > 500) logLines_.removeFirst();
}

void MainWindow::setStatus(const QString &text, bool connected) {
  statusLabel_->setText(text);
  if (connected) {
    statusLabel_->setStyleSheet(QStringLiteral("background:#111111;border:1px solid #111111;color:#ffffff;"));
    headerConnectionLabel_->setText(QStringLiteral("Connected · %1").arg(connectionSummary()));
    headerConnectionLabel_->setStyleSheet(
        QStringLiteral("background:#111111;border:1px solid #111111;color:#ffffff;border-radius:9px;padding:7px 10px;font-weight:600;"));
  } else {
    statusLabel_->setStyleSheet(QStringLiteral("background:#f2f2f2;border:1px solid #d9d9d9;color:#202020;"));
    headerConnectionLabel_->setText(text == QStringLiteral("Disconnected") ? QStringLiteral("No active connection") : text);
    headerConnectionLabel_->setStyleSheet(
        QStringLiteral("background:#f3f3f3;border:1px solid #dfdfdf;color:#444444;border-radius:9px;padding:7px 10px;font-weight:600;"));
  }
}

QString MainWindow::connectionSummary() const {
  const bool useStdio = transportCombo_ && transportCombo_->currentData().toString() == QStringLiteral("stdio");
  if (useStdio) {
    const QString command = stdioCommandEdit_ ? stdioCommandEdit_->text().trimmed() : QString();
    return command.isEmpty() ? QStringLiteral("Local stdio") : QStringLiteral("stdio · %1").arg(command);
  }
  const QString endpoint = endpointEdit_ ? endpointEdit_->text().trimmed() : QString();
  return endpoint.isEmpty() ? QStringLiteral("Streamable HTTP") : QStringLiteral("HTTP · %1").arg(endpoint);
}

QString MainWindow::toolCategory(const QString &name, const QString &description) {
  const QString haystack = (name + QLatin1Char(' ') + description).toLower();
  const auto containsAny = [&haystack](const QStringList &terms) {
    for (const QString &term : terms) {
      if (haystack.contains(term)) return true;
    }
    return false;
  };

  if (containsAny({QStringLiteral("github"), QStringLiteral("git"), QStringLiteral("repo"),
                   QStringLiteral("code"), QStringLiteral("developer"), QStringLiteral("terminal"),
                   QStringLiteral("shell"), QStringLiteral("issue"), QStringLiteral("pull request")})) {
    return QStringLiteral("Developer");
  }
  if (containsAny({QStringLiteral("email"), QStringLiteral("gmail"), QStringLiteral("message"),
                   QStringLiteral("slack"), QStringLiteral("calendar"), QStringLiteral("contact")})) {
    return QStringLiteral("Communication");
  }
  if (containsAny({QStringLiteral("database"), QStringLiteral("sql"), QStringLiteral("data"),
                   QStringLiteral("sheet"), QStringLiteral("table"), QStringLiteral("csv"),
                   QStringLiteral("analytics")})) {
    return QStringLiteral("Data");
  }
  if (containsAny({QStringLiteral("browser"), QStringLiteral("web"), QStringLiteral("http"),
                   QStringLiteral("url"), QStringLiteral("scrape"), QStringLiteral("crawl")})) {
    return QStringLiteral("Web");
  }
  if (containsAny({QStringLiteral("file"), QStringLiteral("folder"), QStringLiteral("document"),
                   QStringLiteral("drive"), QStringLiteral("dropbox"), QStringLiteral("pdf"),
                   QStringLiteral("storage")})) {
    return QStringLiteral("Files");
  }
  return QStringLiteral("General");
}

QIcon MainWindow::iconForCategory(const QString &category) const {
  if (category == QStringLiteral("Developer")) return style()->standardIcon(QStyle::SP_ComputerIcon);
  if (category == QStringLiteral("Communication")) return style()->standardIcon(QStyle::SP_MessageBoxInformation);
  if (category == QStringLiteral("Data")) return style()->standardIcon(QStyle::SP_FileDialogDetailedView);
  if (category == QStringLiteral("Web")) return style()->standardIcon(QStyle::SP_DriveNetIcon);
  if (category == QStringLiteral("Files")) return style()->standardIcon(QStyle::SP_DirIcon);
  return style()->standardIcon(QStyle::SP_FileIcon);
}

QString MainWindow::findDefaultNodeProgram() {
  const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
  const QString packagedNode = QDir(appDir).filePath(QStringLiteral("runtime/node.exe"));
#else
  const QString packagedNode = QDir(appDir).filePath(QStringLiteral("runtime/node"));
#endif
  if (QFileInfo::exists(packagedNode)) return QDir::toNativeSeparators(QFileInfo(packagedNode).absoluteFilePath());

  const QString discovered = QStandardPaths::findExecutable(QStringLiteral("node"));
  return discovered.isEmpty() ? QStringLiteral("node") : QDir::toNativeSeparators(discovered);
}

QString MainWindow::findDefaultHostScript() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString packagedBridge = QDir(appDir).filePath(QStringLiteral("bridge/superpower-host.mjs"));
  if (QFileInfo::exists(packagedBridge)) {
    return QDir::toNativeSeparators(QFileInfo(packagedBridge).absoluteFilePath());
  }

  const QStringList startingPoints{QDir::currentPath(), appDir};
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

bool MainWindow::parseStdioArguments(const QString &text, QStringList *arguments, QString *errorMessage) {
  arguments->clear();
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) return true;

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    *errorMessage = QStringLiteral("Arguments must be a JSON array of strings, for example: [\"server.js\", \"--port\", \"3000\"].");
    return false;
  }

  const QJsonArray array = document.array();
  for (const QJsonValue &value : array) {
    if (!value.isString()) {
      *errorMessage = QStringLiteral("Every stdio argument must be a string.");
      arguments->clear();
      return false;
    }
    arguments->append(value.toString());
  }
  return true;
}