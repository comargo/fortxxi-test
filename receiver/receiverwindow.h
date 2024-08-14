#ifndef RECEIVERWINDOW_H
#define RECEIVERWINDOW_H

#include <QMainWindow>

class QCustomPlot;
class QUdpSocket;
class QSpinBox;

class ReceiverWindowPrivate;
class ReceiverWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ReceiverWindow(QWidget *parent = nullptr);
    ~ReceiverWindow();

public:
    Q_SLOT void readDatagram();
    Q_SLOT void listenClicked(bool checked);

private:
    Q_DISABLE_COPY(ReceiverWindow)

    QUdpSocket *udpSocket = nullptr;
    QSpinBox *portSpinbox = nullptr;
    QCustomPlot *plot = nullptr;
};

#endif //RECEIVERWINDOW_H
