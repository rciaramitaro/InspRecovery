#include "checkrecovery.hpp"
#include "qdebug.h"
#include "qlabel.h"
#include "ui_mainwindow.h"
#include <QProcess>
#include <QDir>
#include <QtConcurrent/QtConcurrent>


QString RECOVERY_DIR = "/mnt/recovery";
QString APP_DIR = "/mnt/app";
QString APPDATA_DIR = "/mnt/appdata";
QString UPDATE_SUCCESS_FLAG = "updateSuccessful.flag";
QString UPDATE_INFO = "insp_LinuxUpdate.txt";
QString CHECKSUM_FILE = "sha1sum.sha1";

QString ROTH_SPLASH = "roth_splash.bmp";
QString UPDATE_SPLASH = "update_splash.bmp";
QString WARNING_SPLASH = "warning_splash.bmp";

checkRecovery::checkRecovery(MainWindow *mainWindow, QApplication * app)
: mParent(mainWindow)
, mApp(app)
{
    displaySplash(ROTH_SPLASH);
}

void checkRecovery::init()
{
    qWarning() << "Launching Inspectron Recovery Check";

    if (!QDir(RECOVERY_DIR).exists())
    {
        qCritical() << "WARNING - Recovery directory doesn't exist, will not perform recovery check";
        return;
    }

    startBackupCheck();
    displaySplash(ROTH_SPLASH);
}

/*!
  * @brief checkRecovery::startBackupCheck
  * Determines if rootfs requires a recovery check or if recovery requires an update
  */
void checkRecovery::startBackupCheck()
{
    if (QDir(RECOVERY_DIR).entryList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0) //check if /mnt/recovery has contents
    {
        displaySplash(UPDATE_SPLASH);
        qWarning() << "Recovery partition is empty! Update recovery partition";
        updateRecoveryPartition();
    }
    else
        if ( fileExists(RECOVERY_DIR + "/" + UPDATE_SUCCESS_FLAG) ) //check if a previous update was successful
        {
            displaySplash(UPDATE_SPLASH);

            qWarning() << "Previous update was successful! Update recovery partition";
            updateRecoveryPartition();
            QDir(RECOVERY_DIR).remove(UPDATE_SUCCESS_FLAG); //wait until everything is synced before removing flag
        }
        else
        {
            qWarning() << "No successful update detected, checking files against recovery";
            startRecoveryProcess();
        }
}

/*!
  * @brief checkRecovery::updateRecoveryPartition
  * This function is what determines what is checked for recovery!!
  */
void checkRecovery::updateRecoveryPartition()
{
    copyFolderToFolder(APP_DIR, RECOVERY_DIR + APP_DIR);
    copyFolderToFolder(APPDATA_DIR, RECOVERY_DIR + APPDATA_DIR);

    //parse through insp_LinuxUpdateFile.txt
    parseUpdateFile(RECOVERY_DIR + "/" + UPDATE_INFO);

    //QDir(RECOVERY_DIR).remove(UPDATE_INFO);
    QDir(RECOVERY_DIR).remove(CHECKSUM_FILE);

    startRecoveryProcess(); //ensure everything is synced
}

/*!
  * @brief checkRecovery::fileExists
  * Determines whether a file exists
  * @param path - absolute file path to check
  * @return - true/false
  */
bool checkRecovery::fileExists(QString path)
{
    QFileInfo check_file(path);

    // check if file exists and if yes: Is it really a file and no directory?
    if (check_file.exists() && check_file.isFile())
    {
        return true;
    }

    return false;
}

/*!
  * @brief checkRecovery::parseUpdateFile
  * Parses the update file recieved from a TAR update for relevant files to check for
  * @param path - absolute file path to insp_LinuxUpdate.txt
  */
void checkRecovery::parseUpdateFile(QString updateFilePath)
{
    QFile updateLinuxFile(updateFilePath);

    if (updateLinuxFile.open(QIODevice::ReadOnly))
    {
        QTextStream line(&updateLinuxFile);
        while (!line.atEnd())
        {
            QString file_path_raw  = line.readLine();

            QStringList fileInfo = file_path_raw.split(",");

            QString type = fileInfo[0];
            QString filename = fileInfo[1];
            QString destination_path = fileInfo[2];

            //Dont parse anything meant to be stored in recovery partition
            if ( destination_path != RECOVERY_DIR )
            {
                //Only parse individual files
                if ( type == "ADD" )
                {
                    qCritical() << destination_path + "/" + filename;
                    qCritical() << RECOVERY_DIR + destination_path + "/" + filename;
                    copyFileToDestination(destination_path + "/" + filename, RECOVERY_DIR + destination_path + "/" + filename);
                }
            }
       }
       updateLinuxFile.close();
    }

}

