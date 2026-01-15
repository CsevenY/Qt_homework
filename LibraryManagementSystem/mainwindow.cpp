#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHeaderView>
#include <QDate>
#include <QSqlQuery>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSet>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , bookModel(nullptr)
    , readerModel(nullptr)
    , borrowModel(nullptr)
    , overdueTimer(new QTimer(this))
    , currentBookId(-1)
    , currentReaderId(-1)
    , currentBorrowId(-1)
{
    ui->setupUi(this);
    
    // 初始化数据库
    QString dbPath = "E:\\Qt_project\\Qt_homework\\LibraryDB\\library.db";
    if (!DatabaseManager::getInstance().initializeDatabase(dbPath)) {
        QMessageBox::critical(this, "错误", "数据库初始化失败！");
        return;
    }
    
    // 创建模型
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    bookModel = new BookModel(this, db);
    readerModel = new ReaderModel(this, db);
    borrowModel = new BorrowModel(this, db);
    
    // 设置UI
    setupUI();
    
    // 设置定时器检查逾期（每小时检查一次）
    connect(overdueTimer, &QTimer::timeout, this, &MainWindow::checkOverdueBooks);
    overdueTimer->start(3600000); // 1小时 = 3600000毫秒
    checkOverdueBooks(); // 启动时立即检查一次
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // UI已经在.ui文件中定义，这里只需要设置模型和连接信号
    setupBookTab();
    setupReaderTab();
    setupBorrowTab();
    setupStatisticsTab();
    
    // 状态栏
    ui->statusbar->showMessage("就绪");
}

void MainWindow::setupBookTab()
{
    // 连接信号
    connect(ui->bookSearchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchBooks);
    connect(ui->bookCategoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onSearchBooks);
    connect(ui->bookAddBtn, &QPushButton::clicked, this, [this]() { showBookDialog(false); });
    connect(ui->bookEditBtn, &QPushButton::clicked, this, [this]() { showBookDialog(true); });
    connect(ui->bookDeleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteBook);
    
    // 设置表格模型
    ui->bookTableView->setModel(bookModel);
    ui->bookTableView->horizontalHeader()->setStretchLastSection(true);
    ui->bookTableView->setColumnWidth(0, 50);
    ui->bookTableView->setColumnWidth(1, 120);
    ui->bookTableView->setColumnWidth(2, 200);
    // 隐藏不需要显示的列：total_copies(7)、available_copies(8)，以及可能的stock和status字段
    ui->bookTableView->setColumnHidden(7, true);  // total_copies (stock)
    ui->bookTableView->setColumnHidden(8, true);  // available_copies
    // 检查并隐藏其他可能的字段
    for (int i = 0; i < bookModel->columnCount(); i++) {
        QString header = bookModel->headerData(i, Qt::Horizontal).toString().toLower();
        if (header.contains("stock") || header.contains("status")) {
            ui->bookTableView->setColumnHidden(i, true);
        }
    }
    connect(ui->bookTableView, &QTableView::doubleClicked, this, [this]() { showBookDialog(true); });
    connect(ui->bookTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onBookSelectionChanged);
    
    // 初始化分类下拉框
    ui->bookCategoryCombo->addItem("全部分类", "");
    
    // 加载分类列表
    loadBookCategories();
}

void MainWindow::setupReaderTab()
{
    // 连接信号
    connect(ui->readerSearchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchReaders);
    connect(ui->readerAddBtn, &QPushButton::clicked, this, [this]() { showReaderDialog(false); });
    connect(ui->readerEditBtn, &QPushButton::clicked, this, [this]() { showReaderDialog(true); });
    connect(ui->readerDeleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteReader);
    
    // 设置表格模型
    ui->readerTableView->setModel(readerModel);
    ui->readerTableView->horizontalHeader()->setStretchLastSection(true);
    // 隐藏不需要显示的列：地址(6)、状态(8)、update_time、create_time
    ui->readerTableView->setColumnHidden(6, true);  // 地址
    ui->readerTableView->setColumnHidden(7, true);  //注册时间
    ui->readerTableView->setColumnHidden(8, true);  // 状态
    // 检查并隐藏update_time和create_time列
    for (int i = 0; i < readerModel->columnCount(); i++) {
        QString header = readerModel->headerData(i, Qt::Horizontal).toString().toLower();
        if (header.contains("update_time") || header.contains("create_time")) {
            ui->readerTableView->setColumnHidden(i, true);
        }
    }
    connect(ui->readerTableView, &QTableView::doubleClicked, this, [this]() { showReaderDialog(true); });
    connect(ui->readerTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onReaderSelectionChanged);
}

