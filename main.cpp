#include <QCoreApplication>
#include "workWithDevices.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    workWithDevices ab;

     if( ab.initDBconnection()){
        ab.initDevices();
        ab.startListeningData();
}


    return a.exec();
}


