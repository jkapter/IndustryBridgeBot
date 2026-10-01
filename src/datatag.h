#ifndef DATATAG_H
#define DATATAG_H

#include <QString>
#include <QVariant>
#include <QMutex>

using ValueVariant = std::variant<int64_t, double, QString>;


namespace DATATAG {

ValueVariant operator-(ValueVariant lhs, ValueVariant rhs);
ValueVariant operator+(ValueVariant lhs, ValueVariant rhs);
bool operator<(ValueVariant lhs, ValueVariant rhs);
bool operator>(ValueVariant lhs, ValueVariant rhs);
bool operator==(ValueVariant lhs, ValueVariant rhs);

QString toString(ValueVariant val);
double toDouble(ValueVariant val);
int64_t toLongLong(ValueVariant val);
}

class DataTag
{
public:
    enum class DataSource : uint8_t {
        NONVALID,
        OPCDA,
        OPCUA
    };

    enum class DataType: uint8_t {
        UNKNOWN,
        UNSUPPORTED,
        FLOAT4,
        REAL8,
        BOOLEAN,
        WSTRING,
        INT1,
        INT2,
        INT4,
        INT8,
        UINT1,
        UINT2,
        UINT4,
        UINT8
    };

    enum class DataQuality: uint8_t {
        BAD,
        UNCERTAIN,
        GOOD
    };

    enum class DataType: uint8_t;
    enum class DataQuality: uint8_t;

    DataTag() = delete;
    explicit DataTag(DataSource src, const QString& host, const QString& endpoint, const QString& tagname);

    DataTag(const DataTag&) = delete;
    DataTag(DataTag&&) = delete;
    DataTag operator=(DataTag&) = delete;
    DataTag operator=(DataTag&&) = delete;
    virtual ~DataTag() = default;
    DataSource GetDataSource() const;

    const QString& GetHostName() const;
    const QString& GetEndpointName() const;
    virtual const QString& GetTagId() const;
    virtual const QString& GetTagName() const;
    QString GetFullTagDescription() const;

    QString GetStringType();
    QString GetStringValue(bool use_substitute_values = true);
    ValueVariant GetValue(bool use_substitute_values = true);

    DataQuality GetTagQuality();
    static QString QualityToString(DataQuality q);
    bool TagQualityIsGood();

    bool ValueIsInteger();
    bool ValueIsReal();
    bool ValueIsUnsignedInteger();
    bool ValueIsString();
    bool ValueIsBool();

    const QString& GetCommentString() const;
    void SetCommentString(const QString& str);
    void SetCommentString(QString&& str);

    std::optional<double> GetGainOption() const;
    void SetGainOption(double gain);
    void ResetGainOption();

    void SetValueToWrite(QVariant val);
    void SetValueToWrite(ValueVariant val);
    void ResetValueToWrite();

    void AddSubstituteStringValue(const QString& raw_value, const QString& substitute_value);
    const std::unordered_map<QString, QString>& GetSubstituteStringValues() const;
    void ClearSubstituteStringValues();
    QJsonObject TagToJson(bool full_info = true) const;

private:
    DataSource tag_source_;

protected:
    DataType data_type_;
    QString comment_;
    QString hostname_;
    QString endpoint_name_;
    QString tag_id_;
    QVariant value_;
    DataQuality tag_quality_;
    std::optional<double> gain_value_ = std::nullopt;
    QVariant value_to_write_;
    std::unordered_map<QString, QString> substitute_values_;
    QMutex mtx_;
};

//=========================================================================
//================== DataBrowseItem =======================================
//=========================================================================

class DataBrowseItem {
public:

    enum class ItemType: uint8_t {
        ROOT,
        HOST,
        ENDPOINT,
        NODE,
        VARIABLE,
        INVALID
    };

    DataBrowseItem(ItemType type, DataTag::DataSource source, const QString &name, const QString &id, DataBrowseItem *parent);
    DataBrowseItem *Child(int row) const;
    DataBrowseItem *Child(const QString &id) const;
    DataBrowseItem *ChildByName(const QString &name) const;
    DataBrowseItem *ChildById(const QString &id) const;
    ~DataBrowseItem();
    int ChildCount(bool exclude_variables = false) const;
    QVariant Data(int column) const;
    int Row() const;
    DataBrowseItem *ParentItem();
    const QString& GetId() const;
    const QString& GetBrowseName() const;
    DataTag::DataSource GetSource() const;
    DataBrowseItem::ItemType GetType() const;
    void SetTempVarCount(size_t n);
    void AbsorbChilds(DataBrowseItem *other_item);
    size_t UpdateItemRecursievly();
    bool AppendChild(std::unique_ptr<DataBrowseItem> &&child, int row = -1);
    bool AppendChild(ItemType type, const QString &name, const QString &id, int row = -1);
    DataBrowseItem *FindChildItemRecursievly(const QString &node_id) const;
    bool DeleteChild(const QString& id);
    void SetBrowsed(bool b) {was_browsed_from_source_ = b;}
    bool IsBrowsed() const {return was_browsed_from_source_;}

private:
    ItemType type_;
    DataTag::DataSource source_;
    QString item_id_;
    QString item_browse_name_;
    DataBrowseItem *parent_;
    size_t n_child_variables_recursive_ = 0;

    bool was_browsed_from_source_ = false;

    std::unordered_map<const QString*, std::unique_ptr<DataBrowseItem>> childs_;
    std::unordered_map<QString, DataBrowseItem*> name_to_child_ptr_cache_;
    std::unordered_map<QString, DataBrowseItem*> id_to_child_ptr_cache_;
    std::list<QString> child_ids_;

    size_t get_variables_count_() const;
    DataBrowseItem *check_childs_(const DataBrowseItem *parent_item, const QString &node_id) const;

};

/*
const WORD OPC_QUALITY_MASK	=	0xc0;
const WORD OPC_STATUS_MASK	=	0xfc;
const WORD OPC_LIMIT_MASK	=	0x3;
const WORD OPC_QUALITY_BAD	=	0;
const WORD OPC_QUALITY_UNCERTAIN	=	0x40;
const WORD OPC_QUALITY_GOOD	=	0xc0;
const WORD OPC_QUALITY_CONFIG_ERROR	=	0x4;
const WORD OPC_QUALITY_NOT_CONNECTED	=	0x8;
const WORD OPC_QUALITY_DEVICE_FAILURE	=	0xc;
const WORD OPC_QUALITY_SENSOR_FAILURE	=	0x10;
const WORD OPC_QUALITY_LAST_KNOWN	=	0x14;
const WORD OPC_QUALITY_COMM_FAILURE	=	0x18;
const WORD OPC_QUALITY_OUT_OF_SERVICE	=	0x1c;
const WORD OPC_QUALITY_WAITING_FOR_INITIAL_DATA	=	0x20;
const WORD OPC_QUALITY_LAST_USABLE	=	0x44;
const WORD OPC_QUALITY_SENSOR_CAL	=	0x50;
const WORD OPC_QUALITY_EGU_EXCEEDED	=	0x54;
const WORD OPC_QUALITY_SUB_NORMAL	=	0x58;
const WORD OPC_QUALITY_LOCAL_OVERRIDE	=	0xd8;
const WORD OPC_LIMIT_OK	=	0;
const WORD OPC_LIMIT_LOW	=	0x1;
const WORD OPC_LIMIT_HIGH	=	0x2;
const WORD OPC_LIMIT_CONST	=	0x3;
*/

#endif // DATATAG_H
