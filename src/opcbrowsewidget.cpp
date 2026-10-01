#include "opcbrowsewidget.h"
#include "ui_opcbrowsewidget.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QMenu>
#include <QMutexLocker>
#include <QPainter>
#include <QPushButton>
#include <QThread>
#include <QMutex>
#include <QComboBox>

#include "plaintextconsole.h"
#include "sourcedrivers/copcclient.h"
#include "sourcedrivers/opcclientworker.h"
#include "sourcedrivers/sourcedrivermanager.h"
#include "datatagregistry.h"

using namespace Qt::Literals::StringLiterals;

OpcBrowseWidget::OpcBrowseWidget(SourceDriverManager* dm_ptr, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OpcBrowseWidget)
    , driver_manager_(dm_ptr)
    , data_model_(new DataBrowserTreeModel(this))
{
    ui->setupUi(this);

    ui->tblvOPCTags->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->tblvOPCTags->setItemDelegateForColumn(1, new SelectReadModeCheckBox(25, this));
    ui->tblvOPCTags->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblvOPCTags->setModel(new OPCTagsViewerModel(nullptr, u""_s, driver_manager_, ui->tblvOPCTags));
    ui->tblvOPCTags->horizontalHeader()->setStretchLastSection(false);
    ui->tblvOPCTags->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tblvOPCTags->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tblvOPCTags->horizontalHeader()->resizeSection(1, 120);

    QObject::connect(ui->tbCheckAll, &QAbstractButton::clicked, this, &OpcBrowseWidget::sl_tb_set_all_tags_clicked);
    QObject::connect(ui->tbDeleteAll, &QAbstractButton::clicked, this, &OpcBrowseWidget::sl_tb_delete_all_tags_clicked);
    QObject::connect(ui->tbAddDataSource, &QAbstractButton::clicked, this, &OpcBrowseWidget::sl_add_endpoint_to_tree);
    QObject::connect(ui->tbDeleteDataSource, &QAbstractButton::clicked, this, &OpcBrowseWidget::sl_delete_endpoint_from_tree);

    console_ = new PlainTextConsole(this);
    console_->setMaximumBlockCount(100);
    console_->AddDTToMessage(true);
    ui->frOPCConsole->setLayout(new QVBoxLayout());
    ui->frOPCConsole->layout()->setContentsMargins(0, 0, 0, 0);
    ui->frOPCConsole->layout()->addWidget(console_);

    QObject::connect(this, &OpcBrowseWidget::sg_send_message_to_console, console_, &PlainTextConsole::sl_add_text_to_console);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_send_message_to_console, console_, &PlainTextConsole::sl_add_text_to_console);

    auto *proxy_tree_model_ptr = new DataBrowserFilterProxyModel(this);
    proxy_tree_model_ptr->setSourceModel(data_model_);
    ui->tvOPCTree->setModel(proxy_tree_model_ptr);

    ui->tvOPCTree->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tvOPCTree->setSortingEnabled(false);
    ui->tvOPCTree->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tvOPCTree->header()->setStretchLastSection(false);
    ui->tvOPCTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tvOPCTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tvOPCTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tvOPCTree->header()->setResizeContentsPrecision(0);
    ui->tvOPCTree->header()->hide();
    QObject::connect(ui->tvOPCTree, &QWidget::customContextMenuRequested, this, &OpcBrowseWidget::sl_opc_servers_tree_widget_context_menu_requested);
    QObject::connect(ui->tvOPCTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &OpcBrowseWidget::sl_refresh_opc_tags_to_table);

    QObject::connect(driver_manager_, &SourceDriverManager::sg_get_part_tag_names_from_server, this, &OpcBrowseWidget::sl_browser_get_part_tags);
    QObject::connect(driver_manager_, &SourceDriverManager::sg_get_all_tag_names_from_server, this, &OpcBrowseWidget::sl_browser_get_all_tags);

    construct_tree_on_start_();
}

