#include "borrowmodel.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QColor>

BorrowModel::BorrowModel(QObject *parent, QSqlDatabase db)
    : QSqlTableModel(parent, db)
{
    setTable("borrow_records");
    setEditStrategy(QSqlTableModel::OnManualSubmit);
    setHeaderData(0, Qt::Horizontal, "ID");
    setHeaderData(1, Qt::Horizontal, "读者编号");
    setHeaderData(2, Qt::Horizontal, "图书ISBN");
    setHeaderData(3, Qt::Horizontal, "借阅日期");
    setHeaderData(4, Qt::Horizontal, "应还日期");
    setHeaderData(5, Qt::Horizontal, "归还日期");
    setHeaderData(6, Qt::Horizontal, "状态");
    setHeaderData(7, Qt::Horizontal, "罚款金额");
    select();
}

QVariant BorrowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    // 逾期记录高亮显示
    if (role == Qt::BackgroundRole && index.column() == 6) {
        QString status = QSqlTableModel::data(index.sibling(index.row(), 6)).toString();
        if (status == "借出") {
            QString dueDateStr = QSqlTableModel::data(index.sibling(index.row(), 4)).toString();
            QDate dueDate = QDate::fromString(dueDateStr, "yyyy-MM-dd");
            if (dueDate.isValid() && dueDate < QDate::currentDate()) {
                return QColor(Qt::red).lighter(180);
            }
        }
    }
    
    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    
    return QSqlTableModel::data(index, role);
}

bool BorrowModel::borrowBook(const QString& readerId, const QString& bookIsbn, int days)
{
    QSqlQuery query(database());
    
    // 检查图书是否可借
    query.prepare("SELECT available_copies FROM books WHERE isbn=?");
    query.addBindValue(bookIsbn);
    if (!query.exec() || !query.next()) {
        qDebug() << "图书不存在或查询失败";
        return false;
    }
    
    int availableCopies = query.value(0).toInt();
    if (availableCopies <= 0) {
        qDebug() << "图书已全部借出";
        return false;
    }
    
    // 检查读者是否存在
    query.prepare("SELECT COUNT(*) FROM readers WHERE reader_id=?");
    query.addBindValue(readerId);
    if (!query.exec() || !query.next() || query.value(0).toInt() == 0) {
        qDebug() << "读者不存在";
        return false;
    }
    
    // 插入借阅记录
    QDate borrowDate = QDate::currentDate();
    QDate dueDate = borrowDate.addDays(days);
    
    query.prepare("INSERT INTO borrow_records (reader_id, book_isbn, borrow_date, due_date, status) "
                  "VALUES (?, ?, ?, ?, '借出')");
    query.addBindValue(readerId);
    query.addBindValue(bookIsbn);
    query.addBindValue(borrowDate.toString("yyyy-MM-dd"));
    query.addBindValue(dueDate.toString("yyyy-MM-dd"));
    
    if (!query.exec()) {
        qDebug() << "借书失败:" << query.lastError().text();
        return false;
    }
    
    // 更新图书可借册数
    query.prepare("UPDATE books SET available_copies = available_copies - 1 WHERE isbn=?");
    query.addBindValue(bookIsbn);
    if (!query.exec()) {
        qDebug() << "更新图书副本数失败";
        return false;
    }
    
    select();
    return true;
}

bool BorrowModel::returnBook(int recordId)
{
    QSqlQuery query(database());
    
    // 获取借阅记录信息
    query.prepare("SELECT book_isbn, status FROM borrow_records WHERE id=?");
    query.addBindValue(recordId);
    if (!query.exec() || !query.next()) {
        qDebug() << "借阅记录不存在";
        return false;
    }
    
    QString bookIsbn = query.value(0).toString();
    QString status = query.value(1).toString();
    
    if (status == "已归还") {
        qDebug() << "该书已归还";
        return false;
    }
    
    // 计算罚款
    double fine = calculateFine(recordId);
    
    // 更新借阅记录
    QDate returnDate = QDate::currentDate();
    query.prepare("UPDATE borrow_records SET return_date=?, status='已归还', fine_amount=? WHERE id=?");
    query.addBindValue(returnDate.toString("yyyy-MM-dd"));
    query.addBindValue(fine);
    query.addBindValue(recordId);
    
    if (!query.exec()) {
        qDebug() << "还书失败:" << query.lastError().text();
        return false;
    }
    
    // 更新图书可借册数
    query.prepare("UPDATE books SET available_copies = available_copies + 1 WHERE isbn=?");
    query.addBindValue(bookIsbn);
    if (!query.exec()) {
        qDebug() << "更新图书副本数失败";
        return false;
    }
    
    select();
    return true;
}

void BorrowModel::filterRecords(const QString& readerId, const QString& bookIsbn, 
                                const QString& status)
{
    QString filter;
    if (!readerId.isEmpty()) {
        filter += QString("reader_id LIKE '%%1%'").arg(readerId);
    }
    if (!bookIsbn.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("book_isbn LIKE '%%1%'").arg(bookIsbn);
    }
    if (!status.isEmpty()) {
        if (!filter.isEmpty()) filter += " AND ";
        filter += QString("status = '%1'").arg(status);
    }
    
    setFilter(filter);
    select();
}

QList<int> BorrowModel::getOverdueRecords()
{
    QList<int> overdueIds;
    QSqlQuery query(database());
    QDate today = QDate::currentDate();
    
    query.prepare("SELECT id FROM borrow_records WHERE status='借出' AND due_date < ?");
    query.addBindValue(today.toString("yyyy-MM-dd"));
    
    if (query.exec()) {
        while (query.next()) {
            overdueIds.append(query.value(0).toInt());
        }
    }
    
    return overdueIds;
}

double BorrowModel::calculateFine(int recordId, double dailyFine)
{
    QSqlQuery query(database());
    query.prepare("SELECT due_date FROM borrow_records WHERE id=?");
    query.addBindValue(recordId);
    
    if (!query.exec() || !query.next()) {
        return 0.0;
    }
    
    QDate dueDate = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
    QDate returnDate = QDate::currentDate();
    
    if (returnDate <= dueDate) {
        return 0.0;
    }
    
    int daysOverdue = dueDate.daysTo(returnDate);
    return daysOverdue * dailyFine;
}

BorrowModel::Statistics BorrowModel::getStatistics()
{
    Statistics stats = {0, 0, 0, 0};
    QSqlQuery query(database());
    
    // 总借阅数
    query.exec("SELECT COUNT(*) FROM borrow_records");
    if (query.next()) {
        stats.totalBorrows = query.value(0).toInt();
    }
    
    // 当前借出数
    query.exec("SELECT COUNT(*) FROM borrow_records WHERE status='借出'");
    if (query.next()) {
        stats.currentBorrows = query.value(0).toInt();
    }
    
    // 逾期数
    QDate today = QDate::currentDate();
    query.prepare("SELECT COUNT(*) FROM borrow_records WHERE status='借出' AND due_date < ?");
    query.addBindValue(today.toString("yyyy-MM-dd"));
    if (query.exec() && query.next()) {
        stats.overdueCount = query.value(0).toInt();
    }
    
    // 已归还数
    query.exec("SELECT COUNT(*) FROM borrow_records WHERE status='已归还'");
    if (query.next()) {
        stats.totalReturns = query.value(0).toInt();
    }
    
    return stats;
}