/*!
  * @brief checkRecovery::startRecoveryProcess
  * Begins recovery process but first check if the checksum list is generated
  * If not, generate it
  */
void checkRecovery::startRecoveryProcess()
{
    if ( ! fileExists(RECOVERY_DIR + "/" + CHECKSUM_FILE))
    {
        qWarning() << "No Checksum List found!";
        generateChecksumList();
    }

    qWarning() << "Checking file system against recovery partition!";
    checkForRecovery();
}

/*!
  * @brief checkRecovery::generateChecksumList
  * Generates a checksum list of files to check for every boot
  */
void checkRecovery::generateChecksumList()
{

    //contents of sha1sum
    //7e737b16d633cc169f1f9ff85e48e5acf32e174c  /mnt/recovery/mnt/app/bin/healthmonitor

    qWarning() << "Generating checksum list";

    //Acquire all regular files
    QList<QByteArray> recovery_sum_raw_files = execCmdLine("find", QStringList() <<
                                                   RECOVERY_DIR <<
                                                   "-type" <<
                                                   "f").split('\n'); //regular files

    //Acquire all sym links
    QList<QByteArray> recovery_sum_raw_sym_links = execCmdLine("find", QStringList() <<
                                                        RECOVERY_DIR <<
                                                        "-type" <<
                                                        "l").split('\n'); //sym links

    //exclude checking for databases, the checksum file, and the update flag
    foreach( QByteArray recovery_sum_raw_file, recovery_sum_raw_files ) {
        if ( !recovery_sum_raw_file.contains(".db") && !recovery_sum_raw_file.contains(".sha1") && !recovery_sum_raw_file.contains(".flag") )
        {
            logToChecksumList(execCmdLine("sha1sum", QStringList() << recovery_sum_raw_file));
        }
    }

    //exclude checking for databases, the checksum file, and the update flag
    foreach( QByteArray recovery_sum_raw_sym_link, recovery_sum_raw_sym_links ) {
        if ( !recovery_sum_raw_sym_link.contains(".db") && !recovery_sum_raw_sym_link.contains(".sha1") && !recovery_sum_raw_sym_link.contains(".flag") )
        {
            logToChecksumList(execCmdLine("sha1sum", QStringList() << recovery_sum_raw_sym_link));
        }
    }
}

/*!
  * @brief checkRecovery::checkForRecovery
  * Compares the rootfs to recovery using the generated checksum list
  * If a file differs or is missing, copy from recovery to rootfs
  */
void checkRecovery::checkForRecovery()
{
    QFile checksumFile(RECOVERY_DIR + "/" + CHECKSUM_FILE);
    if (checksumFile.open(QIODevice::ReadOnly))
    {
        QTextStream line(&checksumFile);
        while (!line.atEnd())
        {
            QString recovery_sum_raw  = line.readLine();

            // get the full path of the recovery file
            QString recovery_path = recovery_sum_raw.split("  ")[1];

            // get only the checksum of the recovery file
            QString recovery_sum = recovery_sum_raw.split("  ")[0];

            // get the full path of the test file by removing recovery directory path
            QString rootfs_path = recovery_sum_raw.split("  ")[1].remove(RECOVERY_DIR);

            if ( ! fileExists(rootfs_path) )
            {
                displaySplash(WARNING_SPLASH);
                qWarning() << rootfs_path + " doesnt exist, copying from recovery partition path " << recovery_path;
                copyFileToDestination(recovery_path, rootfs_path);
            }
            else
            {
                // get the contents of sha1sum of the test file
                QString rootfs_sum_raw = execCmdLine("sha1sum", QStringList() << rootfs_path);
                // get only the checksum of the test file
                QString rootfs_sum = rootfs_sum_raw.split("  ")[0];

                // if the checksums of the test file and the recovery file differ, then "recover" the tested file
                if (rootfs_sum != recovery_sum)
                {
                    displaySplash(WARNING_SPLASH);
                    qWarning() << rootfs_path + "     AND    " + recovery_path + "  DIFFER, copying from recovery partition";
                    // overwrite the test file with the recovery file
                    copyFileToDestination(recovery_path, rootfs_path);
                }
            }
        }
        checksumFile.close();
    }
}

