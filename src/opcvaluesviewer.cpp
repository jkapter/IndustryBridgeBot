#include "opcvaluesviewer.h"
#include "ui_opcvaluesviewer.h"

#include <QLineEdit>
#include <QItemSelection>

#include "plaintextconsole.h"
#include "opctagpostprocessingwidget.h"
#include "sourcedrivers/sourcedrivermanager.h"
#include "datatagregistry.h"
#include "datatag.h"

using namespace Qt::StringLiterals;

OPCValuesViewer::OPCValuesViewer(SourceDriverManager* dm_ptr, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OPCValuesViewer)
    , driver_manager_(dm_ptr)
{
    ui->setupUi(this);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_periodic_started, this, &OPCValuesViewer::sl_periodic_thread_started);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_periodic_finished, this, &OPCValuesViewer::sl_periodic_thread_finished);

    ui->spbPeriodReading->setValue(driver_manager_->GetPeriodReading());
    if(driver_manager_->InWork()) {
        ui->pbStartOPC->setEnabled(false);
    } else {
        ui->pbStartOPC->setEnabled(true);
    }

    opc_values_viewer_model_ = new OPCValuesViewerModel(driver_manager_, driver_manager_->TagRegistry()->GetIdToTagsMap(), this);
    ui->tvOPCValuesViewer->setModel(opc_values_viewer_model_);
    ui->tvOPCValuesViewer->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tvOPCValuesViewer->verticalHeader()->hide();
    ui->tvOPCValuesViewer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    QObject::connect(ui->tvOPCValuesViewer, &QAbstractItemView::clicked, opc_values_viewer_model_, &OPCValuesViewerModel::sl_table_view_cell_clicked);
    QObject::connect(ui->tvOPCValuesViewer, &QAbstractItemView::doubleClicked, opc_values_viewer_model_, &OPCValuesViewerModel::sl_table_view_cell_double_clicked);
    QObject::connect(ui->tvOPCValuesViewer->selectionModel(), &QItemSelectionModel::selectionChanged, this, &OPCValuesViewer::sl_item_comment_processing);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_reading_periodic_complete, opc_values_viewer_model_, &OPCValuesViewerModel::sl_tags_values_updated);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_reading_request_complete, opc_values_viewer_model_, &OPCValuesViewerModel::sl_tags_values_updated);
    QObject::connect(driver_manager_->TagRegistry(), &DataTagRegistry::sg_periodic_list_changed, this, &OPCValuesViewer::sl_periodic_tags_list_changed);
    QObject::connect(ui->pbReadOnce, &QAbstractButton::clicked, this, &OPCValuesViewer::sl_pb_readonce_clicked);
    QObject::connect(ui->spbPeriodReading, &QSpinBox::valueChanged, this, &OPCValuesViewer::sl_spb_periodreading_valuechanged);
    QObject::connect(ui->pbStartOPC, &QAbstractButton::clicked, this, &OPCValuesViewer::sl_pb_startopc_clicked);
    QObject::connect(ui->pbStopOPC, &QAbstractButton::clicked, this, &OPCValuesViewer::sl_pb_stopopc_clicked);

    console_ = new PlainTextConsole(this);
    console_->setMaximumBlockCount(100);
    console_->AddDTToMessage(true);
    ui->frOPCConsole->setLayout(new QVBoxLayout());
    ui->frOPCConsole->layout()->setContentsMargins(0, 0, 0, 0);
    ui->frOPCConsole->layout()->addWidget(console_);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_send_message_to_console, console_, &PlainTextConsole::sl_add_text_to_console);
}

OPCValuesViewer::~OPCValuesViewer()
{
    delete ui;
    ui = nullptr;
}

void OPCValuesViewer::set_column_widths_(QTableView* tbl) {
    if(!tbl) return;
    int sum_width = tbl->horizontalHeader()->geometry().width();
    int w1, w2, w3;
    if(sum_width >= 300) {
        w1 = (10*sum_width) / 100;
        w2 = (46*sum_width) / 100;
        w3 = ((44/4)*sum_width) / 100;
    } else {
        w1 = 10;
        w2 = 100;
        w3 = (sum_width - 111) / 4;
    }
    tbl->setColumnWidth(0, w1);
    tbl->setColumnWidth(1, w2);
    tbl->setColumnWidth(2, w3);
    tbl->setColumnWidth(3, w3);
    tbl->setColumnWidth(4, w3);
    tbl->setColumnWidth(5, w3);
}


