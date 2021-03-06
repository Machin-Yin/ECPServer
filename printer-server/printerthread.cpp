#include "printerthread.h"
#include <QPrinterInfo>
#include <Tlhelp32.h>

//#define PRINTER_NUM 0
//#define COPIES_NUM 1
//#define DMPAPER_SIZE DMPAPER_A4

PrinterThread::PrinterThread(int socketDescriptor, QObject *parent)
	:QThread(parent), socketDescriptor(socketDescriptor)
{
	totalBytes = 0;
	bytesReceived = 0;
	fileNameSize = 0;
	blockSize = 0;
    cliPnum = 0;
    copyCount = 1;
    prn_name = "";
}


void PrinterThread::run()
{
    qDebug() << __FUNCTION__ << endl;
    tcpSocket = new QTcpSocket();
    if (!tcpSocket->setSocketDescriptor(socketDescriptor))
    {
        emit error(tcpSocket->error());
        tcpSocket->close();
        return;
    }
    while(1)
    {
        Sleep(1);
        while (tcpSocket->waitForReadyRead())
        {
            QString bl = recMessage();
            qDebug() << "bl==="<<bl;
            if (bl == "Request printer list!")
            {
                //                sendMessage("Printlist\r\n");
                qDebug() << "request printer list";
                QPrinterInfo pInfo;
                QStringList pname;
                pname = pInfo.availablePrinterNames();
                //pname.prepend("PList");
                qDebug() << "panme:" << pname;
                foreach(auto a, pname)
                {
                    qDebug() << a;
                }
                QString pstr = pname.join(",");

                sendMessage("Printlist\r\n"+pstr);

            }
            else if (bl == "begin send file") {
                qDebug() << "begin send file";
                while (tcpSocket->waitForReadyRead())
                {
                    if (recFile())
                        break;
                }
            }
            else if(bl == "DefaultPrinter"){
                while (tcpSocket->waitForReadyRead())
                {
                    QString prn_nameornum = recMessage();
                    if(prn_nameornum.size() > 2)
                    {
                        prn_name = prn_nameornum;
                        qDebug() << "prn_name==" << prn_name << endl;
                    }
                    else
                    {
                        cliPnum = prn_nameornum.toInt();
                        qDebug() << "cliPnum==" << cliPnum << endl;
                    }
                    break;
                }

            }
            else if(bl == "CopyCount"){
                while(tcpSocket->waitForReadyRead())
                {
                    QString copy = recMessage();
                    copyCount = copy.toShort();
                    qDebug() << "copyCount ==" << copyCount <<endl;
                    break;
                }
            }

            else if(bl == "License")

            {
                qDebug() << "License"<<bl;
                while(tcpSocket->waitForReadyRead())
                {
                    QString au = recMessage();
                    qDebug() << "authcode"<<au;

                    QSettings setting("authcode.ini",QSettings::IniFormat);//�������ļ�
                    setting.beginGroup("authcode");
                    QString authcode=setting.value("authcode").toString();
                    setting.endGroup();
                    if (au != authcode)
                    {
                        sendMessage("AUTH WRONG");
                        break;
                    }
                    else
                    {
                        sendMessage("OK");
                        break;
                    }
                }
            }
            else
            {
                break;
            }

        }
        //mutex.unlock();
    }
    //    exec();
}



void PrinterThread::sendMessage(QString messtr)
{
    qDebug() << "sendMessage()==" << messtr << endl;
	QByteArray authblock;
	QDataStream out(&authblock, QIODevice::WriteOnly);
	out << (quint16)0 <<messtr;
	out.device()->seek(0);
	out << (quint16)(authblock.size() - sizeof(quint16));
	tcpSocket->write(authblock);
	tcpSocket->flush();
}

QString PrinterThread::recMessage()
{
	//mutex.lock();
	qDebug() << __FUNCTION__ << endl;
	QDataStream in(tcpSocket);
	if (blockSize == 0)
	{
		qDebug() <<"blockSize == 0 tcpSocket->bytesAvailable()"<< tcpSocket->bytesAvailable() << endl;
		if (tcpSocket->bytesAvailable() < (int)sizeof(quint16))
		{
			qDebug() << "return" << endl;
			return 0;
		}
		in >> blockSize;
	}
	if (tcpSocket->bytesAvailable() < blockSize)
	{
		return 0;
	}
	in >> message;
	qDebug() << "message ==" << message << endl;
	blockSize = 0;
	//mutex.unlock();
	return message;
}


