#include "updatemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

namespace {
constexpr auto kLatestReleaseApi = "https://api.github.com/repos/stloendays/Superpower/releases/latest";

QNetworkRequest githubRequest(const QUrl &url) {
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QByteArray("SuperpowerDesktop/") + QCoreApplication::applicationVersion().toUtf8());
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  return request;
}

QString safeTempArchivePath(const QString &version) {
  QString safeVersion = version;
  safeVersion.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
  return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
      .filePath(QStringLiteral("Superpower-Desktop-update-%1.zip").arg(safeVersion));
}

QString normalizeAssetName(QString name) {
  name = name.trimmed();
  if (name.startsWith(QLatin1Char('*'))) name.remove(0, 1);
  return name.trimmed();
}
}  // namespace

UpdateManager::UpdateManager(QObject *parent) : QObject(parent) {}

QString UpdateManager::currentVersion() const {
  const QString value = QCoreApplication::applicationVersion().trimmed();
  return value.isEmpty() ? QStringLiteral("0.0.0") : value;
}

bool UpdateManager::isNewerVersion(const QString &candidate) const {
  const QVersionNumber current = QVersionNumber::fromString(currentVersion());
  const QVersionNumber offered = QVersionNumber::fromString(candidate);
  if (current.isNull() || offered.isNull()) return candidate != currentVersion();
  return QVersionNumber::compare(offered, current) > 0;
}

bool UpdateManager::updateReady() const {
  return !downloadedArchive_.isEmpty() && QFileInfo::exists(downloadedArchive_);
}

QString UpdateManager::availableVersion() const { return availableVersion_; }

void UpdateManager::scheduleStartupCheck() {
  QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
  if (!settings.value(QStringLiteral("updates/automatic"), true).toBool()) return;

  const QDateTime now = QDateTime::currentDateTimeUtc();
  const QDateTime last = QDateTime::fromString(
      settings.value(QStringLiteral("updates/last_check_utc")).toString(), Qt::ISODate);
  if (last.isValid() && last.secsTo(now) < 12 * 60 * 60) return;

  QTimer::singleShot(8000, this, [this]() { checkForUpdates(false); });
}

void UpdateManager::checkForUpdates(bool userInitiated) {
  if (checking_ || downloading_) return;
  checking_ = true;
  userInitiated_ = userInitiated;
  emit statusChanged(QStringLiteral("Checking for Superpower Desktop updates..."));

  auto *reply = network_.get(githubRequest(QUrl(QString::fromLatin1(kLatestReleaseApi))));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    checking_ = false;
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      const QString message = QStringLiteral("Update check failed: %1").arg(reply->errorString());
      reply->deleteLater();
      fail(message);
      return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
      reply->deleteLater();
      fail(QStringLiteral("GitHub returned an invalid release response."));
      return;
    }

    const QJsonObject release = document.object();
    QString version = release.value(QStringLiteral("tag_name")).toString().trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) version.remove(0, 1);
    if (QVersionNumber::fromString(version).isNull()) {
      reply->deleteLater();
      fail(QStringLiteral("Latest GitHub release has an invalid version tag."));
      return;
    }

    QSettings settings(QStringLiteral("Superpower"), QStringLiteral("Superpower Desktop"));
    settings.setValue(QStringLiteral("updates/last_check_utc"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!isNewerVersion(version)) {
      emit statusChanged(QStringLiteral("Superpower Desktop is up to date."));
      if (userInitiated_) emit alreadyUpToDate(currentVersion());
      reply->deleteLater();
      return;
    }

    availableVersion_ = version;
    assetName_ = QStringLiteral("Superpower-Desktop-%1-Windows-x64.zip").arg(version);
    packageUrl_ = {};
    checksumsUrl_ = {};

    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &assetValue : assets) {
      const QJsonObject asset = assetValue.toObject();
      const QString name = asset.value(QStringLiteral("name")).toString();
      const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
      if (name == assetName_) packageUrl_ = url;
      if (name == QStringLiteral("SHA256SUMS")) checksumsUrl_ = url;
    }

    if (!packageUrl_.isValid() || !checksumsUrl_.isValid()) {
      reply->deleteLater();
      fail(QStringLiteral("The latest release is missing the Windows desktop package or SHA256SUMS."));
      return;
    }

    emit updateAvailable(availableVersion_);
    emit statusChanged(QStringLiteral("Superpower Desktop %1 is available. Downloading verified update...")
                           .arg(availableVersion_));
    reply->deleteLater();
    fetchChecksumAndPackage();
  });
}

