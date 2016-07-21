#include "workWithDevices.h"


workWithDevices::workWithDevices(QObject *parent) : QObject(parent)
{
    serverStatus=0;
}


bool workWithDevices::initDBconnection(){
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setDatabaseName("mydb");
    db.setUserName("test_user");
    db.setPassword("qwerty");
    if(!db.open()) {
        qWarning() << __FUNCTION__ << db.lastError().text();
        return 0;
    }
    return 1;
}


void workWithDevices::parseData(QString toParse, QTcpSocket* clientSocket){
    // Формат: METEO,S0001,T-20.15,H100
    // METEO - тип устройства, Может быть RFID - эт для посонов
    // S-Уникальный номер устройства
    // T-Температура *С
    // H-Влажность %
    // LOCKER - посоны
    // ФОРМАТ: LOCKER,UPD - отослать девайсу активные ключи
    // ФОРМАТ: LOCKER,INSIDE,1234567890 - записать UID вошедшего в БДшечку
    // ФОРМАТ: LOCKER,NEW,1234567890- отослать девайсу активные ключи

    qDebug() << toParse;
    QStringList toParseSplitted = toParse.split(',');

    if(toParseSplitted[0].startsWith("METEO")){

        toParseSplitted.replaceInStrings(QRegExp("^(S|T|H)"), "");
        this->insertInDB(toParseSplitted, toParseSplitted[0]);
        return;
    }
    else if(toParseSplitted[0].startsWith("LOCKER")){
        if(toParseSplitted[1].startsWith("UPD"))
        {
            // отправка ключей в EEPROM
            this->sendUIDs(clientSocket);
            return;
        }
        else if(toParseSplitted[1].startsWith("NEW"))
        {
            // запись нового ключа
            this->insertInDB(toParseSplitted, toParseSplitted[0]);
            return;
        }
        else if(toParseSplitted[1].startsWith("INSIDE")){
            // запись вошедшего
            this->insertInDB(toParseSplitted, toParseSplitted[0]);
            return;
        }
    }
    else{
        QTextStream os(clientSocket);
        qDebug() << "TbI BTuPaEIIIb MHE KAKYJUTO DICH! o_O";
        os << "TbI BTuPaEIIIb MHE KAKYJUTO DICH! o_O";
        return;
    }
}

void workWithDevices::insertInDB(QStringList toInsert, QString type){
    QSqlQuery query;
    if(type=="METEO"){
        query.prepare("INSERT INTO "+toInsert[0]+toInsert[1]+"(temperature, humidity, datetime) VALUES (:temperature, :humidity, :datetime);");
        query.bindValue(":temperature", toInsert[2]);
        query.bindValue(":humidity", toInsert[3]);
        query.bindValue(":datetime", "now()");
    }else if(type=="LOCKER"){
        if(toInsert[1]=="NEW")
        {
            query.prepare("INSERT INTO locker_users(uid) VALUES (:uid);");
            query.bindValue(":uid", toInsert[2]);
        }
        else if(toInsert[1]=="INSIDE")
        {
            query.prepare("INSERT INTO locker_journal(uid, datetime) VALUES (:uid, :datetime);");
            query.bindValue(":uid", toInsert[2]);
            query.bindValue(":datetime", "now()");
        }
    }

    if(!query.exec()){
        qWarning() << __FUNCTION__ << query.lastError().text();
    }
}

void workWithDevices::initDevices(){
    udpServer = new QUdpSocket(this);
    QByteArray datagram = "INIT"; // + QByteArray::number(messageNo);

    udpServer->writeDatagram(datagram.data(), datagram.size(),
                             QHostAddress::Broadcast, 45454);
}

void workWithDevices::startListeningData(){
    tcpServer = new QTcpServer(this);
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    if(!tcpServer->listen(QHostAddress::Any, 7364) && serverStatus==0){
        qDebug() << QString::fromUtf8("Server does not started :(");
    }
    else
    {
        qDebug() << "Server started!";
        serverStatus=1;
    }
}

void workWithDevices::newConnection(){
    QTcpSocket* clientSocket = tcpServer->nextPendingConnection();
    int clientSocketID = clientSocket->socketDescriptor();
    socketClients[clientSocketID]=clientSocket;
    connect(socketClients[clientSocketID], SIGNAL(readyRead()), this, SLOT(readFromClient()));
}

void workWithDevices::readFromClient(){
    QTcpSocket* clientSocket = (QTcpSocket*)sender();
    int clientSocketID = clientSocket->socketDescriptor();

    while(clientSocket->waitForReadyRead());
    QByteArray volosa = clientSocket->readAll();
    this->parseData(volosa, clientSocket);
    clientSocket->close();
    socketClients.remove(clientSocketID);
}

void workWithDevices::sendUIDs(QTcpSocket* clientSocket){
    QTextStream os(clientSocket);
    os.setAutoDetectUnicode(true);
    QSqlQuery query;
    query.exec("SELECT id, UID FROM locker_users WHERE active=1");
    while (query.next()) {
        //int id = query.value(0).toInt();
        u_int8_t UID = query.value(1).toUInt();
        // qDebug() << id << UIDdec;
        os << UID << "\n";
    }

}

void workWithDevices::stopListeningData(){
    if(serverStatus==1){
        foreach(int i, socketClients.keys()){
            socketClients[i]->close();
            socketClients.remove(i);
        }
        tcpServer->close();
        serverStatus=0;
        qDebug() << QString::fromUtf8("Сервер остановлен!");
    }
}

