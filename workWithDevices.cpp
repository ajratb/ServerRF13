#include "workWithDevices.h"


workWithDevices::workWithDevices(QObject *parent) : QObject(parent)
{
}

void workWithDevices::initDBconnection(){
    QSettings settings("settings.ini",QSettings::IniFormat);
    QTextStream in_s(stdin);
    QTextStream out_s(stdout, QIODevice::WriteOnly);

    if(settings.value("Database_Settings/hostName").toString()==""){
        out_s << "Enter hostName:"<< "\t";
        out_s.flush();
        QString hostName = in_s.readLine();
        settings.setValue("Database_Settings/hostName", hostName);
        settings.sync();
    }

    if(settings.value("Database_Settings/databaseName").toString()==""){
        out_s << "Enter databaseName:"<< "\t";
        out_s.flush();
        QString databaseName = in_s.readLine();
        settings.setValue("Database_Settings/databaseName", databaseName);
        settings.sync();
    }

    if(settings.value("Database_Settings/userName").toString()==""){
        out_s << "Enter userName:"<< "\t";
        out_s.flush();
        QString userName = in_s.readLine();
        settings.setValue("Database_Settings/userName", userName);
        settings.sync();
    }

    if(settings.value("Database_Settings/password").toString()==""){
        out_s << "Enter password:"<< "\t";
        out_s.flush();
        QString password = in_s.readLine();
        settings.setValue("Database_Settings/password", password);
        settings.sync();
    }


    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(settings.value("Database_Settings/hostName").toString());
    db.setDatabaseName(settings.value("Database_Settings/databaseName").toString());
    db.setUserName(settings.value("Database_Settings/userName").toString());
    db.setPassword(settings.value("Database_Settings/password").toString());
    if(!db.open()) {
        qWarning() << __FUNCTION__ << db.lastError().text();
    } else {
    initTimer = new QTimer(this);
    connect (initTimer, SIGNAL(timeout()), this, SLOT(initDevices()));
    initTimer->start(60*1000/6);
    initDevices();
    startListeningData();
    }
}


void workWithDevices::parseData(QString toParse, QTcpSocket* clientSocket){
    // Формат: METEO,S0001,T-20.15,H100
    // METEO - тип устройства, Может быть RFID - эт для посонов
    // N-Уникальный номер устройства
    // T-Температура *С
    // H-Влажность %
    // S-серийник датчика
    // GATE - посоны
    // ФОРМАТ: GATE,UPD - отослать девайсу активные ключи
    // ФОРМАТ: GATE,INSIDE,1234567890 - записать UID вошедшего в БДшечку
    // ФОРМАТ: GATE,NEW,1234567890- отослать девайсу активные ключи

    qDebug() << toParse;
    QStringList toParseSplitted = toParse.split(',');

    if(toParseSplitted[0].startsWith("METEO")){

        toParseSplitted.replaceInStrings(QRegExp("^(S|T|H|N)"), "");
        this->writeMeteoMeasurements(toParseSplitted);
        return;
    }
    else if(toParseSplitted[0].startsWith("GATE")){
        if(toParseSplitted[1].startsWith("UPD"))
        {
            // отправка ключей в EEPROM
            this->sendMasterUIDs(clientSocket);
            return;
        }
        else if(toParseSplitted[1].startsWith("NEW"))
        {
            // запись нового ключа
            this->writeNewKey(toParseSplitted[2]);
            return;
        }
        else if(toParseSplitted[1].startsWith("INSIDE")){
            // запись вошедшего
            // this->insertInDB(toParseSplitted, toParseSplitted[0]);
            this->INSIDE(clientSocket, toParseSplitted[2]);
            return;
        }
    }
}

void workWithDevices::writeMeteoMeasurements(QStringList toInsert){
    QSqlQuery query;
        int i=2;
        do{
         // Нумерация с 0, начинаем с 2 элемента, т.к. в 0 и 1 заведомо нет нужных данных
            QString Serial = toInsert[i++];
            query.prepare("SELECT id FROM meteo_sensors WHERE serial_number=:serial;");
            query.bindValue(":serial", Serial);
            if(!query.exec()){
                qWarning() << __FUNCTION__ << query.lastError().text();
            }
            if (!query.next()){
                query.prepare("INSERT INTO meteo_sensors(serial_number) VALUES (:serial);");
                query.bindValue(":serial", Serial);
                if(!query.exec()){
                    qWarning() << __FUNCTION__ << query.lastError().text();
                }
            }
            if (Serial.toInt() == 256){ // DHT
                query.prepare("INSERT INTO "+toInsert[0]+toInsert[1]+"(temperature, humidity, serial_id, datetime)"
                                                                     " VALUES (:temperature, :humidity, "
                                                                     "(SELECT id from meteo_sensors where serial_number=:serial),"
                                                                     " :datetime);");
                query.bindValue(":serial", Serial);
                query.bindValue(":temperature", toInsert[i++]);
                query.bindValue(":humidity", toInsert[i++]);
                query.bindValue(":datetime", "now()");
            } else {            // DS18b20
                query.prepare("INSERT INTO "+toInsert[0]+toInsert[1]+"(temperature, serial_id, datetime)"
                                                                     " VALUES (:temperature, "
                                                                     "(SELECT id from meteo_sensors where serial_number=:serial), "
                                                                     ":datetime);");
                query.bindValue(":serial", Serial);
                query.bindValue(":temperature", toInsert[i++]);
                query.bindValue(":datetime", "now()");
            }
            query.exec();
        } while (i != toInsert.count());
}

void workWithDevices::initDevices(){
    udpServer = new QUdpSocket(this);
    QByteArray datagram = "INIT"; // + QByteArray::number(messageNo);

    udpServer->writeDatagram(datagram.data(), datagram.size(),
                             QHostAddress::Broadcast, 45454);
    udpServer->close();
}