void MainWindow::setupBorrowTab()
{
    // 连接信号
    connect(ui->borrowSearchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchBorrowRecords);
    connect(ui->borrowBookBtn, &QPushButton::clicked, this, &MainWindow::showBorrowDialog);
    connect(ui->returnBookBtn, &QPushButton::clicked, this, &MainWindow::onReturnBook);
    
    // 设置表格模型
    ui->borrowTableView->setModel(borrowModel);
    ui->borrowTableView->horizontalHeader()->setStretchLastSection(true);
    connect(ui->borrowTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onBorrowSelectionChanged);
}

void MainWindow::setupStatisticsTab()
{
    // UI已经在.ui文件中定义，这里只需要刷新统计数据
    refreshStatistics();
}

// 显示图书对话框
void MainWindow::showBookDialog(bool isEdit)
{
    if (isEdit) {
        QModelIndexList indexes = ui->bookTableView->selectionModel()->selectedRows();
        if (indexes.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择要编辑的图书！");
            return;
        }
        currentBookId = bookModel->data(bookModel->index(indexes.first().row(), 0)).toInt();
    } else {
        currentBookId = -1;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "编辑图书" : "添加图书");
    dialog.setMinimumWidth(400);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QLineEdit *isbnEdit = new QLineEdit();
    QLineEdit *titleEdit = new QLineEdit();
    QLineEdit *authorEdit = new QLineEdit();
    QLineEdit *publisherEdit = new QLineEdit();
    QDateEdit *publishDateEdit = new QDateEdit();
    publishDateEdit->setCalendarPopup(true);
    publishDateEdit->setDate(QDate::currentDate());
    QComboBox *categoryCombo = new QComboBox();
    categoryCombo->setEditable(true); // 允许手动输入其他分类
    categoryCombo->addItems(getDefaultCategories());
    QSpinBox *totalCopiesEdit = new QSpinBox();
    totalCopiesEdit->setMinimum(1);
    totalCopiesEdit->setMaximum(9999);
    totalCopiesEdit->setValue(1);
    
    formLayout->addRow("ISBN*:", isbnEdit);
    formLayout->addRow("书名*:", titleEdit);
    formLayout->addRow("作者:", authorEdit);
    formLayout->addRow("出版社:", publisherEdit);
    formLayout->addRow("出版日期:", publishDateEdit);
    formLayout->addRow("分类:", categoryCombo);
    formLayout->addRow("总册数:", totalCopiesEdit);
    
    // 如果是编辑模式，填充现有数据
    if (isEdit && currentBookId >= 0) {
        QModelIndexList indexes = ui->bookTableView->selectionModel()->selectedRows();
        QModelIndex index = indexes.first();
        isbnEdit->setText(bookModel->data(bookModel->index(index.row(), 1)).toString());
        titleEdit->setText(bookModel->data(bookModel->index(index.row(), 2)).toString());
        authorEdit->setText(bookModel->data(bookModel->index(index.row(), 3)).toString());
        publisherEdit->setText(bookModel->data(bookModel->index(index.row(), 4)).toString());
        QString dateStr = bookModel->data(bookModel->index(index.row(), 5)).toString();
        if (!dateStr.isEmpty()) {
            publishDateEdit->setDate(QDate::fromString(dateStr, "yyyy-MM-dd"));
        }
        QString category = bookModel->data(bookModel->index(index.row(), 6)).toString();
        int categoryIndex = categoryCombo->findText(category);
        if (categoryIndex >= 0) {
            categoryCombo->setCurrentIndex(categoryIndex);
        } else {
            categoryCombo->setCurrentText(category); // 如果不在列表中，设置为当前文本
        }
        totalCopiesEdit->setValue(bookModel->data(bookModel->index(index.row(), 7)).toInt());
    }
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (isbnEdit->text().isEmpty() || titleEdit->text().isEmpty()) {
            QMessageBox::warning(this, "警告", "ISBN和书名不能为空！");
            return;
        }
        
        bool success;
        QString category = categoryCombo->currentText().trimmed();
        if (isEdit) {
            success = bookModel->updateBook(
                currentBookId,
                isbnEdit->text(),
                titleEdit->text(),
                authorEdit->text(),
                publisherEdit->text(),
                publishDateEdit->date().toString("yyyy-MM-dd"),
                category,
                totalCopiesEdit->value()
            );
        } else {
            success = bookModel->addBook(
                isbnEdit->text(),
                titleEdit->text(),
                authorEdit->text(),
                publisherEdit->text(),
                publishDateEdit->date().toString("yyyy-MM-dd"),
                category,
                totalCopiesEdit->value()
            );
        }
        
        if (success) {
            QMessageBox::information(this, "成功", isEdit ? "图书更新成功！" : "图书添加成功！");
            ui->statusbar->showMessage(isEdit ? "图书更新成功" : "图书添加成功", 3000);
            // 刷新分类列表
            loadBookCategories();
        } else {
            // 检查是否是ISBN已存在的问题
            QString errorMsg;
            if (isEdit) {
                errorMsg = "图书更新失败！请检查输入信息是否正确。";
            } else {
                if (bookModel->isbnExists(isbnEdit->text().trimmed())) {
                    errorMsg = QString("图书添加失败！ISBN \"%1\" 已存在，请使用不同的ISBN。").arg(isbnEdit->text().trimmed());
                } else if (isbnEdit->text().trimmed().isEmpty()) {
                    errorMsg = "图书添加失败！ISBN不能为空。";
                } else {
                    errorMsg = "图书添加失败！请检查输入信息是否正确。";
                }
            }
            QMessageBox::warning(this, "失败", errorMsg);
        }
    }
}

