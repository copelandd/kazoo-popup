/* --------------------------------------------------------------------------------------------------------------------------
 * ** 
 * ** Ordered by Kirill Sysoev kirill.sysoev@gmail.com
 * ** (OnNet communications Inc. http://onnet.su)
 * ** 
 * ** Developed by Alexey Lysenko lysenkoalexmail@gmail.com
 * ** 
 * ** Please report bugs and provide any possible patches directly to this repository: https://github.com/onnet/kazoo_popup.git
 * ** 
 * ** If you would like to order additional development, contact Alexey Lysenko over email lysenkoalexmail@gmail.com directly.
 * ** 
 * ** 
 * ** This application:
 * **  - connects to Kazoo whapp blackhole;
 * **  - listens for incoming calls;
 * **  - queries third party server whether it knows anything about caller's number;
 * **  - Pop's Up window with provided info.
 * ** 
 * ** It is:
 * **  - written in Qt which promises to be crossplatform application (hopefully);
 * **  - is NOT production ready, but intended to be a simple example of using blachole whapp
 * **    (please note, that blackhole whapp doesn't support secure connectoin over SSL yet; check KAZOO-2632).
 * ** 
 * ** Good luck!
 * ** 
 * ** -------------------------------------------------------------------------------------------------------------------------*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "defaults.h"

#include "contactinfo.h"
#include "informerdialog.h"
#include "websocketmanager.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopWidget>

#include <QSettings>
#include <QFile>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    createTrayIcon();

    m_wsMan = new WebSocketManager(this);
    connect(m_wsMan, &WebSocketManager::channelCreated,
            this, &MainWindow::onChannelCreated);
    connect(m_wsMan, &WebSocketManager::channelAnswered,
            this, &MainWindow::onChannelAnswered);
    connect(m_wsMan, &WebSocketManager::channelDestroyed,
            this, &MainWindow::onChannelDestroyed);

    connect(ui->cancelPushButton, &QPushButton::clicked,
            this, &MainWindow::close);
    connect(ui->okPushButton, &QPushButton::clicked,
            this, &MainWindow::saveSettings);

    m_wsMan->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(QIcon(":/res/icon.png"), this);

    QMenu *menu = new QMenu(this);
    menu->addAction(tr("Settings"), this, SLOT(show()));
    menu->addSeparator();
    menu->addAction(tr("Quit"), qApp, SLOT(quit()));
    m_trayIcon->setContextMenu(menu);

    m_trayIcon->show();
}

void MainWindow::onChannelCreated(ContactInfo *contactInfo)
{
    if (m_informerDialogsHash.contains(contactInfo))
    {
        InformerDialog *informerDialog = m_informerDialogsHash.value(contactInfo);
        if (informerDialog->isVisible())
            return;

        informerDialog->setAnswered(false);
        informerDialog->show();
    }
    else
    {
        InformerDialog *informerDialog = new InformerDialog();
        informerDialog->setContactInfo(contactInfo);
        m_informerDialogsHash.insert(contactInfo, informerDialog);
        informerDialog->adjustSize();
        QRect rect = qApp->desktop()->availableGeometry();
        informerDialog->setGeometry(rect.width() - informerDialog->width(),
                                    rect.height() - informerDialog->height(),
                                    informerDialog->width(),
                                    informerDialog->height());
        informerDialog->show();
    }
}

void MainWindow::onChannelAnswered(ContactInfo *contactInfo)
{
    m_contactInfoForTimer = contactInfo;
    InformerDialog *informerDialog = m_informerDialogsHash.value(contactInfo);
    if (informerDialog->isVisible())
        informerDialog->setAnswered(true);

    QTimer::singleShot(15000, this, SLOT(timeout()));
}

void MainWindow::timeout()
{
    if (m_contactInfoForTimer == nullptr)
        return;

    if (!m_informerDialogsHash.contains(m_contactInfoForTimer))
        return;

    InformerDialog *informerDialog = m_informerDialogsHash.value(m_contactInfoForTimer);
    if (informerDialog->isVisible())
        informerDialog->close();

    m_informerDialogsHash.remove(m_contactInfoForTimer);
    informerDialog->deleteLater();
    delete m_contactInfoForTimer;
    m_contactInfoForTimer = nullptr;
}

void MainWindow::onChannelDestroyed(ContactInfo *contactInfo)
{
    if (!m_informerDialogsHash.contains(contactInfo))
        return;

    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    bool close = settings.value("close_hung_up", true).toBool();

    InformerDialog *informerDialog = m_informerDialogsHash.value(contactInfo);
    if (informerDialog->isVisible() && close)
        informerDialog->close();

    m_informerDialogsHash.remove(contactInfo);
    informerDialog->deleteLater();
    delete contactInfo;
}

bool MainWindow::isCorrectSettings() const
{
    bool ok = true;
    ok &= !ui->loginLineEdit->text().isEmpty();
    if (!ok)
        return false;

    ok &= !ui->passwordLineEdit->text().isEmpty();
    if (!ok)
        return false;

    ok &= !ui->realmLineEdit->text().isEmpty();
    if (!ok)
        return false;

    ok &= !ui->authUrlLineEdit->text().isEmpty();
    if (!ok)
        return false;

    ok &= !ui->eventUrlLineEdit->text().isEmpty();
    if (!ok)
        return false;

    ok &= !ui->infoUrlLineEdit->text().isEmpty();
    if (!ok)
        return false;

    ok &= !ui->md5HashLineEdit->text().isEmpty();
    if (!ok)
        return false;

    return ok;
}

void MainWindow::saveSettings()
{
    if (!isCorrectSettings())
    {
        QMessageBox::warning(this, tr("Call Informer"), tr("All fields must be filled!"));
        return;
    }

    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.setValue("login", ui->loginLineEdit->text());
    settings.setValue("password", ui->passwordLineEdit->text());
    settings.setValue("realm", ui->realmLineEdit->text());
    settings.setValue("auth_url", ui->authUrlLineEdit->text());
    settings.setValue("event_url", ui->eventUrlLineEdit->text());
    settings.setValue("info_url", ui->infoUrlLineEdit->text());
    settings.setValue("md5_hash", ui->md5HashLineEdit->text());
    settings.setValue("close_hung_up", ui->closeWindowCheckBox->isChecked());

    close();
}

void MainWindow::loadSettings()
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    ui->loginLineEdit->setText(settings.value("login", kLogin).toString());
    ui->passwordLineEdit->setText(settings.value("password", kPassword).toString());
    ui->realmLineEdit->setText(settings.value("realm", kRealm).toString());
    ui->authUrlLineEdit->setText(settings.value("auth_url", kAuthUrl).toString());
    ui->eventUrlLineEdit->setText(settings.value("event_url", kEventUrl).toString());
    ui->infoUrlLineEdit->setText(settings.value("info_url", kInfoUrl).toString());
    ui->md5HashLineEdit->setText(settings.value("md5_hash", kMd5Hash).toString());
    ui->closeWindowCheckBox->setChecked(settings.value("close_hung_up", true).toBool());
}

void MainWindow::showEvent(QShowEvent *event)
{
    if (QFile::exists(qApp->applicationDirPath() + "/settings.ini"))
        loadSettings();

    QMainWindow::showEvent(event);
}
