#ifndef workWithDevices_H
#define workWithDevices_H

#include <QObject>
#include <QtNetwork>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QDebug>
#include <QMap>
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
    bool initDBconnection();
    void initDevices(); // Рассылает мултикаст INIT
    void startListeningData();
signals:
    // void readfromclientOK(QByteArray data);

private slots:
    void newConnection();
    void readFromClient();
    void stopListeningData();


private:
    void insertInDB(QStringList toInsert, QString type);
    void parseData(QString toParse, QTcpSocket* clientSocket);
    void sendUIDs(QTcpSocket* clientSocket);

    QUdpSocket *udpServer;
    QTcpServer *tcpServer;
    int serverStatus;
    QMap <int,QTcpSocket *> socketClients;
    QSqlDatabase db;

};

#endif // workWithDevices_H