OpcBrowseWidget::~OpcBrowseWidget()
{
    emit sg_stop_browsing_tags();
    delete ui;
}

void OpcBrowseWidget::construct_tree_on_start_()
{
    std::unordered_map<QString, std::unordered_set<QString>> host_to_endpoint;
    std::unordered_map<QString, DataTag::DataSource> endpoint_to_source;
    for(const auto& it: driver_manager_->TagRegistry()->GetAllTags()) {
        data_model_->AddHost(it->GetHostName());
        host_to_endpoint[it->GetHostName()].insert(it->GetEndpointName());
        endpoint_to_source[it->GetEndpointName()] = it->GetDataSource();
    }

    for(auto& [host, ep_set]: host_to_endpoint) {
        for(auto& ep: ep_set) {
            sl_add_new_endpoint_to_tree(host, ep, static_cast<uint8_t>(endpoint_to_source.at(ep)));
        }
    }
}

void OpcBrowseWidget::resizeEvent(QResizeEvent* event) {

}

void OpcBrowseWidget::showEvent(QShowEvent* event) {
    int wdt = ui->splitter->width();
    ui->splitter->setSizes({wdt/3, 2*wdt/3});
}

void OpcBrowseWidget::sl_refresh_opc_tags_to_table(const QModelIndex &current, const QModelIndex &previous) {

    selected_item_opc_tree_ = nullptr;

    auto *proxy_model = qobject_cast<QSortFilterProxyModel*>(ui->tvOPCTree->model());
    if (!proxy_model) return;

    QModelIndex source_index = proxy_model->mapToSource(current);
    if(!source_index.isValid()) return;

    selected_item_opc_tree_ = static_cast<DataBrowseItem*>(source_index.internalPointer());
    if(!selected_item_opc_tree_) return;

    QString host;
    QString endpoint;
    DataBrowseItem* parent;

    switch(selected_item_opc_tree_->GetType()) {
        using enum DataBrowseItem::ItemType;
    case HOST: return;
    case INVALID: return;
    case ROOT: return;
    case ENDPOINT:
        endpoint = selected_item_opc_tree_->GetId();
        parent = selected_item_opc_tree_->ParentItem();
        if(!parent || parent->GetType() != HOST) return;
        host = parent->GetId();
        if(!selected_item_opc_tree_->IsBrowsed()) {
            DriverInterface * driver = driver_manager_->GetDriverPtr(selected_item_opc_tree_->GetSource());
            if(!driver) return;
            driver->GetVariablesNode(host, endpoint);
        }
        break;
    case NODE:
        parent = selected_item_opc_tree_->ParentItem();
        while(parent && parent->GetType() != ENDPOINT) {
            parent = parent->ParentItem();
        }
        endpoint = parent->GetId();
        parent = parent->ParentItem();
        if(!parent || parent->GetType() != HOST) return;
        host = parent->GetId();
        break;
    case VARIABLE:
        parent = selected_item_opc_tree_->ParentItem();
        while(parent && parent->GetType() != ENDPOINT) {
            parent = parent->ParentItem();
        }
        endpoint = parent->GetId();
        parent = parent->ParentItem();
        if(!parent || parent->GetType() != HOST) return;
        host = parent->GetId();
        break;
    default: return;
    }

    auto old_model = ui->tblvOPCTags->model();
    QString tag_prefix = QString("[%1][%2]").arg(host, endpoint);
    ui->tblvOPCTags->setModel(new OPCTagsViewerModel(selected_item_opc_tree_, tag_prefix, driver_manager_, ui->tblvOPCTags));
    ui->tblvOPCTags->horizontalHeader()->setStretchLastSection(false);
    ui->tblvOPCTags->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tblvOPCTags->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tblvOPCTags->horizontalHeader()->resizeSection(1, 120);
    old_model->deleteLater();
}

