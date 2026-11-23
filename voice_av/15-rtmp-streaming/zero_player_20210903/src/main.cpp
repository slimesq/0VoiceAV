#include "mainwindow.h"
#include <QApplication>
#include "dlog.h"

#undef main
int main(int argc, char *argv[])
{
    init_logger("rtmp_pull.log", S_INFO);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
