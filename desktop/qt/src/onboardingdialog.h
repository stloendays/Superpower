#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTimer;
class MainWindow;

class OnboardingDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit OnboardingDialog(MainWindow *workspace);

  void showForCurrentState();

 protected:
  void closeEvent(QCloseEvent *event) override;

 private:
  void buildUi();
  void applyStyle();
  void refreshState();
  void focusServerSetup();
  void connectCurrentServer();
  void discoverActions();
  void bringWorkspaceForward();
  void markSeen(bool completed);

  QPushButton *findWorkspaceButton(const QString &text) const;
  int savedServerCount() const;
  int discoveredToolCount() const;
  bool isWorkspaceConnected() const;

  MainWindow *workspace_ = nullptr;
  QLabel *progressLabel_ = nullptr;
  QLabel *serverStatusLabel_ = nullptr;
  QLabel *connectionStatusLabel_ = nullptr;
  QLabel *toolsStatusLabel_ = nullptr;
  QPushButton *serverActionButton_ = nullptr;
  QPushButton *connectionActionButton_ = nullptr;
  QPushButton *toolsActionButton_ = nullptr;
  QPushButton *finishButton_ = nullptr;
  QTimer *refreshTimer_ = nullptr;
};
