#ifndef TGEVENTSCONFIGURATIONWIDGET_H
#define TGEVENTSCONFIGURATIONWIDGET_H

#include <QWidget>

class TgBotManager;
class SelectItemsTableWidget;
class TGTriggerTagValue;
class TGScheduledEvent;
class QValidator;
class QTableWidget;
class DataTag;

namespace Ui {
class TGEventsConfigurationWidget;
}

class TGEventsConfigurationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TGEventsConfigurationWidget(TgBotManager& tg_bot_manager, QWidget *parent = nullptr);
    virtual ~TGEventsConfigurationWidget();

public slots:
    void sl_change_object_data(const std::string &current, const std::string &previous);

private slots:
    void sl_opc_table_messages_double_click(int row, int column);
    void sl_make_scheduled_events_layout(int index);

signals:
    void sg_tgobject_changed(const std::string& cur, const std::string& prev);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::TGEventsConfigurationWidget *ui;

    TgBotManager& tg_bot_manager_;
    SelectItemsTableWidget* message_to_event_ = nullptr;
    SelectItemsTableWidget* message_to_scheduled_event_ = nullptr;
    TGTriggerTagValue* current_event_ = nullptr;
    TGScheduledEvent* current_scheduled_event_ = nullptr;

    void clear_event_data_();
    void clear_scheduled_event_data_();
    void save_to_current_event_();
    void save_to_current_scheduled_event_();
    void load_data_from_event_(const std::string& id);
    void load_data_from_scheduled_event_(const std::string& id);
    void fill_opc_tags_table_(QTableWidget* tbl);
    void opc_table_set_column_width_(QTableWidget* tbl);

    QValidator* get_event_values_validator(DataTag* tag);
};

#endif // TGEVENTSCONFIFURATIONWIDGET_H
