#include "checkrecovery.hpp"
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.show();

    checkRecovery recover = *new checkRecovery(&w, &app);
    app.processEvents();
    recover.init();

    return app.exec();
}
