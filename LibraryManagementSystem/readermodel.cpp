#include "readermodel.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

ReaderModel::ReaderModel(QObject *parent, QSqlDatabase db)
    : QSqlTableModel(parent, db)
{
    setTable("readers");
    setEditStrategy(QSqlTableModel::OnManualSubmit);
    setHeaderData(0, Qt::Horizontal, "ID");
    setHeaderData(1, Qt::Horizontal, "读者编号");
    setHeaderData(2, Qt::Horizontal, "姓名");
    setHeaderData(3, Qt::Horizontal, "性别");
    setHeaderData(4, Qt::Horizontal, "电话");
    setHeaderData(5, Qt::Horizontal, "邮箱");
    setHeaderData(6, Qt::Horizontal, "地址");
    setHeaderData(7, Qt::Horizontal, "注册日期");
    setHeaderData(8, Qt::Horizontal, "状态");
    // 检查是否有create_time字段，如果有则设置表头
    QSqlQuery checkQuery(database());
    if (checkQuery.exec("PRAGMA table_info(readers)")) {
        int colIndex = 9;
        while (checkQuery.next()) {
            QString colName = checkQuery.value(1).toString().toLower();
            if (colName == "create_time") {
                setHeaderData(colIndex, Qt::Horizontal, "创建时间");
                break;
            }
            colIndex++;
        }
    }
    select();
}

QVariant ReaderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    
    return QSqlTableModel::data(index, role);
}

bool ReaderModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    
    return QSqlTableModel::setData(index, value, role);
}

Qt::ItemFlags ReaderModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool ReaderModel::addReader(const QString& readerId, const QString& name, const QString& gender,
                            const QString& phone, const QString& email, const QString& address)
{
    qDebug() << "=== 开始添加读者 ===";
    qDebug() << "步骤1: 清理输入数据";
    QString cleanReaderId = readerId.trimmed();
    QString cleanName = name.trimmed();
    qDebug() << "读者编号:" << cleanReaderId << "姓名:" << cleanName;

    // 检查读者编号是否为空
    if (cleanReaderId.isEmpty() || cleanName.isEmpty()) {
        qDebug() << "添加读者失败: 读者编号和姓名不能为空";
        return false;
    }

    qDebug() << "步骤2: 检查读者编号是否已存在";
    if (readerExists(cleanReaderId)) {
        qDebug() << "添加读者失败: 读者编号已存在:" << cleanReaderId;
        return false;
    }
    qDebug() << "读者编号检查通过";

    qDebug() << "步骤3: 准备SQL语句";
    QSqlQuery query(database());
    // 获取当前时间作为注册日期
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    qDebug() << "当前时间(注册日期):" << currentTime;

    // 直接固定插入注册日期，不处理create_time
    QString sql = "INSERT INTO readers (reader_id, name, gender, phone, email, address, register_date) "
                  "VALUES (:reader_id, :name, :gender, :phone, :email, :address, :register_date)";
    qDebug() << "SQL语句:" << sql;

    qDebug() << "步骤4: 准备查询";
    if (!query.prepare(sql)) {
        qDebug() << "添加读者失败: prepare失败:" << query.lastError().text();
        qDebug() << "错误代码:" << query.lastError().type();
        qDebug() << "SQL:" << sql;

        // 检查表结构
        qDebug() << "步骤4.1: 检查表结构";
        QSqlQuery checkQuery(database());
        if (checkQuery.exec("PRAGMA table_info(readers)")) {
            qDebug() << "表结构信息:";
            while (checkQuery.next()) {
                qDebug() << "  列名:" << checkQuery.value(1).toString()
                    << "类型:" << checkQuery.value(2).toString();
            }
        }
        return false;
    }
    qDebug() << "prepare成功";

    qDebug() << "步骤5: 绑定参数值";
    query.bindValue(":reader_id", cleanReaderId);
    query.bindValue(":name", cleanName);
    query.bindValue(":gender", gender.trimmed());
    query.bindValue(":phone", phone.trimmed());
    query.bindValue(":email", email.trimmed());
    query.bindValue(":address", address.trimmed());
    query.bindValue(":register_date", currentTime); // 仅绑定注册日期
    qDebug() << "参数绑定完成 - 注册日期:" << currentTime;

    qDebug() << "步骤6: 执行SQL";
    if (!query.exec()) {
        QString errorMsg = query.lastError().text();
        qDebug() << "添加读者失败: exec失败:" << errorMsg;
        qDebug() << "错误代码:" << query.lastError().type();
        qDebug() << "执行的SQL:" << query.lastQuery();
        return false;
    }
    qDebug() << "SQL执行成功";

    qDebug() << "步骤7: 刷新模型";
    select();
    qDebug() << "=== 添加读者完成 ===";
    return true;
}

bool ReaderModel::updateReader(int id, const QString& readerId, const QString& name, const QString& gender,
                               const QString& phone, const QString& email, const QString& address)
{
    QSqlQuery query(database());
    // 获取当前时间作为更新时间
    QString updateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    
    // 检查是否有update_time字段
    QSqlQuery checkQuery(database());
    bool hasUpdateTime = false;
    if (checkQuery.exec("PRAGMA table_info(readers)")) {
        while (checkQuery.next()) {
            if (checkQuery.value(1).toString().toLower() == "update_time") {
                hasUpdateTime = true;
                break;
            }
        }
    }
    
    QString sql;
    if (hasUpdateTime) {
        sql = "UPDATE readers SET reader_id=?, name=?, gender=?, phone=?, email=?, address=?, update_time=? WHERE id=?";
    } else {
        sql = "UPDATE readers SET reader_id=?, name=?, gender=?, phone=?, email=?, address=? WHERE id=?";
    }
    
    query.prepare(sql);
    query.addBindValue(readerId.trimmed());
    query.addBindValue(name.trimmed());
    query.addBindValue(gender.trimmed());
    query.addBindValue(phone.trimmed());
    query.addBindValue(email.trimmed());
    query.addBindValue(address.trimmed());
    if (hasUpdateTime) {
        query.addBindValue(updateTime);
    }
    query.addBindValue(id);
    
    if (!query.exec()) {
        qDebug() << "更新读者失败:" << query.lastError().text();
        return false;
    }
    
    select();
    return true;
}

bool ReaderModel::deleteReader(int id)
{
    QSqlQuery query(database());
    query.prepare("DELETE FROM readers WHERE id=?");
    query.addBindValue(id);
    
    if (!query.exec()) {
        qDebug() << "删除读者失败:" << query.lastError().text();
        return false;
    }
    
    select();
    return true;
}

void ReaderModel::filterReaders(const QString& readerId, const QString& name, 
                                const QString& phone, const QString& status)
{
    QString filter;
    if (!readerId.isEmpty()) {
        filter += QString("reader_id LIKE '%%1%'").arg(readerId);
    }
    if (!name.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("name LIKE '%%1%'").arg(name);
    }
    if (!phone.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("phone LIKE '%%1%'").arg(phone);
    }
    if (!status.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("status = '%1'").arg(status);
    }
    
    setFilter(filter);
    select();
}

bool ReaderModel::readerExists(const QString& readerId)
{
    QSqlQuery query(database());
    query.prepare("SELECT COUNT(*) FROM readers WHERE reader_id=?");
    query.addBindValue(readerId);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}
