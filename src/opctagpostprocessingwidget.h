#ifndef OPCTAGPOSTPROCESSINGWIDGET_H
#define OPCTAGPOSTPROCESSINGWIDGET_H

#include <QDialog>

namespace Ui {
class OPCTagPostProcessingWidget;
}

class DataTag;

class OPCTagPostProcessingWidget : public QDialog
{
    Q_OBJECT

public:
    explicit OPCTagPostProcessingWidget(std::shared_ptr<DataTag> tag, QWidget *parent = nullptr);
    virtual ~OPCTagPostProcessingWidget();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void sl_pb_ok_clicked();
    void sl_table_cell_changed(int row, int col);

private:
    Ui::OPCTagPostProcessingWidget *ui;
    std::shared_ptr<DataTag> tag_;

    void set_table_column_width_();
};

#endif // OPCTAGPOSTPROCESSINGWIDGET_H
