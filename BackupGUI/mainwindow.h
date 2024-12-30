#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <QSet>
#include "backupconfigdialog.h"
#include "file_sys.h"
#include <QTreeWidgetItem>
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
    void on_startBackupButton_clicked();

    void on_addDirectoryButton_clicked();

    void on_addFileButton_clicked();

    void on_clearFileButton_clicked();

    void on_deleteFileButton_clicked();

    void on_backupFiles(QSharedPointer<BackupConfig> config);

    void on_browseLocalFile_clicked();

    void on_browseRestoreDirectoryButton_clicked();

  private:
    /// 获取文件类型
    QString getTypeTag(backup::FileType type);
    QString getTypeTag(const QString &file_path);

    void generateTreeItem(const backup::Path &dir, QTreeWidgetItem *parent);
    QTreeWidgetItem *generateOneTreeItem(
        const QString &filename,
        const QString &typetag,
        const QString &fullpath,
        const QString &pack_path);
    QList<QTreeWidgetItem *> getCheckedItems();
    void getCheckedItems(QList<QTreeWidgetItem *> &list, QTreeWidgetItem *root);
    Ui::MainWindow *ui;
    // QString: 文件的绝对路径
    // 只存储top_level（后面level不考虑）
    QSet<QString> files_to_pack;
    BackupConfigDialog *backupDialog;
};
#endif // MAINWINDOW_H
