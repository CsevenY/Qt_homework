#ifndef READERMODEL_H
#define READERMODEL_H

#include <QSqlTableModel>
#include <QSqlDatabase>

class ReaderModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit ReaderModel(QObject *parent = nullptr, QSqlDatabase db = QSqlDatabase());
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    
    // 添加读者
    bool addReader(const QString& readerId, const QString& name, const QString& gender,
                   const QString& phone, const QString& email, const QString& address);
    
    // 更新读者
    bool updateReader(int id, const QString& readerId, const QString& name, const QString& gender,
                      const QString& phone, const QString& email, const QString& address);
    
    // 删除读者
    bool deleteReader(int id);
    
    // 多条件筛选
    void filterReaders(const QString& readerId = "", const QString& name = "", 
                       const QString& phone = "", const QString& status = "");
    
    // 检查读者是否存在
    bool readerExists(const QString& readerId);
};

#endif // READERMODEL_H