void OpcBrowseWidget::sl_tb_delete_all_tags_clicked()
{
    static_cast<OPCTagsViewerModel*>(ui->tblvOPCTags->model())->DeleteAllTags();
}

void OpcBrowseWidget::sl_tb_set_all_tags_clicked()
{
    static_cast<OPCTagsViewerModel*>(ui->tblvOPCTags->model())->SetAllTagsToRead();
}

void OpcBrowseWidget::sl_opc_servers_tree_widget_context_menu_requested(const QPoint &pos)
{
    QAction add_endpoint(u"Добавить источник данных"_s, ui->tvOPCTree);
    QAction delete_endpoint(u"Удалить"_s, ui->tvOPCTree);

    bool delete_allow = false;
    selected_item_opc_tree_ = nullptr;

    QModelIndex proxy_index = ui->tvOPCTree->selectionModel()->currentIndex();

    if (proxy_index.isValid()) {
        auto *proxy_model = qobject_cast<QSortFilterProxyModel*>(ui->tvOPCTree->model());
        if (proxy_model) {
            QModelIndex source_index = proxy_model->mapToSource(proxy_index);
            auto *item = static_cast<DataBrowseItem*>(source_index.internalPointer());
            if (item) {
                selected_item_opc_tree_ = item;
                delete_allow = (item->GetType() == DataBrowseItem::ItemType::ENDPOINT || item->GetType() == DataBrowseItem::ItemType::HOST);
            }
        }
    }

    selected_item_opc_tree_ = delete_allow ? selected_item_opc_tree_ : nullptr;

    delete_endpoint.setEnabled(delete_allow);
    QObject::connect(&add_endpoint, &QAction::triggered, this, &OpcBrowseWidget::sl_add_endpoint_to_tree);
    QObject::connect(&delete_endpoint, &QAction::triggered, this, &OpcBrowseWidget::sl_delete_endpoint_from_tree);

    QMenu context_menu(ui->tvOPCTree);
    context_menu.addAction(&add_endpoint);
    context_menu.addSeparator();
    context_menu.addAction(&delete_endpoint);
    context_menu.exec(ui->tvOPCTree->mapToGlobal(pos));
}

void OpcBrowseWidget::sl_add_endpoint_to_tree()
{
    OPCAddHostDialog* new_host_dialog = new OPCAddHostDialog(driver_manager_, this);
    QObject::connect(new_host_dialog, &OPCAddHostDialog::sg_add_new_endpoint, this, &OpcBrowseWidget::sl_add_new_endpoint_to_tree);
    new_host_dialog->exec();
    new_host_dialog->deleteLater();
}

void OpcBrowseWidget::sl_delete_endpoint_from_tree()
{
    using enum DataBrowseItem::ItemType;
    if(!selected_item_opc_tree_) return;
    if(selected_item_opc_tree_->GetType() == ENDPOINT
        || selected_item_opc_tree_->GetType() == HOST) {
        auto old_model = ui->tblvOPCTags->model();
        ui->tblvOPCTags->setModel(new OPCTagsViewerModel(nullptr, u""_s, driver_manager_, ui->tblvOPCTags));
        ui->tblvOPCTags->horizontalHeader()->setStretchLastSection(false);
        ui->tblvOPCTags->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        ui->tblvOPCTags->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        ui->tblvOPCTags->horizontalHeader()->resizeSection(1, 120);
        old_model->deleteLater();
    }

    if(selected_item_opc_tree_->GetType() == ENDPOINT) {
        data_model_->DeleteEndpoint(selected_item_opc_tree_->ParentItem()->GetId(), selected_item_opc_tree_->GetId());
    } else if(selected_item_opc_tree_->GetType() == HOST) {
        data_model_->DeleteHost(selected_item_opc_tree_->GetId());
    } else {
        return;
    }
    selected_item_opc_tree_ = nullptr;
}