void MainWindow::onDeleteBook()
{
    QModelIndexList indexes = ui->bookTableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择要删除的图书！");
        return;
    }
    
    currentBookId = bookModel->data(bookModel->index(indexes.first().row(), 0)).toInt();
    
    int ret = QMessageBox::question(this, "确认", "确定要删除这本图书吗？",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        if (bookModel->deleteBook(currentBookId)) {
            QMessageBox::information(this, "成功", "图书删除成功！");
            ui->statusbar->showMessage("图书删除成功", 3000);
            // 刷新分类列表
            loadBookCategories();
        } else {
            QMessageBox::warning(this, "失败", "图书删除失败！");
        }
    }
}

void MainWindow::onSearchBooks()
{
    QString keyword = ui->bookSearchEdit->text();
    QString category = ui->bookCategoryCombo->currentData().toString();
    
    // 如果选择了分类，使用分类筛选
    if (!category.isEmpty()) {
        bookModel->filterBooks("", "", "", category);
    } else if (!keyword.isEmpty()) {
        // 如果有关键词，使用关键词搜索（不限制分类）
        bookModel->filterBooks(keyword, keyword, keyword, "");
    } else {
        // 清空筛选
        bookModel->setFilter("");
    }
    bookModel->select();
}

QStringList MainWindow::getDefaultCategories() const
{
    return QStringList() << "社会科学类" << "自然科学类" << "工程技术类" 
                        << "文学艺术类" << "哲学宗教类" << "综合类";
}

void MainWindow::loadBookCategories()
{
    QSqlQuery query(DatabaseManager::getInstance().getDatabase());
    query.exec("SELECT DISTINCT category FROM books WHERE category IS NOT NULL AND category != '' ORDER BY category");
    
    // 保存当前选中的分类
    QString currentCategory = ui->bookCategoryCombo->currentData().toString();
    
    // 清空并重新加载（保留"全部分类"选项）
    ui->bookCategoryCombo->clear();
    ui->bookCategoryCombo->addItem("全部分类", "");
    
    // 添加预设分类
    QStringList defaultCategories = getDefaultCategories();
    for (const QString &cat : defaultCategories) {
        ui->bookCategoryCombo->addItem(cat, cat);
    }
    
    // 添加数据库中已有的其他分类
    QSet<QString> categories(defaultCategories.begin(), defaultCategories.end());
    while (query.next()) {
        QString cat = query.value(0).toString().trimmed();
        if (!cat.isEmpty() && !categories.contains(cat)) {
            categories.insert(cat);
            ui->bookCategoryCombo->addItem(cat, cat);
        }
    }
    
    // 恢复之前选中的分类
    int index = ui->bookCategoryCombo->findData(currentCategory);
    if (index >= 0) {
        ui->bookCategoryCombo->setCurrentIndex(index);
    }
}

void MainWindow::onBookSelectionChanged()
{
    QModelIndexList indexes = ui->bookTableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        currentBookId = -1;
        return;
    }
    QModelIndex index = indexes.first();
    currentBookId = bookModel->data(bookModel->index(index.row(), 0)).toInt();
}

