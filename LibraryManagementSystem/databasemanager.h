#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QDebug>

class DatabaseManager
{
public:
    static DatabaseManager& getInstance();
    bool initializeDatabase(const QString& dbPath);
    QSqlDatabase getDatabase() const;
    bool executeQuery(const QString& query);
    
private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    QSqlDatabase db;
    bool createTables();
    bool checkAndFixTableStructure();
};

#endif // DATABASEMANAGER_H