void OpcBrowseWidget::sl_add_new_endpoint_to_tree(QString hostname, QString endpoint, uint8_t data_source)
{
    data_model_->AddHost(hostname);
    auto host_node_ptr = data_model_->GetHost(hostname);

    DataTag::DataSource src;

    switch(data_source) {
    case 1:
        src = DataTag::DataSource::OPCDA;
        break;
    case 2:
        src = DataTag::DataSource::OPCUA;
        break;
    default: return;
    }

    DriverInterface* driver = driver_manager_->GetDriverPtr(src);

    if(!host_node_ptr->ChildByName(endpoint)) {
        QString mes;
        bool res = data_model_->AddEndpoint(hostname, endpoint, src);
        mes = res ? u"Добавлен источник данных "_s : u"Не удалось добавить источник данных "_s;
        mes.append(QString("[%1] к хосту [%2]").arg(endpoint, hostname));
        emit sg_send_message_to_console(mes);

        if(res) {
            qInfo() << mes;
        } else {
            qWarning() << mes;
            return;
        }
    }

    auto node = driver->GetVariablesNode(hostname, endpoint);
    if(node) {
        data_model_->AbsorbChildNodes(hostname, endpoint, node);
    }
}

void OpcBrowseWidget::sl_browser_get_part_tags(const QString& hostname, const QString& server_name, size_t n_tags)
{
    auto ep_node_ptr = data_model_->GetEndpoint(hostname, server_name);
    if(!ep_node_ptr) {
        QString mes = QString("Не найден источник данных [%1] на хосте [%2]").arg(server_name, hostname);
        qWarning() << mes;
        emit sg_send_message_to_console(mes);
        return;
    }

    QString mes = QString("Хост [%1] обзор источника данных [%2]. Получено [%3] тэгов.").arg(server_name, hostname).arg(n_tags);
    qInfo() << mes;
    emit sg_send_message_to_console(mes);

    ep_node_ptr->SetTempVarCount(n_tags);
    data_model_->sl_data_changed(hostname, server_name);
}

void OpcBrowseWidget::sl_browser_get_all_tags(const QString& hostname, const QString& server_name, size_t n_tags)
{
    auto ep_node_ptr = data_model_->GetEndpoint(hostname, server_name);
    if(!ep_node_ptr) {
        QString mes = QString("Не найден источник данных [%1] на хосте [%2]").arg(server_name, hostname);
        qWarning() << mes;
        emit sg_send_message_to_console(mes);
        return;
    }

    QString mes = QString("Хост [%1] закончен обзор источник данных [%2]. Получено [%3] тэгов.").arg(server_name, hostname).arg(n_tags);
    qInfo() << mes;
    emit sg_send_message_to_console(mes);

    DriverInterface* driver = driver_manager_->GetDriverPtr(ep_node_ptr->GetSource());

    auto node = driver->GetVariablesNode(hostname, server_name);
    if(!node) return;

    data_model_->AbsorbChildNodes(hostname, server_name, node);
    ep_node_ptr->SetBrowsed(true);
    ep_node_ptr->UpdateItemRecursievly();
    delete node;
}

//===============================================================
//================ OPCAddServerDialog ===========================
//===============================================================

