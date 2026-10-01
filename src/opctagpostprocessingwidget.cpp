#include "opctagpostprocessingwidget.h"
#include "ui_opctagpostprocessingwidget.h"

#include "datatag.h"

OPCTagPostProcessingWidget::OPCTagPostProcessingWidget(std::shared_ptr<DataTag> tag, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OPCTagPostProcessingWidget)
    , tag_(tag)
{
    ui->setupUi(this);

    QObject::connect(ui->pbCancel, &QAbstractButton::clicked, this, &QWidget::close);
    QObject::connect(ui->pbOK, &QAbstractButton::clicked, this, &OPCTagPostProcessingWidget::sl_pb_ok_clicked);

    auto tag_gain = tag_->GetGainOption();
    if(tag_gain.has_value()) {
        ui->dsbGainValue->setValue(tag_gain.value());
    }

    const std::unordered_map<QString, QString> substitute_table = tag_->GetSubstituteStringValues();
    ui->twSubstituteValue->setRowCount(substitute_table.size() > 0 ? substitute_table.size() : 1);
    ui->twSubstituteValue->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    for(int i = 0; i < ui->twSubstituteValue->rowCount(); ++i) {
        QTableWidgetItem* it_wdg = new QTableWidgetItem();
        it_wdg->setFlags(it_wdg->flags().setFlag(Qt::ItemIsSelectable, true).setFlag(Qt::ItemIsEditable, true));
        it_wdg->setTextAlignment(Qt::AlignHCenter);
        ui->twSubstituteValue->setItem(i, 0, it_wdg);
        ui->twSubstituteValue->setItem(i, 1, new QTableWidgetItem(*it_wdg));

    }

    int i = 0;
    for(const auto& [val, subst_val]: substitute_table) {

        ui->twSubstituteValue->item(i, 0)->setText(val);
        ui->twSubstituteValue->item(i, 1)->setText(subst_val);
        ++i;
    }

    QObject::connect(ui->twSubstituteValue, &QTableWidget::cellClicked, this, &OPCTagPostProcessingWidget::sl_table_cell_changed);
}

OPCTagPostProcessingWidget::~OPCTagPostProcessingWidget()
{
    delete ui;
}

void OPCTagPostProcessingWidget::resizeEvent(QResizeEvent *event)
{
    set_table_column_width_();
}

void OPCTagPostProcessingWidget::showEvent(QShowEvent *event)
{
    set_table_column_width_();
}

void OPCTagPostProcessingWidget::set_table_column_width_()
{
    QTableWidget* tbl = ui->twSubstituteValue;
    int w = tbl->horizontalHeader()->geometry().width();
    tbl->setColumnWidth(0, w/2 - 2);
    tbl->setColumnWidth(1, w/2 - 2);
}

void OPCTagPostProcessingWidget::sl_pb_ok_clicked()
{
    tag_->ResetGainOption();
    if(std::abs(ui->dsbGainValue->value() - 1.0) > 1e-6) {
        tag_->SetGainOption(ui->dsbGainValue->value());
    }

    tag_->ClearSubstituteStringValues();
    for(int i = 0; i < ui->twSubstituteValue->rowCount(); ++i) {
        QString val = ui->twSubstituteValue->item(i, 0)->text();
        QString subst_val = ui->twSubstituteValue->item(i, 1)->text();
        if((val.size() > 0) && (subst_val.size() > 0)) {
            tag_->AddSubstituteStringValue(val, subst_val);
        }
    }
    close();
}

void OPCTagPostProcessingWidget::sl_table_cell_changed(int row, int col)
{
    int rc = ui->twSubstituteValue->rowCount();
    if(row == rc - 1) {
        ui->twSubstituteValue->setRowCount(rc + 1);
        QTableWidgetItem* it_wdg = new QTableWidgetItem();
        it_wdg->setFlags(it_wdg->flags().setFlag(Qt::ItemIsSelectable, true).setFlag(Qt::ItemIsEditable, true));
        it_wdg->setTextAlignment(Qt::AlignHCenter);
        ui->twSubstituteValue->setItem(rc, 0, it_wdg);
        ui->twSubstituteValue->setItem(rc, 1, new QTableWidgetItem(*it_wdg));
    }
}