bool PrinterThread::recFile() 
{
    qDebug() << __FUNCTION__ << endl;
    mutex.lock();
    tempFolder = QDir::tempPath();
    QDataStream in(tcpSocket);
    qDebug() <<"tcpSocket->bytesAvailable()"<< tcpSocket->bytesAvailable() << endl;
    while (tcpSocket->bytesAvailable() > 0)
    {
        qDebug() << "rec begin" << endl;
        qDebug() << "bytesReceived======" << bytesReceived << endl;
        if (bytesReceived < sizeof(qint64) * 2)
        {
            qDebug() << "bytesReceived < sizeof(qint64) * 2***********" << endl;
            if ((tcpSocket->bytesAvailable() >= sizeof(qint64) * 2) && (fileNameSize == 0))
            {
                in >> totalBytes >> fileNameSize;
                bytesReceived += sizeof(qint64) * 2;
                qDebug() << "totalBytes=" << totalBytes << endl;

                qDebug() << "bytesReceived=" << bytesReceived << endl;
            }
            qDebug() << "fileNameSize=" << fileNameSize << endl;

            qDebug() << "before (tcpSocket->bytesAvailable() >= fileNameSize)" << tcpSocket->bytesAvailable()<< "fileNameSize="<< fileNameSize<< endl;
            if ((tcpSocket->bytesAvailable() >= fileNameSize)
                    && (fileNameSize != 0))
            {
                qDebug() << "(tcpSocket->bytesAvailable() >= fileNameSize)"<< endl;
                in >> fileName;


                qDebug() << "fileName=" << fileName << endl;
                bytesReceived += fileNameSize;
                qDebug() << "(tcpSocket->bytesAvailable()=" << tcpSocket->bytesAvailable() << endl;
                qDebug() << "bytesReceived=" << bytesReceived << endl;
                tempFile = tempFolder + "/" + fileName;
                localFile = new QFile(tempFile);
                if (!localFile->open(QFile::ReadWrite))
                {
                    qDebug() << "open file error!";
                    mutex.unlock();
                    return true;
                }
            }
            else
            {
                mutex.unlock();
                return true;
            }
        }
        qDebug() << "before  bytesReceived < totalBytes" << endl;
        if (bytesReceived < totalBytes)
        {
            qDebug() << "bytesReceived < totalBytes" << endl;
            bytesReceived += tcpSocket->bytesAvailable();
            qDebug() << "bytesReceived==" << bytesReceived << endl;
            qDebug() << "totalBytes==" << totalBytes << endl;
            inBlock = tcpSocket->readAll();
            localFile->write(inBlock);
            inBlock.resize(0);
        }

        qDebug() << "bytesReceived" << bytesReceived << endl;
        if (bytesReceived == (totalBytes+16))

        {
            qDebug() << "bytesReceived == totalBytes:  Receive" << fileName << "success!" << endl;

            bytesReceived = 0;
            //tcpSocket->close();
            fileNameSize = 0;
            localFile->close();
            delete localFile;
            //setDefPrinter(PRINTER_NUM, fileName);
            if(prn_name!="")
                setDefPrinter(prn_name, tempFile);
            else
                setDefPrinter(cliPnum, tempFile);
            mutex.unlock();
            return true;
        }
    }
    qDebug() << "rec over" << endl;
    mutex.unlock();
    return false;
}


//void PrinterThread::displayError(QAbstractSocket::SocketError)
//{
//	qDebug() << tcpSocket->errorString();
//	tcpSocket->close();
//}

