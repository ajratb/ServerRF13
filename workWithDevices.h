#ifndef workWithDevices_H
#define workWithDevices_H

#include <QObject>
#include <QtNetwork>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QDebug>
#include <QtSql>
#include <QSqlQuery>
#include <QDateTime>
#include <QSettings>

//class QTcpServer;

class workWithDevices : public QObject
{
    Q_OBJECT
public:
    explicit workWithDevices(QObject *parent = 0);
    void initDBconnection();
    void startListeningData();
signals:

public slots:
    void initDevices(); // Рассылает мултикаст INIT

private slots:
    void newConnection();
    void readFromClient();


private:
    void parseData(QString toParse, QTcpSocket* clientSocket);
    void writeMeteoMeasurements(QStringList toInsert);
    void sendMasterUIDs(QTcpSocket* clientSocket);
    void INSIDE(QTcpSocket* clientSocket, QString uid);
    void writeNewKey(QString newKey);

    QUdpSocket *udpServer;
    QTcpServer *tcpServer;
    QTimer *initTimer;
    QSqlDatabase db;

};

#endif // workWithDevices_H
