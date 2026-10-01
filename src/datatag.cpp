#include "datatag.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QStringView>

#include <queue>

using namespace Qt::StringLiterals;

namespace DATATAG {

ValueVariant operator-(ValueVariant lhs, ValueVariant rhs) {
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) - toLongLong(rhs);
    case 1: return std::get<double>(lhs) - toDouble(rhs);
    case 2: return std::get<QString>(lhs);
    }
    return 0;
}

ValueVariant operator+(ValueVariant lhs, ValueVariant rhs) {
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) + toLongLong(rhs);
    case 1: return std::get<double>(lhs) + toDouble(rhs);
    case 2: return QString("%1%2").arg(toString(lhs), toString(rhs));
    }
    return 0;
}

bool operator<(ValueVariant lhs, ValueVariant rhs)
{
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) < toLongLong(rhs);
    case 1: return std::get<double>(lhs) < toDouble(rhs);
    case 2: return std::get<QString>(lhs) < toString(rhs);
    }
    return false;
}

bool operator==(ValueVariant lhs, ValueVariant rhs)
{
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) == toLongLong(rhs);
    case 1: return std::get<double>(lhs) == toDouble(rhs);
    case 2: return std::get<QString>(lhs) == toString(rhs);
    }
    return false;
}

bool operator>(ValueVariant lhs, ValueVariant rhs)
{
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) > toLongLong(rhs);
    case 1: return std::get<double>(lhs) > toDouble(rhs);
    case 2: return std::get<QString>(lhs) > toString(rhs);
    }
    return false;
}

QString toString(ValueVariant val)
{
    if(val.valueless_by_exception()) return "";
    switch(val.index()) {
    case 0: return QString("%1").arg(std::get<0>(val));
    case 1: return QString("%1").arg(std::get<1>(val));
    case 2: return QString("%1").arg(std::get<2>(val));
    }
    return "";
}

double toDouble(ValueVariant val) {
    if(val.index() == 0) return static_cast<double>(std::get<int64_t>(val));
    if(val.index() == 1) return std::get<double>(val);

    QString str = std::get<QString>(val);
    bool b = false;
    double ret_val = str.toDouble(&b);
    if(b) {
        return ret_val;
    } else {
        return 0;
    }
}

int64_t toLongLong(ValueVariant val) {
    if(val.index() == 0) return std::get<int64_t>(val);
    if(val.index() == 1) return std::numeric_limits<int64_t>::max() > std::get<double>(val) ? static_cast<int64_t>(std::get<double>(val)) : std::numeric_limits<int64_t>::max();

    QString str = std::get<QString>(val);
    bool b = false;
    double ret_val = str.toLongLong(&b);
    if(b) {
        return ret_val;
    } else {
        return 0;
    }
}
} //namespace

DataTag::DataTag(DataSource src, const QString& host, const QString& endpoint, const QString& tagname)
    : tag_source_(src)
    , data_type_(DataTag::DataType::UNKNOWN)
    , comment_(QString("[%1][%2]").arg(u"localhost"_s, endpoint))
    , hostname_(host)
    , endpoint_name_(endpoint)
    , tag_id_(tagname)
    , tag_quality_(DataTag::DataQuality::BAD)
{}

DataTag::DataSource DataTag::GetDataSource() const
{
    return tag_source_;
}

const QString &DataTag::GetHostName() const
{
    return hostname_;
}

const QString &DataTag::GetTagName() const
{
    return tag_id_;
}

QString DataTag::GetFullTagDescription() const
{
    return QString("[%1][%2][%3]").arg(hostname_, endpoint_name_, tag_id_);
}

