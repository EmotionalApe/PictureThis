#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.resize(800, 500);
    window.setWindowTitle("Picture This");
    window.show();

    return app.exec();
}