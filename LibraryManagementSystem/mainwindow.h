#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlTableModel>
#include <QSqlQueryModel>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QComboBox>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // 初始化各个功能页
    void initBookTab();
    void initReaderTab();
    void initBorrowTab();
    void initStatsTab();
    void initConnections();
    void checkOverdueOnce();

private slots:
    // 图书管理
    void onBookAdd();
    void onBookRemove();
    void onBookSubmit();
    void onBookRevert();
    void onBookFilter();
    void onBookResetFilter();

    // 读者管理
    void onReaderAdd();
    void onReaderRemove();
    void onReaderSubmit();
    void onReaderRevert();
    void onReaderFilter();
    void onReaderResetFilter();

    // 借还书
    void onBorrowBook();
    void onReturnBook();

    // 统计与逾期
    void refreshStatsAndOverdue();

private:
    Ui::MainWindow *ui;

    // 数据模型
    QSqlTableModel  *m_bookModel   = nullptr;
    QSqlTableModel  *m_readerModel = nullptr;
    QSqlQueryModel  *m_borrowModel = nullptr;
    QSqlQueryModel  *m_overdueModel = nullptr;

    QTimer          *m_statsTimer = nullptr;
};
#endif // MAINWINDOW_H