QString DataTag::GetStringType()
{
    QMutexLocker locker(&mtx_);
    QString ret_str;
    switch(data_type_) {
    case DataType::BOOLEAN      : ret_str = u"BOOLEAN"_s; break;
    case DataType::FLOAT4       : ret_str = u"FLOAT4"_s; break;
    case DataType::INT1         : ret_str = u"INT1"_s; break;
    case DataType::INT2         : ret_str = u"INT2"_s; break;
    case DataType::INT4         : ret_str = u"INT4"_s; break;
    case DataType::INT8         : ret_str = u"INT8"_s; break;
    case DataType::REAL8        : ret_str = u"REAL8"_s; break;
    case DataType::UINT1        : ret_str = u"UINT1"_s; break;
    case DataType::UINT2        : ret_str = u"UINT2"_s; break;
    case DataType::UINT4        : ret_str = u"UINT4"_s; break;
    case DataType::UINT8        : ret_str = u"UINT8"_s; break;
    case DataType::UNKNOWN      : ret_str = u"UNKNOWN"_s; break;
    case DataType::WSTRING      : ret_str = u"WSTRING"_s; break;
    case DataType::UNSUPPORTED  : ret_str = u"UNSUPPORTED"_s; break;
    default                     : ret_str = u"UNSUPPORTED"_s; break;
    }
    return ret_str;
}

QString DataTag::GetStringValue(bool use_substitute_values)
{
    QMutexLocker locker(&mtx_);
    QString ret_str;
    if(data_type_ == DataType::UNKNOWN) return ret_str;
    if(data_type_ == DataType::UNSUPPORTED) return value_.canConvert<QString>() ? value_.toString() : ret_str;

    switch(data_type_) {
    case DataType::BOOLEAN:
        ret_str = value_.toBool() ? "ДА" : "НЕТ";
        break;
    case DataType::INT1:
    case DataType::INT2:
    case DataType::INT4:
    case DataType::INT8:
        ret_str = QString::number(value_.toLongLong());
        break;
    case DataType::UINT1:
    case DataType::UINT2:
    case DataType::UINT4:
    case DataType::UINT8:
        ret_str = QString::number(value_.toULongLong());
        break;
    default:
        ret_str = value_.toString();
        break;
    }

    if(use_substitute_values && substitute_values_.contains(ret_str)) ret_str = substitute_values_.at(ret_str);
    return ret_str;
}

ValueVariant DataTag::GetValue(bool use_substitute_values)
{
    if(ValueIsBool()) return static_cast<int64_t>(value_.toBool());
    if(ValueIsInteger()) return static_cast<int64_t>(value_.toLongLong());
    if(ValueIsUnsignedInteger()) return value_.toLongLong() < std::numeric_limits<int64_t>::max() ? static_cast<int64_t>(value_.toLongLong()) : std::numeric_limits<int64_t>::max();
    if(ValueIsReal()) return value_.toDouble();
    return GetStringValue(use_substitute_values);
}

QString DataTag::QualityToString(DataQuality q)
{
    switch(q) {
        using enum DataQuality;
    case BAD:           return u"BAD"_s;
    case GOOD:          return u"GOOD"_s;
    case UNCERTAIN:     return u"UNCERTAIN"_s;
    }
    return "";
}

const QString &DataTag::GetEndpointName() const
{
    return endpoint_name_;
}

const QString &DataTag::GetTagId() const
{
    return tag_id_;
}

DataTag::DataQuality DataTag::GetTagQuality()
{
    QMutexLocker locker(&mtx_);
    return tag_quality_;
}

bool DataTag::TagQualityIsGood()
{
    QMutexLocker locker(&mtx_);
    return tag_quality_ == DataTag::DataQuality::GOOD;
}

bool DataTag::ValueIsInteger()
{
    QMutexLocker locker(&mtx_);
    return data_type_ == DataType::INT1
            || data_type_ == DataType::INT2
            || data_type_ == DataType::INT4
            || data_type_ == DataType::INT8;
}

bool DataTag::ValueIsReal()
{
    QMutexLocker locker(&mtx_);
    return data_type_ == DataType::FLOAT4
           || data_type_ == DataType::REAL8;
}

