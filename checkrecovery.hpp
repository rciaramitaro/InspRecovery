#ifndef CHECKRECOVERY_HPP
#define CHECKRECOVERY_HPP

#include "mainwindow.h"
#include "qapplication.h"
#include <QString>
#include <QByteArray>

class checkRecovery
{
public:
    checkRecovery(MainWindow * mainWindow, QApplication * app);
    void init();

private:
    void startBackupCheck();
    bool fileExists(QString path);
    void updateRecoveryPartition();
    void copyFolderToFolder(QString sourceFolderPath, QString destFolderPath);
    void copyFileToDestination(QString srcPath, QString destPath);
    void parseUpdateFile(QString updateFilePath);
    void startRecoveryProcess();
    void generateChecksumList();
    void checkForRecovery();
    void generateParentDirectory(QString path);
    void logToChecksumList(QString data);
    void displaySplash(QString type);


    QByteArray execCmdLine(QString cmd, QStringList args);


private:
    MainWindow * mParent;
    QApplication * mApp;
};

#endif // CHECKRECOVERY_HPP
