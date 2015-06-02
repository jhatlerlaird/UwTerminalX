/******************************************************************************
** Copyright (C) 2014-2015 Ezurio Ltd
**
** Project: UwTerminalX
**
** Module: UwxMainWindow.cpp
**
** Notes:
**
*******************************************************************************/

/******************************************************************************/
// Include Files
/******************************************************************************/
#include "UwxMainWindow.h"
#include "ui_UwxMainWindow.h"
#include "UwxAutomation.h"

/******************************************************************************/
// Conditional Compile Defines
/******************************************************************************/
#ifdef QT_DEBUG
    //Inclde debug output when compiled for debugging
    #include <QDebug>
#endif
#ifdef _WIN32
    //Windows
    #define OS "Win"
#elif __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_MAC
        //Mac OSX
        #define OS "Mac"
        QString gstrMacBundlePath;
    #endif
#else
    //Assume linux
    #define OS "Linux"
#endif

/******************************************************************************/
// Global/Static Variable Declarations
/******************************************************************************/
PopupMessage *gpmErrorForm; //Error message form
UwxAutomation *guaAutomationForm; //Automation form

/******************************************************************************/
// Local Functions or Private Members
/******************************************************************************/
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    //Setup the GUI
    ui->setupUi(this);

#if TARGET_OS_MAC
    //On mac, get the directory of the bundle (which will be <location>/Term.app/Contents/MacOS) and go up to the folder with the file in
    QDir BundleDir(QCoreApplication::applicationDirPath());
    BundleDir.cdUp();
    BundleDir.cdUp();
    BundleDir.cdUp();
    gstrMacBundlePath = BundleDir.path().append("/");
    gpTermSettings = new QSettings(QString(gstrMacBundlePath).append("UwTerminalX.ini"), QSettings::IniFormat); //Handle to settings
    gpErrorMessages = new QSettings(QString(gstrMacBundlePath).append("codes.csv"), QSettings::IniFormat); //Handle to error codes
    gpPredefinedDevice = new QSettings(QString(gstrMacBundlePath).append("Devices.ini"), QSettings::IniFormat); //Handle to predefined devices

    //Fix mac's resize
    resize(660, 360);
#else
    //Open files in same directory
    gpTermSettings = new QSettings(QString("UwTerminalX.ini"), QSettings::IniFormat); //Handle to settings
    gpErrorMessages = new QSettings(QString("codes.csv"), QSettings::IniFormat); //Handle to error codes
    gpPredefinedDevice = new QSettings(QString("Devices.ini"), QSettings::IniFormat); //Handle to predefined devices
#endif

    //Define default variable values
    gbTermBusy = false;
    gbStreamingFile = false;
    gintRXBytes = 0;
    gintTXBytes = 0;
    gintQueuedTXBytes = 0;
    gchTermBusyLines = 0;
    gchTermMode = 0;
    gchTermMode2 = 0;
    gbMainLogEnabled = false;
    gbLoopbackMode = false;
    gbSysTrayEnabled = false;
    gbIsUWCDownload = false;
    gbCTSStatus = 0;
    gbDCDStatus = 0;
    gbDSRStatus = 0;
    gbRIStatus = 0;
    gbStreamingBatch = false;
    gbaBatchReceive.clear();
    gbFileOpened = false;

    //Clear display buffer byte array.
    gbaDisplayBuffer.clear();

    //Check settings
#if TARGET_OS_MAC
    if (!QFile::exists(QString(gstrMacBundlePath).append("UwTerminalX.ini")))
#else
    if (!QFile::exists("UwTerminalX.ini") || gpTermSettings->value("ConfigVersion", UwVersion).toString() != UwVersion)
#endif
    {
        //No settings, or some config values not present defaults
        if (gpTermSettings->value("LogFile").isNull())
        {
            gpTermSettings->setValue("LogFile", DefaultLogFile); //Default log file
        }
        if (gpTermSettings->value("LogMode").isNull())
        {
            gpTermSettings->setValue("LogMode", DefaultLogMode); //Clear log before opening, 0 = no, 1 = yes
        }
        if (gpTermSettings->value("LogLevel").isNull())
        {
            gpTermSettings->setValue("LogLevel", DefaultLogLevel); //0 = none, 1 = single file, 2 = 1 + new file for each session
        }
        if (gpTermSettings->value("CompilerDir").isNull())
        {
            gpTermSettings->setValue("CompilerDir", DefaultCompilerDir); //Directory that compilers go in
        }
        if (gpTermSettings->value("CompilerSubDirs").isNull())
        {
            gpTermSettings->setValue("CompilerSubDirs", DefaultCompilerSubDirs); //0 = normal, 1 = use BL600, BL620, BT900 etc. subdir
        }
        if (gpTermSettings->value("DelUWCAfterDownload").isNull())
        {
            gpTermSettings->setValue("DelUWCAfterDownload", DefaultDelUWCAfterDownload); //0 = no, 1 = yes (delets UWC file after it's been downloaded to the target device)
        }
        if (gpTermSettings->value("SysTrayIcon").isNull())
        {
            gpTermSettings->setValue("SysTrayIcon", DefaultSysTrayIcon); //0 = no, 1 = yes (Shows a system tray icon and provides balloon messages)
        }
        if (gpTermSettings->value("SerialSignalCheckInterval").isNull())
        {
            gpTermSettings->setValue("SerialSignalCheckInterval", DefaultSerialSignalCheckInterval); //How often to check status of CTS, DSR, etc. signals in mS (lower = faster but more CPU usage)
        }
        if (gpTermSettings->value("PrePostXCompRun").isNull())
        {
            gpTermSettings->setValue("PrePostXCompRun", DefaultPrePostXCompRun); //If pre/post XCompiler executable is enabled: 1 = enable, 0 = disable
        }
        if (gpTermSettings->value("PrePostXCompFail").isNull())
        {
            gpTermSettings->setValue("PrePostXCompFail", DefaultPrePostXCompFail); //If post XCompiler executable should run if XCompilation fails: 1 = yes, 0 = no
        }
        if (gpTermSettings->value("PrePostXCompMode").isNull())
        {
            gpTermSettings->setValue("PrePostXCompMode", DefaultPrePostXCompMode); //If pre/post XCompiler command runs before or after XCompiler: 0 = before, 1 = after
        }
        if (gpTermSettings->value("PrePostXCompPath").isNull())
        {
            gpTermSettings->setValue("PrePostXCompPath", DefaultPrePostXCompPath); //Filename of pre/post XCompiler executable (with additional arguments)
        }
        if (gpTermSettings->value("OnlineXComp").isNull())
        {
            gpTermSettings->setValue("OnlineXComp", DefaultOnlineXComp); //If Online XCompiler support is enabled: 1 = enable, 0 = disable
        }
        if (gpTermSettings->value("OnlineXCompServer").isNull())
        {
            gpTermSettings->setValue("OnlineXCompServer", ServerHost); //Online XCompiler server IP/Hostname
        }
        if (gpTermSettings->value("TextUpdateInterval").isNull())
        {
            gpTermSettings->setValue("TextUpdateInterval", DefaultTextUpdateInterval); //Interval between screen updates in mS, lower = faster but can be problematic when receiving/sending large amounts of data (200 is good for this)
        }
        gpTermSettings->setValue("ConfigVersion", UwVersion);
    }

    //Create logging handle and variables for logging mode
    gpMainLog = new LrdLogger;
    bool bLoggerOpened = false;
    unsigned char chLoggerMode = 0;

    //Move to 'About' tab
    ui->selector_Tab->setCurrentIndex(3);

    //Set default values for combo boxes on 'Config' tab
    ui->combo_Baud->setCurrentIndex(8);
    ui->combo_Stop->setCurrentIndex(0);
    ui->combo_Data->setCurrentIndex(1);
    ui->combo_Handshake->setCurrentIndex(1);

    //Load images
    gimEmptyCircleImage = QImage(":/images/EmptyCircle.png");
    gimRedCircleImage = QImage(":/images/RedCircle.png");
    gimGreenCircleImage = QImage(":/images/GreenCircle.png");
#ifdef _WIN32
    //Load ICOs for windows
    gimUw16Image = QImage(":/images/UwTerminal16.ico");
    gimUw32Image = QImage(":/images/UwTerminal32.ico");
#else
    //Load PNGs for Linux/Mac
    gimUw16Image = QImage(":/images/UwTerminal16.png");
    gimUw32Image = QImage(":/images/UwTerminal32.png");