bool DataTag::ValueIsUnsignedInteger()
{
    QMutexLocker locker(&mtx_);
    return data_type_ == DataType::UINT1
           || data_type_ == DataType::UINT2
           || data_type_ == DataType::UINT4
           || data_type_ == DataType::UINT8;
}

bool DataTag::ValueIsString()
{
    QMutexLocker locker(&mtx_);
    return data_type_ == DataType::WSTRING;
}

bool DataTag::ValueIsBool()
{
    QMutexLocker locker(&mtx_);
    return data_type_ == DataType::BOOLEAN;
}

const QString &DataTag::GetCommentString() const
{
    return comment_;
}

void DataTag::SetCommentString(const QString &str)
{
    comment_ = str;
}

void DataTag::SetCommentString(QString &&str)
{
    comment_ = std::move(str);
}

std::optional<double> DataTag::GetGainOption() const
{
    return gain_value_;
}

void DataTag::SetGainOption(double gain)
{
    gain_value_ = gain;
}

void DataTag::ResetGainOption()
{
    gain_value_.reset();
}

void DataTag::SetValueToWrite(QVariant val)
{
    QMutexLocker locker(&mtx_);
    value_to_write_ = val;
}

void DataTag::SetValueToWrite(ValueVariant val)
{
    QMutexLocker locker(&mtx_);
    if(val.valueless_by_exception()) return;
    switch(val.index()) {
    case 0: value_to_write_ = std::get<0>(val); break;
    case 1: value_to_write_ = std::get<1>(val); break;
    case 2: value_to_write_ = std::get<2>(val); break;
    }
}

void DataTag::ResetValueToWrite()
{
    QMutexLocker locker(&mtx_);
    value_to_write_.clear();
}

void DataTag::AddSubstituteStringValue(const QString &raw_value, const QString &substitute_value)
{
    substitute_values_[raw_value] = substitute_value;
}

const std::unordered_map<QString, QString> &DataTag::GetSubstituteStringValues() const
{

    return substitute_values_;
}

void DataTag::ClearSubstituteStringValues()
{
    substitute_values_.clear();
}

QJsonObject DataTag::TagToJson(bool full_info) const
{
    QJsonObject ret_obj;
    ret_obj.insert("tag_id", tag_id_);
    ret_obj.insert("tag_comment", comment_);
    if(full_info) {
        ret_obj.insert("server", endpoint_name_);
        ret_obj.insert("hostname", hostname_);
    }

    QString source_str;
    switch(tag_source_) {
    case DataSource::OPCDA  : source_str = "OPCDA"; break;
    case DataSource::OPCUA  : source_str = "OPCUA"; break;
    default                 : source_str = "NONVALID"; break;
    }
    ret_obj.insert("source", source_str);
    if(gain_value_.has_value()) {
        ret_obj.insert("gain", gain_value_.value());
    }
    if(substitute_values_.size() > 0) {
        QJsonArray subst_array;
        for(const auto& [val, substitute_val]: substitute_values_) {
            subst_array.append(QJsonObject{{val, substitute_val}});
        }
        ret_obj.insert("substitute_values", std::move(subst_array));
    }
    return ret_obj;
}

//=========================================================================
//================== DataBrowseItem =======================================
//=========================================================================

DataBrowseItem::DataBrowseItem(ItemType type, DataTag::DataSource source, const QString& name, const QString &id, DataBrowseItem *parent)
    : type_(type)
    , source_(source)
    , item_id_(id)
    , item_browse_name_(name)
    , parent_(parent)
{
    bool b = false;

    b = !parent && type_ != ItemType::ROOT;
    b = !b && (parent && parent->type_ == ItemType::ROOT && type_ != ItemType::HOST);
    b = !b && (parent && parent->type_ == ItemType::HOST && type_ != ItemType::ENDPOINT);
    b = !b && (parent && parent->type_ == ItemType::ENDPOINT && !(type_ == ItemType::NODE || type_ == ItemType::VARIABLE));

    if(b) type_ = ItemType::INVALID;
}

