#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QObject>
#include <QMainWindow>
#include <QSystemTrayIcon>

#include "sourcedrivers/copcclient.h"
#include "datatag.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TgBotManager;
class SourceDriverManager;
class QSystemTrayIcon;
class QLabel;
class QMessageBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(TgBotManager* bot_manager, SourceDriverManager* driver_manager, QWidget *parent = nullptr);
    virtual ~MainWindow();

    static int const EXIT_CODE_REBOOT;
    static int const EXIT_CODE_USER_CMD;

protected:
    void closeEvent(QCloseEvent * event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *ev) override;

private slots:
    void changeEvent(QEvent*) override;
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void trayActionExecute();
    void setTrayIconActions();
    void showTrayIcon();

    void sl_close_app_from_tray();
    void sl_pb_main_menu_clicked();
    void sl_pb_close_app_clicked();
    void sl_pb_opcbrowse_page_clicked();
    void sl_pb_opcmanage_page_clicked();
    void sl_status_bar_opc_label_change_text(DataTag::DataSource driver, bool is_connected);
    void sl_status_bar_bot_label_change_text();
    void sl_status_bar_message_label_change_text(QString message);
    void sl_bot_error(QString what);
    void sl_pb_tgsettings_page_clicked();
    void sl_animation_main_menu_finished();
    void sl_pb_tgconfig_page_clicked();
    void sl_pb_savedatafiles_clicked();
    void sl_restart_app_cmd(bool auto_restart = false);
    void sl_auto_restart_app_checkbox_changed(Qt::CheckState state);
    void sl_start_app_mode_checkbox_changed(Qt::CheckState state);

private:
    Ui::MainWindow *ui;

    COPCClient tst_dlg_;
    TgBotManager* tg_bot_manager_ = nullptr;
    SourceDriverManager* source_data_manager_ = nullptr;

    QMenu *trayIconMenu;
    QAction *minimizeAction;
    QAction *restoreAction;
    QAction *quitAction;
    QSystemTrayIcon *trayIcon;
    QLabel *status_bar_opc_da_label_;
    QLabel *status_bar_opc_ua_label_;
    QLabel *status_bar_bot_label_;
    QLabel *status_bar_message_label_;
    std::unique_ptr<QMessageBox> bot_error_message_;

    bool bFirstMinimized_ = false;
    bool bFirstClosed_ = false;

    bool write_settings_to_file_(const QString& folder_path) const;
    bool read_settings_();

    bool opc_was_running_ = false;
    int opc_period_reading_ = 2;
    bool tg_bot_auto_restart_ = false;
    bool app_auto_restart_ = false;
    bool start_app_on_tray_ = false;
};
#endif // MAINWINDOW_H