OPCAddHostDialog::OPCAddHostDialog(SourceDriverManager* driver_manager, QWidget *parent)
    : QDialog(parent, Qt::Dialog)
    , driver_manager_(driver_manager)
{
    setWindowTitle("Добавить источник данных");

    QVBoxLayout* main_la = new QVBoxLayout(this);

    QHBoxLayout* host_la = new QHBoxLayout();
    le_value_ = new QLineEdit();
    le_value_->setAlignment(Qt::AlignCenter);
    le_value_->setFixedWidth(150);
    QObject::connect(le_value_, &QLineEdit::editingFinished, this, [this](){});

    data_type_cb_ = new QComboBox();
    data_type_cb_->addItem(u"OPC DA"_s);
    data_type_cb_->addItem(u"OPC UA"_s);
    data_type_cb_->setCurrentIndex(0);
    QObject::connect(data_type_cb_, &QComboBox::currentIndexChanged, this, &OPCAddHostDialog::sl_data_type_changed);

    QPushButton* get_ep_btn = new QPushButton("Обзор");
    QObject::connect(get_ep_btn, &QAbstractButton::pressed, this, &OPCAddHostDialog::sl_get_ep_pressed);

    host_la->addWidget(le_value_);
    host_la->addWidget(data_type_cb_);
    host_la->addWidget(get_ep_btn);
    main_la->addLayout(host_la);

    ep_combobox_ = new QComboBox();
    main_la->addWidget(ep_combobox_);
    ep_combobox_->setEnabled(false);

    ok_btn_ = new QPushButton("OK");
    ok_btn_->setEnabled(false);
    QObject::connect(ok_btn_, &QAbstractButton::pressed, this, &OPCAddHostDialog::sl_ok_pressed);
    QPushButton* cancel_btn = new QPushButton("Отмена");
    QObject::connect(cancel_btn, &QAbstractButton::pressed, this, &QWidget::close);
    QHBoxLayout* buttons_la = new QHBoxLayout();
    buttons_la->addStretch(1);
    buttons_la->addWidget(ok_btn_);
    buttons_la->addWidget(cancel_btn);


    main_la->addLayout(buttons_la);
}

void OPCAddHostDialog::sl_ok_pressed()
{
    if(le_value_->text().length() > 0 && ep_combobox_->currentText().length() > 0) {
        emit sg_add_new_endpoint(le_value_->text(), ep_combobox_->currentText(), data_type_cb_->currentIndex() + 1);
        close();
    }
    ep_combobox_->setFocus();
}

void OPCAddHostDialog::sl_get_ep_pressed()
{
    if(!driver_manager_) return;

    DriverInterface* driver = get_current_driver_();
    if(!driver) return;

    QObject::connect(driver, &DriverInterface::sg_get_endpoints_names,
                     this, &OPCAddHostDialog::sl_endpoints_received_,
                     Qt::UniqueConnection);

    auto ep_set = driver->GetEndpointNames(le_value_->text());
    if(!ep_set.empty()) {
        sl_endpoints_received_(le_value_->text());
    }
}

void OPCAddHostDialog::sl_endpoints_received_(const QString& host)
{
    if(!driver_manager_) return;
    DriverInterface* driver = get_current_driver_();
    if(!driver) return;

    auto ep_set = driver->GetEndpointNames(host);
    if(ep_set.empty()) return;

    ep_combobox_->clear();
    for(auto& it: ep_set) {
        ep_combobox_->addItem(it);
    }
    ep_combobox_->setEnabled(true);
    ok_btn_->setEnabled(true);
}

DriverInterface* OPCAddHostDialog::get_current_driver_()
{
    switch(data_type_cb_->currentIndex()) {
    case 0: return driver_manager_->GetDriverPtr(DataTag::DataSource::OPCDA);
    case 1: return driver_manager_->GetDriverPtr(DataTag::DataSource::OPCUA);
    default: return nullptr;
    }
}

void OPCAddHostDialog::sl_data_type_changed(int index)
{
    if(data_type_cb_->currentText() == u"OPC UA"_s) {
        QString host_raw = le_value_->text();
        if (!host_raw.startsWith("opc.tcp://")) {
            host_raw = "opc.tcp://" + host_raw;
        }
        if (host_raw.count(':') == 1) {
            host_raw += ":4840";
        }
        le_value_->setText(host_raw);
    }

    if(data_type_cb_->currentText() == u"OPC DA"_s) {
        QString host_raw = le_value_->text();
        if (host_raw.startsWith("opc.tcp://")) {
            host_raw = host_raw.last(host_raw.size() - 10);
        }
        le_value_->setText(host_raw);
    }

}


