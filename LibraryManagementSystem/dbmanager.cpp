#include "dbmanager.h"

// 单例初始化
DBManager& DBManager::getInstance()
{
    static DBManager instance;
    return instance;
}

// 构造函数：初始化数据库连接
DBManager::DBManager()
{
    // 加载SQLite驱动
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    // 设置数据库文件路径（和项目同目录下的library.db）
    m_db.setDatabaseName("E:/Qt_project/Qt_homework/LibraryDB/library.db");

    // 打开数据库
    if (!m_db.open()) {
        qDebug() << "数据库连接失败：" << m_db.lastError().text();
    } else {
        qDebug() << "数据库连接成功！";
        // 可选：如果Navicat没创建表，程序自动创建（防止表不存在）
        QSqlQuery query;
        // 创建图书表
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS books (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                isbn TEXT UNIQUE NOT NULL,
                title TEXT NOT NULL,
                author TEXT NOT NULL,
                publisher TEXT,
                publish_date TEXT,
                category TEXT,
                stock INTEGER DEFAULT 0,
                status INTEGER DEFAULT 0
            )
        )");
        // 创建读者表
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS readers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                reader_id TEXT UNIQUE NOT NULL,
                name TEXT NOT NULL,
                gender TEXT,
                phone TEXT,
                email TEXT,
                register_date TEXT,
                status INTEGER DEFAULT 0
            )
        )");
        // 创建借阅表
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS borrows (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                book_id INTEGER NOT NULL,
                reader_id INTEGER NOT NULL,
                borrow_date TEXT NOT NULL,
                return_date TEXT NOT NULL,
                actual_return_date TEXT,
                status INTEGER DEFAULT 0,
                FOREIGN KEY (book_id) REFERENCES books(id),
                FOREIGN KEY (reader_id) REFERENCES readers(id)
            )
        )");
    }
}

DBManager::~DBManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

// 检查连接状态
bool DBManager::isConnected()
{
    return m_db.isOpen();
}

// ========== 图书表操作实现 ==========
bool DBManager::addBook(const QString& isbn, const QString& title, const QString& author,
                        const QString& publisher, const QString& publishDate, const QString& category,
                        int stock, int status)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO books (isbn, title, author, publisher, publish_date, category, stock, status)
        VALUES (:isbn, :title, :author, :publisher, :publish_date, :category, :stock, :status)
    )");
    // 绑定参数（防止SQL注入）
    query.bindValue(":isbn", isbn);
    query.bindValue(":title", title);
    query.bindValue(":author", author);
    query.bindValue(":publisher", publisher);
    query.bindValue(":publish_date", publishDate);
    query.bindValue(":category", category);
    query.bindValue(":stock", stock);
    query.bindValue(":status", status);

    if (!query.exec()) {
        qDebug() << "添加图书失败：" << query.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::deleteBook(int bookId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM books WHERE id = :id");
    query.bindValue(":id", bookId);
    return query.exec();
}

bool DBManager::updateBook(int bookId, const QString& isbn, const QString& title, const QString& author,
                           const QString& publisher, const QString& publishDate, const QString& category,
                           int stock, int status)
{
    QSqlQuery query;
    query.prepare(R"(
        UPDATE books SET isbn=:isbn, title=:title, author=:author, publisher=:publisher,
        publish_date=:publish_date, category=:category, stock=:stock, status=:status
        WHERE id=:id
    )");
    query.bindValue(":id", bookId);
    query.bindValue(":isbn", isbn);
    query.bindValue(":title", title);
    query.bindValue(":author", author);
    query.bindValue(":publisher", publisher);
    query.bindValue(":publish_date", publishDate);
    query.bindValue(":category", category);
    query.bindValue(":stock", stock);
    query.bindValue(":status", status);
    return query.exec();
}

QSqlQuery DBManager::getAllBooks()
{
    QSqlQuery query;
    query.exec("SELECT * FROM books");
    return query;
}

// ========== 读者表操作实现（和图书表逻辑类似） ==========
bool DBManager::addReader(const QString& readerId, const QString& name, const QString& gender,
                          const QString& phone, const QString& email, const QString& registerDate, int status)
{
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO readers (reader_id, name, gender, phone, email, register_date, status)
        VALUES (:reader_id, :name, :gender, :phone, :email, :register_date, :status)
    )");
    query.bindValue(":reader_id", readerId);
    query.bindValue(":name", name);
    query.bindValue(":gender", gender);
    query.bindValue(":phone", phone);
    query.bindValue(":email", email);
    query.bindValue(":register_date", registerDate);
    query.bindValue(":status", status);
    return query.exec();
}

bool DBManager::deleteReader(int readerId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM readers WHERE id = :id");
    query.bindValue(":id", readerId);
    return query.exec();
}

