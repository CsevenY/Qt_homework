#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTableView>
#include <QGroupBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QTimer>
#include <QStatusBar>

#include "databasemanager.h"
#include "bookmodel.h"
#include "readermodel.h"
#include "borrowmodel.h"

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

private slots:
    // 图书管理
    void onDeleteBook();
    void onSearchBooks();
    void onBookSelectionChanged();
    
    // 读者管理
    void onDeleteReader();
    void onSearchReaders();
    void onReaderSelectionChanged();
    
    // 借还书管理
    void onReturnBook();
    void onSearchBorrowRecords();
    void onBorrowSelectionChanged();
    
    // 统计信息
    void refreshStatistics();
    
    // 逾期提醒
    void checkOverdueBooks();

private:
    Ui::MainWindow *ui;
    
    // 数据库和模型
    BookModel *bookModel;
    ReaderModel *readerModel;
    BorrowModel *borrowModel;
    
    // 定时器（用于逾期提醒）
    QTimer *overdueTimer;
    
    // 当前选中的ID
    int currentBookId;
    int currentReaderId;
    int currentBorrowId;
    
    void setupUI();
    void setupBookTab();
    void setupReaderTab();
    void setupBorrowTab();
    void setupStatisticsTab();
    void showBookDialog(bool isEdit = false);
    void showReaderDialog(bool isEdit = false);
    void showBorrowDialog();
    void loadBookCategories();
    QStringList getDefaultCategories() const;
};

#endif // MAINWINDOW_H