//===============================================================
//================ OPCTagsViewerModel ===========================
//===============================================================

OPCTagsViewerModel::OPCTagsViewerModel(DataBrowseItem* parent_data_item, const QString& tag_prefix, SourceDriverManager* driver_manager, QObject *parent)
    : QAbstractTableModel(parent)
    , driver_manager_(driver_manager)
    , tag_prefix_(tag_prefix)
{
    source_ = parent_data_item ? parent_data_item->GetSource() : DataTag::DataSource::NONVALID;
    get_tags_from_data_item_recursievely_(parent_data_item);
}

int OPCTagsViewerModel::rowCount(const QModelIndex &parent) const
{
    return tags_browse_names_.size();
}

int OPCTagsViewerModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant OPCTagsViewerModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) return {};

    if(role == Qt::DisplayRole && index.column() == 0) {
        if(std::cmp_less(index.row(), tags_browse_names_.size())) {
            return tags_browse_names_.at(index.row());
        } else {
            return {};
        }
    }

    if(role == Qt::DisplayRole && index.column() == 1) {
        return driver_manager_->TagRegistry()->CheckTagExist(tags_browse_name_to_full_tag_name_.at(&tags_browse_names_.at(index.row()))) > 0;
    }

    if(role == Qt::BackgroundRole) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        if(index.column() == 0) {
            return Qt::AlignLeft;
        }
        return Qt::AlignCenter;
    }
    return {};
}

bool OPCTagsViewerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) return false;

    if(index.column() == 1 && std::cmp_less(index.row(), tags_browse_names_.size()))
    {
        QString tag_id = tags_browse_name_to_id_.at(&tags_browse_names_.at(index.row()));
        QString full_tag_name = QString("%1[%2]").arg(tag_prefix_, tag_id);
        if(value.toBool()) {
            driver_manager_->TagRegistry()->AddDataTag(source_, full_tag_name);
        } else {
            driver_manager_->DeleteTag(driver_manager_->TagRegistry()->CheckTagExist(full_tag_name));
        }
        return true;
    }
    return false;
}

QModelIndex OPCTagsViewerModel::index(int row, int column, const QModelIndex &parent) const
{
    if(row >=0 && std::cmp_less(row, tags_browse_names_.size()) && column >=0 && column < 2) {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QVariant OPCTagsViewerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch(section) {
        case 0: return QString("Имя тэга");
        case 1: return QString("Контроль\nизменения");
        }
    }
    if(role == Qt::DisplayRole && orientation == Qt::Vertical) {
        return section + 1;
    }

    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    return {};
}

void OPCTagsViewerModel::reset()
{
    QAbstractTableModel::beginResetModel();
    QAbstractTableModel::endResetModel();
}

void OPCTagsViewerModel::SetAllTagsToRead()
{
    for(const auto& it: tags_browse_names_) {
        QString tag_id = tags_browse_name_to_id_.at(&it);
        QString full_tag_name = QString("%1[%2]").arg(tag_prefix_, tag_id);

        if(driver_manager_->TagRegistry()->CheckTagExist(QString("%1[%2]").arg(tag_prefix_, tag_id)) == 0) {
            driver_manager_->TagRegistry()->AddDataTag(source_, full_tag_name);
        }
    }
    reset();
}

void OPCTagsViewerModel::DeleteAllTags()
{
    for(const auto& it: tags_browse_names_) {
        QString tag_id = tags_browse_name_to_id_.at(&it);
        QString full_tag_name = QString("%1[%2]").arg(tag_prefix_, tag_id);
        size_t id = driver_manager_->TagRegistry()->CheckTagExist(full_tag_name);
        if(id > 0) {
            driver_manager_->DeleteTag(id);
        }
    }
    reset();
}