bool DBManager::updateReader(int readerId, const QString& readerNo, const QString& name, const QString& gender,
                             const QString& phone, const QString& email, const QString& registerDate, int status)
{
    QSqlQuery query;
    query.prepare(R"(
        UPDATE readers SET reader_id=:reader_id, name=:name, gender=:gender, phone=:phone,
        email=:email, register_date=:register_date, status=:status WHERE id=:id
    )");
    query.bindValue(":id", readerId);
    query.bindValue(":reader_id", readerNo);
    query.bindValue(":name", name);
    query.bindValue(":gender", gender);
    query.bindValue(":phone", phone);
    query.bindValue(":email", email);
    query.bindValue(":register_date", registerDate);
    query.bindValue(":status", status);
    return query.exec();
}

QSqlQuery DBManager::getAllReaders()
{
    QSqlQuery query;
    query.exec("SELECT * FROM readers");
    return query;
}

// ========== 借阅表操作实现 ==========
bool DBManager::borrowBook(int bookId, int readerId, const QString& borrowDate, const QString& returnDate)
{
    // 1. 检查图书库存
    QSqlQuery stockQuery;
    stockQuery.prepare("SELECT stock FROM books WHERE id = :id");
    stockQuery.bindValue(":id", bookId);
    if (stockQuery.exec() && stockQuery.next() && stockQuery.value(0).toInt() > 0) {
        // 2. 插入借阅记录
        QSqlQuery borrowQuery;
        borrowQuery.prepare(R"(
            INSERT INTO borrows (book_id, reader_id, borrow_date, return_date, status)
            VALUES (:book_id, :reader_id, :borrow_date, :return_date, 0)
        )");
        borrowQuery.bindValue(":book_id", bookId);
        borrowQuery.bindValue(":reader_id", readerId);
        borrowQuery.bindValue(":borrow_date", borrowDate);
        borrowQuery.bindValue(":return_date", returnDate);
        if (borrowQuery.exec()) {
            // 3. 减少图书库存
            QSqlQuery updateStockQuery;
            updateStockQuery.prepare("UPDATE books SET stock = stock - 1 WHERE id = :id");
            updateStockQuery.bindValue(":id", bookId);
            return updateStockQuery.exec();
        }
    }
    return false;
}

bool DBManager::returnBook(int borrowId, const QString& actualReturnDate)
{
    // 1. 更新借阅记录
    QSqlQuery borrowQuery;
    borrowQuery.prepare(R"(
        UPDATE borrows SET actual_return_date=:actual_return_date, status=1
        WHERE id=:id
    )");
    borrowQuery.bindValue(":id", borrowId);
    borrowQuery.bindValue(":actual_return_date", actualReturnDate);
    if (borrowQuery.exec()) {
        // 2. 获取图书ID，恢复库存
        QSqlQuery bookIdQuery;
        bookIdQuery.prepare("SELECT book_id FROM borrows WHERE id = :id");
        bookIdQuery.bindValue(":id", borrowId);
        if (bookIdQuery.exec() && bookIdQuery.next()) {
            int bookId = bookIdQuery.value(0).toInt();
            QSqlQuery updateStockQuery;
            updateStockQuery.prepare("UPDATE books SET stock = stock + 1 WHERE id = :id");
            updateStockQuery.bindValue(":id", bookId);
            return updateStockQuery.exec();
        }
    }
    return false;
}

// 查询逾期记录（应还日期 < 当前日期 且 未归还）
QSqlQuery DBManager::getOverdueRecords()
{
    QSqlQuery query;
    QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
    query.prepare(R"(
        SELECT b.id, bo.title, r.name, b.borrow_date, b.return_date
        FROM borrows b
        JOIN books bo ON b.book_id = bo.id
        JOIN readers r ON b.reader_id = r.id
        WHERE b.status = 0 AND b.return_date < :current_date
    )");
    query.bindValue(":current_date", currentDate);
    query.exec();
    // 更新逾期状态为2
    QSqlQuery updateStatusQuery;
    updateStatusQuery.prepare("UPDATE borrows SET status=2 WHERE status=0 AND return_date < :current_date");
    updateStatusQuery.bindValue(":current_date", currentDate);
    updateStatusQuery.exec();
    return query;
}

// 多条件筛选图书（示例：按书名+作者）
QSqlQuery DBManager::filterBooks(const QString& title, const QString& author)
{
    QSqlQuery query;
    query.prepare(R"(
        SELECT * FROM books
        WHERE title LIKE :title AND author LIKE :author
    )");
    // 模糊查询（%匹配任意字符）
    query.bindValue(":title", "%" + title + "%");
    query.bindValue(":author", "%" + author + "%");
    query.exec();
    return query;
}
