#include "bookmodel.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

BookModel::BookModel(QObject *parent, QSqlDatabase db)
    : QSqlTableModel(parent, db)
{
    setTable("books");
    setEditStrategy(QSqlTableModel::OnManualSubmit);
    setHeaderData(0, Qt::Horizontal, "ID");
    setHeaderData(1, Qt::Horizontal, "ISBN");
    setHeaderData(2, Qt::Horizontal, "书名");
    setHeaderData(3, Qt::Horizontal, "作者");
    setHeaderData(4, Qt::Horizontal, "出版社");
    setHeaderData(5, Qt::Horizontal, "出版日期");
    setHeaderData(6, Qt::Horizontal, "分类");
    select();
}

QVariant BookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    
    return QSqlTableModel::data(index, role);
}

bool BookModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    
    return QSqlTableModel::setData(index, value, role);
}

Qt::ItemFlags BookModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool BookModel::isbnExists(const QString& isbn)
{
    if (isbn.trimmed().isEmpty()) {
        return false;
    }
    
    QSqlQuery query(database());
    query.prepare("SELECT COUNT(*) FROM books WHERE isbn = ?");
    query.addBindValue(isbn.trimmed());
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

bool BookModel::addBook(const QString& isbn, const QString& title, const QString& author,
                        const QString& publisher, const QString& publishDate, const QString& category,
                        int totalCopies)
{
    qDebug() << "=== 开始添加图书 ===";
    qDebug() << "步骤1: 清理ISBN";
    // 清理ISBN（去除前后空格）
    QString cleanIsbn = isbn.trimmed();
    qDebug() << "清理后的ISBN:" << cleanIsbn;
    
    // 检查ISBN是否为空
    if (cleanIsbn.isEmpty()) {
        qDebug() << "添加图书失败: ISBN不能为空";
        return false;
    }
    
    qDebug() << "步骤2: 检查ISBN是否已存在";
    // 检查ISBN是否已存在
    if (isbnExists(cleanIsbn)) {
        qDebug() << "添加图书失败: ISBN已存在:" << cleanIsbn;
        return false;
    }
    qDebug() << "ISBN检查通过";
    
    qDebug() << "步骤3: 准备SQL语句";
    QSqlQuery query(database());
    // 获取当前时间
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    qDebug() << "当前时间:" << currentTime;
    
    // 使用命名占位符，避免位置占位符在某些环境下出现参数计数异常
    const QString sql =
        "INSERT INTO books (isbn, title, author, publisher, publish_date, "
        "category, total_copies, available_copies, create_time, update_time) "
        "VALUES (:isbn, :title, :author, :publisher, :publish_date, "
        ":category, :total_copies, :available_copies, :create_time, :update_time)";
    qDebug() << "SQL语句:" << sql;

    qDebug() << "步骤4: 准备查询";
    if (!query.prepare(sql)) {
        qDebug() << "添加图书失败: prepare失败:" << query.lastError().text();
        qDebug() << "错误代码:" << query.lastError().type();
        qDebug() << "SQL:" << sql;
        
        // 检查表结构
        qDebug() << "步骤4.1: 检查表结构";
        QSqlQuery checkQuery(database());
        if (checkQuery.exec("PRAGMA table_info(books)")) {
            qDebug() << "表结构信息:";
            while (checkQuery.next()) {
                qDebug() << "  列名:" << checkQuery.value(1).toString() 
                         << "类型:" << checkQuery.value(2).toString();
            }
        } else {
            qDebug() << "无法获取表结构信息:" << checkQuery.lastError().text();
        }
        return false;
    }
    qDebug() << "prepare成功";

    qDebug() << "步骤5: 绑定参数值";
    query.bindValue(":isbn", cleanIsbn);
    query.bindValue(":title", title.trimmed());
    query.bindValue(":author", author.trimmed());
    query.bindValue(":publisher", publisher.trimmed());
    query.bindValue(":publish_date", publishDate);
    query.bindValue(":category", category.trimmed());
    query.bindValue(":total_copies", totalCopies);
    query.bindValue(":available_copies", totalCopies); // 初始可借册数等于总册数
    query.bindValue(":create_time", currentTime);
    query.bindValue(":update_time", currentTime);
    qDebug() << "参数绑定完成 - ISBN:" << cleanIsbn 
             << "书名:" << title.trimmed() 
             << "总册数:" << totalCopies
             << "创建时间:" << currentTime;
    
    qDebug() << "步骤6: 执行SQL";
    if (!query.exec()) {
        QString errorMsg = query.lastError().text();
        qDebug() << "添加图书失败: exec失败:" << errorMsg;
        qDebug() << "错误代码:" << query.lastError().type();
        qDebug() << "执行的SQL:" << query.lastQuery();
        return false;
    }
    qDebug() << "SQL执行成功";
    
    qDebug() << "步骤7: 刷新模型";
    select();
    qDebug() << "=== 添加图书完成 ===";
    return true;
}

bool BookModel::updateBook(int id, const QString& isbn, const QString& title, const QString& author,
                           const QString& publisher, const QString& publishDate, const QString& category,
                           int totalCopies)
{
    QSqlQuery query(database());
    // 获取当前时间作为更新时间
    QString updateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    
    query.prepare("UPDATE books SET isbn=?, title=?, author=?, publisher=?, publish_date=?, "
                  "category=?, total_copies=?, update_time=? WHERE id=?");
    query.addBindValue(isbn.trimmed());
    query.addBindValue(title.trimmed());
    query.addBindValue(author.trimmed());
    query.addBindValue(publisher.trimmed());
    query.addBindValue(publishDate);
    query.addBindValue(category.trimmed());
    query.addBindValue(totalCopies);
    query.addBindValue(updateTime);
    query.addBindValue(id);
    
    if (!query.exec()) {
        qDebug() << "更新图书失败:" << query.lastError().text();
        return false;
    }
    
    select();
    return true;
}

bool BookModel::deleteBook(int id)
{
    QSqlQuery query(database());
    query.prepare("DELETE FROM books WHERE id=?");
    query.addBindValue(id);
    
    if (!query.exec()) {
        qDebug() << "删除图书失败:" << query.lastError().text();
        return false;
    }
    
    select();
    return true;
}

void BookModel::filterBooks(const QString& isbn, const QString& title, 
                            const QString& author, const QString& category)
{
    QString filter;
    if (!isbn.isEmpty()) {
        filter += QString("isbn LIKE '%%1%'").arg(isbn);
    }
    if (!title.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("title LIKE '%%1%'").arg(title);
    }
    if (!author.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("author LIKE '%%1%'").arg(author);
    }
    if (!category.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("category LIKE '%%1%'").arg(category);
    }
    
    setFilter(filter);
    select();
}

int BookModel::getAvailableCopies(const QString& isbn)
{
    QSqlQuery query(database());
    query.prepare("SELECT available_copies FROM books WHERE isbn=?");
    query.addBindValue(isbn);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool BookModel::updateCopies(const QString& isbn, int delta)
{
    QSqlQuery query(database());
    query.prepare("UPDATE books SET available_copies = available_copies + ? WHERE isbn=?");
    query.addBindValue(delta);
    query.addBindValue(isbn);
    
    if (!query.exec()) {
        qDebug() << "更新副本数失败:" << query.lastError().text();
        return false;
    }
    
    select();
    return true;
}
