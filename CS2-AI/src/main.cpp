#include <QApplication>
#include "UI/MainWindow.h"

#include "CS2/GameInformationHandler.h" 
#include "CS2/Overlay.h"            

GameInformationhandler handler;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.show();

    Overlay* overlay = new Overlay(&handler);
    overlay->show();

    return app.exec();
}