#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>

// 单例模式封装数据库操作，确保全局只有一个数据库连接
class DBManager
{
public:
    // 获取单例对象
    static DBManager& getInstance();
    // 关闭数据库连接（禁止拷贝）
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    // 检查数据库连接是否正常
    bool isConnected();

    // ========== 图书表操作 ==========
    // 添加图书
    bool addBook(const QString& isbn, const QString& title, const QString& author,
                 const QString& publisher, const QString& publishDate, const QString& category,
                 int stock, int status = 0);
    // 删除图书（按ID）
    bool deleteBook(int bookId);
    // 修改图书信息
    bool updateBook(int bookId, const QString& isbn, const QString& title, const QString& author,
                    const QString& publisher, const QString& publishDate, const QString& category,
                    int stock, int status);
    // 查询所有图书
    QSqlQuery getAllBooks();

    // ========== 读者表操作 ==========
    bool addReader(const QString& readerId, const QString& name, const QString& gender,
                   const QString& phone, const QString& email, const QString& registerDate,
                   int status = 0);
    bool deleteReader(int readerId);
    bool updateReader(int readerId, const QString& readerNo, const QString& name, const QString& gender,
                      const QString& phone, const QString& email, const QString& registerDate, int status);
    QSqlQuery getAllReaders();

    // ========== 借阅表操作 ==========
    // 借书（生成借阅记录）
    bool borrowBook(int bookId, int readerId, const QString& borrowDate, const QString& returnDate);
    // 还书（更新实际归还日期和状态）
    bool returnBook(int borrowId, const QString& actualReturnDate);
    // 查询逾期记录
    QSqlQuery getOverdueRecords();
    // 多条件筛选图书（示例：按书名+作者）
    QSqlQuery filterBooks(const QString& title, const QString& author);

private:
    // 私有构造函数（单例模式）
    DBManager();
    ~DBManager();

    QSqlDatabase m_db; // 数据库连接对象
};

#endif // DBMANAGER_H
