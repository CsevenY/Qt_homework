#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dbmanager.h"

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QDate>
#include <QStyledItemDelegate>
#include <QComboBox>

namespace {
int findBookIdByIsbn(const QString &isbn)
{
    QSqlQuery q;
    q.prepare("SELECT id FROM books WHERE isbn=:isbn");
    q.bindValue(":isbn", isbn);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return -1;
}

int findReaderIdByReaderNo(const QString &readerNo)
{
    QSqlQuery q;
    q.prepare("SELECT id FROM readers WHERE reader_id=:rid");
    q.bindValue(":rid", readerNo);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return -1;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    DBManager &db = DBManager::getInstance();
    if (!db.isConnected()) {
        QMessageBox::critical(this, tr("错误"),
                              tr("数据库连接失败，部分功能不可用。"));
    }

    initBookTab();
    initReaderTab();
    initBorrowTab();
    initStatsTab();
    initConnections();

    checkOverdueOnce();

    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(60 * 1000);
    connect(m_statsTimer, &QTimer::timeout,
            this, &MainWindow::refreshStatsAndOverdue);
    m_statsTimer->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initBookTab()
{
    m_bookModel = new QSqlTableModel(this);
    m_bookModel->setTable("books");
    m_bookModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_bookModel->select();

    m_bookModel->setHeaderData(0, Qt::Horizontal, tr("ID"));
    m_bookModel->setHeaderData(1, Qt::Horizontal, tr("ISBN"));
    m_bookModel->setHeaderData(2, Qt::Horizontal, tr("书名"));
    m_bookModel->setHeaderData(3, Qt::Horizontal, tr("作者"));
    m_bookModel->setHeaderData(4, Qt::Horizontal, tr("出版社"));
    m_bookModel->setHeaderData(5, Qt::Horizontal, tr("出版日期"));
    m_bookModel->setHeaderData(6, Qt::Horizontal, tr("分类"));
    m_bookModel->setHeaderData(7, Qt::Horizontal, tr("库存"));
    m_bookModel->setHeaderData(8, Qt::Horizontal, tr("状态"));

    ui->tvBooks->setModel(m_bookModel);
    ui->tvBooks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tvBooks->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tvBooks->setEditTriggers(QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::SelectedClicked);
    ui->tvBooks->setColumnHidden(0, true);
    ui->tvBooks->horizontalHeader()->setStretchLastSection(true);
    ui->tvBooks->setSortingEnabled(true);

    // 为分类列（第6列，索引6）设置下拉框代理
    // 创建一个自定义代理，使用QComboBox作为编辑器
    class CategoryDelegate : public QStyledItemDelegate
    {
    public:
        CategoryDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
        
        QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override
        {
            if (index.column() == 6) { // 分类列
                QComboBox *combo = new QComboBox(parent);
                combo->addItem("社会科学类");
                combo->addItem("自然科学类");
                combo->addItem("工程技术类");
                combo->addItem("文学艺术类");
                combo->addItem("哲学宗教类");
                combo->addItem("综合类");
                combo->setEditable(false);
                return combo;
            }
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
        
        void setEditorData(QWidget *editor, const QModelIndex &index) const override
        {
            if (index.column() == 6) {
                QComboBox *combo = qobject_cast<QComboBox*>(editor);
                if (combo) {
                    QString value = index.model()->data(index, Qt::EditRole).toString();
                    int idx = combo->findText(value);
                    if (idx >= 0)
                        combo->setCurrentIndex(idx);
                    else
                        combo->setCurrentIndex(0);
                }
            } else {
                QStyledItemDelegate::setEditorData(editor, index);
            }
        }
        
        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const override
        {
            if (index.column() == 6) {
                QComboBox *combo = qobject_cast<QComboBox*>(editor);
                if (combo) {
                    model->setData(index, combo->currentText(), Qt::EditRole);
                }
            } else {
                QStyledItemDelegate::setModelData(editor, model, index);
            }
        }
    };
    
    ui->tvBooks->setItemDelegateForColumn(6, new CategoryDelegate(this));
}

void MainWindow::initReaderTab()
{
    m_readerModel = new QSqlTableModel(this);
    m_readerModel->setTable("readers");
    m_readerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_readerModel->select();

    m_readerModel->setHeaderData(0, Qt::Horizontal, tr("ID"));
    m_readerModel->setHeaderData(1, Qt::Horizontal, tr("证号"));
    m_readerModel->setHeaderData(2, Qt::Horizontal, tr("姓名"));
    m_readerModel->setHeaderData(3, Qt::Horizontal, tr("性别"));
    m_readerModel->setHeaderData(4, Qt::Horizontal, tr("电话"));
    m_readerModel->setHeaderData(5, Qt::Horizontal, tr("邮箱"));
    m_readerModel->setHeaderData(6, Qt::Horizontal, tr("注册日期"));
    m_readerModel->setHeaderData(7, Qt::Horizontal, tr("状态"));

    ui->tvReaders->setModel(m_readerModel);
    ui->tvReaders->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tvReaders->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tvReaders->setColumnHidden(0, true);
    ui->tvReaders->horizontalHeader()->setStretchLastSection(true);
    ui->tvReaders->setSortingEnabled(true);
}

void MainWindow::initBorrowTab()
{
    if (!m_borrowModel)
        m_borrowModel = new QSqlQueryModel(this);

    QString sql =
        "SELECT b.id, bo.title AS book_title, r.name AS reader_name, "
        "b.borrow_date, b.return_date, b.actual_return_date, b.status "
        "FROM borrows b "
        "JOIN books bo ON b.book_id = bo.id "
        "JOIN readers r ON b.reader_id = r.id";

    m_borrowModel->setQuery(sql);
    m_borrowModel->setHeaderData(0, Qt::Horizontal, tr("借阅ID"));
    m_borrowModel->setHeaderData(1, Qt::Horizontal, tr("书名"));
    m_borrowModel->setHeaderData(2, Qt::Horizontal, tr("读者"));
    m_borrowModel->setHeaderData(3, Qt::Horizontal, tr("借出日期"));
    m_borrowModel->setHeaderData(4, Qt::Horizontal, tr("应还日期"));
    m_borrowModel->setHeaderData(5, Qt::Horizontal, tr("实际归还"));
    m_borrowModel->setHeaderData(6, Qt::Horizontal, tr("状态"));

    ui->tvBorrows->setModel(m_borrowModel);
    ui->tvBorrows->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tvBorrows->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tvBorrows->horizontalHeader()->setStretchLastSection(true);

    ui->deBorrowDate->setDate(QDate::currentDate());
    ui->deReturnDate->setDate(QDate::currentDate().addDays(30));
}

void MainWindow::initStatsTab()
{
    m_overdueModel = new QSqlQueryModel(this);
    ui->tvOverdues->setModel(m_overdueModel);
    ui->tvOverdues->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tvOverdues->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tvOverdues->horizontalHeader()->setStretchLastSection(true);

    refreshStatsAndOverdue();
}

void MainWindow::initConnections()
{
    // 图书
    connect(ui->btnBookAdd,    &QPushButton::clicked, this, &MainWindow::onBookAdd);
    connect(ui->btnBookRemove, &QPushButton::clicked, this, &MainWindow::onBookRemove);
    connect(ui->btnBookSubmit, &QPushButton::clicked, this, &MainWindow::onBookSubmit);
    connect(ui->btnBookRevert, &QPushButton::clicked, this, &MainWindow::onBookRevert);
    connect(ui->btnBookSearch, &QPushButton::clicked, this, &MainWindow::onBookFilter);
    connect(ui->btnBookReset,  &QPushButton::clicked, this, &MainWindow::onBookResetFilter);

    // 读者
    connect(ui->btnReaderAdd,    &QPushButton::clicked, this, &MainWindow::onReaderAdd);
    connect(ui->btnReaderRemove, &QPushButton::clicked, this, &MainWindow::onReaderRemove);
    connect(ui->btnReaderSubmit, &QPushButton::clicked, this, &MainWindow::onReaderSubmit);
    connect(ui->btnReaderRevert, &QPushButton::clicked, this, &MainWindow::onReaderRevert);
    connect(ui->btnReaderSearch, &QPushButton::clicked, this, &MainWindow::onReaderFilter);
    connect(ui->btnReaderReset,  &QPushButton::clicked, this, &MainWindow::onReaderResetFilter);

    // 借还
    connect(ui->btnBorrowDo, &QPushButton::clicked, this, &MainWindow::onBorrowBook);
    connect(ui->btnReturnDo, &QPushButton::clicked, this, &MainWindow::onReturnBook);
}

void MainWindow::checkOverdueOnce()
{
    refreshStatsAndOverdue();

    bool ok = false;
    int overdueCount = ui->lblOverdue->text().toInt(&ok);
    if (ok && overdueCount > 0) {
        QMessageBox::warning(this, tr("逾期提醒"),
                             tr("当前存在 %1 条逾期借阅记录，请及时处理。")
                                 .arg(overdueCount));
    }
}

// 图书管理槽函数
void MainWindow::onBookAdd()
{
    int row = m_bookModel->rowCount();
    m_bookModel->insertRow(row);
    const int colStock = m_bookModel->fieldIndex("stock");
    if (colStock >= 0) {
        m_bookModel->setData(m_bookModel->index(row, colStock), 1);
    }
}

void MainWindow::onBookRemove()
{
    QModelIndex idx = ui->tvBooks->currentIndex();
    if (!idx.isValid()) return;
    m_bookModel->removeRow(idx.row());
}

void MainWindow::onBookSubmit()
{
    if (!m_bookModel->submitAll()) {
        QMessageBox::warning(this, tr("保存失败"),
                             m_bookModel->lastError().text());
        m_bookModel->revertAll();
    } else {
        m_bookModel->select();
    }
}

void MainWindow::onBookRevert()
{
    m_bookModel->revertAll();
}

void MainWindow::onBookFilter()
{
    QString title = ui->leBookTitle->text().trimmed();
    QString author = ui->leBookAuthor->text().trimmed();
    QString category = ui->cbBookCategory->currentText().trimmed();

    QStringList conds;
    if (!title.isEmpty())
        conds << QString("title LIKE '%%1%'").arg(title.replace("'", "''"));
    if (!author.isEmpty())
        conds << QString("author LIKE '%%1%'").arg(author.replace("'", "''"));
    // 只有当分类下拉框有选中项且不是"全部"时才添加分类条件
    if (!category.isEmpty() && category != "全部" && ui->cbBookCategory->currentIndex() > 0)
        conds << QString("category='%1'").arg(category.replace("'", "''"));

    m_bookModel->setFilter(conds.join(" AND "));
    m_bookModel->select();
}

void MainWindow::onBookResetFilter()
{
    ui->leBookTitle->clear();
    ui->leBookAuthor->clear();
    ui->cbBookCategory->setCurrentIndex(0); // 设置为"全部"
    m_bookModel->setFilter(QString());
    m_bookModel->select();
}

// 读者管理槽函数
void MainWindow::onReaderAdd()
{
    int row = m_readerModel->rowCount();
    m_readerModel->insertRow(row);
}

void MainWindow::onReaderRemove()
{
    QModelIndex idx = ui->tvReaders->currentIndex();
    if (!idx.isValid()) return;
    m_readerModel->removeRow(idx.row());
}

void MainWindow::onReaderSubmit()
{
    if (!m_readerModel->submitAll()) {
        QMessageBox::warning(this, tr("保存失败"),
                             m_readerModel->lastError().text());
        m_readerModel->revertAll();
    } else {
        m_readerModel->select();
    }
}

void MainWindow::onReaderRevert()
{
    m_readerModel->revertAll();
}

void MainWindow::onReaderFilter()
{
    QString rid = ui->leReaderId->text().trimmed();
    QString name = ui->leReaderName->text().trimmed();
    QString statusText = ui->cbReaderStatus->currentText().trimmed();

    QStringList conds;
    if (!rid.isEmpty())
        conds << QString("reader_id LIKE '%%1%'").arg(rid.replace("'", "''"));
    if (!name.isEmpty())
        conds << QString("name LIKE '%%1%'").arg(name.replace("'", "''"));
    if (!statusText.isEmpty()) {
        bool ok = false;
        int status = statusText.toInt(&ok);
        if (ok) {
            conds << QString("status=%1").arg(status);
        }
    }

    m_readerModel->setFilter(conds.join(" AND "));
    m_readerModel->select();
}

void MainWindow::onReaderResetFilter()
{
    ui->leReaderId->clear();
    ui->leReaderName->clear();
    ui->cbReaderStatus->setCurrentIndex(-1);
    m_readerModel->setFilter(QString());
    m_readerModel->select();
}

// 借还书槽函数
void MainWindow::onBorrowBook()
{
    QString readerNo = ui->leBorrowReaderId->text().trimmed();
    QString isbn = ui->leBorrowIsbn->text().trimmed();
    if (readerNo.isEmpty() || isbn.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请填写读者证号和图书ISBN。"));
        return;
    }

    int readerId = findReaderIdByReaderNo(readerNo);
    int bookId = findBookIdByIsbn(isbn);
    if (readerId < 0 || bookId < 0) {
        QMessageBox::warning(this, tr("提示"), tr("未找到对应读者或图书。"));
        return;
    }

    QString borrowDate = ui->deBorrowDate->date().toString("yyyy-MM-dd");
    QString returnDate = ui->deReturnDate->date().toString("yyyy-MM-dd");

    // 确保图书库存等修改已经提交到数据库，避免界面有值但数据库仍为0的情况
    if (m_bookModel && m_bookModel->isDirty()) {
        if (!m_bookModel->submitAll()) {
            QMessageBox::warning(this, tr("提示"),
                                 tr("保存图书信息失败：%1").arg(m_bookModel->lastError().text()));
            m_bookModel->revertAll();
            return;
        }
        m_bookModel->select();
    }

    DBManager &db = DBManager::getInstance();
    if (db.borrowBook(bookId, readerId, borrowDate, returnDate)) {
        QMessageBox::information(this, tr("成功"), tr("借书成功。"));
        m_bookModel->select();
        initBorrowTab();
        refreshStatsAndOverdue();
    } else {
        QMessageBox::warning(this, tr("失败"),
                             tr("借书失败，请检查：\n"
                                "1. 图书库存是否大于0；\n"
                                "2. 读者与图书是否存在且有效。"));
    }
}

void MainWindow::onReturnBook()
{
    QModelIndex idx = ui->tvBorrows->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一条借阅记录。"));
        return;
    }

    int row = idx.row();
    int borrowId = m_borrowModel->data(m_borrowModel->index(row, 0)).toInt();
    QString actualReturnDate = QDate::currentDate().toString("yyyy-MM-dd");

    DBManager &db = DBManager::getInstance();
    if (db.returnBook(borrowId, actualReturnDate)) {
        QMessageBox::information(this, tr("成功"), tr("还书成功。"));
        m_bookModel->select();
        initBorrowTab();
        refreshStatsAndOverdue();
    } else {
        QMessageBox::warning(this, tr("失败"), tr("还书失败。"));
    }
}

// 统计与逾期
void MainWindow::refreshStatsAndOverdue()
{
    QSqlQuery q;

    if (q.exec("SELECT COUNT(*), IFNULL(SUM(stock),0) FROM books")) {
        if (q.next()) {
            ui->lblTotalBooks->setText(q.value(0).toString());
            ui->lblTotalStock->setText(q.value(1).toString());
        }
    }

    if (q.exec("SELECT COUNT(*) FROM borrows")) {
        if (q.next())
            ui->lblTotalBorrows->setText(q.value(0).toString());
    }

    if (q.exec("SELECT COUNT(*) FROM borrows WHERE status=0")) {
        if (q.next())
            ui->lblOnLoan->setText(q.value(0).toString());
    }

    if (q.exec("SELECT COUNT(*) FROM borrows WHERE status=2")) {
        if (q.next())
            ui->lblOverdue->setText(q.value(0).toString());
    }

    DBManager &db = DBManager::getInstance();
    QSqlQuery overdueQuery = db.getOverdueRecords();
    m_overdueModel->setQuery(overdueQuery);
    m_overdueModel->setHeaderData(0, Qt::Horizontal, tr("借阅ID"));
    m_overdueModel->setHeaderData(1, Qt::Horizontal, tr("书名"));
    m_overdueModel->setHeaderData(2, Qt::Horizontal, tr("读者"));
    m_overdueModel->setHeaderData(3, Qt::Horizontal, tr("借出日期"));
    m_overdueModel->setHeaderData(4, Qt::Horizontal, tr("应还日期"));
}
