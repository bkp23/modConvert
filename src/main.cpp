#include <QtGui/QApplication>
#include "modConvert.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    modConvert w;
    w.show();

    return a.exec();
}