// 显示读者对话框
void MainWindow::showReaderDialog(bool isEdit)
{
    if (isEdit) {
        QModelIndexList indexes = ui->readerTableView->selectionModel()->selectedRows();
        if (indexes.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择要编辑的读者！");
            return;
        }
        currentReaderId = readerModel->data(readerModel->index(indexes.first().row(), 0)).toInt();
    } else {
        currentReaderId = -1;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "编辑读者" : "添加读者");
    dialog.setMinimumWidth(400);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QLineEdit *readerIdEdit = new QLineEdit();
    QLineEdit *nameEdit = new QLineEdit();
    QComboBox *genderEdit = new QComboBox();
    genderEdit->addItems({"", "男", "女"});
    QLineEdit *phoneEdit = new QLineEdit();
    QLineEdit *emailEdit = new QLineEdit();
    QLineEdit *addressEdit = new QLineEdit();
    
    formLayout->addRow("读者编号*:", readerIdEdit);
    formLayout->addRow("姓名*:", nameEdit);
    formLayout->addRow("性别:", genderEdit);
    formLayout->addRow("电话:", phoneEdit);
    formLayout->addRow("邮箱:", emailEdit);
    formLayout->addRow("地址:", addressEdit);
    
    // 如果是编辑模式，填充现有数据
    if (isEdit && currentReaderId >= 0) {
        QModelIndexList indexes = ui->readerTableView->selectionModel()->selectedRows();
        QModelIndex index = indexes.first();
        readerIdEdit->setText(readerModel->data(readerModel->index(index.row(), 1)).toString());
        nameEdit->setText(readerModel->data(readerModel->index(index.row(), 2)).toString());
        QString gender = readerModel->data(readerModel->index(index.row(), 3)).toString();
        int genderIndex = genderEdit->findText(gender);
        if (genderIndex >= 0) genderEdit->setCurrentIndex(genderIndex);
        phoneEdit->setText(readerModel->data(readerModel->index(index.row(), 4)).toString());
        emailEdit->setText(readerModel->data(readerModel->index(index.row(), 5)).toString());
        addressEdit->setText(readerModel->data(readerModel->index(index.row(), 6)).toString());
    }
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (readerIdEdit->text().isEmpty() || nameEdit->text().isEmpty()) {
            QMessageBox::warning(this, "警告", "读者编号和姓名不能为空！");
            return;
        }
        
        bool success;
        if (isEdit) {
            success = readerModel->updateReader(
                currentReaderId,
                readerIdEdit->text(),
                nameEdit->text(),
                genderEdit->currentText(),
                phoneEdit->text(),
                emailEdit->text(),
                addressEdit->text()
            );
        } else {
            success = readerModel->addReader(
                readerIdEdit->text(),
                nameEdit->text(),
                genderEdit->currentText(),
                phoneEdit->text(),
                emailEdit->text(),
                addressEdit->text()
            );
        }
        
        if (success) {
            QMessageBox::information(this, "成功", isEdit ? "读者更新成功！" : "读者添加成功！");
            ui->statusbar->showMessage(isEdit ? "读者更新成功" : "读者添加成功", 3000);
        } else {
            QMessageBox::warning(this, "失败", isEdit ? "读者更新失败！" : "读者添加失败，读者编号可能已存在！");
        }
    }
}

void MainWindow::onDeleteReader()
{
    QModelIndexList indexes = ui->readerTableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择要删除的读者！");
        return;
    }
    
    currentReaderId = readerModel->data(readerModel->index(indexes.first().row(), 0)).toInt();
    
    int ret = QMessageBox::question(this, "确认", "确定要删除这位读者吗？",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        if (readerModel->deleteReader(currentReaderId)) {
            QMessageBox::information(this, "成功", "读者删除成功！");
            ui->statusbar->showMessage("读者删除成功", 3000);
        } else {
            QMessageBox::warning(this, "失败", "读者删除失败！");
        }
    }
}

void MainWindow::onSearchReaders()
{
    QString keyword = ui->readerSearchEdit->text();
    if (keyword.isEmpty()) {
        readerModel->setFilter("");
    } else {
        readerModel->filterReaders(keyword, keyword, keyword, "");
    }
    readerModel->select();
}