void workWithDevices::startListeningData(){
    tcpServer = new QTcpServer(this);
    if(!tcpServer->listen(QHostAddress::Any, 7364)){
        qDebug() << QString::fromUtf8("Сервер не запущен: ") + tcpServer->errorString();
    }
    else
    {
        qDebug() << QString::fromUtf8("Сервер запущен: ") + tcpServer->serverAddress().toString()
                    + QString::fromUtf8(":") + QString::number(tcpServer->serverPort());
        connect(tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    }
}

void workWithDevices::newConnection(){
    QTcpSocket *clientSocket;
    while (tcpServer->hasPendingConnections()) {
        clientSocket = tcpServer->nextPendingConnection();
        clientSocket->setSocketOption(QAbstractSocket::KeepAliveOption,true);
        qintptr clientSocketID = clientSocket->socketDescriptor();
        connect(clientSocket, SIGNAL(readyRead()), this, SLOT(readFromClient()));
        qDebug() << QTime::currentTime().toString()
                 << "clientSocketID:"<<clientSocketID;
    }
}

void workWithDevices::readFromClient(){
    QTcpSocket* clientSocket = (QTcpSocket*)sender();
    QByteArray incomingString = clientSocket->readAll();
    parseData(incomingString, clientSocket);
    clientSocket->close();
    qDebug()   << "clientSocketID:"<<clientSocket->socketDescriptor();
    qDebug() << "========";
}

void workWithDevices::sendMasterUIDs(QTcpSocket* clientSocket){
    QTextStream os(clientSocket);
    os.setAutoDetectUnicode(true);
    QSqlQuery query;
    query.prepare("SELECT uid FROM gate_keys WHERE isactive=true AND ismaster=true;");
    if(!query.exec()){
        qWarning() << __FUNCTION__ << query.lastError().text();
    }
    while (query.next()) {
        QString UID = query.value(0).toString()+"\n";
        qDebug() << UID;
        os << UID;
    }
        qDebug() << "<END>\n";
        os << "<END>\n";
}

void workWithDevices::INSIDE(QTcpSocket* clientSocket, QString uid){
    QTextStream os(clientSocket);
    os.setAutoDetectUnicode(true);
    QSqlQuery query;
    query.prepare("SELECT start_of_allowedtime, stop_of_allowedtime, isactive FROM gate_keys WHERE uid=:uid;");
    query.bindValue(":uid",uid);
    query.exec();
    if (query.next()){
        // Если ключ есть в базе
        QTime start_of_allowedtime = query.value(0).toTime();
        QTime stop_of_allowedtime = query.value(1).toTime();
        bool isactive = query.value(2).toBool();
        QTime currentTime = QTime::currentTime();
        if (isactive == true){
            if ((currentTime >= start_of_allowedtime)
                    && (currentTime <= stop_of_allowedtime)){
                // ключ активный, в это время разрешено открывать
                os << "GATE,INSIDE,ALLOW\n";
                query.prepare("INSERT INTO gate_journal(name, comment, datetime)"
                              "VALUES ((SELECT name from gate_keys where uid=:uid), 'Вошёл', :datetime);");
                query.bindValue(":uid", uid.toUInt());
                query.bindValue(":datetime", "now()");
                qDebug() << QString::fromUtf8("Ключ: ") << uid << QString::fromUtf8("\t Вошёл.");
            }else{
                // ключ активный, в это время запрещено открывать
                os << "GATE,INSIDE,DENIED\n";
                query.prepare("INSERT INTO gate_journal(name, comment, datetime)"
                              "VALUES ((SELECT name from gate_keys where uid=:uid), 'Пытался зайти в не своё время', :datetime);");
                query.bindValue(":uid", uid.toUInt());
                query.bindValue(":datetime", "now()");
                qDebug() << QString::fromUtf8("Ключ: ") << uid << QString::fromUtf8("\t в это время вход запрещен.");
            }
        } else {
            // ключ не активирован
            os << "GATE,INSIDE,DENIED\n";
            query.prepare("INSERT INTO gate_journal(name, comment, datetime)"
                          "VALUES ((SELECT name from gate_keys where uid=:uid), 'Ключ не активирован', :datetime);");
            query.bindValue(":uid", uid.toUInt());
            query.bindValue(":datetime", "now()");
            qDebug() << QString::fromUtf8("Ключ: ") << uid << QString::fromUtf8("\t не активирован.");
        }
    } else {
        // Если ключа нет в базе
        os << "GATE,INSIDE,DENIED\n";
        query.prepare("INSERT INTO gate_journal(comment, datetime)"
                      "VALUES ('Неизвестный ключ', :datetime);");
        query.bindValue(":datetime", "now()");
        qDebug() << QString::fromUtf8("Неизвестный ключ:") << uid;
    }

    if(!query.exec()){
        qWarning() << __FUNCTION__ << query.lastError().text();
    }
}

void workWithDevices::writeNewKey(QString newKey){
    QSqlQuery query;
    query.prepare("SELECT id FROM gate_keys WHERE uid=:newKey;");
    query.bindValue(":newKey", newKey);
    if(!query.exec()){
        qWarning() << __FUNCTION__ << query.lastError().text();
    }
    if (query.next()){
    qDebug() << "Ключ уже записан!" << newKey;
    return;
    }

    query.prepare("INSERT INTO gate_keys(uid, insert_datetime) VALUES (:uid,:datetime);");
    query.bindValue(":uid", newKey);
    query.bindValue(":datetime","now()");
    if(!query.exec()){
        qWarning() << __FUNCTION__ << query.lastError().text();
    }
    qDebug() << QString::fromUtf8("Записан новый ключ:") << newKey;

}
