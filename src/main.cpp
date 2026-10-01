#include "mainwindow.h"
#include <QApplication>
#include <QMainWindow>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QMessageBox>
#include <QThread>
#include <Qdir>
#include <QFile>
#include <QString>
#include <QtEnvironmentVariables>

#include "logger.h"
#include "tgobjects/tgbotmanager.h"
#include "sourcedrivers/sourcedrivermanager.h"
#include "datatagregistry.h"

bool check_argv(const char* argv, const char* par, std::string_view& value) {
    std::string_view par_str(par);
    std::string_view argv_str(argv);
    if(argv_str.find(par_str) == 0) {
        value = (argv_str.length() > par_str.length()) ? argv_str.substr(par_str.length()) : "";
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    int exit_code = 0;
    Logger log(QDir::currentPath(), QString("log.txt"));
    Logger::SetMaxSize(200000);

    qputenv("https_proxy", "socks5://127.0.0.1:10808");
    qputenv("http_proxy",  "socks5://127.0.0.1:10808");
    qputenv("all_proxy",   "socks5://127.0.0.1:10808");

    QApplication app(argc, argv);

    if(argc > 1) {
        for(int i = 1; i < argc; ++i) {
            std::string_view par_val;

            if(check_argv(argv[i], "-token=", par_val)) {
                bool res = TGHELPER::WriteTokenToFile(QString("%1/%2").arg(QDir::currentPath(), QString("token.dat")), {par_val.begin(), par_val.end()});
                qInfo() << QString("Новый токен записан в файл token.dat: %1").arg(res ? QString("ОК") : QString("ОШИБКА"));
                QMessageBox msgbox(QMessageBox::Information, "Сообщение"
                                   , QString("Установлен токен телеграм бота = %1").arg(QString::fromStdString({par_val.begin(), par_val.end()}))
                                   , QMessageBox::Ok);

                msgbox.exec();
            }

            if(check_argv(argv[i], "-show-token", par_val) && par_val.length() == 0) {
                std::string token = TGHELPER::ReadTokenFromFile(QString("%1/%2").arg(QDir::currentPath(), QString("token.dat")));
                QMessageBox msgbox(QMessageBox::Information
                                   , "Сообщение"
                                   , QString("Текущий токен = %1").arg(QString::fromStdString(token))
                                   , QMessageBox::Ok);

                msgbox.exec();
            }
        }
        return 0;
    }

    do {
        std::unique_ptr<DataTagRegistry> tag_registry = std::make_unique<DataTagRegistry>("opctags.json");
        tag_registry->RestoreDataFromFile();
        std::unique_ptr<SourceDriverManager> driver_manager = std::make_unique<SourceDriverManager>(tag_registry.get());
        std::unique_ptr<TgBotManager> tg_bot_manager_ptr = std::make_unique<TgBotManager>(*driver_manager.get());
        bool start_app_minimized = false;

        {
        QFile input_file("settings.json");
        if(input_file.open(QIODeviceBase::ReadOnly)) {
            QJsonParseError json_error;
            QJsonDocument input_doc = QJsonDocument::fromJson(input_file.readAll(), &json_error);
            if(json_error.error == QJsonParseError::NoError) {
                if(input_doc.object().contains("start_application_on_tray") && input_doc.object().value("start_application_on_tray").isBool()) {
                    start_app_minimized = input_doc.object().value("start_application_on_tray").toBool();
                }

                if(input_doc.object().contains("log_level") && input_doc.object().value("log_level").isString()) {
                    Logger::SetMinLevel(Logger::MinLevelFromString(input_doc.object().value("log_level").toString()));
                }
            }
        }
        input_file.close();
        }

        MainWindow w(tg_bot_manager_ptr.get(), driver_manager.get());

        if(!start_app_minimized) {
            w.show();
        }

        exit_code = app.exec();

        if(exit_code != MainWindow::EXIT_CODE_USER_CMD) {
            QThread::currentThread()->sleep(5);
        }
    } while(exit_code != MainWindow::EXIT_CODE_USER_CMD);

    return exit_code;
}
