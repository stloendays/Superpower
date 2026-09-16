#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class UpdateManager final : public QObject {
  Q_OBJECT

public:
  explicit UpdateManager(QObject *parent = nullptr);

  void scheduleStartupCheck();
  void checkForUpdates(bool userInitiated = false);
  void installDownloadedUpdate();

  bool updateReady() const;
  QString availableVersion() const;

signals:
  void statusChanged(const QString &message);
  void updateAvailable(const QString &version);
  void updateReadyToInstall(const QString &version);
  void alreadyUpToDate(const QString &version);
  void updateError(const QString &message, bool userInitiated);

private:
  QString currentVersion() const;
  bool isNewerVersion(const QString &candidate) const;
  bool isManagedInstall() const;
  void fetchChecksumAndPackage();
  void downloadPackage();
  void fail(const QString &message);

  QNetworkAccessManager network_;
  QString availableVersion_;
  QString assetName_;
  QUrl packageUrl_;
  QUrl checksumsUrl_;
  QByteArray expectedSha256_;
  QString downloadedPackage_;
  bool packageIsInstaller_{false};
  bool checking_{false};
  bool downloading_{false};
  bool userInitiated_{false};
};
