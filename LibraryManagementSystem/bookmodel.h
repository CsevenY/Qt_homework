#ifndef BOOKMODEL_H
#define BOOKMODEL_H

#include <QSqlTableModel>
#include <QSqlDatabase>

class BookModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit BookModel(QObject *parent = nullptr, QSqlDatabase db = QSqlDatabase());
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    
    // 添加图书
    bool addBook(const QString& isbn, const QString& title, const QString& author,
                 const QString& publisher, const QString& publishDate, const QString& category,
                 int totalCopies);
    
    // 检查ISBN是否存在
    bool isbnExists(const QString& isbn);
    
    // 更新图书
    bool updateBook(int id, const QString& isbn, const QString& title, const QString& author,
                    const QString& publisher, const QString& publishDate, const QString& category,
                    int totalCopies);
    
    // 删除图书
    bool deleteBook(int id);
    
    // 多条件筛选
    void filterBooks(const QString& isbn = "", const QString& title = "", 
                     const QString& author = "", const QString& category = "");
    
    // 获取可用副本数
    int getAvailableCopies(const QString& isbn);
    
    // 更新副本数
    bool updateCopies(const QString& isbn, int delta);
};

#endif // BOOKMODEL_H
