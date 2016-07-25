#include "workWithDevices.h"


workWithDevices::workWithDevices(QObject *parent) : QObject(parent)
{
    serverStatus=0;
}


bool workWithDevices::initDBconnection(){
    QSettings *settings = new QSettings("settings.ini",QSettings::IniFormat);
    QTextStream in_s(stdin);
    QTextStream out_s(stdout, QIODevice::WriteOnly);

    if(settings->value("Database_Settings/hostName").toString()==""){
        out_s << "Enter hostName:"<< "\t";
        out_s.flush();
        QString hostName = in_s.readLine();
        settings->setValue("Database_Settings/hostName", hostName);
        settings->sync();
    }

    if(settings->value("Database_Settings/databaseName").toString()==""){
        out_s << "Enter databaseName:"<< "\t";
        out_s.flush();
        QString databaseName = in_s.readLine();
        settings->setValue("Database_Settings/databaseName", databaseName);
        settings->sync();
    }

    if(settings->value("Database_Settings/userName").toString()==""){
        out_s << "Enter userName:"<< "\t";
        out_s.flush();
        QString userName = in_s.readLine();
        settings->setValue("Database_Settings/userName", userName);
        settings->sync();
    }

    if(settings->value("Database_Settings/password").toString()==""){
        out_s << "Enter password:"<< "\t";
        out_s.flush();
        QString password = in_s.readLine();
        settings->setValue("Database_Settings/password", password);
        settings->sync();
    }


    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(settings->value("Database_Settings/hostName").toString());
    db.setDatabaseName(settings->value("Database_Settings/databaseName").toString());
    db.setUserName(settings->value("Database_Settings/userName").toString());
    db.setPassword(settings->value("Database_Settings/password").toString());
    if(!db.open()) {
        qWarning() << __FUNCTION__ << db.lastError().text();
        return 0;
    }

    initTimer = new QTimer(this);
    connect (initTimer, SIGNAL(timeout()), this, SLOT(initDevices()));
    initTimer->start(10*60*1000);
    initDevices();
    startListeningData();
    return 1;
}


void workWithDevices::parseData(QString toParse, QTcpSocket* clientSocket){
    // Формат: METEO,S0001,T-20.15,H100
    // METEO - тип устройства, Может быть RFID - эт для посонов
    // S-Уникальный номер устройства
    // T-Температура *С
    // H-Влажность %
    // GATE - посоны
    // ФОРМАТ: GATE,UPD - отослать девайсу активные ключи
    // ФОРМАТ: GATE,INSIDE,1234567890 - записать UID вошедшего в БДшечку
    // ФОРМАТ: GATE,NEW,1234567890- отослать девайсу активные ключи

    qDebug() << toParse;
    QStringList toParseSplitted = toParse.split(',');

    if(toParseSplitted[0].startsWith("METEO")){

        toParseSplitted.replaceInStrings(QRegExp("^(S|T|H)"), "");
        this->insertInDB(toParseSplitted, toParseSplitted[0]);
        return;
    }
    else if(toParseSplitted[0].startsWith("GATE")){
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
        qDebug() << QString::fromUtf8("Неизвестная строка");
        os << "<html>\n"
              "\t<head>\n"
              "\t<meta charset='utf-8'>\n"
              "\t</head>\n"
              "\t<body>\n"
               <<QString::fromUtf8("Здесь не на что смотреть.")<<
              "\n\t</body>\n"
              "</html>";
        return;
    }
}

void workWithDevices::insertInDB(QStringList toInsert, QString type){
    QSqlQuery query;
    if(type=="METEO"){
        query.prepare("INSERT INTO "+toInsert[0]+toInsert[1]+"(temperature, humidity, datetime)"
                                                             " VALUES (:temperature, :humidity, :datetime);");
        query.bindValue(":temperature", toInsert[2]);
        query.bindValue(":humidity", toInsert[3]);
        query.bindValue(":datetime", "now()");
    }else if(type=="GATE"){
        if(toInsert[1]=="NEW")
        {
            query.prepare("INSERT INTO gate_keys(uid, insert_datetime) VALUES (:uid,:datetime);");
            query.bindValue(":uid", toInsert[2]);
            query.bindValue(":datetime","now()");
        }
        else if(toInsert[1]=="INSIDE")
        {
            query.prepare("insert into gate_journal(name, datetime)"
                          "VALUES ((SELECT name from gate_keys where uid=:uid),:datetime);");
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
        qDebug() << QString::fromUtf8("Сервер не запущен: ") + tcpServer->errorString();
    }
    else
    {
        qDebug() << QString::fromUtf8("Сервер запущен: ") + tcpServer->serverAddress().toString()
                            + QString::fromUtf8(":") + QString::number(tcpServer->serverPort());
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
    query.exec("SELECT id, UID FROM gate_keys WHERE isactive=true");
    while (query.next()) {
        //int id = query.value(0).toInt();
        QString UID = query.value(1).toString();
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

