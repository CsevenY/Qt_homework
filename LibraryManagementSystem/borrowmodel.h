#ifndef BORROWMODEL_H
#define BORROWMODEL_H

#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QDate>

class BorrowModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit BorrowModel(QObject *parent = nullptr, QSqlDatabase db = QSqlDatabase());
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    
    // 借书
    bool borrowBook(const QString& readerId, const QString& bookIsbn, int days = 30);
    
    // 还书
    bool returnBook(int recordId);
    
    // 多条件筛选
    void filterRecords(const QString& readerId = "", const QString& bookIsbn = "", 
                      const QString& status = "");
    
    // 获取逾期记录
    QList<int> getOverdueRecords();
    
    // 计算逾期罚款
    double calculateFine(int recordId, double dailyFine = 0.5);
    
    // 获取统计信息
    struct Statistics {
        int totalBorrows;
        int currentBorrows;
        int overdueCount;
        int totalReturns;
    };
    Statistics getStatistics();
};

#endif // BORROWMODEL_H
