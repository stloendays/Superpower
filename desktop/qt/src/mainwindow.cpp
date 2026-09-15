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
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QUuid>
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

QString compactDescription(const QString &text, int maxLength = 84) {
  QString value = text.simplified();
  if (value.size() > maxLength) value = value.left(maxLength - 3) + QStringLiteral("...");
  return value;
}

QString profileTransportLabel(const MainWindow::ServerProfile &profile) {
  return profile.transport == QStringLiteral("stdio") ? QStringLiteral("STDIO") : QStringLiteral("HTTP");
}
}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), bridge_(new McpBridgeProcess(this)) {
  setWindowTitle(QStringLiteral("Superpower Desktop"));
  resize(1520, 900);
  setMinimumSize(1180, 740);

  nodeProgram_ = findDefaultNodeProgram();
  hostScript_ = findDefaultHostScript();

  buildUi();
  applyStyle();
  connectSignals();
  loadServerProfiles();
  refreshServerList();
  if (!serverProfiles_.isEmpty()) serverList_->setCurrentRow(0);
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
  globalSearchEdit_->setPlaceholderText(
      QStringLiteral("Search apps and tools or type >runs, >settings, >logs  ·  Ctrl+K"));
  globalSearchEdit_->setClearButtonEnabled(true);
  globalSearchEdit_->setMinimumWidth(350);
  globalSearchEdit_->setMaximumWidth(560);
  topLayout->addWidget(globalSearchEdit_, 1);

  settingsButton_ = new QPushButton(QStringLiteral("Settings"), topBar);
  runsButton_ = new QPushButton(QStringLiteral("Runs"), topBar);
  logsButton_ = new QPushButton(QStringLiteral("Logs"), topBar);
  aboutButton_ = new QPushButton(QStringLiteral("About"), topBar);
  topLayout->addWidget(settingsButton_);
  topLayout->addWidget(runsButton_);
  topLayout->addWidget(logsButton_);
  topLayout->addWidget(aboutButton_);
  rootLayout->addWidget(topBar);

  auto *bodyLayout = new QHBoxLayout();
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(12);

  auto *connectionsCard = card(root);
  connectionsCard->setFixedWidth(330);
  auto *connectionLayout = new QVBoxLayout(connectionsCard);
  connectionLayout->setContentsMargins(18, 18, 18, 18);
  connectionLayout->setSpacing(9);

  auto *connectionsTitle = new QLabel(QStringLiteral("Servers / Connections"), connectionsCard);
  connectionsTitle->setProperty("panelTitle", true);
  connectionLayout->addWidget(connectionsTitle);
  auto *connectionsSubtitle = new QLabel(
      QStringLiteral("Save non-secret server metadata, switch profiles, and connect with one workspace."),
      connectionsCard);
  connectionsSubtitle->setProperty("muted", true);
  connectionsSubtitle->setWordWrap(true);
  connectionLayout->addWidget(connectionsSubtitle);

  serverList_ = new QListWidget(connectionsCard);
  serverList_->setMaximumHeight(180);
  serverList_->setMinimumHeight(110);
  serverList_->setWordWrap(true);
  connectionLayout->addWidget(serverList_);

  auto *serverActions = new QHBoxLayout();
  newServerButton_ = new QPushButton(QStringLiteral("New"), connectionsCard);
  saveServerButton_ = new QPushButton(QStringLiteral("Save"), connectionsCard);
  deleteServerButton_ = new QPushButton(QStringLiteral("Delete"), connectionsCard);
  serverActions->addWidget(newServerButton_);
  serverActions->addWidget(saveServerButton_);
  serverActions->addWidget(deleteServerButton_);
  connectionLayout->addLayout(serverActions);

  connectionSummaryLabel_ = new QLabel(QStringLiteral("Unsaved connection"), connectionsCard);
  connectionSummaryLabel_->setProperty("connectionSummary", true);
  connectionSummaryLabel_->setWordWrap(true);
  connectionLayout->addWidget(connectionSummaryLabel_);

  connectionLayout->addWidget(sectionLabel(QStringLiteral("Server details"), connectionsCard));
  serverNameEdit_ = new QLineEdit(connectionsCard);
  serverNameEdit_->setPlaceholderText(QStringLiteral("e.g. GitHub MCP, Local Agent"));
  connectionLayout->addWidget(new QLabel(QStringLiteral("Name"), connectionsCard));
  connectionLayout->addWidget(serverNameEdit_);

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
  stdioLayout->addWidget(new QLabel(QStringLiteral("Session arguments"), stdioConnectionWidget_));
  stdioLayout->addWidget(stdioArgsEdit_);
  connectionLayout->addWidget(stdioConnectionWidget_);

  auto *privacyHint = new QLabel(
      QStringLiteral("Profiles persist names and safe connection metadata only. URL credentials/query tokens and stdio arguments are not persisted."),
      connectionsCard);
  privacyHint->setProperty("muted", true);
  privacyHint->setWordWrap(true);
  connectionLayout->addWidget(privacyHint);
  connectionLayout->addStretch(1);

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
  catalogCard->setMinimumWidth(330);
  auto *catalogLayout = new QVBoxLayout(catalogCard);
  catalogLayout->setContentsMargins(18, 18, 18, 18);
  catalogLayout->setSpacing(9);

  auto *catalogHeader = new QHBoxLayout();
  auto *catalogTitle = new QLabel(QStringLiteral("Tools / Apps"), catalogCard);
  catalogTitle->setProperty("panelTitle", true);
  catalogHeader->addWidget(catalogTitle);
  catalogHeader->addStretch(1);
  toolCountLabel_ = new QLabel(QStringLiteral("0"), catalogCard);
  toolCountLabel_->setProperty("muted", true);
  catalogHeader->addWidget(toolCountLabel_);
  catalogLayout->addLayout(catalogHeader);

  appCombo_ = new QComboBox(catalogCard);
  appCombo_->addItem(QStringLiteral("All apps"), QStringLiteral("All"));
  catalogLayout->addWidget(appCombo_);

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

  toolDescriptionLabel_ = new QLabel(QStringLiteral("Connect to an MCP server to load Apps and Actions."), workCard);
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
  formScrollArea_->setMaximumHeight(310);
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
  outputView_->setPlaceholderText(QStringLiteral("The latest execution result appears here."));
  outputView_->setPlainText(QStringLiteral("Select a tool to begin."));
  workLayout->addWidget(outputView_, 1);

  splitter->addWidget(catalogCard);
  splitter->addWidget(workCard);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({380, 770});
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
    QLabel[muted="true"] { color: #737373; }
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
    QLineEdit:disabled, QComboBox:disabled, QPlainTextEdit:disabled, QListWidget:disabled {
      background: #f5f5f5;
      color: #8a8a8a;
      border-color: #e2e2e2;
    }
    QComboBox::drop-down { border: none; width: 24px; }
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
    QWidget[formSurface="true"] { background: #ffffff; }
    QListWidget { padding: 4px; outline: none; }
    QListWidget::item {
      border-radius: 8px;
      padding: 8px;
      margin: 2px 0;
      color: #111111;
    }
    QListWidget::item:hover { background: #f1f1f1; }
    QListWidget::item:selected { background: #111111; color: #ffffff; }
    QPushButton {
      background: #ffffff;
      color: #111111;
      border: 1px solid #d4d4d4;
      border-radius: 8px;
      padding: 8px 12px;
      font-weight: 650;
    }
    QPushButton:hover { background: #f0f0f0; border-color: #bdbdbd; }
    QPushButton:pressed { background: #e7e7e7; }
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
    QPushButton[primary="true"]:hover { background: #202020; border-color: #202020; }
    QPushButton[primary="true"]:pressed { background: #333333; border-color: #333333; }
    QPushButton[primary="true"]:disabled {
      background: #aaaaaa;
      border-color: #aaaaaa;
      color: #ffffff;
    }
    QSplitter::handle { background: transparent; width: 10px; }
    QScrollBar:vertical { width: 10px; background: transparent; margin: 0; }
    QScrollBar::handle:vertical {
      background: #c8c8c8;
      border-radius: 5px;
      min-height: 24px;
    }
    QScrollBar::handle:vertical:hover { background: #a8a8a8; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
  )"));
}

void MainWindow::connectSignals() {
  connect(connectButton_, &QPushButton::clicked, this, &MainWindow::connectOrDisconnect);
  connect(newServerButton_, &QPushButton::clicked, this, &MainWindow::newServerProfile);
  connect(saveServerButton_, &QPushButton::clicked, this, &MainWindow::saveCurrentServerProfile);
  connect(deleteServerButton_, &QPushButton::clicked, this, &MainWindow::deleteCurrentServerProfile);
  connect(serverList_, &QListWidget::currentItemChanged, this, [this]() { selectServerProfile(); });
  connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshTools);
  connect(runButton_, &QPushButton::clicked, this, &MainWindow::runSelectedTool);
  connect(toolList_, &QListWidget::currentItemChanged, this, [this]() { showSelectedTool(); });
  connect(appCombo_, &QComboBox::currentIndexChanged, this, [this]() { applyToolFilters(); });
  connect(categoryCombo_, &QComboBox::currentIndexChanged, this, [this]() { applyToolFilters(); });
  connect(transportCombo_, &QComboBox::currentIndexChanged, this, [this]() { updateConnectionForm(); });
  connect(serverNameEdit_, &QLineEdit::textChanged, this, [this]() { updateConnectionForm(); });
  connect(endpointEdit_, &QLineEdit::textChanged, this, [this]() { updateConnectionForm(); });
  connect(stdioCommandEdit_, &QLineEdit::textChanged, this, [this]() { updateConnectionForm(); });
  connect(globalSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::handleGlobalSearch);
  connect(globalSearchEdit_, &QLineEdit::returnPressed, this, &MainWindow::executeGlobalCommand);
  connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
  connect(runsButton_, &QPushButton::clicked, this, &MainWindow::openRunsDialog);
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
    setServerStatus(activeProfileId_, QStringLiteral("Connected"));
    setStatus(QStringLiteral("Connected via %1").arg(transport), true);
    appendLog(QStringLiteral("MCP bridge ready via %1 for %2.").arg(transport, activeConnectionName_));
    refreshTools();
  });
  connect(bridge_, &McpBridgeProcess::responseReceived, this, &MainWindow::handleResponse);
  connect(bridge_, &McpBridgeProcess::requestFailed, this, &MainWindow::handleRequestFailure);
  connect(bridge_, &McpBridgeProcess::logLine, this, &MainWindow::appendLog);
  connect(bridge_, &McpBridgeProcess::processError, this, [this](const QString &message) {
    setServerStatus(activeProfileId_, QStringLiteral("Error"));
    appendLog(QStringLiteral("Bridge error: %1").arg(message));
    setStatus(QStringLiteral("Connection error"), false);
    connectButton_->setEnabled(true);
  });
  connect(bridge_, &McpBridgeProcess::processExited, this, [this](int exitCode, QProcess::ExitStatus) {
    const bool errored = statusLabel_->text() == QStringLiteral("Connection error");
    if (!errored) setServerStatus(activeProfileId_, QStringLiteral("Disconnected"));
    setConnectedUi(false);
    if (!errored) setStatus(QStringLiteral("Disconnected"), false);
    appendLog(QStringLiteral("Bridge exited with code %1.").arg(exitCode));
    activeProfileId_.clear();
    activeConnectionName_.clear();
  });
}

void MainWindow::setConnectedUi(bool connected) {
  connectButton_->setEnabled(true);
  connectButton_->setText(connected ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
  serverList_->setEnabled(!connected);
  serverNameEdit_->setEnabled(!connected);
  newServerButton_->setEnabled(!connected);
  saveServerButton_->setEnabled(!connected);
  deleteServerButton_->setEnabled(!connected && serverList_->currentItem() != nullptr);
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

void MainWindow::loadServerProfiles() {
  serverProfiles_.clear();
  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  const int count = settings.beginReadArray(QStringLiteral("servers"));
  for (int i = 0; i < count; ++i) {
    settings.setArrayIndex(i);
    ServerProfile profile;
    profile.id = settings.value(QStringLiteral("id")).toString();
    profile.name = settings.value(QStringLiteral("name")).toString();
    profile.transport = settings.value(QStringLiteral("transport"), QStringLiteral("http")).toString();
    profile.endpoint = settings.value(QStringLiteral("endpoint")).toString();
    profile.command = settings.value(QStringLiteral("command")).toString();
    if (profile.id.isEmpty()) profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (profile.name.isEmpty()) continue;
    serverProfiles_.append(profile);
    serverStatuses_.insert(profile.id, QStringLiteral("Disconnected"));
  }
  settings.endArray();
}

void MainWindow::persistServerProfiles() const {
  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  settings.remove(QStringLiteral("servers"));
  settings.beginWriteArray(QStringLiteral("servers"));
  for (int i = 0; i < serverProfiles_.size(); ++i) {
    settings.setArrayIndex(i);
    const ServerProfile &profile = serverProfiles_.at(i);
    settings.setValue(QStringLiteral("id"), profile.id);
    settings.setValue(QStringLiteral("name"), profile.name);
    settings.setValue(QStringLiteral("transport"), profile.transport);
    settings.setValue(QStringLiteral("endpoint"), profile.endpoint);
    settings.setValue(QStringLiteral("command"), profile.command);
  }
  settings.endArray();
  settings.sync();
}

void MainWindow::refreshServerList() {
  const QString selectedId = currentServerProfileId();
  QSignalBlocker blocker(serverList_);
  serverList_->clear();

  int selectedRow = -1;
  for (int i = 0; i < serverProfiles_.size(); ++i) {
    const ServerProfile &profile = serverProfiles_.at(i);
    const QString status = serverStatuses_.value(profile.id, QStringLiteral("Disconnected"));
    QString detail;
    if (profile.transport == QStringLiteral("stdio")) {
      detail = profile.command.isEmpty() ? QStringLiteral("Local stdio") : profile.command;
    } else {
      detail = profile.endpoint.isEmpty() ? QStringLiteral("Streamable HTTP") : profile.endpoint;
    }
    auto *item = new QListWidgetItem(iconForServerStatus(status),
                                     QStringLiteral("%1\n%2 · %3").arg(profile.name, status, compactDescription(detail, 52)),
                                     serverList_);
    item->setData(Qt::UserRole, profile.id);
    item->setToolTip(QStringLiteral("%1 · %2\n%3").arg(status, profileTransportLabel(profile), detail));
    item->setSizeHint(QSize(0, 52));
    if (profile.id == selectedId) selectedRow = i;
  }

  if (selectedRow >= 0) serverList_->setCurrentRow(selectedRow);
  deleteServerButton_->setEnabled(!bridge_->isRunning() && serverList_->currentItem() != nullptr);
}

void MainWindow::newServerProfile() {
  QSignalBlocker blocker(serverList_);
  serverList_->clearSelection();
  serverList_->setCurrentRow(-1);
  serverNameEdit_->clear();
  transportCombo_->setCurrentIndex(0);
  endpointEdit_->setText(QStringLiteral("http://localhost:3000/mcp"));
  stdioCommandEdit_->clear();
  stdioArgsEdit_->setText(QStringLiteral("[]"));
  statusLabel_->setText(QStringLiteral("Disconnected"));
  deleteServerButton_->setEnabled(false);
  updateConnectionForm();
  serverNameEdit_->setFocus();
}

void MainWindow::saveCurrentServerProfile() {
  const QString name = serverNameEdit_->text().trimmed();
  if (name.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Server name required"),
                         QStringLiteral("Give this connection a name before saving it."));
    return;
  }

  const QString transport = transportCombo_->currentData().toString();
  QString endpoint;
  QString command;
  if (transport == QStringLiteral("http")) {
    endpoint = endpointEdit_->text().trimmed();
    if (!endpointIsSafeToPersist(endpoint)) {
      QMessageBox::warning(
          this, QStringLiteral("Sensitive endpoint not saved"),
          QStringLiteral("Saved profiles do not persist URL credentials, query tokens, or fragments. Remove sensitive URL data and use the MCP host authentication/environment model instead."));
      return;
    }
  } else {
    command = stdioCommandEdit_->text().trimmed();
  }

  QString id = currentServerProfileId();
  int index = serverProfileIndex(id);
  if (index < 0) {
    ServerProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = name;
    profile.transport = transport;
    profile.endpoint = endpoint;
    profile.command = command;
    serverProfiles_.append(profile);
    serverStatuses_.insert(profile.id, QStringLiteral("Disconnected"));
    id = profile.id;
  } else {
    ServerProfile &profile = serverProfiles_[index];
    profile.name = name;
    profile.transport = transport;
    profile.endpoint = endpoint;
    profile.command = command;
  }

  persistServerProfiles();
  refreshServerList();
  for (int row = 0; row < serverList_->count(); ++row) {
    if (serverList_->item(row)->data(Qt::UserRole).toString() == id) {
      serverList_->setCurrentRow(row);
      break;
    }
  }
  appendLog(QStringLiteral("Saved server profile %1. Sensitive stdio arguments remain session-only.").arg(name));
}

void MainWindow::deleteCurrentServerProfile() {
  const QString id = currentServerProfileId();
  const int index = serverProfileIndex(id);
  if (index < 0) return;
  const QString name = serverProfiles_.at(index).name;
  const auto decision = QMessageBox::question(
      this, QStringLiteral("Delete server profile"), QStringLiteral("Delete the saved profile “%1”? ").arg(name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (decision != QMessageBox::Yes) return;

  serverProfiles_.removeAt(index);
  serverStatuses_.remove(id);
  persistServerProfiles();
  refreshServerList();
  if (!serverProfiles_.isEmpty()) {
    serverList_->setCurrentRow(qMin(index, serverProfiles_.size() - 1));
  } else {
    newServerProfile();
  }
  appendLog(QStringLiteral("Deleted server profile %1.").arg(name));
}

void MainWindow::selectServerProfile() {
  auto *item = serverList_->currentItem();
  if (!item || bridge_->isRunning()) return;
  const int index = serverProfileIndex(item->data(Qt::UserRole).toString());
  if (index < 0) return;

  const ServerProfile &profile = serverProfiles_.at(index);
  serverNameEdit_->setText(profile.name);
  const int transportIndex = transportCombo_->findData(profile.transport);
  if (transportIndex >= 0) transportCombo_->setCurrentIndex(transportIndex);
  endpointEdit_->setText(profile.endpoint.isEmpty() ? QStringLiteral("http://localhost:3000/mcp") : profile.endpoint);
  stdioCommandEdit_->setText(profile.command);
  stdioArgsEdit_->setText(QStringLiteral("[]"));
  deleteServerButton_->setEnabled(true);
  updateConnectionForm();
}

void MainWindow::setServerStatus(const QString &profileId, const QString &status) {
  if (profileId.isEmpty()) return;
  serverStatuses_.insert(profileId, status);
  refreshServerList();
}

int MainWindow::serverProfileIndex(const QString &profileId) const {
  if (profileId.isEmpty()) return -1;
  for (int i = 0; i < serverProfiles_.size(); ++i) {
    if (serverProfiles_.at(i).id == profileId) return i;
  }
  return -1;
}

QString MainWindow::currentServerName() const {
  const QString name = serverNameEdit_ ? serverNameEdit_->text().trimmed() : QString();
  return name.isEmpty() ? QStringLiteral("Unsaved server") : name;
}

QString MainWindow::currentServerProfileId() const {
  auto *item = serverList_ ? serverList_->currentItem() : nullptr;
  return item ? item->data(Qt::UserRole).toString() : QString();
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
  activeProfileId_ = currentServerProfileId();
  activeConnectionName_ = currentServerName();

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
  const QString appName = item->data(Qt::UserRole + 3).toString();
  const QString id = bridge_->sendRequest(
      QStringLiteral("call"), {{QStringLiteral("toolName"), toolName},
                                {QStringLiteral("args"), arguments},
                                {QStringLiteral("approve"), false}});
  if (id.isEmpty()) return;

  PendingCall pending;
  pending.toolName = toolName;
  pending.appName = appName;
  pending.serverName = activeConnectionName_.isEmpty() ? currentServerName() : activeConnectionName_;
  pending.risk = QStringLiteral("normal");
  pending.arguments = arguments;
  pending.startedAt = QDateTime::currentDateTimeUtc();
  pendingCalls_.insert(id, pending);
  runButton_->setEnabled(false);
  outputView_->setPlainText(QStringLiteral("Running %1...").arg(toolDisplayName(toolName, appName)));
  appendLog(QStringLiteral("Calling %1 from %2.").arg(toolName, appName));
}

void MainWindow::showSelectedTool() {
  auto *item = toolList_->currentItem();
  if (!item) {
    toolNameLabel_->setText(QStringLiteral("Select a tool"));
    toolDescriptionLabel_->setText(QStringLiteral("Choose an App / Action from the catalog."));
    rebuildArgumentForm(QJsonObject());
    runButton_->setEnabled(false);
    schemaButton_->setEnabled(false);
    resetFormButton_->setEnabled(false);
    outputView_->setPlainText(QStringLiteral("Select a tool to begin."));
    return;
  }

  const QString name = item->data(Qt::UserRole).toString();
  const QString appName = item->data(Qt::UserRole + 3).toString();
  const QString displayName = item->data(Qt::UserRole + 4).toString();
  const QJsonObject tool = toolsByName_.value(name);
  const QJsonObject schema = tool.value(QStringLiteral("inputSchema")).toObject();
  toolNameLabel_->setText(displayName);
  const QString description = tool.value(QStringLiteral("description")).toString();
  toolDescriptionLabel_->setText(QStringLiteral("%1 · %2\n%3").arg(appName, name, description));
  rebuildArgumentForm(schema);
  runButton_->setEnabled(bridge_->isRunning());
  schemaButton_->setEnabled(true);
  resetFormButton_->setEnabled(true);
  outputView_->setPlainText(QStringLiteral("Ready to run %1.").arg(displayName));
}

void MainWindow::applyToolFilters() {
  QString needle = globalSearchEdit_->text().trimmed();
  const bool commandMode = needle.startsWith(QLatin1Char('>'));
  if (commandMode) needle.clear();
  const QString selectedApp = appCombo_->currentData().toString();
  const QString selectedCategory = categoryCombo_->currentData().toString();

  int visible = 0;
  for (int i = 0; i < toolList_->count(); ++i) {
    auto *item = toolList_->item(i);
    const QString category = item->data(Qt::UserRole + 1).toString();
    const QString searchable = item->data(Qt::UserRole + 2).toString();
    const QString appName = item->data(Qt::UserRole + 3).toString();
    const bool appMatch = selectedApp == QStringLiteral("All") || appName == selectedApp;
    const bool categoryMatch = selectedCategory == QStringLiteral("All") || category == selectedCategory;
    const bool textMatch = needle.isEmpty() || searchable.contains(needle, Qt::CaseInsensitive);
    const bool show = appMatch && categoryMatch && textMatch;
    item->setHidden(!show);
    if (show) ++visible;
  }

  toolCountLabel_->setText(commandMode ? QStringLiteral("Command mode")
                                       : QStringLiteral("%1 / %2").arg(visible).arg(toolList_->count()));
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
    const QString appName = toolApp(name, description);
    const QString displayName = toolDisplayName(name, appName);
    const QString summary = compactDescription(description);
    const QString subtitle = summary.isEmpty()
                                 ? QStringLiteral("%1 · %2").arg(appName.toUpper(), category.toUpper())
                                 : QStringLiteral("%1 · %2 · %3").arg(appName.toUpper(), category.toUpper(), summary);

    toolsByName_.insert(name, tool);
    auto *item = new QListWidgetItem(iconForCategory(category),
                                     QStringLiteral("%1\n%2").arg(displayName, subtitle), toolList_);
    item->setData(Qt::UserRole, name);
    item->setData(Qt::UserRole + 1, category);
    item->setData(Qt::UserRole + 2,
                  displayName + QLatin1Char(' ') + name + QLatin1Char(' ') + description + QLatin1Char(' ') +
                      category + QLatin1Char(' ') + appName);
    item->setData(Qt::UserRole + 3, appName);
    item->setData(Qt::UserRole + 4, displayName);
    item->setToolTip(QStringLiteral("%1 · %2\n%3\n\nRaw MCP tool: %4").arg(appName, category, description, name));
    item->setSizeHint(QSize(0, 60));
  }

  rebuildAppFilter();
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

void MainWindow::rebuildAppFilter() {
  const QString previous = appCombo_->currentData().toString();
  QSet<QString> apps;
  for (int i = 0; i < toolList_->count(); ++i) apps.insert(toolList_->item(i)->data(Qt::UserRole + 3).toString());
  QStringList sortedApps = apps.values();
  sortedApps.sort(Qt::CaseInsensitive);

  QSignalBlocker blocker(appCombo_);
  appCombo_->clear();
  appCombo_->addItem(QStringLiteral("All apps"), QStringLiteral("All"));
  for (const QString &appName : sortedApps) appCombo_->addItem(appName, appName);
  const int previousIndex = appCombo_->findData(previous);
  appCombo_->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
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

void MainWindow::applyArgumentsToForm(const QJsonObject &arguments) {
  for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
    QWidget *editor = fieldEditors_.value(it.key());
    if (!editor) continue;
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
      const int index = combo->findData(it.value().toVariant());
      if (index >= 0) combo->setCurrentIndex(index);
    } else if (auto *plainText = qobject_cast<QPlainTextEdit *>(editor)) {
      plainText->setPlainText(jsonText(it.value()));
    } else if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
      lineEdit->setText(it.value().isString() ? it.value().toString() : jsonText(it.value()));
    }
  }
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
      const double numeric = static_cast<double>(value);
      if (schema.contains(QStringLiteral("minimum")) && numeric < schema.value(QStringLiteral("minimum")).toDouble()) {
        *errorMessage = QStringLiteral("%1 must be at least %2.").arg(name).arg(schema.value(QStringLiteral("minimum")).toDouble());
        return false;
      }
      if (schema.contains(QStringLiteral("maximum")) && numeric > schema.value(QStringLiteral("maximum")).toDouble()) {
        *errorMessage = QStringLiteral("%1 must be at most %2.").arg(name).arg(schema.value(QStringLiteral("maximum")).toDouble());
        return false;
      }
      arguments->insert(name, QJsonValue(numeric));
    } else if (type == QStringLiteral("number")) {
      bool ok = false;
      const double value = trimmed.toDouble(&ok);
      if (!ok) {
        *errorMessage = QStringLiteral("%1 must be a number.").arg(name);
        return false;
      }
      if (schema.contains(QStringLiteral("minimum")) && value < schema.value(QStringLiteral("minimum")).toDouble()) {
        *errorMessage = QStringLiteral("%1 must be at least %2.").arg(name).arg(schema.value(QStringLiteral("minimum")).toDouble());
        return false;
      }
      if (schema.contains(QStringLiteral("maximum")) && value > schema.value(QStringLiteral("maximum")).toDouble()) {
        *errorMessage = QStringLiteral("%1 must be at most %2.").arg(name).arg(schema.value(QStringLiteral("maximum")).toDouble());
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
    } else if (command.startsWith(QStringLiteral("runs"))) {
      openRunsDialog();
    } else if (command.startsWith(QStringLiteral("logs"))) {
      openLogsDialog();
    } else if (command.startsWith(QStringLiteral("about"))) {
      openAboutDialog();
    } else if (command.startsWith(QStringLiteral("refresh"))) {
      refreshTools();
    } else if (command.startsWith(QStringLiteral("connect"))) {
      connectOrDisconnect();
    } else if (command.startsWith(QStringLiteral("new server")) || command.startsWith(QStringLiteral("server"))) {
      newServerProfile();
    } else {
      QMessageBox::information(
          this, QStringLiteral("Command palette"),
          QStringLiteral("Available commands:\n>runs\n>settings\n>logs\n>about\n>refresh\n>connect\n>new server"));
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
  dialog.resize(640, 380);

  auto *layout = new QVBoxLayout(&dialog);
  auto *title = new QLabel(QStringLiteral("Settings"), &dialog);
  title->setProperty("panelTitle", true);
  layout->addWidget(title);
  auto *note = new QLabel(
      QStringLiteral("Runtime paths are normally auto-detected. Routing focus and guarded-execution policy remain centralized in the MCP host rather than duplicated in the desktop client."),
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

void MainWindow::openRunsDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Superpower Runs"));
  dialog.resize(900, 600);
  auto *layout = new QVBoxLayout(&dialog);

  auto *title = new QLabel(QStringLiteral("Runs"), &dialog);
  title->setProperty("panelTitle", true);
  layout->addWidget(title);
  auto *note = new QLabel(
      QStringLiteral("Execution history is session-only. Parameters are kept in memory so a previous run can be loaded back into the workspace without persisting credentials or sensitive arguments."),
      &dialog);
  note->setProperty("muted", true);
  note->setWordWrap(true);
  layout->addWidget(note);

  auto *runList = new QListWidget(&dialog);
  for (int i = 0; i < runHistory_.size(); ++i) {
    const RunRecord &record = runHistory_.at(i);
    const QString when = record.startedAt.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    const QString displayName = toolDisplayName(record.toolName, record.appName);
    auto *item = new QListWidgetItem(
        QStringLiteral("%1  %2  %3 · %4\n%5 · %6 ms · %7")
            .arg(when, record.status.toUpper(), record.appName, displayName, record.serverName)
            .arg(record.durationMs)
            .arg(compactDescription(record.summary, 88)),
        runList);
    item->setData(Qt::UserRole, i);
    item->setSizeHint(QSize(0, 58));
  }
  layout->addWidget(runList, 1);

  auto *detail = new QPlainTextEdit(&dialog);
  detail->setReadOnly(true);
  detail->setMaximumHeight(170);
  detail->setPlainText(runHistory_.isEmpty() ? QStringLiteral("No tool runs in this session yet.")
                                             : QStringLiteral("Select a run to inspect it."));
  layout->addWidget(detail);

  connect(runList, &QListWidget::currentItemChanged, &dialog,
          [this, detail](QListWidgetItem *current, QListWidgetItem *) {
            if (!current) return;
            const int index = current->data(Qt::UserRole).toInt();
            if (index < 0 || index >= runHistory_.size()) return;
            const RunRecord &record = runHistory_.at(index);
            detail->setPlainText(
                QStringLiteral("Status: %1\nApp: %2\nTool: %3\nServer: %4\nRisk: %5\nDuration: %6 ms\n\nParameters\n%7\n\nSummary\n%8")
                    .arg(record.status, record.appName, record.toolName, record.serverName, record.risk)
                    .arg(record.durationMs)
                    .arg(QString::fromUtf8(QJsonDocument(record.arguments).toJson(QJsonDocument::Indented)),
                         record.summary));
          });

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  auto *loadButton = buttons->addButton(QStringLiteral("Load parameters"), QDialogButtonBox::ActionRole);
  auto *clearButton = buttons->addButton(QStringLiteral("Clear history"), QDialogButtonBox::ResetRole);
  loadButton->setEnabled(!runHistory_.isEmpty());
  clearButton->setEnabled(!runHistory_.isEmpty());
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(loadButton, &QPushButton::clicked, &dialog, [this, runList, &dialog]() {
    auto *item = runList->currentItem();
    if (!item) return;
    loadRunIntoWorkspace(item->data(Qt::UserRole).toInt());
    dialog.accept();
  });
  connect(clearButton, &QPushButton::clicked, &dialog, [this, runList, detail, loadButton, clearButton]() {
    runHistory_.clear();
    runList->clear();
    detail->setPlainText(QStringLiteral("Run history cleared for this session."));
    loadButton->setEnabled(false);
    clearButton->setEnabled(false);
  });
  layout->addWidget(buttons);
  if (runList->count() > 0) runList->setCurrentRow(0);
  dialog.exec();
}

void MainWindow::openLogsDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Superpower Logs"));
  dialog.resize(800, 520);
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
      QStringLiteral("Superpower Desktop\n\nNative Qt 6 / C++20 workspace for the Superpower MCP stack.\n\nConnections → Apps → Actions → Runs\n\nThe desktop shell reuses the existing MCP Host, Tool Router, guarded execution policy, context budgeting, and privacy-safe telemetry. Saved Server Profiles intentionally exclude stdio arguments and reject HTTP URLs containing credentials or query tokens."));
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

void MainWindow::recordRun(const PendingCall &pending, const QString &status, const QString &summary) {
  RunRecord record;
  record.toolName = pending.toolName;
  record.appName = pending.appName;
  record.serverName = pending.serverName;
  record.status = status;
  record.risk = pending.risk;
  record.summary = compactDescription(summary, 220);
  record.arguments = pending.arguments;
  record.startedAt = pending.startedAt;
  record.durationMs = qMax<qint64>(0, pending.startedAt.msecsTo(QDateTime::currentDateTimeUtc()));
  runHistory_.prepend(record);
  while (runHistory_.size() > 100) runHistory_.removeLast();
}

void MainWindow::loadRunIntoWorkspace(int historyIndex) {
  if (historyIndex < 0 || historyIndex >= runHistory_.size()) return;
  const RunRecord &record = runHistory_.at(historyIndex);
  globalSearchEdit_->clear();
  appCombo_->setCurrentIndex(0);
  categoryCombo_->setCurrentIndex(0);
  applyToolFilters();

  for (int row = 0; row < toolList_->count(); ++row) {
    auto *item = toolList_->item(row);
    if (item->data(Qt::UserRole).toString() == record.toolName) {
      toolList_->setCurrentRow(row);
      applyArgumentsToForm(record.arguments);
      outputView_->setPlainText(QStringLiteral("Loaded parameters from a previous %1 run. Review them before running again.").arg(record.status));
      return;
    }
  }

  QMessageBox::information(this, QStringLiteral("Tool not in current catalog"),
                           QStringLiteral("The previous tool is not available in the current routed catalog. Refresh tools or reconnect to the original server."));
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
    const QString resultText = jsonText(result);
    appendOutput(QStringLiteral("Result: %1").arg(toolDisplayName(pending.toolName, pending.appName)), result);
    recordRun(pending, QStringLiteral("Success"), resultText);
    runButton_->setEnabled(bridge_->isRunning() && toolList_->currentItem() != nullptr);
    appendLog(QStringLiteral("Completed %1 in %2 ms.")
                  .arg(pending.toolName)
                  .arg(qMax<qint64>(0, pending.startedAt.msecsTo(QDateTime::currentDateTimeUtc()))));
  }
}

void MainWindow::handleRequestFailure(const QString &id, const QString &method, const QString &code,
                                      const QString &message, const QJsonObject &details) {
  if (method == QStringLiteral("call") && code == QStringLiteral("confirmation_required")) {
    PendingCall pending = pendingCalls_.take(id);
    const QString risk = details.value(QStringLiteral("risk")).toString();
    pending.risk = risk.isEmpty() ? QStringLiteral("high") : risk;
    const QJsonArray reasons = details.value(QStringLiteral("reasons")).toArray();
    QStringList reasonText;
    for (const QJsonValue &reason : reasons) reasonText << reason.toString();

    const QString body = QStringLiteral("Tool: %1\nApp: %2\nServer: %3\nRisk: %4\n\n%5\n\nAllow this action?")
                             .arg(pending.toolName, pending.appName, pending.serverName, pending.risk,
                                  reasonText.join(QStringLiteral("\n")));
    const auto decision = QMessageBox::warning(this, QStringLiteral("Guarded MCP action"), body,
                                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (decision == QMessageBox::Yes) {
      const QString retryId = bridge_->sendRequest(
          QStringLiteral("call"), {{QStringLiteral("toolName"), pending.toolName},
                                    {QStringLiteral("args"), pending.arguments},
                                    {QStringLiteral("approve"), true}});
      if (!retryId.isEmpty()) pendingCalls_.insert(retryId, pending);
    } else {
      recordRun(pending, QStringLiteral("Cancelled"), QStringLiteral("Guarded action cancelled by user."));
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
    recordRun(pending, QStringLiteral("Failed"), QStringLiteral("%1: %2").arg(code, message));
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
  const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  logLines_.append(QStringLiteral("[%1] %2").arg(stamp, cleaned));
  while (logLines_.size() > 500) logLines_.removeFirst();
}

void MainWindow::setStatus(const QString &text, bool connected) {
  statusLabel_->setText(text);
  if (connected) {
    statusLabel_->setStyleSheet(QStringLiteral("background:#111111;border:1px solid #111111;color:#ffffff;"));
    const QString name = activeConnectionName_.isEmpty() ? currentServerName() : activeConnectionName_;
    headerConnectionLabel_->setText(QStringLiteral("Connected · %1").arg(name));
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
  const QString name = currentServerName();
  const bool useStdio = transportCombo_ && transportCombo_->currentData().toString() == QStringLiteral("stdio");
  if (useStdio) {
    const QString command = stdioCommandEdit_ ? stdioCommandEdit_->text().trimmed() : QString();
    return command.isEmpty() ? QStringLiteral("%1 · Local stdio").arg(name)
                             : QStringLiteral("%1 · stdio · %2").arg(name, command);
  }
  const QString endpoint = endpointEdit_ ? endpointEdit_->text().trimmed() : QString();
  return endpoint.isEmpty() ? QStringLiteral("%1 · Streamable HTTP").arg(name)
                            : QStringLiteral("%1 · HTTP · %2").arg(name, endpoint);
}

QString MainWindow::toolCategory(const QString &name, const QString &description) {
  const QString haystack = (name + QLatin1Char(' ') + description).toLower();
  const auto containsAny = [&haystack](const QStringList &terms) {
    for (const QString &term : terms) {
      if (haystack.contains(term)) return true;
    }
    return false;
  };

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
  if (containsAny({QStringLiteral("github"), QStringLiteral("git"), QStringLiteral("repo"),
                   QStringLiteral("code"), QStringLiteral("developer"), QStringLiteral("terminal"),
                   QStringLiteral("shell"), QStringLiteral("issue"), QStringLiteral("pull request")})) {
    return QStringLiteral("Developer");
  }
  return QStringLiteral("General");
}

QString MainWindow::toolApp(const QString &name, const QString &description) {
  const QString haystack = (name + QLatin1Char(' ') + description).toLower();
  if (haystack.contains(QStringLiteral("github")) || haystack.contains(QStringLiteral("pull request")) ||
      haystack.contains(QStringLiteral("repository"))) return QStringLiteral("GitHub");
  if (haystack.contains(QStringLiteral("gmail"))) return QStringLiteral("Gmail");
  if (haystack.contains(QStringLiteral("google drive")) || haystack.contains(QStringLiteral("gdrive")) ||
      haystack.contains(QStringLiteral("google doc")) || haystack.contains(QStringLiteral("google sheet")) ||
      haystack.contains(QStringLiteral("google slide"))) return QStringLiteral("Google Drive");
  if (haystack.contains(QStringLiteral("dropbox"))) return QStringLiteral("Dropbox");
  if (haystack.contains(QStringLiteral("notion"))) return QStringLiteral("Notion");
  if (haystack.contains(QStringLiteral("slack"))) return QStringLiteral("Slack");
  if (haystack.contains(QStringLiteral("calendar"))) return QStringLiteral("Calendar");
  if (haystack.contains(QStringLiteral("browser")) || haystack.contains(QStringLiteral("web")) ||
      haystack.contains(QStringLiteral("http")) || haystack.contains(QStringLiteral("url"))) return QStringLiteral("Web");
  if (haystack.contains(QStringLiteral("file")) || haystack.contains(QStringLiteral("folder")) ||
      haystack.contains(QStringLiteral("storage"))) return QStringLiteral("Files");
  if (haystack.contains(QStringLiteral("database")) || haystack.contains(QStringLiteral("sql")) ||
      haystack.contains(QStringLiteral("csv")) || haystack.contains(QStringLiteral("analytics"))) return QStringLiteral("Data");
  if (haystack.contains(QStringLiteral("git")) || haystack.contains(QStringLiteral("code")) ||
      haystack.contains(QStringLiteral("terminal")) || haystack.contains(QStringLiteral("shell"))) return QStringLiteral("Developer");
  return QStringLiteral("General");
}

QString MainWindow::toolDisplayName(const QString &name, const QString &appName) {
  QString normalized = name;
  normalized.replace(QStringLiteral("::"), QStringLiteral(" "));
  normalized.replace(QLatin1Char('/'), QLatin1Char(' '));
  normalized.replace(QLatin1Char('.'), QLatin1Char(' '));
  normalized.replace(QLatin1Char('_'), QLatin1Char(' '));
  normalized.replace(QLatin1Char('-'), QLatin1Char(' '));
  normalized = normalized.simplified();

  QString appToken = appName.toLower();
  appToken.replace(QLatin1Char(' '), QLatin1Char(' '));
  if (!appToken.isEmpty() && normalized.toLower().startsWith(appToken + QLatin1Char(' '))) {
    normalized = normalized.mid(appToken.size() + 1).trimmed();
  }

  QStringList words = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString &word : words) {
    if (!word.isEmpty()) word = word.left(1).toUpper() + word.mid(1);
  }
  const QString display = words.join(QLatin1Char(' '));
  return display.isEmpty() ? name : display;
}

QIcon MainWindow::iconForCategory(const QString &category) const {
  if (category == QStringLiteral("Developer")) return style()->standardIcon(QStyle::SP_ComputerIcon);
  if (category == QStringLiteral("Communication")) return style()->standardIcon(QStyle::SP_MessageBoxInformation);
  if (category == QStringLiteral("Data")) return style()->standardIcon(QStyle::SP_FileDialogDetailedView);
  if (category == QStringLiteral("Web")) return style()->standardIcon(QStyle::SP_DriveNetIcon);
  if (category == QStringLiteral("Files")) return style()->standardIcon(QStyle::SP_DirIcon);
  return style()->standardIcon(QStyle::SP_FileIcon);
}

QIcon MainWindow::iconForServerStatus(const QString &status) const {
  if (status == QStringLiteral("Connected")) return style()->standardIcon(QStyle::SP_DialogApplyButton);
  if (status == QStringLiteral("Error")) return style()->standardIcon(QStyle::SP_MessageBoxWarning);
  return style()->standardIcon(QStyle::SP_DriveNetIcon);
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

bool MainWindow::endpointIsSafeToPersist(const QString &endpoint) {
  if (endpoint.isEmpty()) return true;
  const QUrl url(endpoint);
  if (!url.isValid()) return false;
  if (!url.userInfo().isEmpty()) return false;
  if (!url.query().isEmpty()) return false;
  if (!url.fragment().isEmpty()) return false;
  return url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https");
}