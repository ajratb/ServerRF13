#include <QCoreApplication>
#include "workWithDevices.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    workWithDevices ab;
    ab.initDBconnection();

    return a.exec();
}


