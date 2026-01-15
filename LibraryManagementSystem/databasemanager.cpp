#include "databasemanager.h"

DatabaseManager& DatabaseManager::getInstance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initializeDatabase(const QString& dbPath)
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        qDebug() << "无法打开数据库:" << db.lastError().text();
        return false;
    }
    
    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(db);
    
    // 创建图书表
    QString createBooksTable = R"(
        CREATE TABLE IF NOT EXISTS books (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            isbn TEXT UNIQUE NOT NULL,
            title TEXT NOT NULL,
            author TEXT,
            publisher TEXT,
            publish_date TEXT,
            category TEXT,
            total_copies INTEGER DEFAULT 1,
            available_copies INTEGER DEFAULT 1,
            price REAL,
            description TEXT,
            create_time TEXT,
            update_time TEXT
        )
    )";
    
    if (!query.exec(createBooksTable)) {
        qDebug() << "创建图书表失败:" << query.lastError().text();
        return false;
    }
    
    // 检查并修复表结构
    if (!checkAndFixTableStructure()) {
        qDebug() << "检查表结构失败";
        return false;
    }
    
    // 创建读者表
    QString createReadersTable = R"(
        CREATE TABLE IF NOT EXISTS readers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            reader_id TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            gender TEXT,
            phone TEXT,
            email TEXT,
            address TEXT,
            register_date TEXT DEFAULT CURRENT_TIMESTAMP,
            status TEXT DEFAULT '正常'
        )
    )";
    
    if (!query.exec(createReadersTable)) {
        qDebug() << "创建读者表失败:" << query.lastError().text();
        return false;
    }
    
    // 创建借阅记录表
    QString createBorrowTable = R"(
        CREATE TABLE IF NOT EXISTS borrow_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            reader_id TEXT NOT NULL,
            book_isbn TEXT NOT NULL,
            borrow_date TEXT NOT NULL,
            due_date TEXT NOT NULL,
            return_date TEXT,
            status TEXT DEFAULT '借出',
            fine_amount REAL DEFAULT 0,
            FOREIGN KEY (reader_id) REFERENCES readers(reader_id),
            FOREIGN KEY (book_isbn) REFERENCES books(isbn)
        )
    )";
    
    if (!query.exec(createBorrowTable)) {
        qDebug() << "创建借阅记录表失败:" << query.lastError().text();
        return false;
    }
    
    return true;
}

QSqlDatabase DatabaseManager::getDatabase() const
{
    return db;
}

bool DatabaseManager::executeQuery(const QString& queryStr)
{
    QSqlQuery query(db);
    return query.exec(queryStr);
}

bool DatabaseManager::checkAndFixTableStructure()
{
    QSqlQuery query(db);
    
    // 检查books表结构
    if (query.exec("PRAGMA table_info(books)")) {
        QStringList columns;
        while (query.next()) {
            columns << query.value(1).toString().toLower();
        }
        
        qDebug() << "books表现有列:" << columns;
        
        // 检查必需的列是否存在
        if (!columns.contains("total_copies")) {
            qDebug() << "添加缺失的列: total_copies";
            if (!query.exec("ALTER TABLE books ADD COLUMN total_copies INTEGER DEFAULT 1")) {
                qDebug() << "添加total_copies列失败:" << query.lastError().text();
            }
        }
        
        if (!columns.contains("available_copies")) {
            qDebug() << "添加缺失的列: available_copies";
            if (!query.exec("ALTER TABLE books ADD COLUMN available_copies INTEGER DEFAULT 1")) {
                qDebug() << "添加available_copies列失败:" << query.lastError().text();
            } else {
                // 如果新添加了available_copies列，将现有记录的available_copies设置为total_copies
                query.exec("UPDATE books SET available_copies = total_copies WHERE available_copies IS NULL");
            }
        }
        
        if (!columns.contains("create_time")) {
            qDebug() << "添加缺失的列: create_time";
            if (!query.exec("ALTER TABLE books ADD COLUMN create_time TEXT")) {
                qDebug() << "添加create_time列失败:" << query.lastError().text();
            }
        }
        
        if (!columns.contains("update_time")) {
            qDebug() << "添加缺失的列: update_time";
            if (!query.exec("ALTER TABLE books ADD COLUMN update_time TEXT")) {
                qDebug() << "添加update_time列失败:" << query.lastError().text();
            }
        }
    }
    
    // 检查readers表结构
    if (query.exec("PRAGMA table_info(readers)")) {
        QStringList columns;
        while (query.next()) {
            columns << query.value(1).toString().toLower();
        }
        
        qDebug() << "readers表现有列:" << columns;
        
        if (!columns.contains("address")) {
            qDebug() << "添加缺失的列: address";
            if (!query.exec("ALTER TABLE readers ADD COLUMN address TEXT")) {
                qDebug() << "添加address列失败:" << query.lastError().text();
            }
        }
        
        if (!columns.contains("update_time")) {
            qDebug() << "添加缺失的列: update_time";
            if (!query.exec("ALTER TABLE readers ADD COLUMN update_time TEXT")) {
                qDebug() << "添加update_time列失败:" << query.lastError().text();
            }
        }
        
        if (!columns.contains("create_time")) {
            qDebug() << "添加缺失的列: create_time";
            if (!query.exec("ALTER TABLE readers ADD COLUMN create_time TEXT")) {
                qDebug() << "添加create_time列失败:" << query.lastError().text();
            }
        }
    }
    
    return true;
}
