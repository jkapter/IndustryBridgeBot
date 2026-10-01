#ifndef OPCBROWSEWIDGET_H
#define OPCBROWSEWIDGET_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>
#include <QString>

#include <unordered_map>
#include <unordered_set>
#include <deque>

#include "datatag.h"

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
class DataBrowseItem;
class QComboBox;
class DataBrowserTreeModel;
class DriverInterface;

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
    void sg_send_message_to_console(QString message);
    void sg_stop_browsing_tags();


private slots:
    void sl_refresh_opc_tags_to_table(const QModelIndex &current, const QModelIndex &previous);
    void sl_tb_delete_all_tags_clicked();
    void sl_tb_set_all_tags_clicked();
    void sl_opc_servers_tree_widget_context_menu_requested(const QPoint& pos);
    void sl_add_endpoint_to_tree();
    void sl_delete_endpoint_from_tree();
    void sl_add_new_endpoint_to_tree(QString hostname, QString endpoint, uint8_t data_source);
    void sl_browser_get_part_tags(const QString& hostname, const QString& server_name, size_t n_tags);
    void sl_browser_get_all_tags(const QString& hostname, const QString& server_name, size_t n_tags);

private:
    Ui::OpcBrowseWidget *ui;
    PlainTextConsole* console_;
    SourceDriverManager* driver_manager_;
    DataBrowserTreeModel* data_model_;

    std::unordered_set<QString> host_names_;
    std::unordered_map<const QString*, std::set<QString>> host_to_opc_servers_;
    std::unordered_map<const QString*, std::vector<QString>> opc_server_to_tags_list_buffer_;
    std::unordered_map<const QString*, QTreeWidgetItem*> opc_server_to_tree_item_;
    std::unordered_map<const QString*, OPCTagsViewerModel*> opc_server_to_table_model_;

    DataBrowseItem* selected_item_opc_tree_ = nullptr;

    void construct_tree_on_start_();
};


class OPCAddHostDialog: public QDialog {
    Q_OBJECT
public:
    explicit OPCAddHostDialog(SourceDriverManager* driver_manager, QWidget* parent = nullptr);

signals:
    void sg_add_new_endpoint(QString hostname, QString endpoint, uint8_t data_source);

private slots:
    void sl_ok_pressed();
    void sl_get_ep_pressed();
    void sl_data_type_changed(int index);
    void sl_endpoints_received_(const QString& host);

private:
    SourceDriverManager* driver_manager_;
    QLineEdit* le_value_;
    QComboBox* ep_combobox_;
    QPushButton* ok_btn_;
    QComboBox* data_type_cb_;

    DriverInterface* get_current_driver_();
};

class OPCTagsViewerModel: public QAbstractTableModel
{
    Q_OBJECT
public:
    OPCTagsViewerModel() = delete;
    explicit OPCTagsViewerModel(DataBrowseItem* parent_data_item, const QString& tag_prefix, SourceDriverManager* driver_manager, QObject* parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void reset();
    void SetAllTagsToRead();
    void DeleteAllTags();

private:
    SourceDriverManager* driver_manager_;
    std::deque<QString> tags_browse_names_;
    std::unordered_map<const QString*, QString> tags_browse_name_to_id_;
    std::unordered_map<const QString*, QString> tags_browse_name_to_full_tag_name_;
    QString tag_prefix_;
    DataTag::DataSource source_;

    void get_tags_from_data_item_recursievely_(DataBrowseItem* item);
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

class DataBrowserTreeModel: public QAbstractItemModel {
    Q_OBJECT
public:
    DataBrowserTreeModel(QObject *parent);
    DataBrowserTreeModel(std::unique_ptr<DataBrowseItem> &&root_node, QObject *parent);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool AddEndpoint(const QString &hostname, const QString &endpoint, DataTag::DataSource source);
    bool DeleteEndpoint(const QString &hostname, const QString &endpoint);
    DataBrowseItem *GetEndpoint(const QString &hostname, const QString &endpoint) const;
    DataBrowseItem *GetHost(const QString &hostname) const;
    bool AddHost(const QString &hostname);
    bool DeleteHost(const QString& hostname);
    bool AbsorbChildNodes(const QString &hostname, const QString &endpoint, DataBrowseItem* item);

public slots:
    void sl_data_changed(const QString& hostname, const QString& server_name);

private:
    std::unique_ptr<DataBrowseItem> root_item_;
};

class DataBrowserFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DataBrowserFilterProxyModel(QObject *parent = nullptr): QSortFilterProxyModel(parent) {}
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        {
            auto *source_model = this->sourceModel();
            if (!source_model) return false;

            QModelIndex child_index = source_model->index(source_row, 0, source_parent);
            if (!child_index.isValid()) return false;

            auto *item = static_cast<DataBrowseItem*>(child_index.internalPointer());
            if (!item) return false;

            if (item->GetType() == DataBrowseItem::ItemType::VARIABLE) {
                return false;
            }

            return true;
        }
    }
};

#endif // OPCBROWSEWIDGET_H