void PrinterThread::setDefPrinter(QString printer_name,QString fileName1)
{
    qDebug() << __FUNCTION__ << endl;



    //QString printerName = printer.printerName;
    //BOOL setret = FALSE;
    LPCWSTR printerName = (const wchar_t*)printer_name.utf16();
    SetDefaultPrinter(printerName);
    //SetPrinter((const wchar_t*)printer_name.utf16(),);

    /****** Set printer property! ******/

    //LONG lSize = 0;
    LPDEVMODE lpDevMode = NULL;
    HANDLE hPrinter;
    DWORD dwNeeded, dwRet;

    TCHAR defPrinter[256] = { 0 };
    memset(defPrinter, 0, 256);
    DWORD lengthDefpr = 256;
    GetDefaultPrinter(defPrinter, &lengthDefpr);
    qDebug() << "defPrinter===" << defPrinter << endl;
    //	LPDEVMODE defdevmode = getDefaultPdevmode(hPrinter);
    qDebug() << "defPrinter===" << defPrinter << endl;
    if (!OpenPrinter(defPrinter, &hPrinter, NULL))
    {
        qDebug() << "OpenPrinter==" << !OpenPrinter(defPrinter, &hPrinter, NULL) << endl;
        return;
    }

    //get real size of DEVMODE
    dwNeeded = DocumentProperties(NULL, hPrinter, defPrinter, NULL, NULL, 0);
    lpDevMode = (LPDEVMODE)malloc(dwNeeded);
    dwRet = DocumentProperties(NULL, hPrinter, defPrinter, lpDevMode, NULL, DM_OUT_BUFFER);
    qDebug() << "dwRet==" << dwRet << endl;
    if (dwRet != IDOK)
    {
        free(lpDevMode);
        ClosePrinter(hPrinter);
        return;
    }
    if (lpDevMode->dmFields & DM_COPIES)
    {
        lpDevMode->dmCopies = copyCount;
        lpDevMode->dmFields |= DM_COPIES;
    }
    if (lpDevMode->dmFields & DM_ORIENTATION)
    {
        /* If the printer supports paper orientation, set it.*/
        lpDevMode->dmOrientation = DMORIENT_LANDSCAPE;  //landscape:��    portrait:��
        lpDevMode->dmOrientation |= DM_ORIENTATION;
    }
//	if (lpDevMode->dmFields & DM_PAPERSIZE)
//	{
//		lpDevMode->dmPaperSize = DMPAPER_SIZE;
//		lpDevMode->dmOrientation |= DM_PAPERSIZE;
//	}
    dwRet = DocumentProperties(NULL, hPrinter, defPrinter, lpDevMode, lpDevMode, DM_IN_BUFFER | DM_OUT_BUFFER);
    //ClosePrinter(hPrinter);
    if (dwRet != IDOK)
    {
        free(lpDevMode);
        return;
    }

    //HDC hdc = CreateDC( (LPCWSTR)(_T("winspool").AllocSysString(), printerName , NULL, lpDevMode);
    DWORD dw;
    PRINTER_INFO_2 *pi2;
    GetPrinter(hPrinter, 2, NULL, 0, &dw);
    pi2 = (PRINTER_INFO_2*)GlobalAllocPtr(GHND, dw);
    GetPrinter(hPrinter, 2, (LPBYTE)pi2, dw, &dw);

    qDebug() << "pi2->pDevMode before" << pi2->pDevMode << endl;
    qDebug() << "lpDevMode before" << lpDevMode << endl;

    pi2->pDevMode = lpDevMode;
    SetPrinter(hPrinter, 2, (LPBYTE)pi2, 0);

    QString filePath = fileName1;
    doPrint(filePath);
    prn_name = "";

    ClosePrinter(hPrinter);
    GlobalFreePtr(pi2);

    //_sleep(10 * 1000);
//	SetDefaultPrinter(szBufferDefaultPrinterName);
    //	setDefaultPdevmode(hPrinter, defdevmode);   ////

}


