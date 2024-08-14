#include "receiverwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ReceiverWindow w;
    w.show();
    return a.exec();
}