void MainWindow::onReaderSelectionChanged()
{
    QModelIndexList indexes = ui->readerTableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        currentReaderId = -1;
        return;
    }
    QModelIndex index = indexes.first();
    currentReaderId = readerModel->data(readerModel->index(index.row(), 0)).toInt();
}

// 显示借书对话框
void MainWindow::showBorrowDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("借书");
    dialog.setMinimumWidth(350);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QLineEdit *readerIdEdit = new QLineEdit();
    QLineEdit *bookIsbnEdit = new QLineEdit();
    QSpinBox *daysEdit = new QSpinBox();
    daysEdit->setMinimum(1);
    daysEdit->setMaximum(365);
    daysEdit->setValue(30);
    
    formLayout->addRow("读者编号*:", readerIdEdit);
    formLayout->addRow("图书ISBN*:", bookIsbnEdit);
    formLayout->addRow("借阅天数:", daysEdit);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (readerIdEdit->text().isEmpty() || bookIsbnEdit->text().isEmpty()) {
            QMessageBox::warning(this, "警告", "读者编号和图书ISBN不能为空！");
            return;
        }
        
        bool success = borrowModel->borrowBook(
            readerIdEdit->text(),
            bookIsbnEdit->text(),
            daysEdit->value()
        );
        
        if (success) {
            QMessageBox::information(this, "成功", "借书成功！");
            refreshStatistics();
            ui->statusbar->showMessage("借书成功", 3000);
        } else {
            QMessageBox::warning(this, "失败", "借书失败，请检查读者编号、图书ISBN或图书是否可借！");
        }
    }
}

void MainWindow::onReturnBook()
{
    QModelIndexList indexes = ui->borrowTableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择要归还的记录！");
        return;
    }
    
    currentBorrowId = borrowModel->data(borrowModel->index(indexes.first().row(), 0)).toInt();
    
    double fine = borrowModel->calculateFine(currentBorrowId);
    QString message = "确定要归还这本书吗？";
    if (fine > 0) {
        message += QString("\n逾期罚款：%1 元").arg(fine, 0, 'f', 2);
    }
    
    int ret = QMessageBox::question(this, "确认", message,
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        if (borrowModel->returnBook(currentBorrowId)) {
            QMessageBox::information(this, "成功", fine > 0 ? 
                QString("还书成功！逾期罚款：%1 元").arg(fine, 0, 'f', 2) : "还书成功！");
            refreshStatistics();
            ui->statusbar->showMessage("还书成功", 3000);
        } else {
            QMessageBox::warning(this, "失败", "还书失败！");
        }
    }
}

void MainWindow::onSearchBorrowRecords()
{
    QString keyword = ui->borrowSearchEdit->text();
    if (keyword.isEmpty()) {
        borrowModel->setFilter("");
    } else {
        borrowModel->filterRecords(keyword, keyword, "");
    }
    borrowModel->select();
}

void MainWindow::onBorrowSelectionChanged()
{
    QModelIndexList indexes = ui->borrowTableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        currentBorrowId = -1;
        return;
    }
    QModelIndex index = indexes.first();
    currentBorrowId = borrowModel->data(borrowModel->index(index.row(), 0)).toInt();
}

// 统计信息
void MainWindow::refreshStatistics()
{
    // 图书总数
    QSqlQuery query(DatabaseManager::getInstance().getDatabase());
    query.exec("SELECT COUNT(*) FROM books");
    if (query.next()) {
        ui->totalBooksLabel->setText(QString::number(query.value(0).toInt()));
    }
    
    // 读者总数
    query.exec("SELECT COUNT(*) FROM readers");
    if (query.next()) {
        ui->totalReadersLabel->setText(QString::number(query.value(0).toInt()));
    }
    
    // 借阅统计
    BorrowModel::Statistics stats = borrowModel->getStatistics();
    ui->currentBorrowsLabel->setText(QString::number(stats.currentBorrows));
    ui->overdueCountLabel->setText(QString::number(stats.overdueCount));
}

// 逾期提醒
void MainWindow::checkOverdueBooks()
{
    QList<int> overdueIds = borrowModel->getOverdueRecords();
    if (!overdueIds.isEmpty()) {
        QString message = QString("发现 %1 本图书逾期未归还！").arg(overdueIds.size());
        ui->statusbar->showMessage(message, 10000);
        
        // 刷新借阅记录视图以显示逾期高亮
        borrowModel->select();
        
        // 可选：显示消息框提醒
        // QMessageBox::warning(this, "逾期提醒", message);
    }
}
