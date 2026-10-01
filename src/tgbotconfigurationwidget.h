#ifndef TGBOTCONFIGURATIONWIDGET_H
#define TGBOTCONFIGURATIONWIDGET_H

#include <QWidget>
#include <QAbstractItemModel>
#include <QItemSelectionModel>

#include "tgobjects/tgobject.h"

Q_DECLARE_METATYPE(std::string)

namespace Ui {
class TgBotConfigurationWidget;
}

class TGObjectTreeModel;
class TgBotManager;
class TGMessageConfigurationWidget;
class TGCommandConfigurationWidget;
class TGInlineButtonsConfigurationWidget;
class TGEventsConfigurationWidget;


class TgBotConfigurationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TgBotConfigurationWidget(TgBotManager* bot_manager, QWidget *parent = nullptr);
    virtual ~TgBotConfigurationWidget();

protected:
    void showEvent(QShowEvent *event) override;

private slots:

    void sl_add_new_message();
    void sl_add_new_message_wait_answer();
    void sl_delete_message();
    void sl_save_message();

    void sl_add_new_command();
    void sl_delete_command();
    void sl_save_commands();
    void sl_add_command_to_main_menu();

    void sl_add_new_event();
    void sl_add_new_scheduled_event();
    void sl_delete_event();
    void sl_save_events();

    void sl_add_new_inline_button();
    void sl_delete_inline_button();
    void sl_save_inline_buttons();

private:
    Ui::TgBotConfigurationWidget *ui;
    TgBotManager* tg_bot_manager_ = nullptr;

    TGObjectTreeModel* messages_tree_ = nullptr;
    TGMessageConfigurationWidget* message_widget_ = nullptr;
    TGObjectTreeModel* commands_tree_ = nullptr;
    TGCommandConfigurationWidget* command_widget_ = nullptr;
    TGObjectTreeModel* buttons_tree_ = nullptr;
    TGInlineButtonsConfigurationWidget* button_widget_ = nullptr;
    TGObjectTreeModel* events_tree_ = nullptr;
    TGEventsConfigurationWidget* events_widget_ = nullptr;
};

//=================================================================================
//============ T G T R E E M O D E L ==============================================
//=================================================================================

class TGTreeItem {
public:
    TGTreeItem(const std::string& id, TGTreeItem* parent);
    bool AppendChild(std::unique_ptr<TGTreeItem> &&child, int row = -1);
    bool AppendChild(const std::string& id, int row = -1);
    bool DeleteChildRecursively(const std::string& id);
    bool DeleteChild(int row);
    TGTreeItem* Child(int row) const;
    TGTreeItem* Child(const std::string& id) const;
    int ChildCount() const;
    QVariant Data() const;
    int Row() const;
    TGTreeItem* ParentItem();
    const std::string& GetId() const;

private:
    TGTreeItem* parent_;
    const std::string& tgobject_id_;
    std::unordered_map<const std::string*, std::unique_ptr<TGTreeItem>> childs_;
    std::list<std::string> child_ids_;
};


class TGObjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit TGObjectTreeModel(TgBotManager& bot_manager, TGOBJECT_TYPE tree_type, QObject *parent = nullptr);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void sl_tree_selection_changed(const QModelIndex &current, const QModelIndex &previous);
    void sl_tgobject_changed(const std::string& current_id, const std::string& previous_id);

signals:
    void sg_change_tree_item(const std::string& previous, const std::string& current);
    void sg_set_item_selected(QModelIndex index, QItemSelectionModel::SelectionFlags flag);

private:
    TgBotManager& bot_manager_;
    TGOBJECT_TYPE tree_type_;
    std::unique_ptr<TGTreeItem> root_item_;
    std::vector<std::string> headers_;
    std::unordered_map<const std::string*, std::set<std::string>> header_to_ids_;
    const std::string empty_str_ = {};

    void prepare_data_tree_messages_();
    void prepare_data_tree_commands_();
    void prepare_data_tree_events_();
    void prepare_data_tree_inline_buttons_();
    void add_new_item_message_(const std::string& id);
    void add_new_item_command_(const std::string& id);
    void add_new_item_event_(const std::string& id);
    void add_new_item_button_(const std::string& id);
    void build_tree_items_();
    const std::string& get_id_(const QModelIndex& index) const;
};

#endif // TGBOTCONFIGURATIONWIDGET_H
