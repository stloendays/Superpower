#include "updatemanager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace {
QString psQuote(QString value) {
  value.replace(QLatin1Char('\''), QStringLiteral("''"));
  return QStringLiteral("'%1'").arg(value);
}

QString ensureShortcutIcon() {
#ifdef Q_OS_WIN
  const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (dataDir.isEmpty()) return {};
  QDir().mkpath(dataDir);
  const QString iconPath = QDir(dataDir).filePath(QStringLiteral("superpower-desktop.ico"));
  if (QFileInfo::exists(iconPath)) return iconPath;

  QFile png(QStringLiteral(":/icons/superpower.png"));
  if (!png.open(QIODevice::ReadOnly)) return {};
  const QByteArray payload = png.readAll();
  if (payload.isEmpty()) return {};

  // Modern Windows supports PNG-compressed images inside an ICO container.
  QSaveFile ico(iconPath);
  if (!ico.open(QIODevice::WriteOnly)) return {};
  QDataStream stream(&ico);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream << quint16(0) << quint16(1) << quint16(1);  // ICONDIR
  stream << quint8(128) << quint8(128) << quint8(0) << quint8(0);
  stream << quint16(1) << quint16(32);
  stream << quint32(payload.size()) << quint32(22);
  if (ico.write(payload) != payload.size() || !ico.commit()) return {};
  return iconPath;
#else
  return {};
#endif
}

void ensureDesktopShortcut() {
#ifdef Q_OS_WIN
  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  if (settings.value(QStringLiteral("desktop/shortcut_attempted"), false).toBool()) return;

  const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  const QString executable = QCoreApplication::applicationFilePath();
  const QString workingDirectory = QCoreApplication::applicationDirPath();
  const QString iconPath = ensureShortcutIcon();
  if (desktop.isEmpty() || executable.isEmpty()) return;

  const QString shortcut = QDir(desktop).filePath(QStringLiteral("Superpower Desktop.lnk"));
  const QString script = QStringLiteral(
      "$ws=New-Object -ComObject WScript.Shell;"
      "$s=$ws.CreateShortcut(%1);"
      "$s.TargetPath=%2;"
      "$s.WorkingDirectory=%3;"
      "$s.Description='Superpower MCP Desktop';"
      "%4"
      "$s.Save();")
      .arg(psQuote(QDir::toNativeSeparators(shortcut)),
           psQuote(QDir::toNativeSeparators(executable)),
           psQuote(QDir::toNativeSeparators(workingDirectory)),
           iconPath.isEmpty()
               ? QString()
               : QStringLiteral("$s.IconLocation=%1;").arg(psQuote(QDir::toNativeSeparators(iconPath))));

  const bool started = QProcess::startDetached(
      QStringLiteral("powershell.exe"),
      {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
       QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
       QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
       QStringLiteral("-Command"), script});
  if (started) settings.setValue(QStringLiteral("desktop/shortcut_attempted"), true);
#endif
}

void initializeDesktopFeatures() {
  QTimer::singleShot(0, qApp, []() {
    const QIcon icon(QStringLiteral(":/icons/superpower.png"));
    if (!icon.isNull()) QApplication::setWindowIcon(icon);
    ensureDesktopShortcut();

    auto *updater = new UpdateManager(qApp);
    QObject::connect(updater, &UpdateManager::updateAvailable, qApp,
                     [](const QString &version) {
                       qInfo().noquote() << "Superpower Desktop update available:" << version;
                     });
    QObject::connect(updater, &UpdateManager::updateReadyToInstall, qApp,
                     [updater](const QString &version) {
                       const QMessageBox::StandardButton choice = QMessageBox::question(
                           nullptr, QStringLiteral("Superpower Desktop Update"),
                           QStringLiteral("Superpower Desktop %1 has been downloaded and verified.\n\n"
                                          "Restart now to install the update?")
                               .arg(version),
                           QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                       if (choice == QMessageBox::Yes) updater->installDownloadedUpdate();
                     });
    QObject::connect(updater, &UpdateManager::updateError, qApp,
                     [](const QString &message, bool userInitiated) {
                       qWarning().noquote() << message;
                       if (userInitiated) {
                         QMessageBox::warning(nullptr, QStringLiteral("Superpower Desktop Update"), message);
                       }
                     });
    updater->scheduleStartupCheck();
  });
}
}  // namespace

Q_COREAPP_STARTUP_FUNCTION(initializeDesktopFeatures)
