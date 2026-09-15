#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("Superpower Desktop"));
  QCoreApplication::setOrganizationName(QStringLiteral("Superpower"));
  QCoreApplication::setApplicationVersion(QStringLiteral(SUPERPOWER_DESKTOP_VERSION));

  QFont font = app.font();
  font.setPointSize(10);
  app.setFont(font);

  MainWindow window;
  window.show();
  return app.exec();
}
