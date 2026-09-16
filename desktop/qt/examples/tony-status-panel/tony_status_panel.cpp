#include "tony_status_panel.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

QLabel *makeTitle(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    label->setFont(font);
    return label;
}

QLabel *makeKey(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

TonyStatusPanel::TonyStatusPanel(QWidget *parent)
    : QFrame(parent),
      moodValue_(new QLabel("Happy")),
      taskValue_(new QLabel("Waiting for interaction")),
      temperatureValue_(new QLabel("Comfortable")),
      actionValue_(new QLabel("Idle"))
{
    setObjectName("TonyStatusPanel");
    setFrameShape(QFrame::StyledPanel);

    auto *title = makeTitle("Tony Status");

    auto *grid = new QGridLayout;
    grid->addWidget(makeKey("Mood"), 0, 0);
    grid->addWidget(moodValue_, 0, 1);

    grid->addWidget(makeKey("Current task"), 1, 0);
    grid->addWidget(taskValue_, 1, 1);

    grid->addWidget(makeKey("Temperature"), 2, 0);
    grid->addWidget(temperatureValue_, 2, 1);

    grid->addWidget(makeKey("Last action"), 3, 0);
    grid->addWidget(actionValue_, 3, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(grid);

    setStyleSheet(R"(
        QFrame#TonyStatusPanel {
            border: 1px solid palette(mid);
            border-radius: 10px;
            padding: 10px;
        }
    )");
}

void TonyStatusPanel::setMood(const QString &mood)
{
    moodValue_->setText(mood);
}

void TonyStatusPanel::setCurrentTask(const QString &task)
{
    taskValue_->setText(task);
}

void TonyStatusPanel::setTemperature(double celsius)
{
    QString state = "Comfortable";

    if (celsius < 18.0) {
        state = "Cold";
    } else if (celsius > 28.0) {
        state = "Warm";
    }

    temperatureValue_->setText(
        QString("%1 C - %2").arg(celsius, 0, 'f', 1).arg(state));
}

void TonyStatusPanel::setLastAction(const QString &action)
{
    actionValue_->setText(action);
}