void OPCTagsViewerModel::get_tags_from_data_item_recursievely_(DataBrowseItem *item)
{
    if(!item) return;
    for(size_t i = 0; std::cmp_less(i , item->ChildCount()); ++i) {
        DataBrowseItem* child = item->Child(i);
        if(child->GetType() == DataBrowseItem::ItemType::VARIABLE) {
            tags_browse_names_.push_back(child->GetBrowseName());
            tags_browse_name_to_id_[&tags_browse_names_.back()] = child->GetId();
            tags_browse_name_to_full_tag_name_[&tags_browse_names_.back()] = QString("%1[%2]").arg(tag_prefix_, tags_browse_name_to_id_.at(&tags_browse_names_.back()));
        }
        get_tags_from_data_item_recursievely_(child);
    }
}


//===================================================================
//================ SelectReadModeCheckBox ===========================
//===================================================================

void SelectReadModeCheckBox::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == 1) {
        painter->save();
        QCheckBox cb_prototype;
        cb_prototype.setChecked(index.data().toBool());
        const qreal x = option.rect.center().x() - cb_indicator_size_ / 2;
        const qreal y = option.rect.center().y() - cb_indicator_size_ / 2;
        painter->translate(QPointF(x, y));
        cb_prototype.render(painter);
        painter->restore();
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

bool SelectReadModeCheckBox::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{

    if(event->type() == QEvent::MouseButtonRelease)
    {
        model->setData(index, !model->data(index).toBool());
        event->accept();
        return true;
    }
    return false;
}

//======================================================================
//======= OpcBrowserTreeModel ==========================================
//======================================================================

DataBrowserTreeModel::DataBrowserTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , root_item_(std::unique_ptr<DataBrowseItem>(new DataBrowseItem(DataBrowseItem::ItemType::ROOT, DataTag::DataSource::NONVALID, u"root"_s, u"root"_s, nullptr)))
{}

DataBrowserTreeModel::DataBrowserTreeModel(std::unique_ptr<DataBrowseItem>&& root_node, QObject *parent)
    : QAbstractItemModel(parent)
    , root_item_(std::move(root_node))
{}


QVariant DataBrowserTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return {};
}

QModelIndex DataBrowserTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return {};

    DataBrowseItem *parentItem = parent.isValid()
                                     ? static_cast<DataBrowseItem*>(parent.internalPointer())
                                     : root_item_.get();

    if (auto *childItem = parentItem->Child(row))
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex DataBrowserTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return {};

    auto *childItem = static_cast<DataBrowseItem*>(index.internalPointer());
    DataBrowseItem *parentItem = childItem->ParentItem();

    return parentItem != root_item_.get()
               ? createIndex(parentItem->Row(), 0, parentItem) : QModelIndex{};
}

int DataBrowserTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;

    const DataBrowseItem *parentItem = parent.isValid()
                                           ? static_cast<const DataBrowseItem*>(parent.internalPointer())
                                           : root_item_.get();
    return parentItem->ChildCount();
}

int DataBrowserTreeModel::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant DataBrowserTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    const auto *item = static_cast<const DataBrowseItem*>(index.internalPointer());
    QFont font;

    switch(role) {
    case Qt::DisplayRole:
        return item->Data(index.column());

    case Qt::FontRole:
        if(!index.parent().isValid())
            font.setBold(true);
        if(item->GetType() == DataBrowseItem::ItemType::ENDPOINT) {
            font.setItalic(!item->IsBrowsed());
        }
        return font;

    case Qt::TextAlignmentRole:
        return index.column() == 0 ? Qt::AlignLeft : Qt::AlignHCenter;

    case Qt::ToolTipRole:
        if(index.column() == 0) {
            return item->Data(0);
        }
    }
    return {};
}