void PrinterThread::setDefPrinter(int num,QString fileName1)
{
	qDebug() << __FUNCTION__ << endl;

	QPrinterInfo pInfo;
	QStringList pname;
	pname = pInfo.availablePrinterNames();
	qDebug() << "panme:" << pname;
	foreach(auto a, pname)
	{
		qDebug() << a;
	}

	int printer_num = num;
	QPrinter printer;
	QString printer_name;
	printer_name = pname.at(printer_num);
	printer.setPrinterName(printer_name);
	qDebug() << "class::printerName:" << printer.printerName() << endl;

	TCHAR szBufferDefaultPrinterName[256] = { 0 };
	memset(szBufferDefaultPrinterName, 0, 256);
	DWORD length = 256;
	GetDefaultPrinter(szBufferDefaultPrinterName, &length);
	qDebug() << "szBufferDefaultPrinterName===" << szBufferDefaultPrinterName << endl;

	//QString printerName = printer.printerName;
	//BOOL setret = FALSE;
	LPCWSTR printerName = (const wchar_t*)printer_name.utf16();
	SetDefaultPrinter(printerName);
	//SetPrinter((const wchar_t*)printer_name.utf16(),);

	/****** Set printer property! ******/

	//LONG lSize = 0;
	LPDEVMODE lpDevMode = NULL;
	HANDLE hPrinter;
	DWORD dwNeeded, dwRet;

	TCHAR defPrinter[256] = { 0 };
	memset(defPrinter, 0, 256);
	DWORD lengthDefpr = 256;
	GetDefaultPrinter(defPrinter, &lengthDefpr);
	qDebug() << "defPrinter===" << defPrinter << endl;
	//	LPDEVMODE defdevmode = getDefaultPdevmode(hPrinter);
	qDebug() << "defPrinter===" << defPrinter << endl;
	if (!OpenPrinter(defPrinter, &hPrinter, NULL))
	{
		qDebug() << "OpenPrinter==" << !OpenPrinter(defPrinter, &hPrinter, NULL) << endl;
		return;
	}

	//get real size of DEVMODE
	dwNeeded = DocumentProperties(NULL, hPrinter, defPrinter, NULL, NULL, 0);
	lpDevMode = (LPDEVMODE)malloc(dwNeeded);  
	dwRet = DocumentProperties(NULL, hPrinter, defPrinter, lpDevMode, NULL, DM_OUT_BUFFER);
	qDebug() << "dwRet==" << dwRet << endl;
	if (dwRet != IDOK)
	{
		free(lpDevMode);
		ClosePrinter(hPrinter);
		return;
	}
	if (lpDevMode->dmFields & DM_COPIES)
	{
        lpDevMode->dmCopies = copyCount;
		lpDevMode->dmFields |= DM_COPIES;
	}
	if (lpDevMode->dmFields & DM_ORIENTATION)
	{
		/* If the printer supports paper orientation, set it.*/
		lpDevMode->dmOrientation = DMORIENT_LANDSCAPE;  //landscape:��    portrait:��
		lpDevMode->dmOrientation |= DM_ORIENTATION;
	}
//	if (lpDevMode->dmFields & DM_PAPERSIZE)
//	{
//		lpDevMode->dmPaperSize = DMPAPER_SIZE;
//		lpDevMode->dmOrientation |= DM_PAPERSIZE;
//	}
	dwRet = DocumentProperties(NULL, hPrinter, defPrinter, lpDevMode, lpDevMode, DM_IN_BUFFER | DM_OUT_BUFFER);
	//ClosePrinter(hPrinter);
	if (dwRet != IDOK)
	{
		free(lpDevMode);
		return;
	}

	//HDC hdc = CreateDC( (LPCWSTR)(_T("winspool").AllocSysString(), printerName , NULL, lpDevMode);
	DWORD dw;
	PRINTER_INFO_2 *pi2;
	GetPrinter(hPrinter, 2, NULL, 0, &dw);
	pi2 = (PRINTER_INFO_2*)GlobalAllocPtr(GHND, dw);
	GetPrinter(hPrinter, 2, (LPBYTE)pi2, dw, &dw);

	qDebug() << "pi2->pDevMode before" << pi2->pDevMode << endl;
	qDebug() << "lpDevMode before" << lpDevMode << endl;

	pi2->pDevMode = lpDevMode;
	SetPrinter(hPrinter, 2, (LPBYTE)pi2, 0);

	QString filePath = fileName1;
    doPrint(filePath);

	ClosePrinter(hPrinter);
	GlobalFreePtr(pi2);

	//_sleep(10 * 1000);
//	SetDefaultPrinter(szBufferDefaultPrinterName);
    //	setDefaultPdevmode(hPrinter, defdevmode);   ////

}


void PrinterThread::doPrint(QString fileName2)
{
	int ret = 0;   
    ret = (int)ShellExecute(NULL,
        QString("open").toStdWString().c_str(),
        QString("SumatraPDF.exe").toStdWString().c_str(),
        QString("-print-to-default %1 ").arg(fileName2).toStdWString().c_str(),
        NULL,
        SW_HIDE);

    if(ret>32)
    {
        sendMessage("PrintSuccess");
    }
    else
    {
        sendMessage("PrintFailure");
        qDebug() << "PrintFailure" ;
    }


	qDebug() << "ret====" << ret << endl;
    _sleep(10 * 1000);

	remTerm(fileName2);
	qDebug() << "delete" << fileName2 << "success!" << endl;

}

void PrinterThread::remTerm(QString fileName3)
{
    int n=0;
    while(1)
    {
        _sleep(1 * 1000);
        n++;
        if ((QFile::remove(fileName3) == 1) || (n==20))
        {
            qDebug() << "delete" << fileName3 << "success!" << endl;
            break;
        }
        qDebug() << "delete" << fileName3 << "failure!" << endl;

    }
}