void OPCValuesViewer::resizeEvent(QResizeEvent* event) {
    set_column_widths_(ui->tvOPCValuesViewer);
}

void OPCValuesViewer::showEvent(QShowEvent* event) {
    set_column_widths_(ui->tvOPCValuesViewer);
    QTextEdit* txtedit = ui->teTagPeriodicComment;
    txtedit->setPlainText("");
}

void OPCValuesViewer::sl_pb_readonce_clicked()
{
    auto vec_per = driver_manager_->TagRegistry()->GetAllTags();
    driver_manager_->ReadTagsOnce(vec_per);
}

void OPCValuesViewer::sl_item_comment_processing(QItemSelection selected, QItemSelection deselected) {
    QTextEdit* txtedit = ui->teTagPeriodicComment;

    if(!deselected.isEmpty()) {
        auto deselected_ind = deselected.indexes().front();
        auto tag_ptr = opc_values_viewer_model_->GetTagPtr(deselected_ind);
        if(tag_ptr) {
             tag_ptr->SetCommentString(txtedit->toPlainText());
        }
        txtedit->document()->setPlainText("");
    }

    if(!selected.isEmpty()) {
        auto selected_ind =  selected.indexes().front();
        auto tag_ptr = opc_values_viewer_model_->GetTagPtr(selected_ind);
        if(tag_ptr) {
            txtedit->document()->setPlainText(tag_ptr->GetCommentString());
        }
    }
}

void OPCValuesViewer::sl_spb_periodreading_valuechanged(int arg1)
{
    driver_manager_->SetPeriodReading(arg1);
}

void OPCValuesViewer::sl_pb_startopc_clicked()
{
    driver_manager_->StartPeriodReading(ui->spbPeriodReading->value());
}

void OPCValuesViewer::sl_periodic_thread_started() {
    if(ui) {
        ui->pbStartOPC->setEnabled(false);
    }
}

void OPCValuesViewer::sl_periodic_thread_finished() {
    if(ui) {
        ui->pbStartOPC->setEnabled(true);
    }
}

void OPCValuesViewer::sl_pb_stopopc_clicked()
{
    driver_manager_->StopPeriodReading();
}

void OPCValuesViewer::sl_periodic_tags_list_changed()
{
    opc_values_viewer_model_->SetTagsToTable(driver_manager_->TagRegistry()->GetIdToTagsMap());
}

void OPCValuesViewer::sl_get_message_to_console(QString mes)
{
    console_->sl_add_text_to_console(mes);
}

//====================================================================================
//================= O P C V a l u e W r i t e D i a l o g ============================
//====================================================================================

OPCValueWriteDialog::OPCValueWriteDialog(std::shared_ptr<DataTag> tag_ptr, SourceDriverManager* driver_manager, QWidget *parent)
    : QDialog(parent, Qt::Dialog)
    , tag_ptr_(tag_ptr)
    , driver_manager_(driver_manager)
{
    setWindowTitle(u"Значение для записи"_s);
    QVBoxLayout* vbla = new QVBoxLayout();
    le_value_ = new QLineEdit();
    le_value_->setAlignment(Qt::AlignCenter);
    le_value_->setEnabled(tag_ptr_.get());

    QValidator* le_validator = nullptr;
    if(tag_ptr_) {
        if(!le_validator && tag_ptr_->ValueIsReal()) le_validator = new QDoubleValidator(-10000.0, 10000.0, 4);
        if(!le_validator && tag_ptr_->ValueIsInteger()) le_validator = new QIntValidator(-10000, 10000);
        if(!le_validator && tag_ptr_->ValueIsUnsignedInteger()) le_validator = new QIntValidator(0, 10000);
        if(!le_validator && tag_ptr_->ValueIsBool()) le_validator = new QIntValidator(0,1);
        le_value_->setText(tag_ptr_->GetStringValue(false));
    }
    le_value_->setValidator(le_validator);

    QPushButton* ok_btn = new QPushButton(u"OK"_s);
    QObject::connect(ok_btn, &QAbstractButton::pressed, this, &OPCValueWriteDialog::sl_set_value_to_tag_and_close);

    QPushButton* cancel_btn = new QPushButton(u"Отмена"_s);
    QObject::connect(cancel_btn, &QAbstractButton::pressed, this, &QWidget::close);
    QHBoxLayout* hbla = new QHBoxLayout();
    hbla->addWidget(ok_btn);
    hbla->addWidget(cancel_btn);
    vbla->addWidget(le_value_);
    vbla->addItem(hbla);

    setLayout(vbla);
}