bool DataBrowserTreeModel::AddEndpoint(const QString &hostname, const QString &endpoint, DataTag::DataSource source)
{
    DataBrowseItem* host_ptr = root_item_->ChildByName(hostname);
    if(!host_ptr || host_ptr->ChildByName(endpoint)) return false;

    int rows_cnt = host_ptr->ChildCount();
    QModelIndex parent_index = index(host_ptr->Row(), 0, QModelIndex());
    DataBrowseItem* ep_item = new DataBrowseItem(DataBrowseItem::ItemType::ENDPOINT, source, endpoint, endpoint, host_ptr);

    beginInsertRows(parent_index, rows_cnt, rows_cnt);
    host_ptr->AppendChild(std::unique_ptr<DataBrowseItem>(ep_item));
    endInsertRows();

    return true;
}

bool DataBrowserTreeModel::DeleteEndpoint(const QString &hostname, const QString &endpoint)
{
    auto host_ptr = root_item_->ChildByName(hostname);
    auto ep_ptr = host_ptr->ChildByName(endpoint);

    if(!host_ptr || !ep_ptr) return false;

    int row_index = ep_ptr->Row();
    QModelIndex parent_index = index(host_ptr->Row(), 0, QModelIndex());

    beginRemoveRows(parent_index, row_index, row_index);
    host_ptr->DeleteChild(endpoint);
    endRemoveRows();
    return true;
}

DataBrowseItem *DataBrowserTreeModel::GetEndpoint(const QString &hostname, const QString &endpoint) const
{
    DataBrowseItem* host_ptr = root_item_->ChildByName(hostname);
    if(!host_ptr) return nullptr;
    return host_ptr->Child(endpoint);
}

DataBrowseItem *DataBrowserTreeModel::GetHost(const QString &hostname) const
{
    return root_item_->ChildByName(hostname);
}

bool DataBrowserTreeModel::AddHost(const QString &hostname)
{
    if(root_item_->ChildByName(hostname)) return false;

    int row_index = root_item_->ChildCount();

    beginInsertRows(QModelIndex(), row_index, row_index);
    DataBrowseItem* host_item = new DataBrowseItem(DataBrowseItem::ItemType::HOST, DataTag::DataSource::NONVALID, hostname, hostname, root_item_.get());
    root_item_->AppendChild(std::unique_ptr<DataBrowseItem>(host_item));
    endInsertRows();

    return true;
}

bool DataBrowserTreeModel::AbsorbChildNodes(const QString &hostname, const QString &endpoint, DataBrowseItem *item)
{
    if(!item) return false;
    DataBrowseItem* host_ptr = root_item_->ChildByName(hostname);
    if(!host_ptr) return false;
    DataBrowseItem* ep_ptr = host_ptr->Child(endpoint);
    if(!ep_ptr) return false;
    beginResetModel();
    ep_ptr->AbsorbChilds(item);
    endResetModel();
    return true;
}

void DataBrowserTreeModel::sl_data_changed(const QString &hostname, const QString &server_name)
{
    auto ep_ptr = root_item_->FindChildItemRecursievly(server_name);

    if(!ep_ptr
        || !ep_ptr->ParentItem()
        || ep_ptr->ParentItem()->GetType() != DataBrowseItem::ItemType::ENDPOINT)
        return;

    int row_index = ep_ptr->Row();
    QModelIndex ep_index_start = createIndex(row_index, 0, ep_ptr);
    QModelIndex ep_index_stop = createIndex(row_index, columnCount() - 1, ep_ptr);

    emit dataChanged(ep_index_start, ep_index_stop);
}

bool DataBrowserTreeModel::DeleteHost(const QString &hostname)
{
    auto host_ptr = root_item_->ChildByName(hostname);

    if(!host_ptr || host_ptr->GetType() != DataBrowseItem::ItemType::HOST) return false;

    int row_index = host_ptr->Row();

    beginRemoveRows(QModelIndex(), row_index, row_index);
    root_item_->DeleteChild(hostname);
    endRemoveRows();
    return true;
}

//======================================================================
//======= DataBrowserFilterProxyModel ==================================
//======================================================================