void UpdateManager::fetchChecksumAndPackage() {
  downloading_ = true;
  auto *reply = network_.get(githubRequest(checksumsUrl_));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      downloading_ = false;
      const QString message = QStringLiteral("Could not download update checksum: %1").arg(reply->errorString());
      reply->deleteLater();
      fail(message);
      return;
    }

    expectedSha256_.clear();
    const QList<QByteArray> lines = raw.split('\n');
    const QRegularExpression pattern(QStringLiteral("^([0-9a-fA-F]{64})\\s+(.+)$"));
    for (const QByteArray &line : lines) {
      const QRegularExpressionMatch match = pattern.match(QString::fromUtf8(line).trimmed());
      if (!match.hasMatch()) continue;
      if (normalizeAssetName(match.captured(2)) == assetName_) {
        expectedSha256_ = match.captured(1).toLatin1().toLower();
        break;
      }
    }

    reply->deleteLater();
    if (expectedSha256_.size() != 64) {
      downloading_ = false;
      fail(QStringLiteral("SHA256SUMS does not contain the Windows desktop update package."));
      return;
    }
    downloadPackage();
  });
}

void UpdateManager::downloadPackage() {
  auto *reply = network_.get(githubRequest(packageUrl_));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    downloading_ = false;
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      const QString message = QStringLiteral("Update download failed: %1").arg(reply->errorString());
      reply->deleteLater();
      fail(message);
      return;
    }

    const QByteArray actualSha256 =
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().toLower();
    if (actualSha256 != expectedSha256_) {
      reply->deleteLater();
      fail(QStringLiteral("Downloaded update failed SHA-256 verification."));
      return;
    }

    const QString archivePath = safeTempArchivePath(availableVersion_);
    QSaveFile file(archivePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) {
      reply->deleteLater();
      fail(QStringLiteral("Could not save the verified update package."));
      return;
    }

    downloadedArchive_ = archivePath;
    emit statusChanged(QStringLiteral("Superpower Desktop %1 is verified and ready to install.")
                           .arg(availableVersion_));
    emit updateReadyToInstall(availableVersion_);
    reply->deleteLater();
  });
}

void UpdateManager::installDownloadedUpdate() {
  if (!updateReady()) {
    fail(QStringLiteral("No verified update is ready to install."));
    return;
  }

#ifdef Q_OS_WIN
  const QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  const QString scriptPath = QDir(temp).filePath(QStringLiteral("Superpower-Desktop-apply-update.ps1"));
  const QString script = QString::fromUtf8(R"PS1(param(
  [int]$ProcessId,
  [string]$Archive,
  [string]$Destination,
  [string]$Executable
)
$ErrorActionPreference = 'Stop'
try {
  Wait-Process -Id $ProcessId -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 500
  $stage = Join-Path $env:TEMP ("SuperpowerDesktopUpdate-" + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force -Path $stage | Out-Null
  Expand-Archive -LiteralPath $Archive -DestinationPath $stage -Force
  $candidate = Join-Path $stage 'Superpower Desktop.exe'
  if (-not (Test-Path -LiteralPath $candidate)) { throw 'Update package is missing Superpower Desktop.exe' }
  & robocopy $stage $Destination /E /R:3 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }
  Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $Archive -Force -ErrorAction SilentlyContinue
  Start-Process -FilePath (Join-Path $Destination $Executable) -WorkingDirectory $Destination
  Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue
} catch {
  Add-Content -LiteralPath (Join-Path $env:TEMP 'Superpower-Desktop-update-error.log') -Value ("{0:o} {1}" -f (Get-Date), $_.Exception.Message)
  exit 1
}
)PS1");

  QSaveFile scriptFile(scriptPath);
  const QByteArray scriptBytes = script.toUtf8();
  if (!scriptFile.open(QIODevice::WriteOnly) ||
      scriptFile.write(scriptBytes) != scriptBytes.size() || !scriptFile.commit()) {
    fail(QStringLiteral("Could not prepare the Windows update helper."));
    return;
  }

  const QStringList arguments{
      QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
      QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
      QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
      QStringLiteral("-File"), scriptPath,
      QStringLiteral("-ProcessId"), QString::number(QCoreApplication::applicationPid()),
      QStringLiteral("-Archive"), downloadedArchive_,
      QStringLiteral("-Destination"), QCoreApplication::applicationDirPath(),
      QStringLiteral("-Executable"), QFileInfo(QCoreApplication::applicationFilePath()).fileName()};

  if (!QProcess::startDetached(QStringLiteral("powershell.exe"), arguments)) {
    fail(QStringLiteral("Could not start the Windows update helper."));
    return;
  }
  QCoreApplication::quit();
#else
  fail(QStringLiteral("Automatic installation is currently supported on Windows only."));
#endif
}

void UpdateManager::fail(const QString &message) {
  emit statusChanged(message);
  emit updateError(message, userInitiated_);
}