DataBrowseItem *DataBrowseItem::Child(int row) const
{
    if(row < 0 || row >= static_cast<int>(child_ids_.size())) return nullptr;
    auto it = child_ids_.cbegin();
    std::advance(it, row);
    return childs_.at(&(*it)).get();
}

DataBrowseItem *DataBrowseItem::Child(const QString &id) const
{
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it == child_ids_.end()) return nullptr;
    return childs_.at(&(*it)).get();
}

DataBrowseItem *DataBrowseItem::ChildByName(const QString &name) const
{
    if(name_to_child_ptr_cache_.contains(name)) {
        return name_to_child_ptr_cache_.at(name);
    }
    return nullptr;
}

DataBrowseItem *DataBrowseItem::ChildById(const QString &id) const
{

    if(id_to_child_ptr_cache_.contains(id)) {
        return id_to_child_ptr_cache_.at(id);
    }
    return nullptr;
}

DataBrowseItem::~DataBrowseItem()
{
    std::queue<std::unique_ptr<DataBrowseItem>> nodes_to_delete;

    for (auto& [_, child_ptr] : childs_) {
        if (child_ptr) {
            nodes_to_delete.push(std::move(child_ptr));
        }
    }

    childs_.clear();
    child_ids_.clear();

    while (!nodes_to_delete.empty()) {
        std::unique_ptr<DataBrowseItem> current_node = std::move(nodes_to_delete.front());
        nodes_to_delete.pop();

        if (!current_node) continue;

        for (auto& [_, sub_child_ptr] : current_node->childs_) {
            if (sub_child_ptr) {
                nodes_to_delete.push(std::move(sub_child_ptr));
            }
        }

        current_node->childs_.clear();
        current_node->child_ids_.clear();
    }
}

int DataBrowseItem::ChildCount(bool exclude_variables) const
{
    if(exclude_variables) {
        size_t ret_val = 0;
        for(const auto& [id, node_ptr]: childs_) {
            if(node_ptr->type_ != ItemType::VARIABLE) {
                ++ret_val;
            }
        }
        return ret_val;
    }
    return child_ids_.size();
}

QVariant DataBrowseItem::Data(int column) const
{
    if(column == 0 && !(type_ == ItemType::ROOT || type_ == ItemType::VARIABLE || type_ == ItemType::INVALID)) return item_browse_name_;
    if(column == 1 && type_ == ItemType::ENDPOINT) {
        switch(source_) {
        case DataTag::DataSource::OPCDA: return u"[OPC  DA]"_s;
        case DataTag::DataSource::OPCUA: return u"[OPC  UA]"_s;
        default:                return u"[UNKNOWN]"_s;
        }
    }
    if(column == 2 && (type_ == ItemType::ENDPOINT || type_ == ItemType::NODE)) {
        return QString("[%1]").arg(n_child_variables_recursive_);
    }
    return QVariant();
}

int DataBrowseItem::Row() const
{
    if(!parent_) return 0;
    auto it = std::find(parent_->child_ids_.begin(), parent_->child_ids_.end(), item_id_);

    if(it == parent_->child_ids_.end()) return -1;

    return std::distance(parent_->child_ids_.begin(), it);
}

DataBrowseItem *DataBrowseItem::ParentItem()
{
    return parent_;
}

const QString &DataBrowseItem::GetId() const
{
    return item_id_;
}

const QString &DataBrowseItem::GetBrowseName() const
{
    return item_browse_name_;
}

DataTag::DataSource DataBrowseItem::GetSource() const
{
    return source_;
}

DataBrowseItem::ItemType DataBrowseItem::GetType() const
{
    return type_;
}

void DataBrowseItem::SetTempVarCount(size_t n)
{
    n_child_variables_recursive_ = n;
}

void DataBrowseItem::AbsorbChilds(DataBrowseItem *other_item)
{
    for(auto& [_, child_ptr]: other_item->childs_) {
        AppendChild(std::move(child_ptr));
    }
    other_item->childs_.clear();
    other_item->child_ids_.clear();
    other_item->name_to_child_ptr_cache_.clear();
    other_item->id_to_child_ptr_cache_.clear();
}