/*!
  * @brief checkRecovery::generateParentDirectory
  * Generates the parent directory of a given file path
  * @param path - absolute file path of file
  */
void checkRecovery::generateParentDirectory(QString path)
{
    //extract  parent directory
    QString fileName = path.split("/").last();
    path.remove(fileName);

    QDir dir;
    dir.mkpath(path);
}

/*!
  * @brief checkRecovery::copyFolderToFolder
  * Copies the entire contents of one folder to another
  * @param sourceFolderPath - absolute source folder path to be copied
  * @param destFolderPath - absolute destination folder path to be copied to
  */
void checkRecovery::copyFolderToFolder(QString sourceFolderPath, QString destFolderPath)
{
    QDir sourceDir(sourceFolderPath);
    if(!sourceDir.exists())
        return;

    QDir destDir(destFolderPath);
    if(!destDir.exists())
    {
        destDir.mkpath(destFolderPath);
    }

    QStringList files = sourceDir.entryList(QDir::Files);
    for(int i = 0; i< files.count(); i++)
    {
        QString srcName = sourceFolderPath + "/" + files[i];
        QString destName = destFolderPath + "/" + files[i];

        copyFileToDestination(srcName, destName);
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(int i = 0; i< files.count(); i++)
    {
        QString srcName = sourceFolderPath + "/" + files[i];
        QString destName = destFolderPath + "/" + files[i];
        copyFolderToFolder(srcName, destName);
    }
}

/*!
  * @brief checkRecovery::copyFileToDestination
  * Copies a single file to a destination directory
  * @param srcPath - absolute file path to be copied
  * @param destPath - absolute destination folder path to be copied to
  */
void checkRecovery::copyFileToDestination(QString srcPath, QString destPath)
{
    if (fileExists(destPath))
    {
        QFile::remove(destPath);
    }

    generateParentDirectory(destPath);

    QString srcSymLinkTarget = QFile::symLinkTarget(srcPath);
    if (srcSymLinkTarget != "")
    {
        QString destSymLinkTarget = srcSymLinkTarget.remove(RECOVERY_DIR);
        QFile::link(destSymLinkTarget, destPath);
    }
    else
    {
        QFile::copy(srcPath, destPath);
    }
}

/*!
  * @brief checkRecovery::execCmdLine
  * Execute A Command
  * @param cmd Command To Execute
  * @param args Arguments For The Command
  * @return stdout
  */
QByteArray checkRecovery::execCmdLine(QString cmd, QStringList args)
{
    QProcess process;
    QByteArray stdOut;

    if(args.count() > 0)
    {
        process.start(cmd, args);
    }
    else
    {
        process.start(cmd);
    }

    process.waitForFinished(-1);
    stdOut = process.readAllStandardOutput();

    return stdOut;
}

/*!
  * @brief checkRecovery::logToChecksumList
  * Log the raw checksum data to the checksum list file
  * @param data - raw checksum data
  */
void checkRecovery::logToChecksumList(QString data)
{

    // logfile is opened and closed in case of power failure
    QFile logFile(RECOVERY_DIR + "/" + CHECKSUM_FILE);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream out(&logFile);
        out << data;
    }
    else
    {
        qCritical() << "cannot open the file " + RECOVERY_DIR + "/" + CHECKSUM_FILE;
    }

    if ( logFile.isOpen() )
    {
        logFile.close();
    }
}

/*!
  * @brief checkRecovery::displaySplash
  * Displays a splash screen based on whether
  * 1 - Nothing relevant to the user (Roth)
  * 2 - Recovery partition is being updated (Update)
  * 3 - Rootfs is being recovered (Warning)
  * @param type - String to determine what splash screen to display (Roth, Update, Warning)
  */
void checkRecovery::displaySplash(QString type)
{
    mParent->getUi()->splash->setPixmap(QPixmap(":/" + type));
    mApp->processEvents();
    mApp->processEvents();
}
