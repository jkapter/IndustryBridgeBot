#ifndef SELECTITEMSTABLEWIGGET_H
#define SELECTITEMSTABLEWIGGET_H

#include <QTableWidget>

enum class SITW_TYPE {
    Messages,
    InlineButtons,
    WaitAnswerTag
};

class TgBotManager;
class TGTrigger;
class TGMessage;
class TGMessageWaitAnswer;

class SelectItemsTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit SelectItemsTableWidget(TgBotManager& bot_manager, SITW_TYPE type, int rows_num, QWidget *parent = nullptr);
    void ReadCommandContent(const TGTrigger* command);
    void SetMessagesToCommand(TGTrigger* command);
    void ReadMessageContent(const TGMessage* message);
    void ResetContent();
    void SetButtonsToMessage(TGMessage* message);
    void ReadWaitAnswerTagContent(const TGMessageWaitAnswer* message);
    void SetWaitAnswerTagToMessage(TGMessageWaitAnswer* message);

signals:

private slots:
    void sl_table_item_changed(int cb_index);

protected:
    virtual void showEvent(QShowEvent* ev) override;
    virtual void resizeEvent(QResizeEvent* ev) override;
private:
    TgBotManager& bot_manager_;
    SITW_TYPE content_type_;

    void set_column_width_();
    void update_content_messages_();
    void update_content_buttons_();
    void update_content_wait_answer_tag_();
};

#endif // SELECTITEMSTABLEWIGGET_H
