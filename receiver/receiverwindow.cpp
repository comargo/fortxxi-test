#include "receiverwindow.h"

#include "qcustomplot.h"

#include <QtDebug>

#include <QBoxLayout>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QPushButton>
#include <QSpinBox>
#include <QtEndian>

// Copy of structure from sender subproject
struct data_message_t { // all fields are 64bit long to simplify data alignment in communication
    quint64_be secs; //timespec.tv_sec
    quint64_be nsecs; //timespec.tv_usec
    quint64_be ncpu; // number of cpu lines in /proc/stat equal to number of CPUs+total
    double load_avg[]; // load averages
};

ReceiverWindow::ReceiverWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(800,600);
    setCentralWidget(new QWidget);
    auto mainLayout = new QVBoxLayout();
    centralWidget()->setLayout(mainLayout);

    auto serverLayout = new QHBoxLayout();
    mainLayout->addLayout(serverLayout);

    portSpinbox = new QSpinBox();
    portSpinbox->setObjectName(QStringLiteral("portSpinbox"));
    serverLayout->addWidget(portSpinbox);
    portSpinbox->setRange(1, 65535);
    portSpinbox->setValue(1234);

    auto startListenBtn = new QPushButton(QStringLiteral("Listen"));
    serverLayout->addWidget(startListenBtn);
    startListenBtn->setCheckable(true);
    startListenBtn->setChecked(false);
    connect(startListenBtn, &QPushButton::clicked, this, &ReceiverWindow::listenClicked);

    // Push all above controls to left side
    serverLayout->addStretch(1);

    plot = new QCustomPlot();
    mainLayout->addWidget(plot, 1); // make plot to use all free space
    plot->xAxis->setLabel(tr("Time"));
    plot->yAxis->setLabel(tr("Processor usage (%)"));
    plot->yAxis->setRange(0,100);
    auto timeTicker = QSharedPointer<QCPAxisTickerDateTime>::create();
    plot->xAxis->setTicker(timeTicker);

    setStatusBar(new QStatusBar());
}

ReceiverWindow::~ReceiverWindow()
{

}

void ReceiverWindow::readDatagram()
{
    // Without digging into the Qt socket architecture:
    // There could be race condition, when listen button is already pressed, but
    // new datagram signal is already on its way to reciever
    if (!udpSocket)
        return;

    while (udpSocket->hasPendingDatagrams()) {
        auto datagram = udpSocket->receiveDatagram();
        auto data = datagram.data();
        auto message =
            reinterpret_cast<const struct data_message_t *>(data.constData());
        if(plot->graphCount() < message->ncpu) {
            for(int i=plot->graphCount(); i < message->ncpu; ++i) {
                auto graph = plot->addGraph();
                QPen graphPen;
                if(i==0) {
                    graphPen.setColor(Qt::black);
                    graph->setName(tr("CPU Total"));
                }
                else {
                    auto penColor = QColor::fromHsvF((i-1)*1.0/message->ncpu, 1.0, 1.0);
                    graphPen.setColor(penColor);
                    graph->setName(tr("CPU%1").arg(i-1));
                }
                graph->setPen(graphPen);
            }
        }
        for (int i = 0; i < message->ncpu; ++i) {
            plot->graph(i)->addData(message->secs+message->nsecs/1000000000.0, message->load_avg[i]*100);
        }
        bool foundRange;
        auto range = plot->graph(0)->getKeyRange(foundRange);
        if(range.size() < 5*60) {
            range.upper = range.lower+5*60;
        }
        else {
            range.lower = range.upper-5*60;
        }
        plot->xAxis->setRange(range);
        plot->replot();
    }
}

void ReceiverWindow::listenClicked(bool checked)
{
    if(checked) {
        plot->clearGraphs();
        udpSocket = new QUdpSocket(this);
        udpSocket->bind(portSpinbox->value());
        connect(udpSocket, &QIODevice::readyRead, this, &ReceiverWindow::readDatagram);
    }
    else {
        udpSocket->deleteLater();
        udpSocket = nullptr;
    }

    portSpinbox->setDisabled(checked);
}