size_t DataBrowseItem::UpdateItemRecursievly()
{
    n_child_variables_recursive_ = get_variables_count_();
    return n_child_variables_recursive_;
}

size_t DataBrowseItem::get_variables_count_() const
{
    size_t ret_val = ChildCount(false /*all childes*/) - ChildCount(true/*exclude variables*/);
    for(const auto& [_, it]: childs_) {
        ret_val += it->UpdateItemRecursievly();
    }
    return ret_val;
}

bool DataBrowseItem::AppendChild(std::unique_ptr<DataBrowseItem> &&child, int row)
{
    if(!child) return false;
    if(!(type_ == ItemType::ROOT || type_ == ItemType::HOST) && child->source_ != source_) return false;

    switch(child->type_) {
    case ItemType::ROOT:        return false;
    case ItemType::HOST:        if(type_ != ItemType::ROOT) return false; break;
    case ItemType::ENDPOINT:    if(type_ != ItemType::HOST) return false; break;
    case ItemType::NODE:        if(type_ != ItemType::ENDPOINT && type_ != ItemType::NODE) return false; break;
    case ItemType::VARIABLE:    if(type_ != ItemType::ENDPOINT && type_ != ItemType::NODE) return false; break;
    default:                    return false;
    }

    auto it = std::find(child_ids_.begin(), child_ids_.end(), child->item_id_);
    if(it != child_ids_.end()) return false;

    if(row == -1) {
        child_ids_.push_back(child->item_id_);
        childs_[&child_ids_.back()] = std::move(child);
        childs_.at(&child_ids_.back())->parent_ = this;
        DataBrowseItem* child_raw_ptr = childs_.at(&child_ids_.back()).get();
        name_to_child_ptr_cache_[child_raw_ptr->GetBrowseName()] = child_raw_ptr;
        id_to_child_ptr_cache_[child_raw_ptr->GetId()] = child_raw_ptr;
        return true;
    }
    if(row < 0 || row > static_cast<int>(child_ids_.size())) {
        return false;
    }

    auto it_pos = child_ids_.begin();
    std::advance(it_pos, row);
    it = child_ids_.insert(it_pos, child->item_id_);
    childs_[&(*it)] = std::move(child);
    childs_.at(&(*it))->parent_ = this;
    DataBrowseItem* child_raw_ptr = childs_.at(&(*it)).get();
    name_to_child_ptr_cache_[child_raw_ptr->GetBrowseName()] = child_raw_ptr;
    id_to_child_ptr_cache_[child_raw_ptr->GetId()] = child_raw_ptr;
    return true;
}

bool DataBrowseItem::AppendChild(ItemType type, const QString& name, const QString& id, int row)
{
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it != child_ids_.end()) return false;
    return AppendChild(std::make_unique<DataBrowseItem>(type, source_, name, id, this), row);
}

DataBrowseItem *DataBrowseItem::check_childs_(const DataBrowseItem *parent_item, const QString &node_id) const
{
    if(auto node_ptr = parent_item->ChildById(node_id)) return node_ptr;

    for(const auto& [id_ptr, item_ptr]: parent_item->childs_) {
        if(auto item = check_childs_(item_ptr.get(), node_id)) return item;
    }
    return nullptr;
}

DataBrowseItem *DataBrowseItem::FindChildItemRecursievly(const QString &node_id) const
{
    return check_childs_(this, node_id);
}

bool DataBrowseItem::DeleteChild(const QString &id)
{
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it != child_ids_.end()) {
        if (auto* child_ptr = childs_.at(&(*it)).get()) {
            name_to_child_ptr_cache_.erase(child_ptr->GetBrowseName());
            id_to_child_ptr_cache_.erase(child_ptr->GetId());
        }
        childs_.erase(&(*it));
        child_ids_.erase(it);
        return true;
    }
    return false;
}

