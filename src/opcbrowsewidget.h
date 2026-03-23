#ifndef OPCBROWSEWIDGET_H
#define OPCBROWSEWIDGET_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QString>

#include <unordered_map>
#include <unordered_set>

namespace Ui {
class OpcBrowseWidget;
}

class DataTagRegistry;
class SourceDriverManager;
class OPCTagsViewerModel;
class QTreeWidgetItem;
class QMutex;
class QCheckBox;
class QLineEdit;
class PlainTextConsole;

class OpcBrowseWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OpcBrowseWidget(SourceDriverManager* driver_manager, QWidget *parent = nullptr);
    virtual ~OpcBrowseWidget();

    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

signals:
    void sg_set_main_window_status_bar_message(QString message);
    void sg_stop_browsing_tags();

private slots:
    void sl_refresh_opc_tags_to_table(QTreeWidgetItem* cur, QTreeWidgetItem* last);
    void sl_tb_cleartagslist_clicked();
    void sl_tb_refreshopclist_clicked();
    void sl_tb_refreshopctags_clicked();
    void sl_opc_servers_tree_widget_context_menu_requested(const QPoint& pos);
    void sl_add_opc_server_to_tree();
    void sl_delete_opc_server_from_tree();
    void sl_add_new_host_to_tree(const QString& hostname);
    void sl_browser_get_part_tags(const QString& hostname, const QString& server_name, size_t n_tags);
    void sl_browser_get_all_tags(const QString& hostname, const QString& server_name, size_t n_tags);

private:
    Ui::OpcBrowseWidget *ui;
    PlainTextConsole* console_;
    SourceDriverManager* driver_manager_;
    std::unordered_set<QString> host_names_;
    std::unordered_map<const QString*, std::set<QString>> host_to_opc_servers_;
    std::unordered_map<const QString*, std::vector<QString>> opc_server_to_tags_list_buffer_;
    std::unordered_map<const QString*, QTreeWidgetItem*> opc_server_to_tree_item_;
    std::unordered_map<const QString*, OPCTagsViewerModel*> opc_server_to_table_model_;

    QTreeWidgetItem* selected_item_opc_tree_ = nullptr;

    void opctable_set_column_widths_();
    void opc_tree_set_column_width();
    void fill_opc_list_();
    void fill_tags_list_(const QString& hostname, const QString& server_name);
};


/*class OPCCheckBoxTableItem: public QWidget
{
    Q_OBJECT
public:
    explicit OPCCheckBoxTableItem(QString tag, bool checked, QWidget *parent = nullptr);
    void SetCheckBoxState(bool state);

signals:
    void sg_change_state(QString tag, int state);

private slots:
    void sl_checkbox_changed_state(int state);

private:
    QString tag_name_;
    OPC_HELPER::TAG_STATUS tag_place_;
    QCheckBox* chb_;
};*/

class OPCAddHostDialog: public QDialog {
    Q_OBJECT
public:
    explicit OPCAddHostDialog(QWidget* parent = nullptr);

signals:
    void sg_add_new_host(QString hostname);

private slots:
    void sl_ok_pressed();

private:
    QLineEdit* le_value_;

};

class OPCTagsViewerModel: public QAbstractTableModel
{
    Q_OBJECT
public:
    OPCTagsViewerModel() = delete;
    explicit OPCTagsViewerModel(const std::vector<QString>& tags, const QString& tag_prefix, DataTagRegistry* data_manager, QObject* parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void reset();

private:
    DataTagRegistry* data_manager_;
    const std::vector<QString>& tags_;
    QString tag_prefix_;
};

class SelectReadModeCheckBox: public QStyledItemDelegate
{
    Q_OBJECT
public:
    SelectReadModeCheckBox(qreal cb_indicator_size = 25, QObject* parent = nullptr) : QStyledItemDelegate(parent), cb_indicator_size_(cb_indicator_size) {}
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

private:
    qreal cb_indicator_size_;
};


#endif // OPCBROWSEWIDGET_H