void OPCValueWriteDialog::sl_set_value_to_tag_and_close()
{
    if(tag_ptr_) {
        std::optional<ValueVariant> val;
        if(tag_ptr_->ValueIsReal()) {
            bool b = false;
            double dblVal = QString(le_value_->text()).replace(u',', u'.').toDouble(&b);
            if(b) val = dblVal;
        } else if(tag_ptr_->ValueIsInteger() || tag_ptr_->ValueIsBool()) {
            bool b = false;
            int64_t intVal = le_value_->text().toLongLong(&b);
            if(b) val = intVal;
        } else if(tag_ptr_->ValueIsUnsignedInteger()) {
            bool b = false;
            int64_t uintVal = le_value_->text().toULongLong(&b);
            if(b) val = uintVal;
        } else if(tag_ptr_->ValueIsString()) {
            val = le_value_->text();
        }

        if(val.has_value()) {
            tag_ptr_->SetValueToWrite(val.value());
            if(driver_manager_) {
                driver_manager_->WriteTagNow(tag_ptr_);
            }
        }
    }
    close();
}


//====================================================================================
//================= O P C V a l u e s V i e w e r M o d e l ==========================
//====================================================================================

OPCValuesViewerModel::OPCValuesViewerModel(SourceDriverManager* driver_manager, QObject *parent)
    : QAbstractTableModel(parent)
    , driver_manager_(driver_manager)
{

}

OPCValuesViewerModel::OPCValuesViewerModel(SourceDriverManager* driver_manager, const std::unordered_map<size_t, std::shared_ptr<DataTag>>& tags_map, QObject* parent)
    : QAbstractTableModel(parent)
    , driver_manager_(driver_manager)
    , id_to_tag_(tags_map)
{
    auto ids_view = std::views::keys(id_to_tag_);
    id_tags_ordered_ = {ids_view.begin(), ids_view.end()};
}

void OPCValuesViewerModel::SetTagsToTable(const std::unordered_map<size_t, std::shared_ptr<DataTag>>& tags_map)
{
    id_to_tag_ = tags_map;
    auto ids_view = std::views::keys(id_to_tag_);
    id_tags_ordered_ = {ids_view.begin(), ids_view.end()};
    reset();
}

int OPCValuesViewerModel::rowCount(const QModelIndex &parent) const
{
    return id_tags_ordered_.size();
}

int OPCValuesViewerModel::columnCount(const QModelIndex &parent) const
{
    return 6;
    //{"ID", "Имя тэга", "Тип", "Значение", "Качество", "Обработка"}
}

QVariant OPCValuesViewerModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) return {};

    if(role == Qt::DisplayRole) {
        return get_column_data_from_tag_(index);
    }

    if(role == Qt::BackgroundRole) {
        if(index.column() == 5) { /*постобработка тэга*/
            if(index.row() >= static_cast<int>(id_tags_ordered_.size())) return {};
            size_t id = id_tags_ordered_.at(index.row());
            if(id_to_tag_.count(id) == 0 || !id_to_tag_.at(id)) return {};
            auto tag_ptr = id_to_tag_.at(id);
            bool tag_has_modificators = (tag_ptr->GetGainOption().has_value()) || (tag_ptr->GetSubstituteStringValues().size() > 0);
            return tag_has_modificators ? QBrush(QColor(114, 255, 138)) : QBrush(QColor(245, 245, 245));
            /* {background-color: #72FF8A;} : {background-color: #F5F5F5;} */
        }
    }

    if(role == Qt::TextAlignmentRole) {
        if(index.column() == 1) {
            return Qt::AlignLeft;
        }
        return Qt::AlignCenter;
    }
    return {};
}

