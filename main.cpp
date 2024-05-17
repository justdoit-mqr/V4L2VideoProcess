#include "videodisplaywidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{

    //QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QApplication a(argc, argv);

    VideoDisplayWidget w;
    w.showFullScreen();

    return a.exec();
}