#endif

    //Create pixmaps
    gpEmptyCirclePixmap = new QPixmap(QPixmap::fromImage(gimEmptyCircleImage));
    gpRedCirclePixmap = new QPixmap(QPixmap::fromImage(gimRedCircleImage));
    gpGreenCirclePixmap = new QPixmap(QPixmap::fromImage(gimGreenCircleImage));
    gpUw16Pixmap = new QPixmap(QPixmap::fromImage(gimUw16Image));
    gpUw32Pixmap = new QPixmap(QPixmap::fromImage(gimUw32Image));

    //Show images on help
    ui->label_AboutI1->setPixmap(*gpEmptyCirclePixmap);
    ui->label_AboutI2->setPixmap(*gpRedCirclePixmap);
    ui->label_AboutI3->setPixmap(*gpGreenCirclePixmap);

    //Enable custom context menu policy
    ui->text_TermEditData->setContextMenuPolicy(Qt::CustomContextMenu);

    //Connect process termination to signal
    connect(&gprocCompileProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(process_finished(int, QProcess::ExitStatus)));

    //Connect quit signals
    connect(ui->btn_Decline, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btn_Quit, SIGNAL(clicked()), this, SLOT(close()));

    //Connect key-press signals
    connect(ui->text_TermEditData, SIGNAL(EnterPressed()), this, SLOT(EnterPressed()));
    connect(ui->text_TermEditData, SIGNAL(KeyPressed(QChar)), this, SLOT(KeyPressed(QChar)));

    //Connect file drag/drop signal
    connect(ui->text_TermEditData, SIGNAL(FileDropped(QString)), this, SLOT(DroppedFile(QString)));

    //Initialise popup message
    gpmErrorForm = new PopupMessage;

    //Initialise automation popup
    guaAutomationForm = new UwxAutomation;

    //Populate the list of devices
    RefreshSerialDevices();

    //Display version
    ui->statusBar->showMessage(QString("UwTerminalX version ").append(UwVersion).append(" (").append(OS).append("), Built ").append(__DATE__).append(" Using QT ").append(QT_VERSION_STR)
#ifdef QT_DEBUG
    .append(" [DEBUG BUILD]")
#endif
    );
    setWindowTitle(QString("UwTerminalX (v").append(UwVersion).append(")"));

    //Create menu items
    gpMenu = new QMenu(this);
    gpMenu->addAction(new QAction("XCompile", this));
    gpMenu->addAction(new QAction("XCompile + Load", this));
    gpMenu->addAction(new QAction("XCompile + Load + Run", this));
    gpMenu->addAction(new QAction("Load", this));
    gpMenu->addAction(new QAction("Load + Run", this));
    gpMenu->addAction(new QAction("Lookup Selected Error-Code (Hex)", this));
    gpMenu->addAction(new QAction("Lookup Selected Error-Code (Int)", this));
    gpMenu->addAction(new QAction("Enable Loopback (Rx->Tx)", this));
    gpSMenu1 = gpMenu->addMenu("Download");
    gpSMenu2 = gpSMenu1->addMenu("BASIC");
    gpSMenu2->addAction(new QAction("Load Precompiled BASIC", this));
    gpSMenu2->addAction(new QAction("Erase File", this)); //AT+DEL (from file open)
    gpSMenu2->addAction(new QAction("Dir", this)); //AT+DIR
    gpSMenu2->addAction(new QAction("Run", this));
    gpSMenu3 = gpSMenu1->addMenu("Data");
    gpSMenu3->addAction(new QAction("Data File +", this));
    gpSMenu3->addAction(new QAction("Erase File +", this));
    gpSMenu3->addAction(new QAction("Multi Data File +", this)); //Downloads more than 1 data file
    gpSMenu1->addAction(new QAction("Stream File Out", this));
    gpMenu->addAction(new QAction("Font", this));
    gpMenu->addAction(new QAction("Run", this));
    gpMenu->addAction(new QAction("Automation", this));
    gpMenu->addAction(new QAction("Batch", this));
    gpMenu->addAction(new QAction("Clear Display", this));
    gpMenu->addAction(new QAction("Clear RX/TX count", this));
    gpMenu->addSeparator();
    gpMenu->addAction(new QAction("Copy", this));
    gpMenu->addAction(new QAction("Copy All", this));
    gpMenu->addAction(new QAction("Paste", this));
    gpMenu->addAction(new QAction("Select All", this));

    //Create balloon menu items
    gpBalloonMenu = new QMenu(this);
    gpBalloonMenu->addAction(new QAction("Show UwTerminalX", this));
    gpBalloonMenu->addAction(new QAction("Exit", this));

    //Disable unimplemented actions
    gpSMenu3->actions()[2]->setEnabled(false); //Multi Data File +

    //Connect the menu actions
    connect(gpMenu, SIGNAL(triggered(QAction*)), this, SLOT(triggered(QAction*)), Qt::AutoConnection);
    connect(gpMenu, SIGNAL(aboutToHide()), this, SLOT(ContextMenuClosed()), Qt::AutoConnection);
    connect(gpBalloonMenu, SIGNAL(triggered(QAction*)), this, SLOT(balloontriggered(QAction*)), Qt::AutoConnection);

    //Configure the module timeout timer
    gtmrDownloadTimeoutTimer.setSingleShot(true);
    gtmrDownloadTimeoutTimer.setInterval(ModuleTimeout);
    connect(&gtmrDownloadTimeoutTimer, SIGNAL(timeout()), this, SLOT(DevRespTimeout()));

    //Configure the signal timer
    gpSignalTimer = new QTimer(this);
    connect(gpSignalTimer, SIGNAL(timeout()), this, SLOT(SerialStatusSlot()));

    //Connect serial ready and serial error signals
    connect(&gspSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(&gspSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(SerialError(QSerialPort::SerialPortError)));
    connect(&gspSerialPort, SIGNAL(bytesWritten(qint64)), this, SLOT(SerialBytesWritten(qint64)));

    //Populate window handles for automation object
    guaAutomationForm->SetPopupHandle(gpmErrorForm);
    guaAutomationForm->SetMainHandle(this);

    //Set update text display timer to be single shot only and connect to slot
    gtmrTextUpdateTimer.setSingleShot(true);
    gtmrTextUpdateTimer.setInterval(gpTermSettings->value("TextUpdateInterval", DefaultTextUpdateInterval).toInt());
    connect(&gtmrTextUpdateTimer, SIGNAL(timeout()), this, SLOT(UpdateReceiveText()));

    //Setup timer for batch file timeout
    gtmrBatchTimeoutTimer.setSingleShot(true);
    connect(&gtmrBatchTimeoutTimer, SIGNAL(timeout()), this, SLOT(BatchTimeoutSlot()));

    //Check if default devices were created
    if (gpPredefinedDevice->value("DoneSetup").isNull())
    {
        //Create default device configurations... BT900
        gpPredefinedDevice->setValue(QString("Port1Name"), "BT900");
        gpPredefinedDevice->setValue(QString("Port1Baud"), "115200");
        gpPredefinedDevice->setValue(QString("Port1Parity"), "0");
        gpPredefinedDevice->setValue(QString("Port1Stop"), "1");
        gpPredefinedDevice->setValue(QString("Port1Data"), "8");
        gpPredefinedDevice->setValue(QString("Port1Flow"), "1");

        //BL600/BL620
        gpPredefinedDevice->setValue(QString("Port2Name"), "BL600/BL620");
        gpPredefinedDevice->setValue(QString("Port2Baud"), "9600");
        gpPredefinedDevice->setValue(QString("Port2Parity"), "0");
        gpPredefinedDevice->setValue(QString("Port2Stop"), "1");
        gpPredefinedDevice->setValue(QString("Port2Data"), "8");
        gpPredefinedDevice->setValue(QString("Port2Flow"), "1");

        //BL620-US
        gpPredefinedDevice->setValue(QString("Port3Name"), "BL620-US");
        gpPredefinedDevice->setValue(QString("Port3Baud"), "9600");
        gpPredefinedDevice->setValue(QString("Port3Parity"), "0");
        gpPredefinedDevice->setValue(QString("Port3Stop"), "1");
        gpPredefinedDevice->setValue(QString("Port3Data"), "8");
        gpPredefinedDevice->setValue(QString("Port3Flow"), "0");

        //Mark as completed
        gpPredefinedDevice->setValue(QString("DoneSetup"), "1");
    }

    //Add predefined devices
    unsigned char i = 1;
    while (i < 255)
    {
        if (gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Name")).isNull())
        {
            break;
        }
        ui->combo_PredefinedDevice->addItem(gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Name")).toString());
        ++i;
    }

    //Load settings from first device
    if (ui->combo_PredefinedDevice->count() > 0)
    {
        on_combo_PredefinedDevice_currentIndexChanged(ui->combo_PredefinedDevice->currentIndex());
    }

    //Add tooltips
    ui->check_ShowCLRF->setToolTip("Enable this to escape various characters (CR will show as \\r, LF will show as \\n and Tab will show as \\t).");

    //Give focus to accept button
    if (ui->btn_Accept->isEnabled() == true)
    {
        ui->btn_Accept->setFocus();
    }

    //Enable system tray if it's available and enabled
    if (gpTermSettings->value("SysTrayIcon", DefaultSysTrayIcon).toBool() == true && QSystemTrayIcon::isSystemTrayAvailable())
    {
        //System tray enabled and available on system, set it up with contect menu/icon and show it
        gpSysTray = new QSystemTrayIcon;
        gpSysTray->setContextMenu(gpBalloonMenu);
        gpSysTray->setIcon(QIcon(*gpUw16Pixmap));
        gpSysTray->show();
        gbSysTrayEnabled = true;
    }

    //Update pre/post XCompile executable options
    ui->check_PreXCompRun->setChecked(gpTermSettings->value("PrePostXCompRun", DefaultPrePostXCompRun).toBool());
    ui->check_PreXCompFail->setChecked(gpTermSettings->value("PrePostXCompFail", DefaultPrePostXCompFail).toBool());
    if (gpTermSettings->value("PrePostXCompMode", DefaultPrePostXCompMode).toInt() == 1)
    {
        //Post-XCompiler run
        ui->radio_XCompPost->setChecked(true);
    }
    else
    {
        //Pre-XCompiler run
        ui->radio_XCompPre->setChecked(true);
    }
    ui->edit_PreXCompFilename->setText(gpTermSettings->value("PrePostXCompPath", DefaultPrePostXCompPath).toString());

    //Update GUI for pre/post XComp executable
    on_check_PreXCompRun_stateChanged(ui->check_PreXCompRun->isChecked()*2);

    //Set Online XCompilation test
#ifdef _WIN32
    //Windows
    ui->label_OnlineXCompInfo->setText("By enabling Online XCompilation support, if a local XCompiler is not found when attempting to compile an application, the source data will be uploaded to a Laird cloud server, compiled remotely and downloaded. Uploaded data is not stored by Laird.");
#else
    //Mac or Linux
    ui->label_OnlineXCompInfo->setText("By enabling Online XCompilation support, when compiling an application, the source data will be uploaded to a Laird cloud server, compiled remotely and downloaded. Uploaded data is not stored by Laird.");
#endif
    ui->check_OnlineXComp->setChecked(gpTermSettings->value("OnlineXComp", DefaultOnlineXComp).toBool());

    //Setup QNetwork for Online XCompiler
    gnmManager = new QNetworkAccessManager();
    connect(gnmManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
#ifdef UseSSL
    connect(gnmManager, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)), this, SLOT(sslErrors(QNetworkReply*, QList<QSslError>)));
#endif

    //Check command line
    QStringList slArgs = QCoreApplication::arguments();
    unsigned char chi = 1;
    bool bArgCom = false;
    bool bArgAccept = false;
    bool bArgNoConnect = false;
    while (chi < slArgs.length())
    {
        if (slArgs[chi].toUpper() == "ACCEPT")
        {
            //Skip the front panel - disable buttons
            ui->btn_Accept->setEnabled(false);
            ui->btn_Decline->setEnabled(false);

            //Switch to config tab
            ui->selector_Tab->setCurrentIndex(1);

            //Default empty images
            ui->image_CTS->setPixmap(*gpEmptyCirclePixmap);
            ui->image_DCD->setPixmap(*gpEmptyCirclePixmap);
            ui->image_DSR->setPixmap(*gpEmptyCirclePixmap);
            ui->image_RI->setPixmap(*gpEmptyCirclePixmap);

            bArgAccept = true;
        }
        else if (slArgs[chi].left(4).toUpper() == "COM=")
        {
            //Set com port
            ui->combo_COM->setCurrentText(slArgs[chi].right(slArgs[chi].length()-4));
            bArgCom = true;

            //Update serial port info
            on_combo_COM_currentIndexChanged(0);
        }
        else if (slArgs[chi].left(5).toUpper() == "BAUD=")
        {
            //Set baud rate
            ui->combo_Baud->setCurrentText(slArgs[chi].right(slArgs[chi].length()-5));
        }
        else if (slArgs[chi].left(5).toUpper() == "STOP=")
        {
            //Set stop bits
            if (slArgs[chi].right(1) == "1")
            {
                //One
                ui->combo_Stop->setCurrentIndex(0);
            }
            else if (slArgs[chi].right(1) == "2")
            {
                //Two
                ui->combo_Stop->setCurrentIndex(1);
            }
        }
        else if (slArgs[chi].left(5).toUpper() == "DATA=")
        {
            //Set data bits
            if (slArgs[chi].right(1) == "7")
            {
                //Seven
                ui->combo_Data->setCurrentIndex(0);
            }
            else if (slArgs[chi].right(1).toUpper() == "8")
            {
                //Eight
                ui->combo_Data->setCurrentIndex(1);
            }
        }
        else if (slArgs[chi].left(4).toUpper() == "PAR=")
        {
            //Set parity
            if (slArgs[chi].right(1).toInt() >= 0 && slArgs[chi].right(1).toInt() < 3)
            {
                ui->combo_Parity->setCurrentIndex(slArgs[chi].right(1).toInt());
            }
        }
        else if (slArgs[chi].left(5).toUpper() == "FLOW=")
        {
            //Set flow control
            if (slArgs[chi].right(1).toInt() >= 0 && slArgs[chi].right(1).toInt() < 3)
            {
                //Valid
                ui->combo_Handshake->setCurrentIndex(slArgs[chi].right(1).toInt());
            }
        }
        else if (slArgs[chi].left(7).toUpper() == "ENDCHR=")
        {
            //Sets the end of line character
            if (slArgs[chi].right(1) == "0")
            {
                //CR
                ui->radio_LCR->setChecked(true);
            }
            else if (slArgs[chi].right(1) == "1")
            {
                //LF
                ui->radio_LLF->setChecked(true);
            }
            else if (slArgs[chi].right(1) == "2")
            {
                //CRLF
                ui->radio_LCRLF->setChecked(true);
            }
            else if (slArgs[chi].right(1) == "3")
            {
                //LFCR
                ui->radio_LLFCR->setChecked(true);
            }
        }
        else if (slArgs[chi].left(10).toUpper() == "LOCALECHO=")
        {
            //Enable or disable local echo
            if (slArgs[chi].right(1) == "0")
            {
                //Off
                ui->check_Echo->setChecked(false);
            }
            else if (slArgs[chi].right(1) == "1")
            {
                //On (default)
                ui->check_Echo->setChecked(true);
            }
        }
        else if (slArgs[chi].left(9).toUpper() == "LINEMODE=")
        {
            //Enable or disable line mode
            if (slArgs[chi].right(1) == "0")
            {
                //Off
                ui->check_Line->setChecked(false);
                on_check_Line_stateChanged();
            }
            else if (slArgs[chi].right(1) == "1")
            {
                //On (default)
                ui->check_Line->setChecked(true);
                on_check_Line_stateChanged();
            }
        }
        else if (slArgs[chi].toUpper() == "LOG")
        {
            //Enables logging
            if (bLoggerOpened == true)
            {
                //Clear the file contents
                gpMainLog->ClearLog();

                //Add log opened message
                gpMainLog->WriteLogData(tr("-").repeated(31));
                gpMainLog->WriteLogData(tr("\n Log opened ").append(QDate::currentDate().toString("dd/MM/yyyy")).append(" @ ").append(QTime::currentTime().toString("hh:mm")).append(" \n"));
                gpMainLog->WriteLogData(tr("-").repeated(31).append("\n\n"));
            }
            chLoggerMode = 1;
        }
        else if (slArgs[chi].toUpper() == "LOG+")
        {
            //Enables appending to the previous log file instead of erasing
            chLoggerMode = 2;
        }
        else if (slArgs[chi].left(4).toUpper() == "LOG=" && bLoggerOpened == false)
        {
            //Specifies log filename
            if (chLoggerMode == 1)
            {
                //Clear log file before opening
                QFile::remove(slArgs[chi].mid(4, -1));
            }

            if (gpMainLog->OpenLogFile(slArgs[chi].mid(4, -1)) == LOG_OK)
            {
                //Log opened
                gpMainLog->WriteLogData(tr("-").repeated(31));
                gpMainLog->WriteLogData(tr("\n Log opened ").append(QDate::currentDate().toString("dd/MM/yyyy")).append(" @ ").append(QTime::currentTime().toString("hh:mm")).append(" \n"));
                gpMainLog->WriteLogData(tr("-").repeated(31).append("\n\n"));
                gbMainLogEnabled = true;
            }
            else
            {
                //Log not writeable
                QString strMessage = tr("Error whilst opening log:\nPlease ensure you have access to the log file ").append(slArgs[chi].mid(4, -1)).append(" and have enough free space on your hard drive.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }
            bLoggerOpened = true;
        }
        else if (slArgs[chi].toUpper() == "SHOWCRLF")
        {
            //Displays \t, \r, \n etc. as \t, \r, \n instead of [tab], [new line], [carriage return]
            ui->check_ShowCLRF->setChecked(true);
        }
        else if (slArgs[chi].toUpper() == "NOCONNECT")
        {
            //Connect to device at startup
            bArgNoConnect = true;
        }
        ++chi;
    }

    if (bLoggerOpened == false)
    {
        //Log file was not opened from command line
        if (gpTermSettings->value("LogLevel", DefaultLogLevel).toInt() == 1 || gpTermSettings->value("LogLevel", DefaultLogLevel).toInt() == 2)
        {
            //Logging is enabled
#if TARGET_OS_MAC
            if (gpMainLog->OpenLogFile(QString(gstrMacBundlePath).append(gpTermSettings->value("LogFile").toString())) == LOG_OK)
#else
            if (gpMainLog->OpenLogFile(gpTermSettings->value("LogFile").toString()) == LOG_OK)
#endif
            {
                //Log opened
                if (gpTermSettings->value("LogMode", DefaultLogMode).toBool() == true)
                {
                    //Clear the log file
                    gpMainLog->ClearLog();
                }
                gpMainLog->WriteLogData(tr("-").repeated(31));
                gpMainLog->WriteLogData(tr("\n Log opened ").append(QDate::currentDate().toString("dd/MM/yyyy")).append(" @ ").append(QTime::currentTime().toString("hh:mm")).append(" \n"));
                gpMainLog->WriteLogData(tr("-").repeated(31).append("\n\n"));
                gbMainLogEnabled = true;
            }
            else
            {
                //Log not writeable
                QString strMessage = tr("Error whilst opening log.\nPlease ensure you have access to the log file ").append(gpTermSettings->value("LogFile").toString()).append(" and have enough free space on your hard drive.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }
        }
    }

    //Change terminal font to a monospaced font
    QFont fntTmpFnt = QFontDatabase::systemFont(QFontDatabase::FixedFont);
//      fntTmpFnt.setPointSize(10);
    QFontMetrics tmTmpFM(fntTmpFnt);
    ui->text_TermEditData->setFont(fntTmpFnt);
    ui->text_TermEditData->setTabStopWidth(tmTmpFM.width(" ")*6);

    if (bArgAccept == true && bArgCom == true && bArgNoConnect == false)
    {
        //Enough information to connect!
        OpenSerial();
    }

#ifdef UseSSL
    //Load SSL certificate
    QFile certFile(":/certificates/UwTerminalX.crt");
    if (certFile.open(QIODevice::ReadOnly))
    {
        //Load certificate data
        sslcLairdSSL = new QSslCertificate(certFile.readAll());
        QSslSocket::addDefaultCaCertificate(*sslcLairdSSL);
        certFile.close();
    }
#endif
}

//=============================================================================
//=============================================================================
MainWindow::~MainWindow()
{
    //Disconnect all signals
    disconnect(this, SLOT(process_finished(int, QProcess::ExitStatus)));
    disconnect(this, SLOT(close()));
    disconnect(this, SLOT(EnterPressed()));
    disconnect(this, SLOT(KeyPressed(QChar)));
    disconnect(this, SLOT(DroppedFile(QString)));
    disconnect(this, SLOT(triggered(QAction*)));
    disconnect(this, SLOT(balloontriggered(QAction*)));
    disconnect(this, SLOT(ContextMenuClosed()));
    disconnect(this, SLOT(DevRespTimeout()));
    disconnect(this, SLOT(SerialStatusSlot()));
    disconnect(this, SLOT(readData()));
    disconnect(this, SLOT(SerialError(QSerialPort::SerialPortError)));
    disconnect(this, SLOT(SerialBytesWritten(qint64)));
    disconnect(this, SLOT(UpdateReceiveText()));
    disconnect(this, SLOT(BatchTimeoutSlot()));
    disconnect(this, SLOT(replyFinished(QNetworkReply*)));

    if (gspSerialPort.isOpen() == true)
    {
        //Close serial connection before quitting
        gspSerialPort.close();
        gpSignalTimer->stop();
    }

    if (gbMainLogEnabled == true)
    {
        //Close main log file before quitting
        gpMainLog->CloseLogFile();
        delete gpMainLog;
    }

    //Close popups if open
    if (gpmErrorForm->isVisible())
    {
        //Close warning message
        gpmErrorForm->close();
    }
    if (guaAutomationForm->isVisible())
    {
        //Close automation form
        guaAutomationForm->close();
    }

    //Delete system tray object
    if (gbSysTrayEnabled == true)
    {
        gpSysTray->hide();
        delete gpSysTray;
    }

    //Clear up streaming data if opened
    if (gbTermBusy == true && gbStreamingFile == true)
    {
        gpStreamFileHandle->close();
        delete gpStreamFileHandle;
    }
    else if (gbTermBusy == true && gbStreamingBatch == true)
    {
        //Clear up batch
        gpStreamFileHandle->close();
        delete gpStreamFileHandle;
    }

#ifdef UseSSL
    if (sslcLairdSSL != NULL)
    {
        delete sslcLairdSSL;
    }
#endif

    //Delete variables
    delete gpPredefinedDevice;
    delete gpTermSettings;
    delete gpErrorMessages;
    delete gpSignalTimer;
    delete gpBalloonMenu;
    delete gpSMenu3;
    delete gpSMenu2;
    delete gpSMenu1;
    delete gpMenu;
    delete gpEmptyCirclePixmap;
    delete gpRedCirclePixmap;
    delete gpGreenCirclePixmap;
    delete gpmErrorForm;
    delete guaAutomationForm;
    delete gnmManager;
    delete ui;
}

//=============================================================================
//=============================================================================
void
MainWindow::closeEvent
    (
    QCloseEvent *event
    )
{
    //Runs when the form is closed. Close child popups to exit the application
    if (gpmErrorForm->isVisible())
    {
        //Close warning message form
        gpmErrorForm->close();
    }
    if (guaAutomationForm->isVisible())
    {
        //Close automation form
        guaAutomationForm->close();
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_Accept_clicked
    (
    )
{
    //Runs when accept button is clicked - disable buttons
    ui->btn_Accept->setEnabled(false);
    ui->btn_Decline->setEnabled(false);

    //Switch to config tab
    ui->selector_Tab->setCurrentIndex(1);

    //Default empty images
    ui->image_CTS->setPixmap(*gpEmptyCirclePixmap);
    ui->image_DCD->setPixmap(*gpEmptyCirclePixmap);
    ui->image_DSR->setPixmap(*gpEmptyCirclePixmap);
    ui->image_RI->setPixmap(*gpEmptyCirclePixmap);
}

//=============================================================================
//=============================================================================
void
MainWindow::on_selector_Tab_currentChanged
    (
    int intIndex
    )
{
    if (ui->btn_Accept->isEnabled() == true && intIndex != 3)
    {
        //Not accepted the terms yet
        ui->selector_Tab->setCurrentIndex(3);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_Connect_clicked
    (
    )
{
    //Connect to COM port button clicked.
    OpenSerial();
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_TermClose_clicked
    (
    )
{
    if (ui->btn_TermClose->text() == "&Open Port")
    {
        //Open connection
        OpenSerial();
    }
    else if (ui->btn_TermClose->text() == "C&lose Port")
    {
        //Close, but first clear up from download/streaming
        gbTermBusy = false;
        gchTermMode = 0;
        gchTermMode2 = 0;
        ui->btn_Cancel->setEnabled(false);
        if (gbStreamingFile == true)
        {
            //Clear up file stream
            gtmrStreamTimer.invalidate();
            gbStreamingFile = false;
            gpStreamFileHandle->close();
            delete gpStreamFileHandle;
        }
        else if (gbStreamingBatch == true)
        {
            //Clear up batch
            gtmrStreamTimer.invalidate();
            gtmrBatchTimeoutTimer.stop();
            gbStreamingBatch = false;
            gpStreamFileHandle->close();
            delete gpStreamFileHandle;
            gbaBatchReceive.clear();
        }

        //Close the serial port
        while (gspSerialPort.isOpen() == true)
        {
            gspSerialPort.clear();
            gspSerialPort.close();
        }
        gpSignalTimer->stop();

        //Re-enable inputs
        ui->edit_FWRH->setEnabled(true);

        //Disable active checkboxes
        ui->check_Break->setEnabled(false);
        ui->check_DTR->setEnabled(false);
        ui->check_Echo->setEnabled(false);
        ui->check_Line->setEnabled(false);
        ui->check_RTS->setEnabled(false);

        //Disable text entry
        ui->text_TermEditData->setReadOnly(true);

        //Change status message
        ui->statusBar->showMessage("");

        //Change button text
        ui->btn_TermClose->setText("&Open Port");

        //Notify automation form
        guaAutomationForm->ConnectionChange(false);

        //Disallow file drops
        setAcceptDrops(false);
    }

    //Update images
    UpdateImages();
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_Refresh_clicked
    (
    )
{
    //Refresh the list of serial ports
    RefreshSerialDevices();
}

//=============================================================================
//=============================================================================
void
MainWindow::RefreshSerialDevices
    (
    )
{
    //Clears and refreshes the list of serial devices
    QString strPrev = "";
    QRegularExpression reTempRE("^(\\D*?)(\\d+)$");
    QList<unsigned int> Entries;
    Entries.clear();

    if (ui->combo_COM->count() > 0)
    {
        //Remember previous option
        strPrev = ui->combo_COM->currentText();
    }
    ui->combo_COM->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        QRegularExpressionMatch remTempREM = reTempRE.match(info.portName());
        if (remTempREM.hasMatch() == true)
        {
            //Can sort this item
            int i = Entries.count()-1;
            while (i >= 0)
            {
                if (remTempREM.captured(2).toInt() > Entries[i])
                {
                    //Found correct order position, add here
                    ui->combo_COM->insertItem(i+1, info.portName());
                    Entries.insert(i+1, remTempREM.captured(2).toInt());
                    i = -1;
                }
                --i;
            }

            if (i == -1)
            {
                //Position not found, add to beginning
                ui->combo_COM->insertItem(0, info.portName());
                Entries.insert(0, remTempREM.captured(2).toInt());
            }
        }
        else
        {
            //Cannot sort this item
            ui->combo_COM->insertItem(ui->combo_COM->count(), info.portName());
        }
    }

    //Search for previous item if one was selected
    if (strPrev == "")
    {
        //Select first item
        ui->combo_COM->setCurrentIndex(0);
    }
    else
    {
        //Search for previos
        unsigned int i = 0;
        while (i < ui->combo_COM->count())
        {
            if (ui->combo_COM->itemText(i) == strPrev)
            {
                //Found previous item
                ui->combo_COM->setCurrentIndex(i);
                break;
            }
            ++i;
        }
    }

    //Update serial port info
    on_combo_COM_currentIndexChanged(0);
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_TermClear_clicked
    (
    )
{
    //Clears the screen of the terminal tab
    ui->text_TermEditData->ClearDatIn();
}

//=============================================================================
//=============================================================================
void
MainWindow::readData
    (
    )
{
    //Read the data into a buffer and copy it to edit for the display data
    QByteArray baOrigData = gspSerialPort.readAll();

    if (ui->check_SkipDL->isChecked() == false || (gbTermBusy == false || (gbTermBusy == true && baOrigData.length() > 6) || (gbTermBusy == true && (gchTermMode == MODE_CHECK_ERROR_CODE_VERSIONS || gchTermMode == MODE_CHECK_UWTERMINALX_VERSIONS || gchTermMode == MODE_UPDATE_ERROR_CODE || gchTermMode == MODE_CHECK_FIRMWARE_VERSIONS || gchTermMode == 50))))
    {
        //Update the display with the data
        QByteArray baDispData = baOrigData;

        //Add to log
        gpMainLog->WriteRawLogData(baOrigData);

        if (ui->check_ShowCLRF->isChecked() == true)
        {
            //Escape \t, \r and \n
            baDispData.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
        }

        //Replace unprintable characters
        baDispData.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

        //Update display buffer
        gbaDisplayBuffer.append(baDispData);
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }

        if (gbLoopbackMode == true)
        {
            //Loopback enabled, send this data back
            gspSerialPort.write(baOrigData);
            gintQueuedTXBytes += baOrigData.length();
            gpMainLog->WriteRawLogData(baOrigData);
            gbaDisplayBuffer.append(baDispData);
        }
    }

    //Update number of recieved bytes
    gintRXBytes = gintRXBytes + baOrigData.length();
    ui->label_TermRx->setText(QString::number(gintRXBytes));

    if (gbStreamingBatch == true)
    {
        //Batch stream in progress
        gbaBatchReceive += baOrigData;
        if (gbaBatchReceive.indexOf("\n00\r") != -1)
        {
            //Success code, next statement
            if (gpStreamFileHandle->atEnd())
            {
                //Finished sending
                FinishBatch(false);
            }
            else
            {
                //Send more data
                QByteArray baFileData = gpStreamFileHandle->readLine().replace("\n", "").replace("\r", "");
                gspSerialPort.write(baFileData);
                gintQueuedTXBytes += baFileData.length();
                DoLineEnd();
                gpMainLog->WriteLogData(QString(baFileData).append("\n"));
//        gintStreamBytesRead += FileData.length();
                gtmrBatchTimeoutTimer.start(BatchTimeout);
                ++gintStreamBytesRead;

                //Update the display buffer
                gbaDisplayBuffer.append(baFileData);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
            }
            gbaBatchReceive.clear();
        }
        else if (gbaBatchReceive.indexOf("\n01\t") != -1 && gbaBatchReceive.indexOf("\r", gbaBatchReceive.indexOf("\n01\t")+4) != -1)
        {
            //Failure code
            QRegularExpression reTempRE("\t([a-zA-Z0-9]{1,9})(\t|\r)");
            QRegularExpressionMatch remTempREM = reTempRE.match(gbaBatchReceive);
            if (remTempREM.hasMatch() == true)
            {
                //Got the error code
                gbaDisplayBuffer.append("\nError during batch command, error code: ").append(remTempREM.captured(1)).append("\n");

                //Lookup error code
                bool bTmpBool;
                unsigned int ErrCode = QString("0x").append(remTempREM.captured(1)).toUInt(&bTmpBool, 16);
                if (bTmpBool == true)
                {
                    //Converted
                    LookupErrorCode(ErrCode);
                }
            }
            else
            {
                //Unknown error code
                gbaDisplayBuffer.append("\nError during batch command, unknown error code.\n");
            }
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }

            //Show status message
            ui->statusBar->showMessage(QString("Failed sending batch file at line ").append(QString::number(gintStreamBytesRead)));

            //Clear up and cancel timer
            gtmrBatchTimeoutTimer.stop();
            gbTermBusy = false;
            gbStreamingBatch = false;
            gchTermMode = 0;
            gpStreamFileHandle->close();
            delete gpStreamFileHandle;
            gbaBatchReceive.clear();
            ui->btn_Cancel->setEnabled(false);
        }
    }

    if (gbTermBusy == true && gchTermMode2 == 0)
    {
        //Currently waiting for a response
        gstrTermBusyData = gstrTermBusyData.append(baOrigData);
        gchTermBusyLines = gchTermBusyLines + baOrigData.count("\n");
        if (gchTermBusyLines == 4)
        {
            //Enough data, check it.
            QRegularExpression reTempRE("\n10	0	([a-zA-Z0-9]{2,14})\r\n00\r\n10	13	([a-zA-Z0-9]{4}) ([a-zA-Z0-9]{4}) \r\n00\r");
            QRegularExpressionMatch remTempREM = reTempRE.match(gstrTermBusyData);
            if (remTempREM.hasMatch() == true)
            {
                if (gchTermMode == MODE_SERVER_COMPILE || gchTermMode == MODE_SERVER_COMPILE_LOAD || gchTermMode == MODE_SERVER_COMPILE_LOAD_RUN)
                {
                    if (ui->check_OnlineXComp->isChecked() == true)
                    {
                        //Check if online XCompiler supports this device
                        gnmManager->get(QNetworkRequest(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/supported.php?JSON=1&Dev=").append(remTempREM.captured(1).left(8)).append("&HashA=").append(remTempREM.captured(2)).append("&HashB=").append(remTempREM.captured(3)))));
                    }
                    else
                    {
                        //Online XCompiler not enabled
                        QString strMessage = tr("Unable to XCompile application: Online XCompilation support must be enabled to XCompile applications on Mac/Linux.\nPlease enable it from the 'Config' tab and try again.");
                        gpmErrorForm->show();
                        gpmErrorForm->SetMessage(&strMessage);
                    }
                }
                else
                {
                    //Matched and split, now start the compilation!
                    gtmrDownloadTimeoutTimer.stop();

                    //
                    QList<QString> lstFI = SplitFilePath(gstrTermFilename);
#ifdef _WIN32
                    //Windows
                    if (QFile::exists(QString(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3)).append(".exe")) == true)
#elif TARGET_OS_MAC
                    //Mac
                    if (QFile::exists(QString(gstrMacBundlePath).append(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3))) == true)
#else
                    //Linux
                    if (QFile::exists(QString(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3))) == true)
#endif
                    {
                        //XCompiler found! - First run the Pre XCompile program if enabled and it exists
                        if (ui->check_PreXCompRun->isChecked() == true && ui->radio_XCompPre->isChecked() == true)
                        {
                            //Run Pre-XComp program
                            RunPrePostExecutable(gstrTermFilename);
                        }
#ifdef _WIN32
                        //Windows
                        gprocCompileProcess.start(QString(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3)).append(".exe"), QStringList(gstrTermFilename));
#elif TARGET_OS_MAC
                        //Mac
                        gprocCompileProcess.start(QString(gstrMacBundlePath).append(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3)), QStringList(gstrTermFilename));
                        //gprocCompileProcess.start(QString(gstrMacBundlePath).append(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3), QStringList(gstrTermFilename));
#else
                        //Assume linux
//                      gprocCompileProcess.start(QString("wine"), QStringList(QString(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(rx.cap(1).left(8)).append("_").append(rx.cap(2)).append("_").append(rx.cap(3)).append(".exe"))<<gstrTermFilename);
                        gprocCompileProcess.start(QString(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8).append("/") : "")).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3)), QStringList(gstrTermFilename));
#endif
                        //gprocCompileProcess.waitForFinished(-1);
                    }
#ifdef _WIN32
                    else if (QFile::exists(QString(lstFI[0]).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3)).append(".exe")) == true)
                    {
                        //XCompiler found in directory with sB file
                        if (ui->check_PreXCompRun->isChecked() == true && ui->radio_XCompPre->isChecked() == true)
                        {
                            //Run Pre-XComp program
                            RunPrePostExecutable(gstrTermFilename);
                        }
                        gprocCompileProcess.start(QString(lstFI[0]).append("XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3)).append(".exe"), QStringList(gstrTermFilename));
                    }
#endif
                    else if (ui->check_OnlineXComp->isChecked() == true)
                    {
                        //XCompiler not found, try Online XCompiler
                        gnmManager->get(QNetworkRequest(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/supported.php?JSON=1&Dev=").append(remTempREM.captured(1).left(8)).append("&HashA=").append(remTempREM.captured(2)).append("&HashB=").append(remTempREM.captured(3)))));
                        ui->statusBar->showMessage("Device support request sent...", 2000);

                        if (gchTermMode == MODE_COMPILE)
                        {
                            gchTermMode = MODE_SERVER_COMPILE;
                        }
                        else if (gchTermMode == MODE_COMPILE_LOAD)
                        {
                            gchTermMode = MODE_SERVER_COMPILE_LOAD;
                        }
                        else if (gchTermMode == MODE_COMPILE_LOAD_RUN)
                        {
                            gchTermMode = MODE_SERVER_COMPILE_LOAD_RUN;
                        }
                    }
                    else
                    {
                        //XCompiler not found, Online XCompiler disabled
                        QString strMessage = tr("Error during XCompile:\nXCompiler \"XComp_").append(remTempREM.captured(1).left(8)).append("_").append(remTempREM.captured(2)).append("_").append(remTempREM.captured(3))
#ifdef _WIN32
                        .append(".exe")
#endif
                        .append("\" was not found.\r\n\r\nPlease ensure you put XCompile binaries in the correct directory (").append(gpTermSettings->value("CompilerDir", DefaultCompilerDir).toString()).append((gpTermSettings->value("CompilerSubDirs", DefaultCompilerSubDirs).toBool() == true ? remTempREM.captured(1).left(8) : "")).append(").\n\nYou can also enable Online XCompilation from the 'Config' tab to XCompile applications using Laird's online server.");
#pragma warning("Add full file path for XCompilers?")
                        gpmErrorForm->show();
                        gpmErrorForm->SetMessage(&strMessage);
                        gbTermBusy = false;
                        ui->btn_Cancel->setEnabled(false);
                    }
                }
            }
        }
    }
    else if (gbTermBusy == true && gchTermMode2 > 0 && gchTermMode2 < 20)
    {
        gstrTermBusyData = gstrTermBusyData.append(baOrigData);
        if (gstrTermBusyData.length() > 3)
        {
            if (gchTermMode2 == 1)
            {
                QByteArray baTmpBA = QString("AT+FOW \"").append(gstrDownloadFilename).append("\"").toUtf8();
                gspSerialPort.write(baTmpBA);
                gbFileOpened = true;
                gintQueuedTXBytes += baTmpBA.size();
                DoLineEnd();
                gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
                if (ui->check_SkipDL->isChecked() == false)
                {
                    //Output download details
                    if (ui->check_ShowCLRF->isChecked() == true)
                    {
                        //Escape \t, \r and \n
                        baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
                    }

                    //Replace unprintable characters
                    baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

                    //Update display buffer
                    gbaDisplayBuffer.append(baTmpBA);
                    if (!gtmrTextUpdateTimer.isActive())
                    {
                        gtmrTextUpdateTimer.start();
                    }
                }
                gstrTermBusyData = "";
            }
            else if (gchTermMode2 == 2)
            {
                //Append data to buffer
                gpMainLog->WriteLogData(gstrTermBusyData);
                if (QString::fromUtf8(baOrigData).indexOf("\n01", 0, Qt::CaseSensitive) == -1 && QString::fromUtf8(baOrigData).indexOf("~FAULT", 0, Qt::CaseSensitive) == -1 && gstrTermBusyData.indexOf("00", 0, Qt::CaseSensitive) != -1)
                {
                    //Success
                    gstrTermBusyData = "";
                    if (gstrHexData.length() > 0)
                    {
                        gtmrDownloadTimeoutTimer.stop();
                        QByteArray baTmpBA = QString("AT+FWRH \"").append(gstrHexData.left(ui->edit_FWRH->toPlainText().toInt())).append("\"").toUtf8();
                        gspSerialPort.write(baTmpBA);
                        gintQueuedTXBytes += baTmpBA.size();
                        DoLineEnd();
                        gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
                        if (ui->check_SkipDL->isChecked() == false)
                        {
                            //Output download details
                            if (ui->check_ShowCLRF->isChecked() == true)
                            {
                                //Escape \t, \r and \n
                                baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
                            }

                            //Replace unprintable characters
                            baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

                            //Update display buffer
                            gbaDisplayBuffer.append(baTmpBA);
                            if (!gtmrTextUpdateTimer.isActive())
                            {
                                gtmrTextUpdateTimer.start();
                            }
                        }

                        if (gstrHexData.length() < ui->edit_FWRH->toPlainText().toInt())
                        {
                            //Finished
                            gstrHexData = "";
                            gbaDisplayBuffer.append("\n-- Finished downloading file --\n");
                            gspSerialPort.write("AT+FCL");
                            gintQueuedTXBytes += 6;
                            DoLineEnd();
                            gpMainLog->WriteLogData("AT+FCL\n");
                            if (ui->check_SkipDL->isChecked() == false)
                            {
                                //Output download details
                                gbaDisplayBuffer.append("AT+FCL\n");
                            }
                            if (!gtmrTextUpdateTimer.isActive())
                            {
                                gtmrTextUpdateTimer.start();
                            }
                            QList<QString> lstFI = SplitFilePath(gstrTermFilename);
                            if (gpTermSettings->value("DelUWCAfterDownload", DefaultDelUWCAfterDownload).toBool() == true && gbIsUWCDownload == true && QFile::exists(QString(lstFI[0]).append(lstFI[1]).append(".uwc")))
                            {
                                //Remove UWC
                                QFile::remove(QString(lstFI[0]).append(lstFI[1]).append(".uwc"));
                            }
                        }
                        else
                        {
                            //More data to send
                            gstrHexData = gstrHexData.right(gstrHexData.length()-ui->edit_FWRH->toPlainText().toInt());
                            --gchTermMode2;
                            gtmrDownloadTimeoutTimer.start();
                        }
                        //Update amount of data left to send
                        ui->label_TermTxLeft->setText(QString::number(gstrHexData.length()));
                    }
                    else
                    {
                        gstrHexData = "";
                        gbaDisplayBuffer.append("\n-- Finished downloading file --\n");
                        gspSerialPort.write("AT+FCL");
                        gintQueuedTXBytes += 6;
                        DoLineEnd();
                        gpMainLog->WriteLogData("AT+FCL\n");
                        if (ui->check_SkipDL->isChecked() == false)
                        {
                            //Output download details
                            gbaDisplayBuffer.append("AT+FCL\n");
                        }
                        if (!gtmrTextUpdateTimer.isActive())
                        {
                            gtmrTextUpdateTimer.start();
                        }
                        QList<QString> lstFI = SplitFilePath(gstrTermFilename);
                        if (gpTermSettings->value("DelUWCAfterDownload", DefaultDelUWCAfterDownload).toBool() == true && gbIsUWCDownload == true && QFile::exists(QString(lstFI[0]).append(lstFI[1]).append(".uwc")))
                        {
                            //Remove UWC
                            QFile::remove(QString(lstFI[0]).append(lstFI[1]).append(".uwc"));
                        }
                    }
                }
                else
                {
                    //Presume error
                    gbTermBusy = false;
                    gchTermBusyLines = 0;
                    gchTermMode = 0;
                    gchTermMode2 = 0;
                    QString strMessage = tr("Error whilst downloading data to device. If filesystem is full, please restart device with 'atz' and clear the filesystem using 'at&f 1'.\nPlease note this will erase ALL FILES on the device, configuration keys and all bonding keys.\n\nReceived: ").append(QString::fromUtf8(baOrigData));
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                    QList<QString> lstFI = SplitFilePath(gstrTermFilename);
                    if (gpTermSettings->value("DelUWCAfterDownload", DefaultDelUWCAfterDownload).toBool() == true && gbIsUWCDownload == true && QFile::exists(QString(lstFI[0]).append(lstFI[1]).append(".uwc")))
                    {
                        //Remove UWC
                        QFile::remove(QString(lstFI[0]).append(lstFI[1]).append(".uwc"));
                    }
                    ui->btn_Cancel->setEnabled(false);
                }
            }
            else if (gchTermMode2 == MODE_COMPILE_LOAD_RUN)
            {
                if (gchTermMode == MODE_COMPILE_LOAD_RUN || gchTermMode == MODE_LOAD_RUN)
                {
                    //Run!
                    RunApplication();
                }
            }
            ++gchTermMode2;

            if (gchTermMode2 > gchTermMode)
            {
                //Finished, no longer busy
                gchTermMode = 0;
                gchTermMode2 = 0;
                gbTermBusy = false;
                ui->btn_Cancel->setEnabled(false);
            }
            else if ((gchTermMode == MODE_LOAD || gchTermMode == 6) && gchTermMode2 == MODE_LOAD)
            {
                gchTermMode = 0;
                gchTermMode2 = 0;
                gbTermBusy = false;
                ui->btn_Cancel->setEnabled(false);
            }
        }
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_text_TermEditData_customContextMenuRequested
    (
    const QPoint &pos
    )
{
    //Creates the custom context menu
    gpMenu->popup(ui->text_TermEditData->viewport()->mapToGlobal(pos));
    ui->text_TermEditData->mbContextMenuOpen = true;
}

void
MainWindow::triggered
    (
    QAction* qaAction
    )
{
    //Runs when a menu item is selected
    if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
    {
        //Serial is open, allow xcompile functions
        if (qaAction->text() == "XCompile")
        {
            //Compiles SB applet
#ifdef _WIN32
            CompileApp(MODE_COMPILE);
#else
            CompileApp(MODE_SERVER_COMPILE);
#endif
        }
        else if (qaAction->text() == "XCompile + Load")
        {
            //Compiles and loads a SB applet
#ifdef _WIN32
            CompileApp(MODE_COMPILE_LOAD);
#else
            CompileApp(MODE_SERVER_COMPILE_LOAD);
#endif
        }
        else if (qaAction->text() == "XCompile + Load + Run")
        {
            //Compiles, loads and runs a SB applet
#ifdef _WIN32
            CompileApp(MODE_COMPILE_LOAD_RUN);
#else
            CompileApp(MODE_SERVER_COMPILE_LOAD_RUN);
#endif
        }
        else if (qaAction->text() == "Load" || qaAction->text() == "Load Precompiled BASIC" || qaAction->text() == "Data File +")
        {
            //Just load an application
            CompileApp(MODE_LOAD);
        }
        else if (qaAction->text() == "Load + Run")
        {
            //Load and run an application
            CompileApp(MODE_LOAD_RUN);
        }
    }

    if (qaAction->text() == "Lookup Selected Error-Code (Hex)")
    {
        //Shows a meaning for the error code selected (number in hex)
        bool bTmpBool;
        unsigned int ErrCode = QString("0x").append(ui->text_TermEditData->textCursor().selection().toPlainText()).toUInt(&bTmpBool, 16);
        if (bTmpBool == true)
        {
            //Converted
            LookupErrorCode(ErrCode);
        }
    }
    else if (qaAction->text() == "Lookup Selected Error-Code (Int)")
    {
        //Shows a meaning for the error code selected (number as int)
        LookupErrorCode(ui->text_TermEditData->textCursor().selection().toPlainText().toInt());
    }
    else if (qaAction->text() == "Enable Loopback (Rx->Tx)" || qaAction->text() == "Disable Loopback (Rx->Tx)")
    {
        //Enable/disable loopback mode
        gbLoopbackMode = !gbLoopbackMode;
        if (gbLoopbackMode == true)
        {
            //Enabled
            gbaDisplayBuffer.append("\n[Loopback Enabled]\n");
            gpMenu->actions()[7]->setText("Disable Loopback (Rx->Tx)");
        }
        else
        {
            //Disabled
            gbaDisplayBuffer.append("\n[Loopback Disabled]\n");
            gpMenu->actions()[7]->setText("Enable Loopback (Rx->Tx)");
        }
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }
    }
    else if (qaAction->text() == "Erase File" || qaAction->text() == "Erase File +")
    {
        //Erase file
        if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
        {
            //Not currently busy
            QString strFilename;
            strFilename = QFileDialog::getOpenFileName(this, "Open File", gpTermSettings->value("LastFileDirectory", "").toString(), "SmartBasic Applications (*.uwc);;All Files (*.*)");

            if (strFilename.length() > 1)
            {
                //Set last directory config
                gpTermSettings->setValue("LastFileDirectory", SplitFilePath(strFilename)[0]);

                //Delete file
                if (strFilename.lastIndexOf(".") >= 0)
                {
                    //Get up until the first dot
                    QRegularExpression reTempRE("^(.*)/(.*)$");
                    QRegularExpressionMatch remTempREM = reTempRE.match(strFilename);

                    if (remTempREM.hasMatch() == true)
                    {
                        strFilename = remTempREM.captured(2);
                        if (strFilename.count(".") > 0)
                        {
                            //Strip off after the dot
                            strFilename = strFilename.left(strFilename.indexOf("."));
                        }
                    }
                }
                QByteArray baTmpBA = QString("at+del \"").append(strFilename).append("\"").append((qaAction->text() == "Erase File +" ? " +" : "")).toUtf8();
                gspSerialPort.write(baTmpBA);
                gintQueuedTXBytes += baTmpBA.size();
                DoLineEnd();
                gbaDisplayBuffer.append(baTmpBA);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
                gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
            }
        }
    }
    else if (qaAction->text() == "Dir")
    {
        //Dir
        if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
        {
            //List current directory contents
            gspSerialPort.write("at+dir");
            gintQueuedTXBytes += 6;
            DoLineEnd();
            gbaDisplayBuffer.append("\nat+dir\n");
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
            gpMainLog->WriteLogData("at+dir\n");
        }
    }
    else if (qaAction->text() == "Run")
    {
        //Run an application
        if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
        {
            //Not currently busy
            QString strFilename;
            strFilename = QFileDialog::getOpenFileName(this, "Open File", gpTermSettings->value("LastFileDirectory", "").toString(), "SmartBasic Applications (*.uwc);;All Files (*.*)");

            if (strFilename.length() > 1)
            {
                //Set last directory config
                gpTermSettings->setValue("LastFileDirectory", SplitFilePath(strFilename)[0]);

                //Delete file
                if (strFilename.lastIndexOf(".") >= 0)
                {
                    //Get up until the first dot
                    QRegularExpression reTempRE("^(.*)/(.*)$");
                    QRegularExpressionMatch remTempREM = reTempRE.match(strFilename);
                    if (remTempREM.hasMatch() == true)
                    {
                        //Got a match
                        strFilename = remTempREM.captured(2);
                        if (strFilename.count(".") > 0)
                        {
                            //Strip off after the dot
                            strFilename = strFilename.left(strFilename.indexOf("."));
                        }
                    }
                }
                QByteArray baTmpBA = QString("at+run \"").append(strFilename).append("\"").toUtf8();
                gspSerialPort.write(baTmpBA);
                gintQueuedTXBytes += baTmpBA.size();
                DoLineEnd();
                gbaDisplayBuffer.append(baTmpBA);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
                gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
            }
        }
    }
    else if (qaAction->text() == "Stream File Out")
    {
        //Stream out a file
        if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
        {
            //Not currently busy
            QString strDataFilename = QFileDialog::getOpenFileName(this, tr("Open File To Stream"), gpTermSettings->value("LastFileDirectory", "").toString(), tr("Text Files (*.txt);;All Files (*.*)"));

            if (strDataFilename.length() > 1)
            {
                //Set last directory config
                gpTermSettings->setValue("LastFileDirectory", SplitFilePath(strDataFilename)[0]);

                //File was selected - start streaming it out
                gpStreamFileHandle = new QFile(strDataFilename);

                if (!gpStreamFileHandle->open(QIODevice::ReadOnly))
                {
                    //Unable to open file
                    QString strMessage = tr("Error during file streaming: Access to selected file is denied: ").append(strDataFilename);
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                    return;
                }

                //We're now busy
                gbTermBusy = true;
                gbStreamingFile = true;
                gchTermMode = 50;
                ui->btn_Cancel->setEnabled(true);

                //Save the size of the file
                gintStreamBytesSize = gpStreamFileHandle->size();
                gintStreamBytesRead = 0;
                gintStreamBytesProgress = StreamProgress;

                //Start a timer
                gtmrStreamTimer.start();

                //Reads out each block
                QByteArray baFileData = gpStreamFileHandle->read(FileReadBlock);
                gspSerialPort.write(baFileData);
                gintQueuedTXBytes += baFileData.size();
                gintStreamBytesRead = baFileData.length();
                gpMainLog->WriteLogData(QString(baFileData).append("\n"));
                if (gpStreamFileHandle->atEnd())
                {
                    //Finished sending
                    FinishStream(false);
                }
            }
        }
    }
    else if (qaAction->text() == "Font")
    {
        //Change font
        bool bTmpBool;
        QFont fntTmpFnt = QFontDialog::getFont(&bTmpBool, ui->text_TermEditData->font(), this);
        if (bTmpBool == true)
        {
            //Set font and re-adjust tab spacing
            QFontMetrics tmTmpFM(fntTmpFnt);
            ui->text_TermEditData->setFont(fntTmpFnt);
            ui->text_TermEditData->setTabStopWidth(tmTmpFM.width(" ")*6);
        }
    }
    else if (qaAction->text() == "Run")
    {
        //Runs an application
        if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
        {
            //Not currently busy
            QString strFilename;
            strFilename = QFileDialog::getOpenFileName(this, "Open File", gpTermSettings->value("LastFileDirectory", "").toString(), "SmartBasic Applications (*.uwc);;All Files (*.*)");

            if (strFilename.length() > 1)
            {
                //Set last directory config
                gpTermSettings->setValue("LastFileDirectory", SplitFilePath(strFilename)[0]);

                //Run file
                if (strFilename.lastIndexOf(".") >= 0)
                {
                    //Get up until the first dot
                    QRegularExpression reTmpRE("^(.*)/(.*)$");
                    QRegularExpressionMatch remTempREM = reTmpRE.match(strFilename);

                    if (remTempREM.hasMatch() == true)
                    {
                        strFilename = remTempREM.captured(2);
                        if (strFilename.count(".") > 0)
                        {
                            //Strip off after the dot
                            strFilename = strFilename.left(strFilename.indexOf("."));
                        }
                    }
                }
                QByteArray baTmpBA = QString("at+run \"").append(strFilename).append("\"").toUtf8();
                gspSerialPort.write(baTmpBA);
                gintQueuedTXBytes += baTmpBA.size();
                gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
                DoLineEnd();

                //Update display buffer
                gbaDisplayBuffer.append(baTmpBA);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
            }
        }
    }
    else if (qaAction->text() == "Automation")
    {
        //Show automation window
        guaAutomationForm->show();
    }
    else if (qaAction->text() == "Batch")
    {
        //Start a Batch file script
        if (gspSerialPort.isOpen() == true && gbLoopbackMode == false && gbTermBusy == false)
        {
            //Not currently busy
            QString strDataFilename = QFileDialog::getOpenFileName(this, tr("Open Batch File"), gpTermSettings->value("LastFileDirectory", "").toString(), tr("Text Files (*.txt);;All Files (*.*)"));
            if (strDataFilename.length() > 1)
            {
                //Set last directory config
                gpTermSettings->setValue("LastFileDirectory", SplitFilePath(strDataFilename)[0]);

                //File selected
                gpStreamFileHandle = new QFile(strDataFilename);

                if (!gpStreamFileHandle->open(QIODevice::ReadOnly))
                {
                    //Unable to open file
                    QString strMessage = tr("Error during batch streaming: Access to selected file is denied: ").append(strDataFilename);
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                    return;
                }

                //We're now busy
                gbTermBusy = true;
                gbStreamingBatch = true;
                gchTermMode = 50;
                gbaBatchReceive.clear();
                ui->btn_Cancel->setEnabled(true);

                //Start a timer
                gtmrStreamTimer.start();

                //Reads out first block
                QByteArray baFileData = gpStreamFileHandle->readLine().replace("\n", "").replace("\r", "");
                gspSerialPort.write(baFileData);
                gintQueuedTXBytes += baFileData.size();
                DoLineEnd();
                gpMainLog->WriteLogData(QString(baFileData).append("\n"));
                gintStreamBytesRead = 1;

                //Update the display buffer
                gbaDisplayBuffer.append(baFileData);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }

                //Start a timeout timer
                gtmrBatchTimeoutTimer.start(BatchTimeout);
            }
        }
    }
    else if (qaAction->text() == "Clear Display")
    {
        //Clear display
        ui->text_TermEditData->ClearDatIn();
    }
    else if (qaAction->text() == "Clear RX/TX count")
    {
        //Clear counts
        gintRXBytes = 0;
        gintTXBytes = 0;
        ui->label_TermRx->setText(QString::number(gintRXBytes));
        ui->label_TermTx->setText(QString::number(gintTXBytes));
    }
    else if (qaAction->text() == "Copy")
    {
        //Copy selected data
        QApplication::clipboard()->setText(ui->text_TermEditData->textCursor().selection().toPlainText());
    }
    else if (qaAction->text() == "Copy All")
    {
        //Copy all data
        QApplication::clipboard()->setText(ui->text_TermEditData->toPlainText());
    }
    else if (qaAction->text() == "Paste")
    {
        //Paste data from clipboard
        ui->text_TermEditData->AddDatOutText(QApplication::clipboard()->text());
    }
    else if (qaAction->text() == "Select All")
    {
        //Select all text
        ui->text_TermEditData->selectAll();
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::balloontriggered
    (
    QAction* qaAction
    )
{
    //Runs when a balloon menu item is selected
    if (qaAction->text() == "Show UwTerminalX")
    {
        //Make active window
        this->raise();
        this->activateWindow();
    }
    else if (qaAction->text() == "Exit")
    {
        //Exit
        QApplication::quit();
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::EnterPressed
    (
    )
{
    //Enter pressed in line mode
    if (gspSerialPort.isOpen() == true && gbTermBusy == false && gbLoopbackMode == false)
    {
        QByteArray baTmpBA = ui->text_TermEditData->GetDatOut()->replace("\n\r", "\n").replace("\r\n", "\n").replace("\n", (ui->radio_LCR->isChecked() ? "\r" : ui->radio_LLF->isChecked() ? "\n" : ui->radio_LCRLF->isChecked() ? "\r\n" : ui->radio_LLFCR->isChecked() ? "\n\r" : "")).replace("\r", (ui->radio_LCR->isChecked() ? "\r" : ui->radio_LLF->isChecked() ? "\n" : ui->radio_LCRLF->isChecked() ? "\r\n" : ui->radio_LLFCR->isChecked() ? "\n\r" : "")).toUtf8();
        gspSerialPort.write(baTmpBA);
        gintQueuedTXBytes += baTmpBA.size();
        /*if (ui->check_Echo->isChecked() == true)
        {
            baTmpBA = ui->text_TermEditData->toPlainText().toUtf8();
            if (ui->check_ShowCLRF->isChecked() == true)
            {
                //Escape \t, \r and \n
                baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
            }
            baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");
            gbaDisplayBuffer.append(baTmpBA.append("\n"));

            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
        }*/
        //gpMainLog->WriteLogData(ui->text_TermEditData->GetDatOut().append("\n"));
        //ui->text_TermEditData->setPlainText("");
        DoLineEnd();
    }
    else if (gspSerialPort.isOpen() == true && gbLoopbackMode == true)
    {
        //Loopback is enabled
        gbaDisplayBuffer.append("\n[Cannot send: Loopback mode is enabled.]\n");
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }
    }

    if (ui->check_Echo->isChecked() == true)
    {
        //Local echo
        QByteArray baTmpBA = ui->text_TermEditData->GetDatOut()->toUtf8();
        baTmpBA.append("\n");
        ui->text_TermEditData->AddDatInText(&baTmpBA);
    }
    ui->text_TermEditData->ClearDatOut();
}

//=============================================================================
//=============================================================================
unsigned char
MainWindow::CompileApp
    (
    unsigned char chMode
    )
{
    //Runs when an application is to be compiled
    gchTermMode = chMode;
    gstrTermFilename = QFileDialog::getOpenFileName(this, (chMode == 6 || chMode == 7 ? tr("Open File") : (chMode == MODE_LOAD || chMode == MODE_LOAD_RUN ? tr("Open SmartBasic Application") : tr("Open SmartBasic Source"))), gpTermSettings->value("LastFileDirectory", "").toString(), (chMode == 6 || chMode == 7 ? tr("All Files (*.*)") : (chMode == MODE_LOAD || chMode == MODE_LOAD_RUN ? tr("SmartBasic Applications (*.uwc);;All Files (*.*)") : tr("Text/SmartBasic Files (*.txt *.sb);;All Files (*.*)"))));
    if (gstrTermFilename != "")
    {
        //Set last directory config
        gpTermSettings->setValue("LastFileDirectory", SplitFilePath(gstrTermFilename)[0]);

        if (chMode == MODE_LOAD || chMode == MODE_LOAD_RUN)
        {
            //Loading a compiled application
            gbTermBusy = true;
            LoadFile(false);

            //Download to the device
            gchTermMode2 = MODE_COMPILE;
            QByteArray baTmpBA = QString("AT+DEL \"").append(gstrDownloadFilename).append("\" +").toUtf8();
            gspSerialPort.write(baTmpBA);
            gintQueuedTXBytes += baTmpBA.size();
            DoLineEnd();
            gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
            if (ui->check_SkipDL->isChecked() == false)
            {
                //Output download details
                if (ui->check_ShowCLRF->isChecked() == true)
                {
                    //Escape \t, \r and \n
                    baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
                }

                //Replace unprintable characters
                baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

                //Update display buffer
                gbaDisplayBuffer.append(baTmpBA);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
            }

            //Start the timeout timer
            gtmrDownloadTimeoutTimer.start();
        }
        else if (chMode == 6 || chMode == 7)
        {
            //Download any file to device
            gbTermBusy = true;
            LoadFile(false);

            //Download to the device
            gchTermMode2 = MODE_COMPILE;
            QByteArray baTmpBA = QString("AT+DEL \"").append(gstrDownloadFilename).append("\" +").toUtf8();
            gspSerialPort.write(baTmpBA);
            gintQueuedTXBytes += baTmpBA.size();
            DoLineEnd();
            gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
            if (ui->check_SkipDL->isChecked() == false)
            {
                //Output download details
                if (ui->check_ShowCLRF->isChecked() == true)
                {
                    //Escape \t, \r and \n
                    baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
                }

                //Replace unprintable characters
                baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

                //Update display buffer
                gbaDisplayBuffer.append(baTmpBA);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
            }

            //Start the timeout timer
            gtmrDownloadTimeoutTimer.start();
        }
        else
        {
            //A file was selected, get the version number
            gbTermBusy = true;
            gchTermMode2 = 0;
            gchTermBusyLines = 0;
            gstrTermBusyData = tr("");
            gspSerialPort.write("at i 0");
            gintQueuedTXBytes += 6;
            DoLineEnd();
            gpMainLog->WriteLogData("at i 0\n");
            gspSerialPort.write("at i 13");
            gintQueuedTXBytes += 7;
            DoLineEnd();
            gpMainLog->WriteLogData("at i 13\n");

            //Start the timeout timer
            gtmrDownloadTimeoutTimer.start();
        }
    }
    ui->btn_Cancel->setEnabled(true);
    return 0;
}

//=============================================================================
//=============================================================================
void
MainWindow::UpdateImages
    (
    )
{
    //Updates images to reflect status
    if (gspSerialPort.isOpen() == true)
    {
        //Port open
        SerialStatus(true);
    }
    else
    {
        //Port closed
        ui->image_CTS->setPixmap(*gpEmptyCirclePixmap);
        ui->image_DCD->setPixmap(*gpEmptyCirclePixmap);
        ui->image_DSR->setPixmap(*gpEmptyCirclePixmap);
        ui->image_RI->setPixmap(*gpEmptyCirclePixmap);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::KeyPressed
    (
    QChar intKeyValue
    )
{
    //Key pressed, send it out
    if (gbTermBusy == false && gbLoopbackMode == false)
    {
        if (ui->check_Echo->isChecked() == true)
        {
            //Echo mode on
            if (intKeyValue == Qt::Key_Enter || intKeyValue == Qt::Key_Return)
            {
                gbaDisplayBuffer.append("\n");
            }
            else
            {
                /*if (ui->check_ShowCLRF->isChecked() == true)
                {
                    //Escape \t, \r and \n in addition to normal escaping
                    gbaDisplayBuffer.append(QString(QChar(intKeyValue)).toUtf8().replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n").replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f"));
                }
                else
                {
                    //Normal escaping
                    gbaDisplayBuffer.append(QString(QChar(intKeyValue)).toUtf8().replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f"));
                }*/
            }
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
        }

        //Convert character to a byte array (in case it's UTF-8 and more than 1 byte)
        QByteArray baTmpBA = QString(intKeyValue).toUtf8();

        //Character mode, send right on
        if (intKeyValue == Qt::Key_Enter || intKeyValue == Qt::Key_Return || intKeyValue == '\r' || intKeyValue == '\n')
        {
            //Return key or newline
            gpMainLog->WriteLogData("\n");
            DoLineEnd();
        }
        else
        {
            //Not return
            gspSerialPort.write(baTmpBA);
            gintQueuedTXBytes += baTmpBA.size();
        }

        //Output back to screen buffer if echo mode is enabled
        if (ui->check_Echo->isChecked())
        {
            if (ui->check_ShowCLRF->isChecked() == true)
            {
                //Escape \t, \r and \n in addition to normal escaping
                gbaDisplayBuffer.append(baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n").replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f"));
            }
            else
            {
                //Normal escaping
                gbaDisplayBuffer.append(baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f"));
            }

            //Run display update timer
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }

            //Output to log file
            gpMainLog->WriteLogData(QString(intKeyValue).toUtf8());
        }
    }
    else if (gbLoopbackMode == true)
    {
        //Loopback is enabled
        gbaDisplayBuffer.append("[Cannot send: Loopback mode is enabled.]");
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::DoLineEnd
    (
    )
{
    //Outputs a line ending
    if (ui->radio_LCR->isChecked())
    {
        //CR
        gspSerialPort.write("\r");
        gintQueuedTXBytes += 1;
    }
    else if (ui->radio_LLF->isChecked())
    {
        //LF
        gspSerialPort.write("\n");
        gintQueuedTXBytes += 1;
    }
    else if (ui->radio_LCRLF->isChecked())
    {
        //CR LF
        gspSerialPort.write("\r\n");
        gintQueuedTXBytes += 2;
    }
    else if (ui->radio_LLFCR->isChecked())
    {
        //LF CR
        gspSerialPort.write("\n\r");
        gintQueuedTXBytes += 2;
    }
    return;
}

//=============================================================================
//=============================================================================
void
MainWindow::DevRespTimeout
    (
    )
{
    //Runs to indiciate a device timeout when waiting to compile an application
    if (gbTermBusy == true)
    {
        //Update buffer
        gbaDisplayBuffer.append("\nTimeout occured whilst attempting to XCompile application or download to module.\n");
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }

        //Reset variables
        gbTermBusy = false;
        gchTermBusyLines = 0;
        gstrTermBusyData = tr("");
        ui->btn_Cancel->setEnabled(false);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::process_finished
    (
    int intExitCode,
    QProcess::ExitStatus esExitStatus
    )
{
    if (ui->check_PreXCompRun->isChecked() == true && ui->radio_XCompPost->isChecked() == true && (intExitCode == 0 || ui->check_PreXCompFail->isChecked() == true))
    {
        //Run Post-XComp program
        RunPrePostExecutable(gstrTermFilename);
    }

    //Exit code 1: Fail, Exit code 0: Success
    if (intExitCode == 1)
    {
        //Display an error message
        QString strMessage = tr("Error during XCompile:\n").append(gprocCompileProcess.readAllStandardOutput());
        gpmErrorForm->show();
        gpmErrorForm->SetMessage(&strMessage);
        gbTermBusy = false;
        ui->btn_Cancel->setEnabled(false);
    }
    else if (intExitCode == 0)
    {
        //Success
        if (gchTermMode == MODE_COMPILE)
        {
            //XCompile complete
            gtmrDownloadTimeoutTimer.stop();
            gstrHexData = "";
            gbaDisplayBuffer.append("\n-- XCompile complete --\n");
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
            gchTermMode = 0;
            gchTermMode2 = 0;
            gbTermBusy = false;
            ui->btn_Cancel->setEnabled(false);
        }
        else if (gchTermMode == MODE_COMPILE_LOAD || gchTermMode == MODE_COMPILE_LOAD_RUN)
        {
            //Load the file
            LoadFile(true);
            gchTermMode2 = MODE_COMPILE;

            //Download to the device
            QByteArray baTmpBA = QString("AT+DEL \"").append(gstrDownloadFilename).append("\" +").toUtf8();
            gspSerialPort.write(baTmpBA);
            gintQueuedTXBytes += baTmpBA.size();
            DoLineEnd();
            gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
            gtmrDownloadTimeoutTimer.start();
            if (ui->check_SkipDL->isChecked() == false)
            {
                //Output download details
                if (ui->check_ShowCLRF->isChecked() == true)
                {
                    //Escape \t, \r and \n
                    baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
                }

                //Replace unprintable characters
                baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

                //Update display buffer
                gbaDisplayBuffer.append(baTmpBA);
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
            }
        }
    }
    else
    {
        //Unknown exit reason
        QString strMessage = tr("Err code: ").append(QString::number(intExitCode));
        gpmErrorForm->show();
        gpmErrorForm->SetMessage(&strMessage);
        gbTermBusy = false;
        ui->btn_Cancel->setEnabled(false);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::SerialStatus
    (
    bool bType
    )
{
    if (gspSerialPort.isOpen() == true)
    {
        if ((gspSerialPort.pinoutSignals() & QSerialPort::ClearToSendSignal) != gbCTSStatus || bType == true)
        {
            //CTS changed
            gbCTSStatus = (gspSerialPort.pinoutSignals() & QSerialPort::ClearToSendSignal);
            ui->image_CTS->setPixmap((gbCTSStatus == true ? *gpGreenCirclePixmap : *gpRedCirclePixmap));
        }
        if ((gspSerialPort.pinoutSignals() & QSerialPort::DataCarrierDetectSignal) != gbDCDStatus || bType == true)
        {
            //DCD changed
            gbDCDStatus = (gspSerialPort.pinoutSignals() & QSerialPort::DataCarrierDetectSignal);
            ui->image_DCD->setPixmap((gbDCDStatus == true ? *gpGreenCirclePixmap : *gpRedCirclePixmap));
        }
        if ((gspSerialPort.pinoutSignals() & QSerialPort::DataSetReadySignal) != gbDSRStatus || bType == true)
        {
            //DSR changed
            gbDSRStatus = (gspSerialPort.pinoutSignals() & QSerialPort::DataSetReadySignal);
            ui->image_DSR->setPixmap((gbDSRStatus == true ? *gpGreenCirclePixmap : *gpRedCirclePixmap));
        }
        if ((gspSerialPort.pinoutSignals() & QSerialPort::RingIndicatorSignal) != gbRIStatus || bType == true)
        {
            //RI changed
            gbRIStatus = (gspSerialPort.pinoutSignals() & QSerialPort::RingIndicatorSignal);
            ui->image_RI->setPixmap((gbRIStatus == true ? *gpGreenCirclePixmap : *gpRedCirclePixmap));
        }
    }
    return;
}

//=============================================================================
//=============================================================================
void
MainWindow::SerialStatusSlot
    (
    )
{
    //Slot function to update serial pinout status
    SerialStatus(0);
}

//=============================================================================
//=============================================================================
void
MainWindow::OpenSerial
    (
    )
{
    //Function to open serial port
    if (gspSerialPort.isOpen() == true)
    {
        //Serial port is already open - cancel any pending operations
        if (gbTermBusy == true && gbFileOpened == true)
        {
            //Run cancel operation
            on_btn_Cancel_clicked();
        }

        //Close serial port
        while (gspSerialPort.isOpen() == true)
        {
            gspSerialPort.clear();
            gspSerialPort.close();
        }
        gpSignalTimer->stop();

        //Change status message
        ui->statusBar->showMessage("");

        //Notify automation form
        guaAutomationForm->ConnectionChange(false);

        //Update images
        UpdateImages();
    }

    gspSerialPort.setPortName(ui->combo_COM->currentText());
    gspSerialPort.setBaudRate(ui->combo_Baud->currentText().toInt());
    gspSerialPort.setDataBits((QSerialPort::DataBits)ui->combo_Data->currentText().toInt());
    gspSerialPort.setStopBits((QSerialPort::StopBits)ui->combo_Stop->currentText().toInt());

    //Parity
    gspSerialPort.setParity((ui->combo_Parity->currentIndex() == 1 ? QSerialPort::OddParity : (ui->combo_Parity->currentIndex() == 2 ? QSerialPort::EvenParity : QSerialPort::NoParity)));

    //Flow control
    gspSerialPort.setFlowControl((ui->combo_Handshake->currentIndex() == 1 ? QSerialPort::HardwareControl : (ui->combo_Handshake->currentIndex() == 2 ? QSerialPort::SoftwareControl : QSerialPort::NoFlowControl)));

    if (gspSerialPort.open(QIODevice::ReadWrite))
    {
        //Successful
        ui->statusBar->showMessage(QString("[").append(ui->combo_COM->currentText()).append(":").append(ui->combo_Baud->currentText()).append(",").append((ui->combo_Parity->currentIndex() == 0 ? "N" : ui->combo_Parity->currentIndex() == 1 ? "O" : ui->combo_Parity->currentIndex() == 2 ? "E" : "")).append(",").append(ui->combo_Data->currentText()).append(",").append(ui->combo_Stop->currentText()).append(",").append((ui->combo_Handshake->currentIndex() == 0 ? "N" : ui->combo_Handshake->currentIndex() == 1 ? "S" : ui->combo_Handshake->currentIndex() == 2 ? "H" : "")).append("]{").append((ui->radio_LCR->isChecked() ? "cr" : (ui->radio_LLF->isChecked() ? "lf" : (ui->radio_LCRLF->isChecked() ? "cr lf" : (ui->radio_LLFCR->isChecked() ? "lf cr" : ""))))).append("}"));
        ui->label_TermConn->setText(ui->statusBar->currentMessage());

        //Switch to Terminal tab
        ui->selector_Tab->setCurrentIndex(0);

        //Disable read-only mode
        ui->text_TermEditData->setReadOnly(false);

        //DTR
        gspSerialPort.setDataTerminalReady(ui->check_DTR->isChecked());

        //Flow control
        if (ui->combo_Handshake->currentIndex() != 1)
        {
            //Not hardware handshaking - RTS
            ui->check_RTS->setEnabled(true);
            gspSerialPort.setRequestToSend(ui->check_RTS->isChecked());
        }

        //Break
        gspSerialPort.setBreakEnabled(ui->check_Break->isChecked());

        //Enable checkboxes
        ui->check_Break->setEnabled(true);
        ui->check_DTR->setEnabled(true);
        ui->check_Echo->setEnabled(true);
        ui->check_Line->setEnabled(true);

        //Update button text
        ui->btn_TermClose->setText("C&lose Port");

        //Signal checking
        SerialStatus(1);

        //Enable timer
        gpSignalTimer->start(gpTermSettings->value("SerialSignalCheckInterval", DefaultSerialSignalCheckInterval).toUInt());

        //Notify automation form
        guaAutomationForm->ConnectionChange(true);

        //Notify scroll edit
        ui->text_TermEditData->SetSerialOpen(true);

        //Set focus to input text edit
        ui->text_TermEditData->setFocus();

        //Allow file drops for uploads
        setAcceptDrops(true);
    }
    else
    {
        //Error whilst opening
        ui->statusBar->showMessage("Error: ");
        ui->statusBar->showMessage(ui->statusBar->currentMessage().append(gspSerialPort.errorString()));

        QString strMessage = tr("Error whilst attempting to open the serial device: ").append(gspSerialPort.errorString()).append("\n\nIf the serial port is open in another application, please close the other application")
#if !defined(_WIN32) && !defined( __APPLE__)
        .append(", please also ensure you have been granted permission to the serial device in /dev/")
#endif
        .append(" and try again.");
        ;
        gpmErrorForm->show();
        gpmErrorForm->SetMessage(&strMessage);
        ui->text_TermEditData->SetSerialOpen(false);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::LoadFile
    (
    bool bToUWC
    )
{
    //Load
    QList<QString> lstFI = SplitFilePath(gstrTermFilename);
    QFile file((bToUWC == true ? QString(lstFI[0]).append(lstFI[1]).append(".uwc") : gstrTermFilename));
    if (!file.open(QIODevice::ReadOnly))
    {
        //Unable to open file
        QString strMessage = tr("Error during XCompile: Access to selected file is denied: ").append((bToUWC ? QString(lstFI[0]).append(lstFI[1]).append(".uwc") : gstrTermFilename));
        gpmErrorForm->show();
        gpmErrorForm->SetMessage(&strMessage);
        gbTermBusy = false;
        ui->btn_Cancel->setEnabled(false);
        return;
    }

    //Is this a UWC download?
    gbIsUWCDownload = bToUWC;

    //Create a data stream and hex string holder
    QDataStream in(&file);
    gstrHexData = "";
    while (!in.atEnd())
    {
        //One byte at a time, convert the data to hex
        quint8 ThisByte;
        in >> ThisByte;
        QString strThisHex;
        strThisHex.setNum(ThisByte, 16);
        if (strThisHex.length() == 1)
        {
            //Expand to 2 characters
            gstrHexData.append("0");
        }

        //Add the hex character to the string
        gstrHexData.append(strThisHex.toUpper());
    }

    //Close the file handle
    file.close();

//TODO: FIX THIS! Get the last slash and check for dot after
    //Calculate the filename
    if (gstrTermFilename.lastIndexOf(".") >= 0)
    {
        //Get up until the first dot
        QRegularExpression reTempRE("^(.*)/(.*)$");
        QRegularExpressionMatch remTempREM = reTempRE.match(gstrTermFilename);
        if (remTempREM.hasMatch() == true)
        {
            //Got a match
            gstrDownloadFilename = remTempREM.captured(2);
            if (gstrDownloadFilename.count(".") > 0)
            {
                //Strip off after the dot
                gstrDownloadFilename = gstrDownloadFilename.left(gstrDownloadFilename.indexOf("."));
            }
        }
    }
    else
    {
        //Use the whole name
        gstrDownloadFilename = gstrTermFilename;
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::RunApplication
    (
    )
{
    //Runs an application
    QByteArray baTmpBA = QString("AT+RUN \"").append(gstrDownloadFilename).append("\"").toUtf8();
    gspSerialPort.write(baTmpBA);
    gintQueuedTXBytes += baTmpBA.size();
    DoLineEnd();
    if (ui->check_SkipDL->isChecked() == false)
    {
        //Output download details
        if (ui->check_ShowCLRF->isChecked() == true)
        {
            //Escape \t, \r and \n
            baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
        }

        //Replace unprintable characters
        baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

        //Update display buffer
        gbaDisplayBuffer.append(baTmpBA);
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }
    }

    if (gchTermMode == MODE_COMPILE_LOAD_RUN || gchTermMode == MODE_LOAD_RUN)
    {
        //Finished
        gbTermBusy = false;
        ui->btn_Cancel->setEnabled(false);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_Break_stateChanged
    (
    )
{
    //Break status changed
    gspSerialPort.setBreakEnabled(ui->check_Break->isChecked());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_RTS_stateChanged
    (
    )
{
    //RTS status changed
    gspSerialPort.setRequestToSend(ui->check_RTS->isChecked());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_DTR_stateChanged
    (
    )
{
    //DTR status changed
    gspSerialPort.setDataTerminalReady(ui->check_DTR->isChecked());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_Line_stateChanged
    (
    )
{
    //Line mode status changed
    ui->text_TermEditData->SetLineMode(ui->check_Line->isChecked());
}

//=============================================================================
//=============================================================================
void
MainWindow::SerialError
    (
    QSerialPort::SerialPortError speErrorCode
    )
{
    if (speErrorCode == QSerialPort::NoError)
    {
        //No error. Why this is ever emitted is a mystery to me.
        return;
    }
    else if (speErrorCode == QSerialPort::ParityError)
    {
        //Parity error
    }
    else if (speErrorCode == QSerialPort::FramingError)
    {
        //Framing error
    }
    else if (speErrorCode == QSerialPort::ResourceError || speErrorCode == QSerialPort::PermissionError)
    {
        //Resource error or permission error (device unplugged?)
        QString strMessage = tr("Fatal error with serial connection.\nPlease reconnect to the device to continue.");
        gpmErrorForm->show();
        gpmErrorForm->SetMessage(&strMessage);
        ui->text_TermEditData->SetSerialOpen(false);

        //Disable timer
        gpSignalTimer->stop();

        if (gspSerialPort.isOpen() == true)
        {
            //Close active connection
            gspSerialPort.close();
        }

        if (gbStreamingFile == true)
        {
            //Clear up file stream
            gtmrStreamTimer.invalidate();
            gbStreamingFile = false;
            gpStreamFileHandle->close();
            delete gpStreamFileHandle;
        }
        else if (gbStreamingBatch == true)
        {
            //Clear up batch
            gtmrStreamTimer.invalidate();
            gtmrBatchTimeoutTimer.stop();
            gbStreamingBatch = false;
            gpStreamFileHandle->close();
            delete gpStreamFileHandle;
            gbaBatchReceive.clear();
        }

        //No longer busy
        gbTermBusy = false;
        gchTermMode = 0;
        gchTermMode2 = 0;

        //Disable cancel button
        ui->btn_Cancel->setEnabled(false);

        //Disable active checkboxes
        ui->check_Break->setEnabled(false);
        ui->check_DTR->setEnabled(false);
        ui->check_Echo->setEnabled(false);
        ui->check_Line->setEnabled(false);
        ui->check_RTS->setEnabled(false);

        //Disable text entry
        ui->text_TermEditData->setReadOnly(true);

        //Change status message
        ui->statusBar->showMessage("");

        //Change button text
        ui->btn_TermClose->setText("&Open Port");

        //Update images
        UpdateImages();

        //Notify automation form
        guaAutomationForm->ConnectionChange(false);

        //Show disconnection balloon
        if (gbSysTrayEnabled == true && !this->isActiveWindow() && !gpmErrorForm->isActiveWindow() && !guaAutomationForm->isActiveWindow())
        {
            gpSysTray->showMessage(ui->combo_COM->currentText().append(" Removed"), QString("Connection to device ").append(ui->combo_COM->currentText()).append(" has been lost due to disconnection."), QSystemTrayIcon::Critical);
        }

        //Disallow file drops
        setAcceptDrops(false);
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_Duplicate_clicked
    (
    )
{
    //Duplicates instance of UwTerminalX
    QProcess DuplicateProcess;
    DuplicateProcess.startDetached(QCoreApplication::applicationFilePath(), QStringList() << "ACCEPT" << tr("COM=").append(ui->combo_COM->currentText()) << tr("BAUD=").append(ui->combo_Baud->currentText()) << tr("STOP=").append(ui->combo_Stop->currentText()) << tr("DATA=").append(ui->combo_Data->currentText()) << tr("PAR=").append(ui->combo_Parity->currentText()) << tr("FLOW=").append(QString::number(ui->combo_Handshake->currentIndex())) << tr("ENDCHR=").append((ui->radio_LCR->isChecked() == true ? "0" : ui->radio_LLF->isChecked() == true ? "1" : ui->radio_LCRLF->isChecked() == true ? "2" : "3")) << tr("LOCALECHO=").append((ui->check_Echo->isChecked() == true ? "1" : "0")) << tr("LINEMODE=").append((ui->check_Line->isChecked() == true ? "1" : "0")) << "NOCONNECT");
}

//=============================================================================
//=============================================================================
void
MainWindow::MessagePass
    (
    QString strDataString,
    bool bEscapeString
    )
{
    //Receive a command from the automation window
    if (gspSerialPort.isOpen() == true && gbTermBusy == false && gbLoopbackMode == false)
    {
        if (bEscapeString == true)
        {
            //Escape string sequences - This could probably be optomised or done a better way
            QRegularExpression reESeq("[\\\\]([0-9A-Fa-f]{2})");
            QRegularExpressionMatchIterator remiESeqMatch = reESeq.globalMatch(strDataString);
            while (remiESeqMatch.hasNext())
            {
                //We have escape sequences
                bool bSuccess;
                QRegularExpressionMatch remThisESeqMatch = remiESeqMatch.next();
                strDataString.replace(QString("\\").append(remThisESeqMatch.captured(1)), QString((char)remThisESeqMatch.captured(1).toInt(&bSuccess, 16)));
            }
        }
        QByteArray baTmpBA = strDataString.toUtf8();
        gspSerialPort.write(baTmpBA);
        gintQueuedTXBytes += baTmpBA.size();
        if (ui->check_Echo->isChecked() == true)
        {
            if (ui->check_ShowCLRF->isChecked() == true)
            {
                //Escape \t, \r and \n
                baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
            }

            //Replace unprintable characters
            baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

            //Output to display buffer
            gbaDisplayBuffer.append(baTmpBA);
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
        }
        gpMainLog->WriteLogData(strDataString);
        if (bEscapeString == false)
        {
            //Not escaping sequences so send line end
            DoLineEnd();
        }
    }
    else if (gspSerialPort.isOpen() == true && gbLoopbackMode == true)
    {
        //Loopback is enabled
        gbaDisplayBuffer.append("\n[Cannot send: Loopback mode is enabled.]\n");
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::LookupErrorCode
    (
    unsigned int intErrorCode
    )
{
    //Looks up an error code and outputs it in the edit (does NOT store it to the log)
    gbaDisplayBuffer.append(QString("\nError code 0x").append(QString::number(intErrorCode, 16)).append(": ").append(gpErrorMessages->value(QString::number(intErrorCode), "Undefined Error Code").toString()).append("\n"));
    if (!gtmrTextUpdateTimer.isActive())
    {
        gtmrTextUpdateTimer.start();
    }
    ui->text_TermEditData->moveCursor(QTextCursor::End);
}

//=============================================================================
//=============================================================================
void
MainWindow::SerialBytesWritten
    (
    qint64 intByteCount
    )
{
    //Updates the display with the number of bytes written
    gintTXBytes += intByteCount;
    ui->label_TermTx->setText(QString::number(gintTXBytes));
//    ui->label_TermQueue->setText(QString::number(gintQueuedTXBytes));

    if (gbStreamingFile == true)
    {
        //File stream in progress, read out a block
        QByteArray baFileData = gpStreamFileHandle->read(FileReadBlock);
        gspSerialPort.write(baFileData);
        gintQueuedTXBytes += baFileData.size();
        gintStreamBytesRead += baFileData.length();
        if (gpStreamFileHandle->atEnd())
        {
            //Finished sending
            FinishStream(false);
        }
        else if (gintStreamBytesRead > gintStreamBytesProgress)
        {
            //Progress output
            gbaDisplayBuffer.append(QString("Streamed ").append(QString::number(gintStreamBytesRead)).append(" bytes.\n"));
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
            gintStreamBytesProgress = gintStreamBytesProgress + StreamProgress;
        }

        //Update status bar
        if (gintStreamBytesRead == gintStreamBytesSize)
        {
            //Finished streaming file
            ui->statusBar->showMessage("File streaming complete!");
        }
        else
        {
            //Still streaming
            ui->statusBar->showMessage(QString("Streamed ").append(QString::number(gintStreamBytesRead).append(" bytes of ").append(QString::number(gintStreamBytesSize))));
        }
    }
    else if (gbStreamingBatch == true)
    {
        //Batch file command
        ui->statusBar->showMessage(QString("Sending Batch line number ").append(QString::number(gintStreamBytesRead)));
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_Cancel_clicked
    (
    )
{
    //Cancel current stream or file download
    if (gbTermBusy == true)
    {
        if (gchTermMode >= MODE_COMPILE && gchTermMode < MODE_SERVER_COMPILE)
        {
            //Cancel download
            gtmrDownloadTimeoutTimer.stop();
            gstrHexData = "";
            gbaDisplayBuffer.append("\n-- File download cancelled --\n");
            if (gbFileOpened == true)
            {
                gspSerialPort.write("AT+FCL");
                gintQueuedTXBytes += 6;
                DoLineEnd();
                gpMainLog->WriteLogData("AT+FCL\n");
                if (ui->check_SkipDL->isChecked() == false)
                {
                    //Output download details
                    gbaDisplayBuffer.append("AT+FCL\n");
                }
            }
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
            QList<QString> lstFI = SplitFilePath(gstrTermFilename);
            if (gpTermSettings->value("DelUWCAfterDownload", DefaultDelUWCAfterDownload).toBool() == true && gbIsUWCDownload == true && QFile::exists(QString(lstFI[0]).append(lstFI[1]).append(".uwc")))
            {
                //Remove UWC
                QFile::remove(QString(lstFI[0]).append(lstFI[1]).append(".uwc"));
            }
            gchTermMode = 0;
            gchTermMode2 = 0;
            gbTermBusy = false;
        }
        else if (gbStreamingFile == true)
        {
            //Cancel stream
            FinishStream(true);
        }
        else if (gbStreamingBatch == true)
        {
            //Cancel batch streaming
            FinishBatch(true);
        }
        else if (gchTermMode >= MODE_SERVER_COMPILE || gchTermMode <= MODE_SERVER_COMPILE_LOAD_RUN)
        {
            //Cancel network request and download
#pragma warning("Cannot currently cancel network request")

            //Change to just a compile
            if (gchTermMode != MODE_SERVER_COMPILE)
            {
                gchTermMode = MODE_SERVER_COMPILE;
                gchTermMode2 = MODE_SERVER_COMPILE;
            }
            return;

        }
        else if (gchTermMode == MODE_CHECK_ERROR_CODE_VERSIONS || gchTermMode == MODE_CHECK_UWTERMINALX_VERSIONS)
        {
            //Cancel network request
#pragma warning("Cannot currently cancel network request")
            return;
        }
    }

    //Disable button
    ui->btn_Cancel->setEnabled(false);
}

//=============================================================================
//=============================================================================
void
MainWindow::FinishStream
    (
    bool bType
    )
{
    //Sending a file stream has finished
    if (bType == true)
    {
        //Stream cancelled
        gbaDisplayBuffer.append(QString("\nCancelled stream after ").append(QString::number(gintStreamBytesRead)).append(" bytes (").append(QString::number(1+(gtmrStreamTimer.nsecsElapsed()/1000000000))).append(" seconds) [~").append(QString::number((gintStreamBytesRead/(1+gtmrStreamTimer.nsecsElapsed()/1000000000)))).append(" bytes/second].\n"));
        ui->statusBar->showMessage("File streaming cancelled.");
    }
    else
    {
        //Stream finished
        gbaDisplayBuffer.append(QString("\nFinished streaming file, ").append(QString::number(gintStreamBytesRead)).append(" bytes sent in ").append(QString::number(1+(gtmrStreamTimer.nsecsElapsed()/1000000000))).append(" seconds [~").append(QString::number((gintStreamBytesRead/(1+gtmrStreamTimer.nsecsElapsed()/1000000000)))).append(" bytes/second].\n"));
        ui->statusBar->showMessage("File streaming complete!");
    }

    //Initiate timer for buffer update
    if (!gtmrTextUpdateTimer.isActive())
    {
        gtmrTextUpdateTimer.start();
    }

    //Clear up
    gtmrStreamTimer.invalidate();
    gbTermBusy = false;
    gbStreamingFile = false;
    gchTermMode = 0;
    gpStreamFileHandle->close();
    delete gpStreamFileHandle;
    ui->btn_Cancel->setEnabled(false);
}

//=============================================================================
//=============================================================================
void
MainWindow::FinishBatch
    (
    bool bType
    )
{
    //Sending a file stream has finished
    if (bType == true)
    {
        //Stream cancelled
        gbaDisplayBuffer.append(QString("\nCancelled batch (").append(QString::number(1+(gtmrStreamTimer.nsecsElapsed()/1000000000))).append(" seconds)\n"));
        ui->statusBar->showMessage("Batch file sending cancelled.");
    }
    else
    {
        //Stream finished
        gbaDisplayBuffer.append(QString("\nFinished sending batch file, ").append(QString::number(gintStreamBytesRead)).append(" lines sent in ").append(QString::number(1+(gtmrStreamTimer.nsecsElapsed()/1000000000))).append(" seconds\n"));
        ui->statusBar->showMessage("Batch file sending complete!");
    }

    //Initiate timer for buffer update
    if (!gtmrTextUpdateTimer.isActive())
    {
        gtmrTextUpdateTimer.start();
    }

    //Clear up and cancel timer
    gtmrStreamTimer.invalidate();
    gtmrBatchTimeoutTimer.stop();
    gbTermBusy = false;
    gbStreamingBatch = false;
    gchTermMode = 0;
    gpStreamFileHandle->close();
    delete gpStreamFileHandle;
    gbaBatchReceive.clear();
    ui->btn_Cancel->setEnabled(false);
}

//=============================================================================
//=============================================================================
void
MainWindow::UpdateReceiveText
    (
    )
{
    //Updates the receive text buffer
    ui->text_TermEditData->AddDatInText(&gbaDisplayBuffer);
    gbaDisplayBuffer.clear();
}

//=============================================================================
//=============================================================================
void
MainWindow::BatchTimeoutSlot
    (
    )
{
    //A response to a batch command has timed out
    gbaDisplayBuffer.append("\nModule command timed out.\n");
    if (!gtmrTextUpdateTimer.isActive())
    {
        gtmrTextUpdateTimer.start();
    }
    gbTermBusy = false;
    gbStreamingBatch = false;
    gchTermMode = 0;
    gpStreamFileHandle->close();
    ui->btn_Cancel->setEnabled(false);
    delete gpStreamFileHandle;
}

//=============================================================================
//=============================================================================
void
MainWindow::on_combo_COM_currentIndexChanged
    (
    int index
    )
{
    //Serial port selection has been changed, update text
    QSerialPortInfo SerialInfo(ui->combo_COM->currentText());
    if (SerialInfo.isValid())
    {
        //Port exists
        QString strDisplayText(SerialInfo.description());
        if (SerialInfo.manufacturer().length() > 1)
        {
            //Add manufacturer
            strDisplayText.append(" (").append(SerialInfo.manufacturer()).append(")");
        }
        if (SerialInfo.serialNumber().length() > 1)
        {
            //Add serial
            strDisplayText.append(" [").append(SerialInfo.serialNumber()).append("]");
        }
        ui->label_SerialInfo->setText(strDisplayText);
    }
    else
    {
        //No such port
        ui->label_SerialInfo->setText("Invalid");
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::dragEnterEvent
    (
    QDragEnterEvent *event
    )
{
    //A file is being dragged onto the window
    if (event->mimeData()->urls().count() == 1 && gbTermBusy == false && gspSerialPort.isOpen() == true)
    {
        //Nothing is running, serial handle is open and a single file is being dragged - accept action
        event->acceptProposedAction();
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::dropEvent
    (
    QDropEvent *event
    )
{
    //A file has been dragged onto the window - send this file if possible
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
    {
        //No files
        return;
    }
    else if (urls.length() > 1)
    {
        //More than 1 file - ignore
        return;
    }

    QString strFileName = urls.first().toLocalFile();
    if (strFileName.isEmpty())
    {
        //Invalid filename
        return;
    }

    //Pass to other function call
    DroppedFile(strFileName);
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_PreXCompRun_stateChanged
(
    int iChecked
)
{
    //User has changed running pre/post XCompiler executable, update GUI
    if (iChecked == 2)
    {
        //Enabled
        ui->check_PreXCompFail->setDisabled(false);
        ui->radio_XCompPost->setDisabled(false);
        ui->radio_XCompPre->setDisabled(false);
        ui->edit_PreXCompFilename->setDisabled(false);
        ui->btn_PreXCompSelect->setDisabled(false);
        ui->label_PreXCompInfo->setDisabled(false);
        ui->label_PreXCompText->setDisabled(false);
        gpTermSettings->setValue("PrePostXCompRun", "1");
    }
    else
    {
        //Disabled
        ui->check_PreXCompFail->setDisabled(true);
        ui->radio_XCompPost->setDisabled(true);
        ui->radio_XCompPre->setDisabled(true);
        ui->edit_PreXCompFilename->setDisabled(true);
        ui->btn_PreXCompSelect->setDisabled(true);
        ui->label_PreXCompInfo->setDisabled(true);
        ui->label_PreXCompText->setDisabled(true);
        gpTermSettings->setValue("PrePostXCompRun", "0");
    }
}

//=============================================================================
//=============================================================================
bool
MainWindow::RunPrePostExecutable
(
    QString strFilename
)
{
    //Runs the pre/post XCompile executable
    QProcess pXProcess;
    if (ui->edit_PreXCompFilename->text().left(1) == "\"")
    {
        //First character is a quote, search for end quote
        int i = 0;
        while (i != -1)
        {
            i = ui->edit_PreXCompFilename->text().indexOf("\"", i+1);
            if (i == -1)
            {
                //No end quote found
                QString strMessage = tr("Error with Pre/Post-XCompile executable: No end quote found for executable file.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
                return false;
            }
            else if (ui->edit_PreXCompFilename->text().mid(i-1, 2) != "\\\"")
            {
                //Found end of quote
                if (!QFile::exists(ui->edit_PreXCompFilename->text().mid(1, i-1)))
                {
                    //Executable file doesn't appear to exist
                    QString strMessage = tr("Error with Pre/Post-XCompile executable: File ").append(ui->edit_PreXCompFilename->text().left(i+1)).append(" does not exist.");
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                    return false;
                }

                //Split up all the arguments
                QList<QString> lArguments;
                int l = ui->edit_PreXCompFilename->text().indexOf(" ", i+1);
                while (l != -1)
                {
                    lArguments << ui->edit_PreXCompFilename->text().mid(l+1, ui->edit_PreXCompFilename->text().indexOf(" ", l+1)-l-1).replace("%1", strFilename);
                    l = ui->edit_PreXCompFilename->text().indexOf(" ", l+1);
                }

                if (ui->edit_PreXCompFilename->text().mid(1, i-1).right(4).toLower() == ".bat")
                {
                    //Batch file - run through cmd
                    lArguments.insert(0, ui->edit_PreXCompFilename->text().mid(1, i-1));
                    lArguments.insert(0, "/C");
                    pXProcess.start("cmd", lArguments);
                }
                else
                {
                    //Normal executable
                    pXProcess.start(ui->edit_PreXCompFilename->text().mid(1, i-1), lArguments);
                }
                pXProcess.waitForFinished(PrePostXCompTimeout);
                return true;
            }
        }

        return false;
    }
    else
    {
        //No quote character, search for a space
        if (ui->edit_PreXCompFilename->text().indexOf(" ") == -1)
        {
            //No spaces found, assume whole string is executable
            if (!QFile::exists(ui->edit_PreXCompFilename->text()))
            {
                //Executable file doesn't appear to exist
                QString strMessage = tr("Error with Pre/Post-XCompile executable: File \"").append(ui->edit_PreXCompFilename->text().left(ui->edit_PreXCompFilename->text().indexOf(" "))).append("\" does not exist.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
                return false;
            }

            if (ui->edit_PreXCompFilename->text().right(4).toLower() == ".bat")
            {
                //Batch file - run through cmd
                pXProcess.start("cmd", QList<QString>() << "/C" << ui->edit_PreXCompFilename->text().replace("/", "\\"));
            }
            else
            {
                //Normal executable
                pXProcess.start(ui->edit_PreXCompFilename->text());
            }
            pXProcess.waitForFinished(PrePostXCompTimeout);
        }
        else
        {
            //Space found, assume everything before the space is a filename and after are arguments
            QList<QString> lArguments;
            int i = ui->edit_PreXCompFilename->text().indexOf(" ");
            while (i != -1)
            {
                lArguments << ui->edit_PreXCompFilename->text().mid(i+1, ui->edit_PreXCompFilename->text().indexOf(" ", i+1)-i-1).replace("%1", strFilename);
                i = ui->edit_PreXCompFilename->text().indexOf(" ", i+1);
            }

            if (!QFile::exists(ui->edit_PreXCompFilename->text().left(ui->edit_PreXCompFilename->text().indexOf(" "))))
            {
                //Executable file doesn't appear to exist
                QString strMessage = tr("Error with Pre/Post-XCompile executable: File \"").append(ui->edit_PreXCompFilename->text().left(ui->edit_PreXCompFilename->text().indexOf(" "))).append("\" does not exist.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
                return false;
            }

            if (ui->edit_PreXCompFilename->text().left(ui->edit_PreXCompFilename->text().indexOf(" ")).right(4).toLower() == ".bat")
            {
                //Batch file - run through cmd
                lArguments.insert(0, ui->edit_PreXCompFilename->text().left(ui->edit_PreXCompFilename->text().indexOf(" ")));
                lArguments.insert(0, "/C");
                pXProcess.start("cmd", lArguments);
            }
            else
            {
                //Normal executable
                pXProcess.start(ui->edit_PreXCompFilename->text().left(ui->edit_PreXCompFilename->text().indexOf(" ")), lArguments);
            }
            pXProcess.waitForFinished(PrePostXCompTimeout);
        }
    }

    //Return success code
    return true;
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_PreXCompSelect_clicked
    (
    )
{
    //Opens an executable selector
    QString strFilename;
    strFilename = QFileDialog::getOpenFileName(this, "Open Executable/batch", gpTermSettings->value("LastFileDirectory", "").toString(), "Executables/Batch/Bash files (*.exe *.bat *.sh);;All Files (*.*)");

    if (strFilename.length() > 1)
    {
        //Set last directory config
        gpTermSettings->setValue("LastFileDirectory", SplitFilePath(strFilename)[0]);

        if ((unsigned int)(QFile(strFilename).permissions() & (QFileDevice::ExeOther | QFileDevice::ExeGroup | QFileDevice::ExeUser | QFileDevice::ExeOwner)) == 0)
        {
            //File is not executable, give an error
            QString strMessage = tr("Error: Selected file \"").append(strFilename).append("\" is not an executable file.");
            gpmErrorForm->show();
            gpmErrorForm->SetMessage(&strMessage);
            return;
        }

        //File selected is executable, update text box
        ui->edit_PreXCompFilename->setText(QString("\"").append(strFilename).append("\""));

        //Update the INI settings
        gpTermSettings->setValue("PrePostXCompPath", ui->edit_PreXCompFilename->text());
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_radio_XCompPre_toggled
    (
    bool bChecked
    )
{
    //Pre/post XCompiler run time changed to run before XCompiler - update settings
    gpTermSettings->setValue("PrePostXCompMode", "0");
}

//=============================================================================
//=============================================================================
void
MainWindow::on_radio_XCompPost_toggled
    (
    bool bChecked
    )
{
    //Pre/post XCompiler run time changed to run after XCompiler - update settings
    gpTermSettings->setValue("PrePostXCompMode", "1");
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_PreXCompFail_stateChanged
    (
    int bChecked
    )
{
    //Pre/post XCompiler run if XCompiler failed changed - update settings
    gpTermSettings->setValue("PrePostXCompFail", ui->check_PreXCompFail->isChecked());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_edit_PreXCompFilename_editingFinished
    (
    )
{
    //Pre/post XCompiler executable changed - update settings
    gpTermSettings->setValue("PrePostXCompPath", ui->edit_PreXCompFilename->text());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_GitHub_clicked
    (
    )
{
    //Open webpage at the UwTerminalX github page)
    QDesktopServices::openUrl(QUrl("https://github.com/LairdCP/UwTerminalX"));
}

//=============================================================================
//=============================================================================
void
MainWindow::replyFinished
    (
    QNetworkReply* nrReply
    )
{
    //Response received from server regarding online XCompilation
    if (nrReply->error() != QNetworkReply::NoError && nrReply->error() != QNetworkReply::ServiceUnavailableError)
    {
        //An error occured
        ui->btn_Cancel->setEnabled(false);
        ui->btn_ErrorCodeUpdate->setEnabled(true);
        ui->btn_UwTerminalXUpdate->setEnabled(true);
        ui->btn_ModuleFirmware->setEnabled(true);
        gtmrDownloadTimeoutTimer.stop();
        gstrHexData = "";
        if (!gtmrTextUpdateTimer.isActive())
        {
            gtmrTextUpdateTimer.start();
        }
        gchTermMode = 0;
        gchTermMode2 = 0;
        gbTermBusy = false;

        //Display error message
        QString strMessage = QString("An error occured during Online XCompilation: ").append(nrReply->errorString());
        gpmErrorForm->show();
        gpmErrorForm->SetMessage(&strMessage);
    }
    else
    {
        if (gchTermMode2 == 0)
        {
            //Check if device is supported
            QJsonParseError jpeJsonError;
            QJsonDocument jdJsonData = QJsonDocument::fromJson(nrReply->readAll(), &jpeJsonError);
            if (jpeJsonError.error == QJsonParseError::NoError)
            {
                //Decoded JSON
                QJsonObject joJsonObject = jdJsonData.object();
                if (nrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 503)
                {
                    //Server responded with error
                    gtmrDownloadTimeoutTimer.stop();
                    gstrHexData = "";
                    if (!gtmrTextUpdateTimer.isActive())
                    {
                        gtmrTextUpdateTimer.start();
                    }
                    gchTermMode = 0;
                    gchTermMode2 = 0;
                    gbTermBusy = false;
                    ui->btn_Cancel->setEnabled(false);

                    QString strMessage = QString("Server responded with error code ").append(joJsonObject["Result"].toString()).append("; ").append(joJsonObject["Error"].toString());
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
                else if (nrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
                {
                    //Server responded with OK
                    if (joJsonObject["Result"].toString() == "1")
                    {
                        //Device supported
                        gstrDeviceID = joJsonObject["ID"].toString();
                        gchTermMode2 = MODE_SERVER_COMPILE;

                        //Compile application
                        QNetworkRequest nrThisReq(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/xcompile.php?JSON=1")));
                        QByteArray baPostData;
                        baPostData.append("-----------------------------17192614014659\r\nContent-Disposition: form-data; name=\"file_XComp\"\r\n\r\n").append(joJsonObject["ID"].toString()).append("\r\n");
                        baPostData.append("-----------------------------17192614014659\r\nContent-Disposition: form-data; name=\"file_sB\"; filename=\"test.sb\"\r\nContent-Type: application/octet-stream\r\n\r\n");

                        //Add file data
                        QFile file(gstrTermFilename);
                        QByteArray tmpData;
                        if (!file.open(QIODevice::ReadOnly))
                        {
                            //Failed to open file selected for download
                            nrReply->deleteLater();
                            return;
                        }

                        while (!file.atEnd())
                        {
                            tmpData.append(file.readAll());
                        }
                        file.close();

                        QFileInfo fiFileInfo(gstrTermFilename);

                        //Include other files
                        QRegularExpression reTempRE("(^|:)(\\s{0,})#(\\s{0,})include(\\s{1,})\"(.*?)\"");
                        reTempRE.setPatternOptions(QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
                        bool bChangedState = true;
                        while (bChangedState == true)
                        {
                            bChangedState = false;
                            QRegularExpressionMatchIterator rx1match = reTempRE.globalMatch(tmpData);
                            while (rx1match.hasNext())
                            {
                                //Found an include, add the file data
                                QRegularExpressionMatch ThisMatch = rx1match.next();

                                file.setFileName(QString(fiFileInfo.path()).append("/").append(ThisMatch.captured(5)));
                                if (!file.open(QIODevice::ReadOnly))
                                {
                                    //Failed to open include file
                                    nrReply->deleteLater();
                                    return;
                                }

                                bChangedState = true;

                                QByteArray tmpData2;
                                while (!file.atEnd())
                                {
                                    tmpData2.append(file.readAll());
                                }
                                file.close();
                                unsigned int i = tmpData.indexOf(ThisMatch.captured(0));
                                tmpData.remove(i, ThisMatch.captured(0).length());
                                tmpData.insert(i, "\r\n");
                                tmpData.insert(i+2, tmpData2);
                            }
                        }

                        //Remove all extra #include statments
                        tmpData.replace("#include", "");

                        //Append the data to the POST request
                        baPostData.append(tmpData);
                        baPostData.append("\r\n-----------------------------17192614014659--\r\n");
                        nrThisReq.setRawHeader("Content-Type", "multipart/form-data; boundary=---------------------------17192614014659");
                        nrThisReq.setRawHeader("Content-Length", QString(baPostData.length()).toUtf8());
                        gnmManager->post(nrThisReq, baPostData);
                        ui->statusBar->showMessage("Sending smartBASIC application for online compilation...", 500);

                        //Stop the module timeout timer
                        gtmrDownloadTimeoutTimer.stop();
                    }
                    else
                    {
                        //Device should be supported but something went wrong...
                        gtmrDownloadTimeoutTimer.stop();
                        gstrHexData = "";
                        if (!gtmrTextUpdateTimer.isActive())
                        {
                            gtmrTextUpdateTimer.start();
                        }
                        gchTermMode = 0;
                        gchTermMode2 = 0;
                        gbTermBusy = false;
                        ui->btn_Cancel->setEnabled(false);

                        QString strMessage = QString("Unfortunately your device is not supported for online XCompiling.");
                        gpmErrorForm->show();
                        gpmErrorForm->SetMessage(&strMessage);
                    }
                }
                else
                {
                    //Unknown response
                    gtmrDownloadTimeoutTimer.stop();
                    gstrHexData = "";
                    if (!gtmrTextUpdateTimer.isActive())
                    {
                        gtmrTextUpdateTimer.start();
                    }
                    gchTermMode = 0;
                    gchTermMode2 = 0;
                    gbTermBusy = false;
                    ui->btn_Cancel->setEnabled(false);

                    QString strMessage = QString("Server responded with unknown response, code: ").append(nrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
            }
            else
            {
                //Error whilst decoding JSON
                gtmrDownloadTimeoutTimer.stop();
                gstrHexData = "";
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
                gchTermMode = 0;
                gchTermMode2 = 0;
                gbTermBusy = false;
                ui->btn_Cancel->setEnabled(false);

                QString strMessage = QString("Error: Unable to decode server JSON response, debug: ").append(jpeJsonError.errorString());
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }
            ui->statusBar->showMessage("");
        }
        else if (gchTermMode2 == MODE_SERVER_COMPILE)
        {
            //XCompile result
            if (nrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 503)
            {
                //Error compiling
                QJsonParseError jpeJsonError;
                QJsonDocument jdJsonData = QJsonDocument::fromJson(nrReply->readAll(), &jpeJsonError);
                gtmrDownloadTimeoutTimer.stop();
                gstrHexData = "";
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
                gchTermMode = 0;
                gchTermMode2 = 0;
                gbTermBusy = false;
                ui->btn_Cancel->setEnabled(false);

                if (jpeJsonError.error == QJsonParseError::NoError)
                {
                    //Decoded JSON
                    QJsonObject joJsonObject = jdJsonData.object();

                    //Server responded with error
                    if (joJsonObject["Result"].toString() == "-9")
                    {
                        //Error whilst compiling, show results
                        QString strMessage = QString("Failed to compile ").append(joJsonObject["Result"].toString()).append("; ").append(joJsonObject["Error"].toString().append("\r\n").append(joJsonObject["Description"].toString()));
                        gpmErrorForm->show();
                        gpmErrorForm->SetMessage(&strMessage);
                    }
                    else
                    {
                        //Server responded with error
                        QString strMessage = QString("Server responded with error code ").append(joJsonObject["Result"].toString()).append("; ").append(joJsonObject["Error"].toString());
                        gpmErrorForm->show();
                        gpmErrorForm->SetMessage(&strMessage);
                    }
                }
                else
                {
                    //Error whilst decoding JSON
                    QString strMessage = QString("Unable to decode JSON data from server, debug data: ").append(jdJsonData.toBinaryData());
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
            }
            else if (nrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
            {
                //Compiled - save file
                QList<QString> lstFI = SplitFilePath(gstrTermFilename);
                if (!QFile::exists(QString(lstFI[0]).append(lstFI[1]).append(".uwc")))
                {
                    //Remove file
                    QFile::remove(QString(lstFI[0]).append(lstFI[1]).append(".uwc"));
                }

                QFile file(QString(lstFI[0]).append(lstFI[1]).append(".uwc"));
                if (file.open(QIODevice::WriteOnly))
                {
                    file.write(nrReply->readAll());
                }
                file.flush();
                file.close();

                gstrTermFilename = QString(lstFI[0]).append(lstFI[1]).append(".uwc");

                gbaDisplayBuffer.append("\n-- Online XCompile complete --\n");
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
                if (gchTermMode == MODE_SERVER_COMPILE)
                {
                    //Done
                    gtmrDownloadTimeoutTimer.stop();
                    gstrHexData = "";
                    gchTermMode = 0;
                    gchTermMode2 = 0;
                    gbTermBusy = false;
                    ui->btn_Cancel->setEnabled(false);
                }
                else
                {
                    //Next step
                    if (gchTermMode == MODE_SERVER_COMPILE_LOAD)
                    {
                        //Just load the file
                        gchTermMode = MODE_LOAD;
                    }
                    else
                    {
                        //Load the file then run it
                        gchTermMode = MODE_LOAD_RUN;
                    }

                    //Loading a compiled application
                    LoadFile(false);

                    //Download to the device
                    gchTermMode2 = MODE_COMPILE;
                    QByteArray baTmpBA = QString("AT+DEL \"").append(gstrDownloadFilename).append("\" +").toUtf8();
                    gspSerialPort.write(baTmpBA);
                    gintQueuedTXBytes += baTmpBA.size();
                    DoLineEnd();
                    gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
                    if (ui->check_SkipDL->isChecked() == false)
                    {
                        //Output download details
                        if (ui->check_ShowCLRF->isChecked() == true)
                        {
                            //Escape \t, \r and \n
                            baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
                        }

                        //Replace unprintable characters
                        baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

                        //Update display buffer
                        gbaDisplayBuffer.append(baTmpBA);
                        if (!gtmrTextUpdateTimer.isActive())
                        {
                            gtmrTextUpdateTimer.start();
                        }
                    }

                    //Start the timeout timer
                    gtmrDownloadTimeoutTimer.start();
                }
            }
            else
            {
                //Unknown response
                gtmrDownloadTimeoutTimer.stop();
                gstrHexData = "";
                if (!gtmrTextUpdateTimer.isActive())
                {
                    gtmrTextUpdateTimer.start();
                }
                gchTermMode = 0;
                gchTermMode2 = 0;
                gbTermBusy = false;
                ui->btn_Cancel->setEnabled(false);

                QString strMessage = tr("Unknown response from server.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }
            ui->statusBar->showMessage("");
        }
        else if (gchTermMode == MODE_CHECK_ERROR_CODE_VERSIONS)
        {
            //Error code update response
            QByteArray tmpBA = nrReply->readAll();
            QJsonParseError jpeJsonError;
            QJsonDocument jdJsonData = QJsonDocument::fromJson(tmpBA, &jpeJsonError);

            if (jpeJsonError.error == QJsonParseError::NoError)
            {
                //Decoded JSON
                QJsonObject joJsonObject = jdJsonData.object();

                //Server responded with error
                if (joJsonObject["Result"].toString() == "-1")
                {
                    //Outdated version
                    ui->label_ErrorCodeUpdate->setText("Update is available!");
                    QPalette palBGColour = QPalette();
                    palBGColour.setColor(QPalette::Active, QPalette::WindowText, Qt::darkGreen);
                    palBGColour.setColor(QPalette::Inactive, QPalette::WindowText, Qt::darkGreen);
                    palBGColour.setColor(QPalette::Disabled, QPalette::WindowText, Qt::darkGreen);
                    ui->label_ErrorCodeUpdate->setPalette(palBGColour);
                }
                else if (joJsonObject["Result"].toString() == "-2")
                {
                    //Server error
                    QString strMessage = QString("A server error was encountered whilst checking for an updated error code file.");
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
                else if (joJsonObject["Result"].toString() == "1")
                {
                    //Version is OK
                    ui->label_ErrorCodeUpdate->setText("No updates avialable.");
                    QPalette palBGColour = QPalette();
                    ui->label_ErrorCodeUpdate->setPalette(palBGColour);
                }
                else
                {
                    //Server responded with error
                    QString strMessage = QString("Server responded with error code ").append(joJsonObject["Result"].toString()).append("; ").append(joJsonObject["Error"].toString());
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
            }
            else
            {
                //Error whilst decoding JSON
                QString strMessage = QString("Unable to decode JSON data from server, debug data: ").append(jdJsonData.toBinaryData());
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }

            gchTermMode = 0;
            gchTermMode2 = 0;
            gbTermBusy = false;
            ui->btn_Cancel->setEnabled(false);
            ui->btn_ErrorCodeUpdate->setEnabled(true);
            ui->btn_UwTerminalXUpdate->setEnabled(true);
            ui->btn_ModuleFirmware->setEnabled(true);
            ui->statusBar->showMessage("");
        }
        else if (gchTermMode == MODE_CHECK_UWTERMINALX_VERSIONS)
        {
            //UwTerminalX update response
            QByteArray tmpBA = nrReply->readAll();
            QJsonParseError jpeJsonError;
            QJsonDocument jdJsonData = QJsonDocument::fromJson(tmpBA, &jpeJsonError);

            if (jpeJsonError.error == QJsonParseError::NoError)
            {
                //Decoded JSON
                QJsonObject joJsonObject = jdJsonData.object();

                //Server responded with error
                if (joJsonObject["Result"].toString() == "-1")
                {
                    //Outdated version
                    ui->label_UwTerminalXUpdate->setText(QString("Update available: ").append(joJsonObject["Version"].toString()));
                    QPalette palBGColour = QPalette();
                    palBGColour.setColor(QPalette::Active, QPalette::WindowText, Qt::darkGreen);
                    palBGColour.setColor(QPalette::Inactive, QPalette::WindowText, Qt::darkGreen);
                    palBGColour.setColor(QPalette::Disabled, QPalette::WindowText, Qt::darkGreen);
                    ui->label_UwTerminalXUpdate->setPalette(palBGColour);
                }
                else if (joJsonObject["Result"].toString() == "-2")
                {
                    //Server error
                    QString strMessage = QString("A server error was encountered whilst checking for an updated error code file.");
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
                else if (joJsonObject["Result"].toString() == "1")
                {
                    //Version is OK
                    ui->label_UwTerminalXUpdate->setText("No updates avialable.");
                    QPalette palBGColour = QPalette();
                    ui->label_UwTerminalXUpdate->setPalette(palBGColour);
                }
                else
                {
                    //Server responded with error
                    QString strMessage = QString("Server responded with error code ").append(joJsonObject["Result"].toString()).append("; ").append(joJsonObject["Error"].toString());
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
            }
            else
            {
                //Error whilst decoding JSON
                QString strMessage = QString("Unable to decode JSON data from server, debug data: ").append(jdJsonData.toBinaryData());
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }

            gchTermMode = 0;
            gchTermMode2 = 0;
            gbTermBusy = false;
            ui->btn_Cancel->setEnabled(false);
            ui->btn_ErrorCodeUpdate->setEnabled(true);
            ui->btn_UwTerminalXUpdate->setEnabled(true);
            ui->btn_ModuleFirmware->setEnabled(true);
            ui->statusBar->showMessage("");
        }
        else if (gchTermMode == MODE_UPDATE_ERROR_CODE)
        {
            //Error code update
            if (nrReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
            {
                //Got updated error code file
                delete gpErrorMessages;
#if TARGET_OS_MAC
                if (!QFile::exists(QString(gstrMacBundlePath).append("codes.csv")))
                {
                    //Remove file
                    QFile::remove(QString(gstrMacBundlePath).append("codes.csv"));
                }
#else
                if (!QFile::exists("codes.csv"))
                {
                    //Remove file
                    QFile::remove("codes.csv");
                }
#endif

#if TARGET_OS_MAC
                QFile file(QString(gstrMacBundlePath).append("codes.csv"));
#else
                QFile file("codes.csv");
#endif
                if (file.open(QIODevice::WriteOnly))
                {
                    file.write(nrReply->readAll());
                    file.flush();
                    file.close();

                    //Reopen error code file and update status
#if TARGET_OS_MAC
                    gpErrorMessages = new QSettings(QString(gstrMacBundlePath).append("codes.csv"), QSettings::IniFormat);
#else
                    gpErrorMessages = new QSettings(QString("codes.csv"), QSettings::IniFormat);
#endif
                    ui->label_ErrorCodeUpdate->setText("Error code file updated!");
                }
                else
                {
                    //Failed to open error code file
                    QString strMessage = tr("Failed to open error code file for writing (codes.csv), ensure relevant permissions exist for this file and try again.");
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
            }
            else
            {
                //Unknown response
                QString strMessage = tr("Unknown response from server.");
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }

            //Reset everything
            gchTermMode = 0;
            gchTermMode2 = 0;
            gbTermBusy = false;
            ui->btn_Cancel->setEnabled(false);
            ui->btn_ErrorCodeUpdate->setEnabled(true);
            ui->btn_UwTerminalXUpdate->setEnabled(true);
            ui->btn_ModuleFirmware->setEnabled(true);
            ui->statusBar->showMessage("");
        }
        else if (gchTermMode == MODE_CHECK_FIRMWARE_VERSIONS)
        {
            //Response containing latest firmware versions for modules
            QByteArray tmpBA = nrReply->readAll();
            QJsonParseError jpeJsonError;
            QJsonDocument jdJsonData = QJsonDocument::fromJson(tmpBA, &jpeJsonError);

            if (jpeJsonError.error == QJsonParseError::NoError)
            {
                //Decoded JSON
                QJsonObject joJsonObject = jdJsonData.object();

                //Server responded with error
                if (joJsonObject["Result"].toString() == "1")
                {
                    //Outdated version
                    ui->label_BL600Firmware->setText(QString("BL600: ").append(joJsonObject["BL600r2"].toString()));
                    ui->label_BL620Firmware->setText(QString("BL620: ").append(joJsonObject["BL620"].toString()));
                    ui->label_BT900Firmware->setText(QString("BT900: ").append(joJsonObject["BT900"].toString()));
                    QPalette palBGColour = QPalette();
                    palBGColour.setColor(QPalette::Active, QPalette::WindowText, Qt::darkGreen);
                    palBGColour.setColor(QPalette::Inactive, QPalette::WindowText, Qt::darkGreen);
                    palBGColour.setColor(QPalette::Disabled, QPalette::WindowText, Qt::darkGreen);
                    ui->label_BL600Firmware->setPalette(palBGColour);
                    ui->label_BL620Firmware->setPalette(palBGColour);
                    ui->label_BT900Firmware->setPalette(palBGColour);
                }
                else
                {
                    //Server responded with error
                    QString strMessage = QString("Server responded with error code ").append(joJsonObject["Result"].toString()).append("; ").append(joJsonObject["Error"].toString());
                    gpmErrorForm->show();
                    gpmErrorForm->SetMessage(&strMessage);
                }
            }
            else
            {
                //Error whilst decoding JSON
                QString strMessage = QString("Unable to decode JSON data from server, debug data: ").append(jdJsonData.toBinaryData());
                gpmErrorForm->show();
                gpmErrorForm->SetMessage(&strMessage);
            }

            gchTermMode = 0;
            gchTermMode2 = 0;
            gbTermBusy = false;
            ui->btn_Cancel->setEnabled(false);
            ui->btn_ErrorCodeUpdate->setEnabled(true);
            ui->btn_UwTerminalXUpdate->setEnabled(true);
            ui->btn_ModuleFirmware->setEnabled(true);
            ui->statusBar->showMessage("");
        }
    }

    //Queue the network reply object to be deleted
    nrReply->deleteLater();
}

//=============================================================================
//=============================================================================
#ifdef UseSSL
void
MainWindow::sslErrors
    (
    QNetworkReply* nrReply,
    QList<QSslError> lstSSLErrors
    )
{
    //Error detected with SSL
    if (sslcLairdSSL != NULL && nrReply->sslConfiguration().peerCertificate() == *sslcLairdSSL)
    {
        //Server certificate matches
        nrReply->ignoreSslErrors(lstSSLErrors);
    }
}
#endif

//=============================================================================
//=============================================================================
void
MainWindow::on_check_OnlineXComp_stateChanged
    (
    int bChecked
    )
{
    //Online XCompiler checkbox state changed
    ui->label_OnlineXCompInfo->setEnabled(ui->check_OnlineXComp->isChecked());
    gpTermSettings->setValue("OnlineXComp", ui->check_OnlineXComp->isChecked());
}

//=============================================================================
//=============================================================================
QList<QString>
MainWindow::SplitFilePath
    (
    QString strFilename
    )
{
    //Extracts various parts from a file path; [0] path, [1] filename, [2] file extension
    QFileInfo fiFile(strFilename);
    QString strFilenameOnly = fiFile.fileName();
    QString strFileExtension = "";
    if (strFilenameOnly.indexOf(".") != -1)
    {
        //Dot found, only keep characters up to the dot
        strFileExtension = strFilenameOnly.mid(strFilenameOnly.lastIndexOf(".")+1, -1);
        strFilenameOnly = strFilenameOnly.left(strFilenameOnly.lastIndexOf("."));
    }

    //Return an array with path, filename and extension
    QList<QString> lstReturnData;
    lstReturnData << fiFile.path().append("/") << strFilenameOnly << strFileExtension;
    return lstReturnData;
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_BL600Apps_clicked
    (
    )
{
    //BL600 Applications button clicked
    QDesktopServices::openUrl(QUrl("https://github.com/LairdCP/BL600-Applications"));
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_BL620Apps_clicked
    (
    )
{
    //BL620 Applications button clicked
    QDesktopServices::openUrl(QUrl("https://github.com/LairdCP/BL620-Applications"));
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_BT900Apps_clicked
    (
    )
{
    //BT900 Applications button clicked
    QDesktopServices::openUrl(QUrl("https://github.com/LairdCP/BT900-Applications"));
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_ErrorCodeUpdate_clicked
    (
    )
{
    //Check for updates to error codes
    if (gbTermBusy == false)
    {
        //Send request
        gbTermBusy = true;
        gchTermMode = MODE_CHECK_ERROR_CODE_VERSIONS;
        gchTermMode2 = MODE_CHECK_ERROR_CODE_VERSIONS;
        ui->btn_Cancel->setEnabled(true);
        ui->btn_ErrorCodeUpdate->setEnabled(false);
        ui->btn_UwTerminalXUpdate->setEnabled(false);
        ui->btn_ModuleFirmware->setEnabled(false);
        gnmManager->get(QNetworkRequest(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/update_errorcodes.php?Ver=").append(gpErrorMessages->value("Version", "0.00").toString()))));
        ui->statusBar->showMessage("Checking for Error Code file updates...");
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_UwTerminalXUpdate_clicked
    (
    )
{
    //Check for updates to UwTerminalX
    if (gbTermBusy == false)
    {
        //Send request
        gbTermBusy = true;
        gchTermMode = MODE_CHECK_UWTERMINALX_VERSIONS;
        gchTermMode2 = MODE_CHECK_UWTERMINALX_VERSIONS;
        ui->btn_Cancel->setEnabled(true);
        ui->btn_ErrorCodeUpdate->setEnabled(false);
        ui->btn_UwTerminalXUpdate->setEnabled(false);
        ui->btn_ModuleFirmware->setEnabled(false);
        gnmManager->get(QNetworkRequest(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/update_uwterminalx.php?Ver=").append(UwVersion))));
        ui->statusBar->showMessage("Checking for UwTerminalX updates...");
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_check_Echo_stateChanged
    (
    int bChecked
    )
{
    //Local echo checkbox state changed
    ui->text_TermEditData->mbLocalEcho = ui->check_Echo->isChecked();
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_ErrorCodeDownload_clicked
    (
    )
{
    //Download latest error code file button clicked
    if (gbTermBusy == false)
    {
        //Send request
        gbTermBusy = true;
        gchTermMode = MODE_UPDATE_ERROR_CODE;
        gchTermMode2 = MODE_UPDATE_ERROR_CODE;
        ui->btn_Cancel->setEnabled(true);
        ui->btn_ErrorCodeUpdate->setEnabled(false);
        ui->btn_UwTerminalXUpdate->setEnabled(false);
        ui->btn_ModuleFirmware->setEnabled(false);
        gnmManager->get(QNetworkRequest(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/codes.csv"))));
        ui->statusBar->showMessage("Downloading Error Code file...");
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_combo_PredefinedDevice_currentIndexChanged
    (
    int iIndex
    )
{
    //Load settings for current device
    ui->combo_Baud->setCurrentText(gpPredefinedDevice->value(QString("Port").append(QString::number(iIndex+1).append("Baud")), "115200").toString());
    ui->combo_Parity->setCurrentIndex(gpPredefinedDevice->value(QString("Port").append(QString::number(iIndex+1).append("Parity")), "0").toInt());
    ui->combo_Stop->setCurrentText(gpPredefinedDevice->value(QString("Port").append(QString::number(iIndex+1).append("Stop")), "1").toString());
    ui->combo_Data->setCurrentText(gpPredefinedDevice->value(QString("Port").append(QString::number(iIndex+1).append("Data")), "8").toString());
    ui->combo_Handshake->setCurrentIndex(gpPredefinedDevice->value(QString("Port").append(QString::number(iIndex+1).append("Flow")), "1").toInt());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_PredefinedAdd_clicked
    (
    )
{
    //Adds a new predefined device entry
    ui->combo_PredefinedDevice->addItem("New");
    ui->combo_PredefinedDevice->setCurrentIndex(ui->combo_PredefinedDevice->count()-1);
    gpPredefinedDevice->setValue(QString("Port").append(QString::number((ui->combo_PredefinedDevice->count()))).append("Name"), "New");
    gpPredefinedDevice->setValue(QString("Port").append(QString::number((ui->combo_PredefinedDevice->count()))).append("Baud"), ui->combo_Baud->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number((ui->combo_PredefinedDevice->count()))).append("Parity"), ui->combo_Parity->currentIndex());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number((ui->combo_PredefinedDevice->count()))).append("Stop"), ui->combo_Stop->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number((ui->combo_PredefinedDevice->count()))).append("Data"), ui->combo_Data->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number((ui->combo_PredefinedDevice->count()))).append("Flow"), ui->combo_Handshake->currentIndex());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_PredefinedDelete_clicked
    (
    )
{
    //Remove current device configuration
    if (ui->combo_PredefinedDevice->count() > 0)
    {
        //
        unsigned int uiDeviceNumber = ui->combo_PredefinedDevice->currentIndex();
        unsigned int i = uiDeviceNumber+2;
        gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Name"));
        gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Baud"));
        gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Parity"));
        gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Stop"));
        gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Data"));
        gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Flow"));
        while (!gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Name")).isNull())
        {
            //Shift element up
            gpPredefinedDevice->setValue(QString("Port").append(QString::number(i-1)).append("Name"), gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Name")).toString());
            gpPredefinedDevice->setValue(QString("Port").append(QString::number(i-1)).append("Baud"), gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Baud")).toInt());
            gpPredefinedDevice->setValue(QString("Port").append(QString::number(i-1)).append("Parity"), gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Parity")).toInt());
            gpPredefinedDevice->setValue(QString("Port").append(QString::number(i-1)).append("Stop"), gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Stop")).toInt());
            gpPredefinedDevice->setValue(QString("Port").append(QString::number(i-1)).append("Data"), gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Data")).toInt());
            gpPredefinedDevice->setValue(QString("Port").append(QString::number(i-1)).append("Flow"), gpPredefinedDevice->value(QString("Port").append(QString::number(i)).append("Flow")).toInt());
            ++i;
        }
        if (!gpPredefinedDevice->value(QString("Port").append(QString::number(i-1)).append("Name")).isNull())
        {
            //Remove last element
            gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Name"));
            gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Baud"));
            gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Parity"));
            gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Stop"));
            gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Data"));
            gpPredefinedDevice->remove(QString("Port").append(QString::number(i-1)).append("Flow"));
        }
        ui->combo_PredefinedDevice->removeItem(uiDeviceNumber);

        //Load configuration options from new item (if one exists)
        if (ui->combo_PredefinedDevice->count() > 0)
        {
//TODO
        }
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::DroppedFile
    (
    QString strFilename
    )
{
    //Check file extension
    if (strFilename.right(3).toLower() == ".sb")
    {
        //smartBASIC source file - compile
        gchTermMode = MODE_COMPILE_LOAD;
        gstrTermFilename = strFilename;

        //Get the version number
        gbTermBusy = true;
        gchTermMode2 = 0;
        gchTermBusyLines = 0;
        gstrTermBusyData = tr("");
        gspSerialPort.write("at i 0");
        gintQueuedTXBytes += 6;
        DoLineEnd();
        gpMainLog->WriteLogData("at i 0\n");
        gspSerialPort.write("at i 13");
        gintQueuedTXBytes += 7;
        DoLineEnd();
        gpMainLog->WriteLogData("at i 13\n");
    }
    else
    {
        //Normal download
        gchTermMode = MODE_LOAD;
        gstrTermFilename = strFilename;
        gbTermBusy = true;
        LoadFile(false);

        //Download to the device
        gchTermMode2 = 1;
        QByteArray baTmpBA = QString("AT+DEL \"").append(gstrDownloadFilename).append("\" +").toUtf8();
        gspSerialPort.write(baTmpBA);
        gintQueuedTXBytes += baTmpBA.size();
        DoLineEnd();
        gpMainLog->WriteLogData(QString(baTmpBA).append("\n"));
        if (ui->check_SkipDL->isChecked() == false)
        {
            //Output download details
            if (ui->check_ShowCLRF->isChecked() == true)
            {
                //Escape \t, \r and \n
                baTmpBA.replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n");
            }

            //Replace unprintable characters
            baTmpBA.replace('\0', "\\00").replace("\x01", "\\01").replace("\x02", "\\02").replace("\x03", "\\03").replace("\x04", "\\04").replace("\x05", "\\05").replace("\x06", "\\06").replace("\x07", "\\07").replace("\x08", "\\08").replace("\x0b", "\\0B").replace("\x0c", "\\0C").replace("\x0e", "\\0E").replace("\x0f", "\\0F").replace("\x10", "\\10").replace("\x11", "\\11").replace("\x12", "\\12").replace("\x13", "\\13").replace("\x14", "\\14").replace("\x15", "\\15").replace("\x16", "\\16").replace("\x17", "\\17").replace("\x18", "\\18").replace("\x19", "\\19").replace("\x1a", "\\1a").replace("\x1b", "\\1b").replace("\x1c", "\\1c").replace("\x1d", "\\1d").replace("\x1e", "\\1e").replace("\x1f", "\\1f");

            //Update display buffer
            gbaDisplayBuffer.append(baTmpBA);
            if (!gtmrTextUpdateTimer.isActive())
            {
                gtmrTextUpdateTimer.start();
            }
        }
    }

    //Start the timeout timer
    gtmrDownloadTimeoutTimer.start();

    //Enable cancel button
    ui->btn_Cancel->setEnabled(true);
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_SaveDevice_clicked
    (
    )
{
    //Saves changes to a configuration
    ui->combo_PredefinedDevice->setItemText(ui->combo_PredefinedDevice->currentIndex(), ui->combo_PredefinedDevice->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number(((ui->combo_PredefinedDevice->currentIndex()+1)))).append("Name"), ui->combo_PredefinedDevice->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number(((ui->combo_PredefinedDevice->currentIndex()+1)))).append("Baud"), ui->combo_Baud->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number(((ui->combo_PredefinedDevice->currentIndex()+1)))).append("Parity"), ui->combo_Parity->currentIndex());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number(((ui->combo_PredefinedDevice->currentIndex()+1)))).append("Stop"), ui->combo_Stop->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number(((ui->combo_PredefinedDevice->currentIndex()+1)))).append("Data"), ui->combo_Data->currentText());
    gpPredefinedDevice->setValue(QString("Port").append(QString::number(((ui->combo_PredefinedDevice->currentIndex()+1)))).append("Flow"), ui->combo_Handshake->currentIndex());
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_OpenLog_clicked
    (
    )
{
    //Opens the UwTerminalX log file
    QDesktopServices::openUrl(QUrl::fromLocalFile(gpMainLog->GetLogName()));
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_OpenConfig_clicked
    (
    )
{
    //Opens the UwTerminalX configuration file
#if TARGET_OS_MAC
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString(gstrMacBundlePath).append("UwTerminalX.ini")));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile("UwTerminalX.ini"));
#endif
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_ModuleFirmware_clicked
    (
    )
{
    //Checks what the latest module firmware versions are
    if (gbTermBusy == false)
    {
        //Send request
        gbTermBusy = true;
        gchTermMode = MODE_CHECK_FIRMWARE_VERSIONS;
        gchTermMode2 = MODE_CHECK_FIRMWARE_VERSIONS;
        ui->btn_Cancel->setEnabled(true);
        ui->btn_ErrorCodeUpdate->setEnabled(false);
        ui->btn_UwTerminalXUpdate->setEnabled(false);
        ui->btn_ModuleFirmware->setEnabled(false);
        gnmManager->get(QNetworkRequest(QUrl(QString(WebProtocol).append("://").append(gpTermSettings->value("OnlineXCompServer", ServerHost).toString()).append("/firmwares.php"))));
        ui->statusBar->showMessage("Checking for latest firmware versions...");
    }
}

//=============================================================================
//=============================================================================
void
MainWindow::on_btn_LairdModules_clicked
    (
    )
{
    //Opens the Laird Bluetooh modules page
    QDesktopServices::openUrl(QUrl("http://www.lairdtech.com/products/category/741"));
}

//=============================================================================
//=============================================================================
void
MainWindow::ContextMenuClosed
    (
    )
{
    //Right click context menu closed, send message to text edit object
    ui->text_TermEditData->mbContextMenuOpen = false;
    ui->text_TermEditData->UpdateDisplay();
}

/******************************************************************************/
// END OF FILE
/******************************************************************************/