QVariant OPCValuesViewerModel::get_column_data_from_tag_(const QModelIndex &index) const
{
    //{"ID", "Имя тэга", "Тип", "Значение", "Качество", "Обработка"}
    if(!index.isValid()) return {};
    if(index.row() >= static_cast<int>(id_tags_ordered_.size())) return {};
    size_t id = id_tags_ordered_.at(index.row());

    if(id_to_tag_.count(id) == 0 || !id_to_tag_.at(id)) return {};
    auto tag_ptr = id_to_tag_.at(id);

    switch(index.column()) {
    case 0: return id;
    case 1: return tag_ptr->GetTagName();
    case 2: return tag_ptr->GetStringType();
    case 3: return tag_ptr->GetStringValue();
    case 4: return DataTag::QualityToString(tag_ptr->GetTagQuality());
    case 5: {
        bool tag_has_modificators = (tag_ptr->GetGainOption().has_value()) || (tag_ptr->GetSubstituteStringValues().size() > 0);
        return tag_has_modificators ? QString("ОБРАБОТКА") : QString("НЕТ");
       }
    }
       return {};
}

QModelIndex OPCValuesViewerModel::index(int row, int column, const QModelIndex &parent) const
{
    if(row >=0 && row < static_cast<int>(id_tags_ordered_.size()) && column >=0 && column < 6) {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QVariant OPCValuesViewerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch(section) {
        case 0: return QString("ID");
        case 1: return QString("Имя тэга");
        case 2: return QString("Тип");
        case 3: return QString("Значение");
        case 4: return QString("Качество");
        case 5: return QString("Обработка");
        }
    }
    return {};
}

void OPCValuesViewerModel::reset()
{
    QAbstractTableModel::beginResetModel();
    QAbstractTableModel::endResetModel();
}

DataTag *OPCValuesViewerModel::GetTagPtr(const QModelIndex &index) const
{
    if(!index.isValid() || index.row() >= static_cast<int>(id_tags_ordered_.size())) return nullptr;
    if(id_to_tag_.count(id_tags_ordered_.at(index.row())) == 0) return nullptr;
    return id_to_tag_.at(id_tags_ordered_.at(index.row())).get();
}

void OPCValuesViewerModel::sl_table_view_cell_clicked(const QModelIndex &index)
{
    if(!index.isValid() || index.row() >= static_cast<int>(id_tags_ordered_.size())) return;
    if(index.column() == 5) { /*постобработка тэга*/
        if(id_to_tag_.count(id_tags_ordered_.at(index.row())) == 0 || !id_to_tag_.at(id_tags_ordered_.at(index.row()))) return;
        auto tag_ptr = id_to_tag_.at(id_tags_ordered_.at(index.row()));

        OPCTagPostProcessingWidget* process_wdg = new OPCTagPostProcessingWidget(tag_ptr);
        process_wdg->setWindowFlag(Qt::Window);
        process_wdg->setWindowModality(Qt::WindowModality::WindowModal);
        process_wdg->setWindowTitle(QString("Пост обработка тэга %1").arg(tag_ptr->GetTagName()));
        process_wdg->exec();
        process_wdg->deleteLater();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::BackgroundRole});
    }
}

void OPCValuesViewerModel::sl_table_view_cell_double_clicked(const QModelIndex &index)
{
    if(!index.isValid() || index.row() >= static_cast<int>(id_tags_ordered_.size())) return;
    if(index.column() == 3) { /*значение*/
        if(id_to_tag_.count(id_tags_ordered_.at(index.row())) == 0 || !id_to_tag_.at(id_tags_ordered_.at(index.row()))) return;
        auto tag_ptr = id_to_tag_.at(id_tags_ordered_.at(index.row()));

        OPCValueWriteDialog* set_value_dialog = new OPCValueWriteDialog(tag_ptr, driver_manager_);
        set_value_dialog->exec();
        set_value_dialog->deleteLater();
    }
}

void OPCValuesViewerModel::sl_tags_values_updated()
{
    if(rowCount(QModelIndex()) == 0) return;
    auto index_start = createIndex(0, 1);
    auto index_end = createIndex(rowCount(QModelIndex()) - 1, 4);
    emit dataChanged(index_start, index_end, {Qt::DisplayRole});
}

