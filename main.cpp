#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QMetaType>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 注册 std::vector<float> 和 std::size_t 元类型
    qRegisterMetaType<std::vector<bool>>("std::vector<bool>");
    //全局字体
    QFont font("Arial", 10);
    a.setFont(font);
    QFile f("qdarkstyle/dark/darkstyle.qss");

    if (!f.exists())   {
        printf("Unable to set stylesheet, file not found\n");
    }
    else   {
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        a.setStyleSheet(ts.readAll());
    }

    MainWindow w;
    w.show();
    return a.exec();
}
