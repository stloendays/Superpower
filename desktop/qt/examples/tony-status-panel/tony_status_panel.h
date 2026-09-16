#pragma once

#include <QFrame>
#include <QString>

class QLabel;

class TonyStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit TonyStatusPanel(QWidget *parent = nullptr);

    void setMood(const QString &mood);
    void setCurrentTask(const QString &task);
    void setTemperature(double celsius);
    void setLastAction(const QString &action);

private:
    QLabel *moodValue_;
    QLabel *taskValue_;
    QLabel *temperatureValue_;
    QLabel *actionValue_;
};
