/***************************************************************************
                          kgpginterface.cpp  -  description
                             -------------------
    begin                : Mon Jul 8 2002
    copyright          : (C) 2002 by Jean-Baptiste Mardelle
    email                : bj@altern.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QTextStream>
#include <QTextCodec>
#include <QClipboard>
#include <QFile>

#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <ktempfile.h>
#include <kpassdlg.h>
#include <klocale.h>
#include <kcodecs.h>
#include <kprocio.h>
#include <kconfig.h>
#include <kdebug.h>

#include "kgpginterface.h"
#include "detailedconsole.h"

KgpgInterface::KgpgInterface()
{
}

int KgpgInterface::getGpgVersion()
{
    KProcIO p;
    QString line;
    QString gpgString;

    p << "gpg" << "--version";
    p.start();
    if (p.readln(line) != -1)
        gpgString = line.simplified().section(' ', -1);

    return (100 * gpgString.section('.', 0, 0).toInt() + 10 * gpgString.section('.', 1, 1).toInt() + gpgString.section('.', 2, 2).toInt());
}

QString KgpgInterface::checkForUtf8(QString txt)
{
    // code borrowed from gpa
    const char *s;

    /* Make sure the encoding is UTF-8.
     * Test structure suggested by Werner Koch */
    if (txt.isEmpty())
        return QString::null;

    for (s = txt.ascii(); *s && !(*s & 0x80); s++)
                ;
        if (*s && !strchr (txt.ascii(), 0xc3) && (txt.find("\\x") == -1))
                return txt;

    /* The string is not in UTF-8 */
    //if (strchr (txt.ascii(), 0xc3)) return (txt+" +++");
    if (txt.find("\\x") == -1)
        return QString::fromUtf8(txt.ascii());

    // if (!strchr (txt.ascii(), 0xc3) || (txt.find("\\x")!=-1)) {
    for (int idx = 0; (idx = txt.find( "\\x", idx )) >= 0 ; ++idx)
    {
        char str[2] = "x";
        str[0] = (char) QString(txt.mid(idx + 2, 2)).toShort(0, 16);
        txt.replace(idx, 4, str);
    }
//  if (!strchr (txt.ascii(), 0xc3))
    return QString::fromUtf8(txt.ascii());
//        else
//                return QString::fromUtf8(QString::fromUtf8(txt.ascii()).ascii());  // perform Utf8 twice, or some keys display badly
}

QString KgpgInterface::checkForUtf8bis(QString txt)
{
    if (strchr(txt.ascii(), 0xc3) || (txt.find("\\x") != -1))
        txt = checkForUtf8(txt);
    else
    {
        txt = checkForUtf8(txt);
        txt = QString::fromUtf8(txt.ascii());
    }
    return txt;
}

QStringList KgpgInterface::getGpgGroupNames(const QString &configfile)
{
    QStringList groups;
    QFile qfile(QFile::encodeName(configfile));
    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            result = result.simplified();
            if (result.startsWith("group "))
            {
                result.remove(0, 6);
                groups << result.section("=", 0, 0).simplified();
            }
            result = t.readLine();
        }
        qfile.close();
    }
    return groups;
}

QStringList KgpgInterface::getGpgGroupSetting(const QString &name, const QString &configfile)
{
    QFile qfile(QFile::encodeName(configfile));
    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            result = result.simplified();
            if (result.startsWith("group "))
            {
                kdDebug(2100) << "Found 1 group" << endl;
                result.remove(0, 6);
                if (result.simplified().startsWith(name))
                {
                    kdDebug(2100) << "Found group: " << name << endl;
                    result = result.section('=', 1);
                    result=result.section('#', 0, 0);
                    return QStringList::split (" ", result);
                }
            }
            result = t.readLine();
        }
        qfile.close();
    }
    return QStringList();
}

void KgpgInterface::setGpgGroupSetting(const QString &name, const QStringList &values, const QString &configfile)
{
    QString texttowrite;
    bool found = false;
    QFile qfile(QFile::encodeName(configfile));

    kdDebug(2100) << "Changing group: " << name << endl;
    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            if (result.simplified().startsWith("group "))
            {
                QString result2 = result.simplified();
                result2.remove(0, 6);
                result2 = result2.simplified();
                if (result2.startsWith(name) && (result2.remove(0, name.length()).simplified().startsWith("=")))
                {
                    // kdDebug(2100) << "Found group: " << name << endl;
                    // kdDebug(2100) << "New values: " << values << endl;
                    result = QString("group %1=%2").arg(name).arg(values.join(" "));
                    found = true;
                }
            }
            texttowrite += result + "\n";
            result = t.readLine();
        }
        qfile.close();

        if (!found)
            texttowrite += "\n" + QString("group %1=%2").arg(name).arg(values.join(" "));

        if (qfile.open(QIODevice::WriteOnly))
        {
            QTextStream t(&qfile);
            t << texttowrite;
            qfile.close();
        }
    }
}

void KgpgInterface::delGpgGroup(const QString &name, const QString &configfile)
{
    QString texttowrite;
    QFile qfile(QFile::encodeName(configfile));
    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            if (result.simplified().startsWith("group "))
            {
                QString result2 = result.simplified();
                result2.remove(0, 6);
                result2 = result2.simplified();
                if (result2.startsWith(name) && (result2.remove(0, name.length()).simplified().startsWith("=")))
                    result = QString::null;
            }

            texttowrite += result + "\n";
            result = t.readLine();
        }

        qfile.close();
        if (qfile.open(QIODevice::WriteOnly))
        {
            QTextStream t(&qfile);
            t << texttowrite;
            qfile.close();
        }
    }
}

QString KgpgInterface::getGpgSetting(QString name, const QString &configfile)
{
    name = name.simplified() + " ";
    QFile qfile(QFile::encodeName(configfile));
    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            if (result.simplified().startsWith(name))
            {
                result = result.simplified();
                result.remove(0, name.length());
                result = result.simplified();
                return result.section(" ", 0, 0);
            }
            result = t.readLine();
        }
        qfile.close();
    }

    return QString::null;
}

void KgpgInterface::setGpgSetting(const QString &name, const QString &value, const QString &url)
{
    QString temp = name + " ";
    QString texttowrite;
    bool found = false;
    QFile qfile(QFile::encodeName(url));

    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            if (result.simplified().startsWith(temp))
            {
                if (!value.isEmpty())
                    result = temp + " " + value;
                else
                    result = QString::null;
                found = true;
            }

            texttowrite += result + "\n";
            result = t.readLine();
        }

        qfile.close();
        if ((!found) && (!value.isEmpty()))
            texttowrite += "\n" + temp + " " + value;

        if (qfile.open(QIODevice::WriteOnly))
        {
            QTextStream t(&qfile);
            t << texttowrite;
            qfile.close();
        }
    }
}

bool KgpgInterface::getGpgBoolSetting(const QString &name, const QString &configfile)
{
    QFile qfile(QFile::encodeName(configfile));
    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();
        while (result != NULL)
        {
            if (result.simplified().startsWith(name))
                return true;
            result = t.readLine();
        }
        qfile.close();
    }
    return false;
}

void KgpgInterface::setGpgBoolSetting(const QString &name, const bool &enable, const QString &url)
{
    QString texttowrite;
    bool found = false;
    QFile qfile(QFile::encodeName(url));

    if (qfile.open(QIODevice::ReadOnly) && (qfile.exists()))
    {
        QString result;
        QTextStream t(&qfile);
        result = t.readLine();

        while (result != NULL)
        {
            if (result.simplified().startsWith(name))
            {
                if (enable)
                    result = name;
                else
                    result = QString::null;

                found = true;
            }

            texttowrite += result+"\n";
            result = t.readLine();
        }
        qfile.close();

        if ((!found) && (enable))
            texttowrite += name;

        if (qfile.open(QIODevice::WriteOnly))
        {
            QTextStream t(&qfile);
            t << texttowrite;
            qfile.close();
        }
    }
}

int KgpgInterface::checkUID(const QString &keyid)
{
    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--with-colon" << "--list-sigs" << keyid;
    process->start(KProcess::Block, false);

    int  uidcnt = 0;
    QString line;
    while (process->readln(line, true) != -1)
        if (line.startsWith("uid"))
            uidcnt++;

    delete process;
    return uidcnt;
}

int KgpgInterface::sendPassphrase(const QString &text, KProcess *process, const bool isnew)
{
    QByteArray passphrase;
    int code;
    if (isnew)
        code = KPasswordDialog::getNewPassword(0, passphrase, text);
    else
        code = KPasswordDialog::getPassword(0, passphrase, text);

    if (code != KPasswordDialog::Accepted)
        return 1;

    passphrase.append('\n');
    process->writeStdin(passphrase, passphrase.length());

    // This will erase the password in the memory
    // If passphrase contains "password", after that line, it will contains "xxxxxxxx".
    // This is more secure.
    passphrase.fill('x');
    passphrase = QByteArray();

    return 0;
}

void KgpgInterface::updateIDs(QString txt)
{
    int cut = txt.find(' ', 22, false);
    txt.remove(0, cut);

    if (txt.find("(", 0, false) != -1)
        txt = txt.section('(', 0, 0) + txt.section(')', -1);

    txt.replace(QRegExp("<"), "&lt;");

    if (userIDs.find(txt) == -1)
    {
        if (!userIDs.isEmpty())
            userIDs += i18n(" or ");
        userIDs += txt;
    }
}

KgpgListKeys KgpgInterface::readPublicKeys(const bool &block, const QStringList &ids)
{
    m_partialline = QString::null;
    m_ispartial = false;
    m_publiclistkeys = KgpgListKeys();
    m_publickey = 0;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--status-fd=2" << "--command-fd=0" << "--with-colon" << "--with-fingerprint" << "--list-keys";
    for (int i = 0; i < ids.count(); ++i)
        *process << ids.at(i);

    if (!block)
    {
        kdDebug(2100) << "(KgpgInterface::readPublicKeys) Extract public keys with KProcess::NotifyOnExit" << endl;
        connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(readPublicKeysProcess(KProcIO *)));
        connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(readPublicKeysFin(KProcess *)));
        process->start(KProcess::NotifyOnExit, false);
        emit readPublicKeysStarted(this);
        return KgpgListKeys();
    }
    else
    {
        kdDebug(2100) << "(KgpgInterface::readPublicKeys) Extract public keys with KProcess::Block" << endl;
        process->start(KProcess::Block, false);
        readPublicKeysProcess(process);
        readPublicKeysFin(process, true);
        return m_publiclistkeys;
    }
}

void KgpgInterface::readPublicKeysProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.startsWith("pub"))
            {
                if (m_publickey)
                    m_publiclistkeys << m_publickey;

                m_publickey = new KgpgKey();
                m_publickey->gpgsecretkey = false;

                QString algo = line.section(':', 3, 3);
                m_publickey->gpgkeyalgo = algo.toInt();

                QString trust = line.section(':', 1, 1);
                m_publickey->gpgkeytrust = trust[0];

                m_publickey->gpgkeyvalide = true;
                if (line.section(':', 11, 11).find("D", 0, true) != -1)  // disabled key
                    m_publickey->gpgkeyvalide = false;

                QString fullid = line.section(':', 4, 4);
                m_publickey->gpgfullid = fullid;
                m_publickey->gpgkeyid = fullid.right(8);

                QDate date = QDate::fromString(line.section(':', 5, 5), Qt::ISODate);
                m_publickey->gpgkeycreation = date;

                if (line.section(':', 6, 6).isEmpty())
                {
                    m_publickey->gpgkeyunlimited = true;
                    m_publickey->gpgkeyexpiration = QDate();
                }
                else
                {
                    m_publickey->gpgkeyunlimited = false;
                    date = QDate::fromString(line.section(':', 6, 6), Qt::ISODate);
                    m_publickey->gpgkeyexpiration = date;
                }

                m_publickey->gpgkeysize = line.section(':', 2, 2);

                QString otrust = line.section(':', 8, 8);
                m_publickey->gpgkeyownertrust = otrust[0];

                QString fullname = line.section(':', 9, 9);
                if (fullname.find("<") != -1)
                {
                    QString kmail = fullname;

                    if (fullname.find(")") != -1)
                        kmail = kmail.section(')', 1);

                    kmail = kmail.section('<', 1);
                    kmail.truncate(kmail.length() - 1);

                    if (kmail.find("<") != -1) // several email addresses in the same key
                    {
                        kmail = kmail.replace(">", ";");
                        kmail.remove("<");
                    }

                    m_publickey->gpgkeymail = kmail;
                }
                else
                    m_publickey->gpgkeymail = QString();

                QString kname = fullname.section('<', 0, 0);
                if (fullname.find("(") != -1)
                {
                    kname = kname.section('(', 0, 0);
                    QString comment = fullname.section('(', 1, 1);
                    comment = comment.section(')', 0, 0);

                    m_publickey->gpgkeycomment = comment;
                }
                else
                    m_publickey->gpgkeycomment = QString();
                m_publickey->gpgkeyname = kname;
            }

            if (line.startsWith("fpr"))
            {
                QString fingervalue = line.section(':', 9, 9);
                uint len = fingervalue.length();
                if ((len > 0) && (len % 4 == 0))
                    for (uint n = 0; 4 * (n + 1) < len; ++n)
                        fingervalue.insert(5 * n + 4, ' ');

                m_publickey->gpgkeyfingerprint = fingervalue;
            }

            if (line.startsWith("uat"))
            {
                m_publickey->gpgkeynumberuat += 1;
                m_publickey->gpghasphoto = true;
            }

            if (line.startsWith("uid"))
                m_publickey->gpgkeynumberuid += 1;
        }
    }

    p->ackRead();
}

void KgpgInterface::readPublicKeysFin(KProcess *p, const bool &block)
{
    // insert the last key
    if (m_publickey)
        m_publiclistkeys << m_publickey;

    delete p;
    if (!block)
        emit readPublicKeysFinished(m_publiclistkeys, this);
}


KgpgListKeys KgpgInterface::readSecretKeys(const bool &block, const QStringList &ids)
{
    m_partialline = QString::null;
    m_ispartial = false;
    m_secretlistkeys = KgpgListKeys();
    m_secretkey = 0;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--status-fd=2" << "--command-fd=0" << "--with-colon" << "--list-secret-keys";
    for (int i = 0; i < ids.count(); ++i)
        *process << ids.at(i);

    if (!block)
    {
        kdDebug(2100) << "(KgpgInterface::readSecretKeys) Extract secret keys with KProcess::NotifyOnExit" << endl;
        connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(readSecretKeysProcess(KProcIO *)));
        connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(readSecretKeysFin(KProcess *)));
        process->start(KProcess::NotifyOnExit, false);
        emit readSecretKeysStarted(this);
        return KgpgListKeys();
    }
    else
    {
        kdDebug(2100) << "(KgpgInterface::readSecretKeys) Extract secret keys with KProcess::Block" << endl;
        process->start(KProcess::Block, false);
        readSecretKeysProcess(process);
        readSecretKeysFin(process, true);
        return m_secretlistkeys;
    }
}

void KgpgInterface::readSecretKeysProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.startsWith("sec"))
            {
                if (m_secretkey)
                    m_secretlistkeys << m_secretkey;

                m_secretkey = new KgpgKey();
                m_secretkey->gpgsecretkey = true;

                QString algo = line.section(':', 3, 3);
                m_secretkey->gpgkeyalgo = algo.toInt();

                QString trust = line.section(':', 1, 1);
                m_secretkey->gpgkeytrust = trust[0];

                QString fullid = line.section(':', 4, 4);
                m_secretkey->gpgfullid = fullid;
                m_secretkey->gpgkeyid = fullid.right(8);

                QDate date = QDate::fromString(line.section(':', 5, 5), Qt::ISODate);
                m_secretkey->gpgkeycreation = date;

                if (line.section(':', 6, 6).isEmpty())
                {
                    m_secretkey->gpgkeyunlimited = true;
                    m_secretkey->gpgkeyexpiration = QDate();
                }
                else
                {
                    m_secretkey->gpgkeyunlimited = false;
                    date = QDate::fromString(line.section(':', 6, 6), Qt::ISODate);
                    m_secretkey->gpgkeyexpiration = date;
                }

                m_secretkey->gpgkeysize = line.section(':', 2, 2);

                QString fullname = line.section(':', 9, 9);
                if (fullname.find("<") != -1)
                {
                    QString kmail = fullname;

                    if (fullname.find(")") != -1)
                        kmail = kmail.section(')', 1);

                    kmail = kmail.section('<', 1);
                    kmail.truncate(kmail.length() - 1);

                    if (kmail.find("<") != -1) // several email addresses in the same key
                    {
                        kmail = kmail.replace(">", ";");
                        kmail.remove("<");
                    }

                    m_secretkey->gpgkeymail = kmail;
                }
                else
                    m_secretkey->gpgkeymail = QString();

                QString kname = fullname.section('<', 0, 0);
                if (fullname.find("(") != -1)
                {
                    kname = kname.section('(', 0, 0);
                    QString comment = fullname.section('(', 1, 1);
                    comment = comment.section(')', 0, 0);

                    m_secretkey->gpgkeycomment = comment;
                }
                else
                    m_secretkey->gpgkeycomment = QString();
                m_secretkey->gpgkeyname = kname;
            }

            if (line.startsWith("uat"))
            {
                m_secretkey->gpgkeynumberuat += 1;
                m_secretkey->gpghasphoto = true;
            }

            if (line.startsWith("uid"))
                m_secretkey->gpgkeynumberuid += 1;
        }
    }

    p->ackRead();
}

void KgpgInterface::readSecretKeysFin(KProcess *p, const bool &block)
{
    if (m_secretkey)
        m_secretlistkeys << m_secretkey;

    delete p;
    if (!block)
        emit readSecretKeysFinished(m_secretlistkeys, this);
}

QStringList KgpgInterface::getPhotoList(const QString &keyid, const bool &block)
{
    photolist = QStringList();
    userIDs = keyid;

    if (!block)
    {
        kdDebug(2100) << "(KgpgInterface::getPhotoList) Get photo list with KProcess::NotifyOnExit" << endl;
        connect(this, SIGNAL(readPublicKeysFinished(KgpgListKeys, KgpgInterface*)), this, SLOT(getPhotoListProcess(KgpgListKeys, KgpgInterface*)));
        readPublicKeys(false, QStringList(keyid));
        return QStringList();
    }
    else
    {
        kdDebug(2100) << "(KgpgInterface::getPhotoList) Get photo list with KProcess::Block" << endl;
        KgpgListKeys list = readPublicKeys(true, QStringList(keyid));
        getPhotoListProcess(list, 0, true);
        return photolist;
    }
}

void KgpgInterface::getPhotoListProcess(KgpgListKeys listkeys, KgpgInterface*, const bool &block)
{
    uint number = 1;
    number += (listkeys.at(0))->gpgkeynumberuat;
    number += (listkeys.at(0))->gpgkeynumberuid;

    QStringList photolist;
    for (uint i = 1; i <= number; ++i)
        if (isPhotoId(i))
            photolist += QString::number(i);

    if (!block)
        emit getPhotoListFinished(photolist, this);
}

bool KgpgInterface::isPhotoId(uint uid)
{
    KTempFile *kgpginfotmp = new KTempFile();
    kgpginfotmp->setAutoDelete(true);

    QString pgpgoutput = "cp %i " + kgpginfotmp->name();

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0";
    *process << "--photo-viewer" << QFile::encodeName(pgpgoutput) << "--edit-key" << userIDs << "uid" << QString::number(uid) << "showphoto";
    process->start(KProcess::Block);

    if (kgpginfotmp->file()->size() > 0)
    {
        kgpginfotmp->unlink();
        return true;
    }

    kgpginfotmp->unlink();
    return false;
}

QString KgpgInterface::getKeys(const bool &block, const bool &attributes, const QStringList &ids)
{
    m_partialline = QString::null;
    m_ispartial = false;
    m_keystring = QString::null;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0";
    *process << "--export" << "--armor";

    if (!attributes)
        *process << "--export-options" << "no-include-attributes";

    for (QStringList::ConstIterator it = ids.begin(); it != ids.end(); ++it)
        *process << *it;

    if (!block)
    {
        kdDebug(2100) << "(KgpgInterface::getKeys) Get a key with KProcess::NotifyOnExit" << endl;
        connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(getKeysProcess(KProcIO *)));
        connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(getKeysFin(KProcess *)));
        process->start(KProcess::NotifyOnExit, false);
        emit getKeysStarted(this);
        return QString();
    }
    else
    {
        kdDebug(2100) << "(KgpgInterface::getKeys) Get a key with KProcess::Block" << endl;
        process->start(KProcess::Block, false);
        getKeysProcess(process);
        delete process;
        return m_keystring;
    }
}

void KgpgInterface::getKeysProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (!line.startsWith("gpg:"))
                m_keystring += line + "\n";
        }
    }
    p->ackRead();
}

void KgpgInterface::getKeysFin(KProcess *p)
{
    delete p;
    emit getKeysFinished(m_keystring, this);
}

void KgpgInterface::encryptText(const QString &text, const QStringList &userids, const QStringList &options)
{
    m_partialline = QString::null;
    m_ispartial = false;
    message = QString::null;

    QTextCodec *codec = QTextCodec::codecForLocale();
    if (codec->canEncode(text))
        message = text;
    else
        message = text.utf8();

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--command-fd=0" << "--status-fd=1";

    for (QStringList::ConstIterator it = options.begin(); it != options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    if (!userids.isEmpty())
    {
        *process << "-e";
        for (QStringList::ConstIterator it = userids.begin(); it != userids.end(); ++it)
            *process << "--recipient" << *it;
    }
    else
        *process << "-c";

    kdDebug(2100) << "(KgpgInterface::encryptText) Encrypt a text" << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(encryptTextProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(encryptTextFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, false);
}

void KgpgInterface::encryptTextProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("BEGIN_ENCRYPTION") != -1)
            {
                emit txtEncryptionStarted();
                p->writeStdin(message, false);
                p->closeWhenDone();
                message.fill('x');
                message = QString::null;
            }
            else
            if (line.find("passphrase.enter") != -1)
            {
                if (sendPassphrase(i18n("Enter passphrase (symmetrical encryption)"), p))
                {
                    delete p;
                    emit processaborted(true);
                    return;
                }
            }
            else
            if (!line.startsWith("[GNUPG:]"))
                message += line + "\n";
        }
    }

    p->ackRead();
}

void KgpgInterface::encryptTextFin(KProcess *p)
{
    delete p;
    if (!message.isEmpty())
    {
        emit txtEncryptionFinished(QString::fromUtf8(message.ascii()).trimmed(), this);
        message.fill('x');
        message = QString::null;
    }
    else
        emit txtEncryptionFinished(QString::null, this);
}

void KgpgInterface::decryptText(const QString &text, const QStringList &options)
{
    m_partialline = QString::null;
    message = QString::null;
    userIDs = QString::null;
    log = QString::null;
    m_ispartial = false;
    anonymous = false;
    badmdc = false;
    decok = false;
    m_textlength = 0;
    step = 3;

    KProcess *process = new KProcess();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--command-fd=0" << "--status-fd=1"; // << "--no-batch";

    for (QStringList::ConstIterator it = options.begin(); it != options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    *process << "-d";

    kdDebug(2100) << "(KgpgInterface::decryptText) Decrypt a text" << endl;
    connect(process, SIGNAL(receivedStdout(KProcess *, char *, int)), this, SLOT(decryptTextStdOut(KProcess *, char *, int)));
    connect(process, SIGNAL(receivedStderr(KProcess *, char *, int)), this, SLOT(decryptTextStdErr(KProcess *, char *, int)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(decryptTextFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, KProcess::All);

    // send the encrypted text to gpg
    process->writeStdin(text.ascii(), text.length());
}

void KgpgInterface::decryptTextStdErr(KProcess *, char *data, int)
{
    log.append(data);
}

void KgpgInterface::decryptTextStdOut(KProcess *p, char *data, int)
{
    m_partialline.append(data);

    int pos;
    while ((pos = m_partialline.find("\n")) != -1)
    {
        if (m_textlength == 0)
        {
            QString line = m_partialline.left(pos);
            m_partialline.remove(0, pos + 1);

            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if ((line.find("ENC_TO") != -1) && (line.find("0000000000000000") != -1))
                anonymous = true;
            else
            if (line.find("GET_") != -1)
            {
                if ((line.find("passphrase.enter") != -1))
                {
                    if (userIDs.isEmpty())
                        userIDs = i18n("[No user id found]");

                    QString passdlgmessage;
                    if (anonymous)
                        passdlgmessage = i18n("<b>No user id found</b>. Trying all secret keys.<br>");
                    if ((step < 3) && (!anonymous))
                        passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                    passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                    if (sendPassphrase(passdlgmessage, p, false))
                    {
                        delete p;
                        emit processaborted(true);
                        return;
                    }

                    userIDs = QString::null;

                    if (step > 1)
                        step--;
                    else
                        step = 3;
                }
                else
                {
                    p->writeStdin("quit", 5);
                    p->closeStdin();
                }
            }
            else
            if (line.find("BEGIN_DECRYPTION") != -1)
            {
                emit txtDecryptionStarted();
                p->closeStdin();
            }
            else
            if (line.find("PLAINTEXT_LENGTH") != -1)
                m_textlength = line.mid(line.indexOf("[GNUPG:] PLAINTEXT_LENGTH") + 25).toInt();
            if (line.find("DECRYPTION_OKAY") != -1)
                decok = true;
            else
            if (line.find("BADMDC") != -1)
                badmdc = true;
        }
        else
        {
            if (m_partialline.length() <= m_textlength)
            {
                message.append(m_partialline);
                m_textlength -= m_partialline.length();
                m_partialline.clear();
            }
            else
            {
                message.append(m_partialline.left(m_textlength));
                m_partialline.remove(0, m_textlength);
                m_textlength = 0;
            }
        }
    }
}

void KgpgInterface::decryptTextFin(KProcess *p)
{
    delete p;
    if ((decok) && (!badmdc))
    {
        emit txtDecryptionFinished(message, this);
        message.fill('x');
        message = QString::null;
    }
    else
    if (badmdc)
    {
        KMessageBox::sorry(0, i18n("Bad MDC detected. The encrypted text has been manipulated."));
        emit txtDecryptionFailed(log, this);
    }
    else
        emit txtDecryptionFailed(log, this);
}

void KgpgInterface::signText(const QString &text, const QString &userid, const QStringList &options)
{
    m_partialline = QString::null;
    m_ispartial = false;
    message = QString::null;
    badpassword = false;
    step = 3;

    QTextCodec *codec = QTextCodec::codecForLocale();
    if (codec->canEncode(text))
        message = text;
    else
        message = text.utf8();

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--command-fd=0" << "--status-fd=1";

    for (QStringList::ConstIterator it = options.begin(); it != options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    *process << "--clearsign" << "-u" << userid;

    kdDebug(2100) << "(KgpgInterface::signText) Sign a text" << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(signTextProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(signTextFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::signTextProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if (line.find("BAD_PASSPHRASE") != -1)
            {
                message.fill('x');
                message = QString::null;
                badpassword = true;
            }
            else
            if (line.find("GOOD_PASSPHRASE") != -1)
            {
                emit txtSigningStarted();
                p->writeStdin(message, true);
                p->closeWhenDone();
                message.fill('x');
                message = QString::null;
            }
            else
            if ((line.find("passphrase.enter") != -1))
            {
                if (userIDs.isEmpty())
                    userIDs = i18n("[No user id found]");

                QString passdlgmessage;
                if (step < 3)
                    passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p, false))
                {
                    delete p;
                    emit processaborted(true);
                    return;
                }
                step--;
            }
            else
            if (!line.startsWith("[GNUPG:]"))
                message += line + "\n";
        }
    }

    p->ackRead();
}

void KgpgInterface::signTextFin(KProcess *p)
{
    delete p;
    if (badpassword)
    {
        emit txtSigningFailed(message, this);
        message = QString::null;
    }
    else
    if (!message.isEmpty())
    {
        emit txtSigningFinished(message.trimmed(), this);
        message.fill('x');
        message = QString::null;
    }
    else
        emit txtSigningFinished(QString::null, this);
}

void KgpgInterface::verifyText(const QString &text)
{
    m_partialline = QString::null;
    m_ispartial = false;
    QString temp;
    QTextCodec *codec = QTextCodec::codecForLocale();
    if (codec->canEncode(text))
        temp = text;
    else
        temp = text.utf8();

    signmiss = false;
    signID = QString::null;
    message = QString::null;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0" << "--verify";

    kdDebug(2100) << "(KgpgInterface::verifyText) Verify a text" << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(verifyTextProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(verifyTextFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
    emit txtVerifyStarted();

    process->writeStdin(temp);
    temp.fill('x');
    process->closeWhenDone();
}

void KgpgInterface::verifyTextProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (!line.startsWith("[GNUPG:]"))
                message += line + "\n";
            else
            {
                line = line.section("]", 1, -1).simplified();

                if (line.startsWith("GOODSIG"))
                {
                    QString userName = line.section(" ", 2, -1).replace(QRegExp("<"), "&lt;");
                    userName = checkForUtf8(userName);
                    signID = i18n("<qt>Good signature from:<br><b>%1</b><br>Key ID: %2</qt>").arg(userName).arg("0x" + line.section(" ", 1, 1).right(8));
                }
                else
                if (line.startsWith("BADSIG"))
                {
                    signID = i18n("<qt><b>Bad signature</b> from:<br>%1<br>Key ID: %2<br><br><b>Text is corrupted.</b></qt>").arg(line.section(" ", 2, -1).replace(QRegExp("<"), "&lt;")).arg("0x" + line.section(" ", 1, 1).right(8));
                }
                else
                if (line.startsWith("NO_PUBKEY"))
                {
                    signID = "0x" + line.section(" ", 1, 1).right(8);
                    signmiss = true;
                }
                else
                if (line.startsWith("UNEXPECTED") || line.startsWith("NODATA"))
                    signID = i18n("No signature found.");
                else
                if (line.startsWith("TRUST_UNDEFINED"))
                    signID += i18n("The signature is valid, but the key is untrusted");
                else
                if (line.startsWith("TRUST_ULTIMATE"))
                    signID += i18n("The signature is valid, and the key is ultimately trusted");
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::verifyTextFin(KProcess *p)
{
    delete p;
    if (signmiss)
        emit txtVerifyMissingSignature(signID, this);
    else
    {
        if (signID.isEmpty())
            signID = i18n("No signature found.");

        emit txtVerifyFinished(signID, message, this);
    }
}

void KgpgInterface::encryptFile(const QStringList &encryptkeys, const KURL &srcurl, const KURL &desturl, const QStringList &options, const bool &symetrical)
{
    m_partialline = QString::null;
    m_ispartial = false;
    encok = false;
    sourceFile = srcurl;
    message = QString::null;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0";

    for (QStringList::ConstIterator it = options.begin(); it != options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    *process << "--output" << QFile::encodeName(desturl.path());

    if (!symetrical)
    {
        *process << "-e";
        for (QStringList::ConstIterator it = encryptkeys.begin(); it != encryptkeys.end(); ++it)
            *process << "--recipient" << *it;
    }
    else  // symetrical encryption, prompt for password
        *process << "-c";

    *process << QFile::encodeName(srcurl.path());

    kdDebug(2100) << "(KgpgInterface::encryptFile) Encrypt a file" << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(fileReadEncProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(fileEncryptFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

// TODO : there is a bug when we want to encrypt a file
void KgpgInterface::fileReadEncProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            kdDebug(2100) << line << endl;
            if (line.find("BEGIN_ENCRYPTION") != -1)
                emit processstarted(sourceFile.path());
            else
            if (line.find("GET_") != -1)
            {
                if (line.find("openfile.overwrite.okay") != -1)
                    p->writeStdin(QByteArray("Yes"));
                else
                if ((line.find("passphrase.enter") != -1))
                {
                    if (sendPassphrase(i18n("Enter passphrase for your file (symmetrical encryption):"), p));
                    {
                        delete p;
                        emit processaborted(true);
                        return;
                    }
                }
                else
                {
                    p->writeStdin(QByteArray("quit"));
                    p->closeWhenDone();
                }
            }
            else
            if (line.find("END_ENCRYPTION") != -1)
                encok = true;
            else
            if (!line.startsWith("[GNUPG:]"))
                message += line + "\n";
        }
    }

    p->ackRead();
}

void KgpgInterface::fileEncryptFin(KProcess *p)
{
    delete p;
    if (encok)
        emit fileEncryptionFinished(sourceFile, this);
    else
        emit errorMessage(message, this);
}

void KgpgInterface::signKey(const QString &keyid, const QString &signkeyid, const bool &local, const int &checking, const bool &terminal)
{
    m_partialline = QString::null;
    m_ispartial = false;
    log = QString::null;
    m_signkey = signkeyid;
    m_keyid = keyid;
    m_checking = checking;
    m_local = local;
    step = 3;

    if (terminal)
    {
        signKeyOpenConsole();
        return;
    }

    m_success = 0;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--command-fd=0" << "--status-fd=2" << "-u" << signkeyid;
    *process << "--edit-key" << keyid;

    if (local)
        *process << "lsign";
    else
        *process << "sign";

    kdDebug(2100) << "(KgpgInterface::signKey) Sign a key" << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(signKeyProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(signKeyFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::signKeyProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.startsWith("[GNUPG:]"))
            {
                if (line.find("USERID_HINT") != -1)
                    updateIDs(line);
                else
                if (m_success == 3)
                {
                    // user has aborted the process and don't want to sign the key
                    if (line.find("GET_") != -1)
                        p->writeStdin(QByteArray("quit"), true);
                    p->closeWhenDone();
                    return;
                }
                else
                if (line.find("ALREADY_SIGNED") != -1)
                    m_success = 4;
                if (line.find("GOOD_PASSPHRASE") != -1)
                {
                    emit signKeyStarted();
                    m_success = 2;
                }
                else
                if (line.find("sign_uid.expire") != -1)
                    p->writeStdin(QByteArray("Never"), true);
                else
                if (line.find("sign_uid.class") != -1)
                    p->writeStdin(QString::number(m_checking), true);
                else
                if (line.find("sign_uid.okay") != -1)
                    p->writeStdin(QByteArray("Y"), true);
                else
                if (line.find("sign_all.okay") != -1)
                    p->writeStdin(QByteArray("Y"), true);
                else
                if (line.find("passphrase.enter") != -1)
                {
                    QString passdlgmessage;
                    if (step < 3)
                        passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                    passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                    if (sendPassphrase(passdlgmessage, p, false))
                    {
                        m_success = 3;
                        p->writeStdin(QByteArray("quit"), true);
                        return;
                    }

                    if (step > 1)
                        step--;
                    else
                        step = 3;
                }
                else
                if ((m_success != 1) && (line.find("keyedit.prompt") != -1))
                    p->writeStdin(QByteArray("save"), true);
                else
                if (line.find("BAD_PASSPHRASE") != -1)
                    m_success = 1;
                else
                if (line.find("GET_") != -1) // gpg asks for something unusal, turn to konsole mode
                {
                    if (m_success != 1)
                        m_success = 5; // switching to console mode
                    p->writeStdin(QByteArray("quit"), true);
                }
            }
            else
                log += line + "\n";
        }
    }

    p->ackRead();
}

void KgpgInterface::signKeyFin(KProcess *p)
{
    delete p;
    if ((m_success != 0) && (m_success != 5))
        emit signKeyFinished(m_success, this); // signature successful or bad passphrase or aborted or already signed
    else
    {
        KgpgDetailedConsole *q = new KgpgDetailedConsole(0, "sign_error", i18n("<qt>Signing key <b>%1</b> with key <b>%2</b> failed.<br>Do you want to try signing the key in console mode?</qt>").arg(m_keyid).arg(m_signkey), log);
        if (q->exec() == QDialog::Accepted)
            signKeyOpenConsole();
        else
            emit signKeyFinished(3, this);
    }
}

void KgpgInterface::signKeyOpenConsole()
{
    KConfig *config = KGlobal::config();
    config->setGroup("General");

    KProcess process;
    process << config->readPathEntry("TerminalApplication", "konsole");
    process << "-e" << "gpg" << "--no-secmem-warning" << "--expert" << "-u" << m_signkey;

    if (!m_local)
        process << "--sign-key" << m_keyid;
    else
        process << "--lsign-key" << m_keyid;

    process.start(KProcess::Block);
    emit signKeyFinished(2, this);
}

void KgpgInterface::keyExpire(const QString &keyid, const QDate &date, const bool &unlimited)
{
    m_partialline = QString::null;
    m_ispartial = false;
    log = QString::null;
    m_success = 0;
    step = 3;

    if (unlimited)
        expirationDelay = 0;
    else
        expirationDelay = QDate::currentDate().daysTo(date);

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--command-fd=0" << "--status-fd=2";
    *process << "--edit-key" << keyid << "expire";

    kdDebug(2100) << "(KgpgInterface::keyExpire) Change expiration of the key " << keyid << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(keyExpireProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(keyExpireFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::keyExpireProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (!line.startsWith("[GNUPG:]"))
                log += line + "\n";
            else
            if (m_success == 3)
            {
                if (line.find("GET_") != -1)
                    p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
                return;
            }
            else
            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if ((line.find("GOOD_PASSPHRASE") != -1))
            {
                m_success = 2;
                emit keyExpireStarted();
            }
            else
            if (line.find("keygen.valid") != -1)
                p->writeStdin(QString::number(expirationDelay), true);
            else
            if (line.find("passphrase.enter") != -1)
            {
                QString passdlgmessage;
                if (step < 3)
                    passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p, false))
                {
                    m_success = 3;  // aborted by user mode
                    p->writeStdin(QByteArray("quit"), true);
                    p->closeWhenDone();
                    return;
                }
                --step;
            }
            else
            if ((m_success == 2) && (line.find("keyedit.prompt") != -1))
                p->writeStdin(QByteArray("save"), true);
            else
            if ((m_success == 2) && (line.find("keyedit.save.okay") != -1))
                p->writeStdin(QByteArray("YES"), true);
            else
            if (line.find("BAD_PASSPHRASE") != -1)
            {
                m_success = 1; // bad passphrase
                p->writeStdin(QByteArray("quit"), true);
            }
            else
            if (line.find("GET_") != -1) // gpg asks for something unusal, turn to konsole mode
            {
                if (m_success != 1)
                    m_success = 4; // switching to console mode
                p->writeStdin(QByteArray("quit"), true);
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::keyExpireFin(KProcess *p)
{
    delete p;
    if (m_success < 4)
        emit keyExpireFinished(m_success, this); // signature successful or bad passphrase
    else
    {
        KgpgDetailedConsole *q = new KgpgDetailedConsole(0,"sign_error",i18n("<qt><b>Changing expiration failed.</b><br>"
                                    "Do you want to try changing the key expiration in console mode?</qt>"),output);
        if (q->exec() == QDialog::Accepted)
            KMessageBox::sorry(0, "work in progress...");
        else
            emit keyExpireFinished(3, this);
    }
}

void KgpgInterface::changePass(const QString &keyid)
{
    m_partialline = QString::null;
    m_ispartial = false;
    m_success = 1;
    step = 3;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--no-use-agent" << "--command-fd=0" << "--status-fd=2";
    *process << "--edit-key" << keyid << "passwd";

    kdDebug(2100) << "(KgpgInterface::changePass) Change passphrase of the key " << keyid << endl;
    connect(process,SIGNAL(readReady(KProcIO *)), this, SLOT(changePassProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(changePassFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::changePassProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if ((m_success == 4) && (line.find("keyedit.prompt") != -1))
            {
                m_success = 2;
                p->writeStdin(QByteArray("save"), true);
            }
            else
            if (line.find("GOOD_PASSPHRASE") != -1)
                m_success = 4;
            else
            if ((line.find("passphrase.enter") != -1))
            {
                if (userIDs.isEmpty())
                    userIDs = i18n("[No user id found]");
                userIDs.replace(QRegExp("<"), "&lt;");

                if (m_success == 1)
                {
                    QString passdlgmessage;
                    if (step < 3)
                        passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                    passdlgmessage += i18n("Enter old passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                    if (sendPassphrase(passdlgmessage, p, false))
                    {
                        delete p;
                        emit changePassFinished(3, this);
                        return;
                    }
                    --step;
                }
                else
                if (m_success == 4)
                {
                    if (sendPassphrase(i18n("<qt>Enter new passphrase for <b>%1</b><br>If you forget this passphrase, all your encrypted files and messages will be lost !<br></qt>").arg(userIDs), p))
                    {
                        delete p;
                        emit changePassFinished(3, this);
                        return;
                    }
                }
            }
            else
            if (line.find("GET_") != -1) // gpg asks for something unusal, turn to konsole mode
            {
                p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::changePassFin(KProcess *p)
{
    delete p;
    emit changePassFinished(m_success, this);
}

void KgpgInterface::changeTrust(const QString &keyid, const int &keytrust)
{
    m_partialline = QString::null;
    m_ispartial = false;
    m_trustvalue = keytrust + 1;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--command-fd=0" << "--status-fd=2";
    *process << "--edit-key" << keyid << "trust";

    kdDebug(2100) << "(KgpgInterface::changeTrust) Change trust of the key " << keyid << " to " << keytrust << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(changeTrustProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(changeTrustFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::changeTrustProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("edit_ownertrust.set_ultimate.okay") != -1)
                p->writeStdin(QByteArray("YES"), true);
            else
            if (line.find("edit_ownertrust.value") != -1)
                p->writeStdin(QString::number(m_trustvalue), true);
            else
            if (line.find("keyedit.prompt") != -1)
            {
                p->writeStdin(QByteArray("save"), true);
                p->closeWhenDone();
            }
            else
            if (line.find("GET_") != -1) // gpg asks for something unusal
            {
                p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::changeTrustFin(KProcess *p)
{
    delete p;
    emit changeTrustFinished(this);
}

void KgpgInterface::changeDisable(const QString &keyid, const bool &ison)
{
    m_partialline = QString::null;
    m_ispartial = false;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--command-fd=0" << "--status-fd=2";
    *process << "--edit-key" << keyid;

    if (ison)
        *process << "disable";
    else
        *process << "enable";

    kdDebug(2100) << "(KgpgInterface::changeDisable) Change disable of the key " << keyid << " to " << ison << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(changeDisableProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(changeDisableFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::changeDisableProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("keyedit.prompt") != -1)
            {
                p->writeStdin(QByteArray("save"), true);
                p->closeWhenDone();
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::changeDisableFin(KProcess *p)
{
    delete p;
    changeDisableFinished(this);
}

void KgpgInterface::loadPhoto(const QString &keyid, const QString &uid)
{
    m_partialline = QString::null;
    m_ispartial = false;

    m_kgpginfotmp = new KTempFile();
    m_kgpginfotmp->setAutoDelete(true);
    QString pgpgoutput = "cp %i " + m_kgpginfotmp->name();

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--command-fd=0" << "--status-fd=2";
    *process << "--show-photos" << "--photo-viewer" << pgpgoutput << "--edit-key" << keyid << "uid" << uid << "showphoto";

    kdDebug(2100) << "(KgpgInterface::loadPhoto) Load a photo for the key " << keyid << " uid " << uid << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(loadPhotoProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(loadPhotoFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::loadPhotoProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("keyedit.prompt") != -1)
            {
                p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::loadPhotoFin(KProcess *p)
{
    delete p;
    QPixmap pixmap;
    pixmap.load(m_kgpginfotmp->name());
    m_kgpginfotmp->unlink();
    emit loadPhotoFinished(pixmap, this);
}

void KgpgInterface::addPhoto(const QString &keyid, const QString &imagepath)
{
    m_partialline = QString::null;
    m_ispartial = false;

    photoUrl = imagepath;
    m_success = 0;
    step = 3;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0";
    *process << "--edit-key" << keyid << "addphoto";

    kdDebug(2100) << "(KgpgInterface::addPhoto) Add the photo " << imagepath << " to the key " << keyid << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(addPhotoProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(addPhotoFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::addPhotoProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if (line.find("BAD_PASSPHRASE") != -1)
                m_success = 1;
            else
            if (line.find("GOOD_PASSPHRASE") != -1)
                m_success = 2;
            if (line.find("photoid.jpeg.add") != -1)
                p->writeStdin(photoUrl, true);
            else
            if (line.find("photoid.jpeg.size") != -1)
            {
                if (KMessageBox::questionYesNo(0, i18n("This image is very large. Use it anyway?"), QString::null, i18n("Use Anyway"), i18n("Do Not Use")) == KMessageBox::Yes)
                    p->writeStdin(QByteArray("Yes"), true);
                else
                {
                    delete p;
                    emit addPhotoFinished(3, this);
                    return;
                }
            }
            else
            if (line.find("passphrase.enter") != -1)
            {
                QString passdlgmessage;
                if (step < 3)
                    passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p, false))
                {
                    delete p;
                    emit addPhotoFinished(3, this);
                    return;
                }

                step--;
            }
            else
            if ((m_success == 2) && (line.find("keyedit.prompt") != -1))
                p->writeStdin(QByteArray("save"), true);
            else
            if ((line.find("GET_") != -1)) // gpg asks for something unusal, turn to konsole mode
            {
                p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::addPhotoFin(KProcess *p)
{
    delete p;
    emit addPhotoFinished(m_success, this);
}

void KgpgInterface::deletePhoto(const QString &keyid, const QString &uid)
{
    m_partialline = QString::null;
    m_ispartial = false;

    m_success = 0;
    step = 3;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--status-fd=2" << "--command-fd=0";
    *process << "--edit-key" << keyid << "uid" << uid << "deluid";

    kdDebug(2100) << "(KgpgInterface::deletePhoto) Delete a photo from the key " << keyid << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(deletePhotoProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(deletePhotoFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::deletePhotoProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if (line.find("BAD_PASSPHRASE") != -1)
                m_success = 1;
            else
            if (line.find("GOOD_PASSPHRASE") != -1)
                m_success = 2;
            else
            if (line.find("keyedit.remove.uid.okay") != -1)
                p->writeStdin(QByteArray("YES"), true);
            else
            if (line.find("passphrase.enter") != -1)
            {
                QString passdlgmessage;
                if (step < 3)
                    passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p, false))
                {
                    delete p;
                    emit deletePhotoFinished(3, this);
                    return;
                }
            }
            else
            if (line.find("keyedit.prompt") != -1)
                p->writeStdin(QByteArray("save"), true);
            else
            if (line.find("GET_") != -1) // gpg asks for something unusal, turn to konsole mode
            {
                p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
            }
        }
    }
    p->ackRead();
}

void KgpgInterface::deletePhotoFin(KProcess *p)
{
    delete p;
    emit deletePhotoFinished(m_success, this);
}

void KgpgInterface::importKey(QString keystr)
{
    m_partialline = QString::null;
    m_ispartial = false;
    message = QString::null;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--import";
    *process << "--allow-secret-key-import";

    kdDebug(2100) << "(KgpgInterface::importKey) Import a key (text)" << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(importKeyProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(importKeyFinished(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);

    process->writeStdin(keystr, true);
    process->closeWhenDone();
}

void KgpgInterface::importKey(KURL url)
{
    m_partialline = QString::null;
    m_ispartial = false;
    message = QString::null;

    if(KIO::NetAccess::download(url, m_tempkeyfile, 0))
    {
        KProcIO *process = new KProcIO();
        *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--import";
        *process << "--allow-secret-key-import";
        *process << m_tempkeyfile;

        kdDebug(2100) << "(KgpgInterface::importKey) Import a key (file)" << endl;
        connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(importKeyProcess(KProcIO *)));
        connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(importURLover(KProcess *)));
        process->start(KProcess::NotifyOnExit, true);
    }
}

void KgpgInterface::importURLover(KProcess *p)
{
    KIO::NetAccess::removeTempFile(m_tempkeyfile);
    importKeyFinished(p);
}

void KgpgInterface::importKeyProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("http-proxy") == -1)
                message += line + "\n";
        }
    }
    p->ackRead();
}

void KgpgInterface::importKeyFinished(KProcess *p)
{
    delete p;

    QStringList importedKeysIds;
    QStringList importedKeys;
    QStringList messageList;
    QString resultMessage;
    bool secretImport = false;

    QString parsedOutput = message;

    while (parsedOutput.find("IMPORTED") != -1)
    {
        parsedOutput.remove(0, parsedOutput.find("IMPORTED") + 8);
        importedKeys << parsedOutput.section("\n", 0, 0).simplified();
        importedKeysIds << parsedOutput.simplified().section(' ', 0, 0);
    }

    if (message.find("IMPORT_RES") != -1)
    {
        parsedOutput = message.section("IMPORT_RES", -1, -1).simplified();
        messageList = QStringList::split(" ", parsedOutput, true);

        resultMessage = i18n("<qt>%n key processed.<br></qt>", "<qt>%n keys processed.<br></qt>", messageList[0].toULong());

        if (messageList[1] != "0")
            resultMessage += i18n("<qt>One key without ID.<br></qt>", "<qt>%n keys without ID.<br></qt>", messageList[1].toULong());
        if (messageList[2] != "0")
            resultMessage += i18n("<qt><b>One key imported:</b><br></qt>", "<qt><b>%n keys imported:</b><br></qt>", messageList[2].toULong());
        if (messageList[3] != "0")
            resultMessage += i18n("<qt>One RSA key imported.<br></qt>", "<qt>%n RSA keys imported.<br></qt>", messageList[3].toULong());
        if (messageList[4] != "0")
            resultMessage += i18n("<qt>One key unchanged.<br></qt>", "<qt>%n keys unchanged.<br></qt>", messageList[4].toULong());
        if (messageList[5] != "0")
            resultMessage += i18n("<qt>One user ID imported.<br></qt>", "<qt>%n user IDs imported.<br></qt>", messageList[5].toULong());
        if (messageList[6] != "0")
            resultMessage += i18n("<qt>One subkey imported.<br></qt>", "<qt>%n subkeys imported.<br></qt>", messageList[6].toULong());
        if (messageList[7] != "0")
            resultMessage += i18n("<qt>One signature imported.<br></qt>", "<qt>%n signatures imported.<br></qt>", messageList[7].toULong());
        if (messageList[8] != "0")
            resultMessage += i18n("<qt>One revocation certificate imported.<br></qt>", "<qt>%n revocation certificates imported.<br></qt>", messageList[8].toULong());
        if (messageList[9] != "0")
        {
            resultMessage += i18n("<qt>One secret key processed.<br></qt>", "<qt>%n secret keys processed.<br></qt>", messageList[9].toULong());
            secretImport = true;
        }
        if (messageList[10] != "0")
            resultMessage += i18n("<qt><b>One secret key imported.</b><br></qt>", "<qt><b>%n secret keys imported.</b><br></qt>", messageList[10].toULong());
        if (messageList[11] != "0")
            resultMessage += i18n("<qt>One secret key unchanged.<br></qt>", "<qt>%n secret keys unchanged.<br></qt>", messageList[11].toULong());
        if (messageList[12] != "0")
            resultMessage += i18n("<qt>One secret key not imported.<br></qt>", "<qt>%n secret keys not imported.<br></qt>", messageList[12].toULong());

        if (secretImport)
            resultMessage += i18n("<qt><br><b>You have imported a secret key.</b> <br>"
                                  "Please note that imported secret keys are not trusted by default.<br>"
                                  "To fully use this secret key for signing and encryption, you must edit the key (double click on it) and set its trust to Full or Ultimate.</qt>");
    }
    else
        resultMessage = i18n("No key imported... \nCheck detailed log for more infos");

    if (messageList[8] != "0")
        importedKeysIds = QStringList("ALL");

    if ((messageList[9] != "0") && (importedKeysIds.isEmpty())) // orphaned secret key imported
        emit importKeyOrphaned();

    emit importKeyFinished(importedKeysIds);

    // TODO : a supprimer d'une mani�re ou d'une autre
    (void) new KgpgDetailedInfo(0, "import_result", resultMessage, message, importedKeys);
}

void KgpgInterface::addUid(const QString &keyid, const QString &name, const QString &email, const QString &comment)
{
    m_partialline = QString::null;
    m_ispartial = false;
    step = 3;
    m_success = 0;

    if ((!email.isEmpty()) && ((email.find(" ") != -1) || (email.find(".") == -1) || (email.find("@") == -1)))
    {
        emit addUidFinished(4, this);
        return;
    }

    uidName = name;
    uidComment = comment;
    uidEmail = email;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-secmem-warning" << "--no-tty" << "--status-fd=2" << "--command-fd=0";
    *process << "--edit-key" << keyid << "adduid";

    kdDebug(2100) << "(KgpgInterface::addUid) Add Uid " << name << ", " << email << ", " << comment << " to key " << keyid << endl;
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(addUidProcess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(addUidFin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::addUidProcess(KProcIO *p)
{
    QString line;
    bool partial = false;
    while (p->readln(line, false, &partial) != -1)
    {
        if (partial == true)
        {
            m_partialline += line;
            m_ispartial = true;
            partial = false;
        }
        else
        {
            if (m_ispartial)
            {
                m_partialline += line;
                line = m_partialline;

                m_partialline = "";
                m_ispartial = false;
            }

            if (line.find("USERID_HINT") != -1)
                updateIDs(line);
            else
            if (line.find("BAD_PASSPHRASE") != -1)
                m_success = 1;
            else
            if (line.find("GOOD_PASSPHRASE") != -1)
                m_success = 2;
            if (line.find("keygen.name") != -1)
                p->writeStdin(uidName, true);
            else
            if (line.find("keygen.email") != -1)
                p->writeStdin(uidEmail, true);
            else
            if (line.find("keygen.comment") != -1)
                p->writeStdin(uidComment, true);
            else
            if (line.find("passphrase.enter") != -1)
            {
                QString passdlgmessage;
                if (step < 3)
                    passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p, false))
                {
                    delete p;
                    emit addUidFinished(3, this);
                    return;
                }
                step--;
            }
            else
            if (line.find("keyedit.prompt") != -1)
                p->writeStdin(QByteArray("save"), true);
            else
            if (line.find("GET_") != -1) // gpg asks for something unusal, turn to konsole mode
            {
                p->writeStdin(QByteArray("quit"), true);
                p->closeWhenDone();
            }
        }
    }

    p->ackRead();
}

void KgpgInterface::addUidFin(KProcess *p)
{
    delete p;
    emit addUidFinished(m_success, this);
}






























void KgpgInterface::KgpgDecryptFile(KURL srcUrl, KURL destUrl, QStringList Options)
{
    message = QString::null;
    step=3;
    decryptUrl = srcUrl.path();
    userIDs = QString::null;
    anonymous = false;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0";

    for (QStringList::Iterator it = Options.begin(); it != Options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    if (!destUrl.fileName().isEmpty())
        *process << "-o" << QFile::encodeName(destUrl.path());

    *process << "-d" << QFile::encodeName(srcUrl.path());

    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(readdecprocess(KProcIO *)));
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(decryptfin(KProcess *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::decryptfin(KProcess *)
{
    if ((message.find("DECRYPTION_OKAY") != -1) && (message.find("END_DECRYPTION") != -1))
        emit decryptionfinished();
    else
        emit errorMessage(message, this);
}

void KgpgInterface::readdecprocess(KProcIO *p)
{
    QString required;
    while (p->readln(required, true) != -1)
    {
        if (required.find("BEGIN_DECRYPTION", 0, false) != -1)
            emit processstarted(decryptUrl);

        if (required.find("USERID_HINT", 0, false) != -1)
            updateIDs(required);

        if ((required.find("ENC_TO") != -1) && (required.find("0000000000000000")!=-1))
            anonymous = true;

        if (required.find("GET_") != -1)
        {
            if (required.find("openfile.overwrite.okay") != -1)
                p->writeStdin(QByteArray("Yes"));
            else
            if ((required.find("passphrase.enter") != -1))
            {
                if (userIDs.isEmpty())
                    userIDs = i18n("[No user id found]");
                userIDs.replace(QRegExp("<"), "&lt;");

                QString passdlgmessage;
                if (anonymous)
                    passdlgmessage = i18n("<b>No user id found</b>. Trying all secret keys.<br>");
                if ((step < 3) && (!anonymous))
                    passdlgmessage = i18n("<b>Bad passphrase</b>. You have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p))
                {
                    delete p;
                    emit processaborted(true);
                    return;
                }

                userIDs = QString::null;
                step--;
            }
            else
            {
                p->writeStdin(QByteArray("quit"));
                p->closeWhenDone();
            }
        }
        message += required + "\n";
    }
}

// decrypt file to text
void KgpgInterface::KgpgDecryptFileToText(KURL srcUrl, QStringList Options)
{
    message = QString::null;
    userIDs = QString::null;
    step = 3;
    anonymous = false;
    decfinished = false;
    decok = false;
    badmdc = false;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--command-fd=0" << "--status-fd=1" << "--no-batch" << "-o" << "-";

    for (QStringList::Iterator it = Options.begin(); it != Options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    *process << "-d" << QFile::encodeName(srcUrl.path());

    // when process ends, update dialog infos
    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(txtdecryptfin(KProcess *)));
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(txtreaddecprocess(KProcIO *)));
    process->start(KProcess::NotifyOnExit, false);
}

// signatures
void KgpgInterface::KgpgSignFile(QString keyID, KURL srcUrl, QStringList Options)
{
    message = QString::null;
    step = 3;

    keyID = keyID.simplified();

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--command-fd=0" << "-u" << keyID.local8Bit();

    for (QStringList::Iterator it = Options.begin(); it != Options.end(); ++it)
        if (!QFile::encodeName(*it).isEmpty())
            *process << QFile::encodeName(*it);

    *process << "--output" << QFile::encodeName(srcUrl.path() + ".sig");
    *process << "--detach-sig" << QFile::encodeName(srcUrl.path());

    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(signfin(KProcess *)));
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(readsignprocess(KProcIO *)));
    process->start(KProcess::NotifyOnExit, true);
}

void KgpgInterface::signfin(KProcess *p)
{
    delete p;

    if (message.find("SIG_CREATED") != -1)
        KMessageBox::information(0, i18n("The signature file %1 was successfully created.").arg(file.fileName()));
    else
    if (message.find("BAD_PASSPHRASE") != -1)
        KMessageBox::sorry(0, i18n("Bad passphrase, signature was not created."));
    else
        KMessageBox::sorry(0,message);

    emit signfinished();
}

void KgpgInterface::readsignprocess(KProcIO *p)
{
    QString required;
    while (p->readln(required, true) != -1)
    {
        if (required.find("USERID_HINT", 0, false) != -1)
            updateIDs(required);

        if (required.find("GET_") != -1)
        {
            if (required.find("openfile.overwrite.okay") != -1)
                p->writeStdin(QByteArray("Yes"));
            else
            if ((required.find("passphrase.enter") != -1))
            {
                if (userIDs.isEmpty())
                    userIDs = i18n("[No user id found]");

                QString passdlgmessage;
                if (step < 3)
                    passdlgmessage = i18n("<b>Bad passphrase</b>. you have %1 tries left.<br>").arg(step);
                passdlgmessage += i18n("Enter passphrase for <b>%1</b>").arg(checkForUtf8bis(userIDs));

                if (sendPassphrase(passdlgmessage, p))
                {
                    delete p;
                    emit signfinished();
                    return;
                }

                userIDs = QString::null;
                step--;
            }
            else
            {
                p->writeStdin(QByteArray("quit"));
                p->closeWhenDone();
            }
        }
        message += required + "\n";
    }
}

void KgpgInterface::KgpgVerifyFile(KURL sigUrl, KURL srcUrl)
{
    message = QString::null;
    signID = QString::null;
    signmiss = false;

    file = sigUrl;

    KProcIO *process = new KProcIO();
    *process << "gpg" << "--no-tty" << "--no-secmem-warning" << "--status-fd=2" << "--verify";
    if (!srcUrl.isEmpty())
        *process << QFile::encodeName(srcUrl.path());
    *process << QFile::encodeName(sigUrl.path());

    connect(process, SIGNAL(processExited(KProcess *)), this, SLOT(verifyfin(KProcess *)));
    connect(process, SIGNAL(readReady(KProcIO *)), this, SLOT(readprocess(KProcIO *)));
    process->start(KProcess::NotifyOnExit,true);
}

void KgpgInterface::readprocess(KProcIO *p)
{
    QString required;
    while (p->readln(required,true) != -1)
    {
        message += required + "\n";
        if (required.find("GET_") != -1)
        {
            p->writeStdin(QByteArray("quit"));
            p->closeWhenDone();
        }

        required = required.section("]", 1, -1).simplified();
        if (required.startsWith("UNEXPECTED") || required.startsWith("NODATA"))
            signID = i18n("No signature found.");

        if (required.startsWith("GOODSIG"))
            signID = i18n("<qt>Good signature from:<br><b>%1</b><br>Key ID: %2</qt>").arg(required.section(" ", 2, -1).replace(QRegExp("<"), "&lt;")).arg("0x" + required.section(" ", 1, 1).right(8));

        if (required.startsWith("BADSIG"))
    signID=i18n("<qt><b>BAD signature</b> from:<br> %1<br>Key id: %2<br><br><b>The file is corrupted!</b></qt>").arg(required.section(" ", 2, -1).replace(QRegExp("<"), "&lt;")).arg("0x" + required.section(" ", 1, 1).right(8));

        if (required.startsWith("NO_PUBKEY"))
        {
            signmiss = true;
            signID = "0x" + required.section(" ", 1, 1).right(8);
        }

        if (required.startsWith("TRUST_UNDEFINED"))
            signID += i18n("The signature is valid, but the key is untrusted");

        if (required.startsWith("TRUST_ULTIMATE"))
            signID += i18n("The signature is valid, and the key is ultimately trusted");
    }
}

void KgpgInterface::verifyfin(KProcess *)
{
    if (!signmiss)
    {
        if (signID.isEmpty())
            signID = i18n("No signature found.");

        (void) new KgpgDetailedInfo(0, "verify_result", signID,message);
    }
    else
    {
        if (KMessageBox::questionYesNo(0, i18n("<qt><b>Missing signature:</b><br>Key id: %1<br><br>Do you want to import this key from a keyserver?</qt>").arg(signID), file.fileName(), i18n("Import"), i18n("Do Not Import")) == KMessageBox::Yes)
        emit verifyquerykey(signID);
    }
    emit verifyfinished();
}

// delete signature
void KgpgInterface::KgpgDelSignature(QString keyID,QString signKeyID)
{
    if (checkUID(keyID) > 0)
    {
        KMessageBox::sorry(0, i18n("This key has more than one user ID.\nEdit the key manually to delete signature."));
        return;
    }

    message = signKeyID.remove(0, 2);
    deleteSuccess = false;
    step = 0;

    FILE *fp;
    QString encResult;
    char buffer[200];
    signb = 0;
    sigsearch = 0;

    QString gpgcmd = "gpg --no-tty --no-secmem-warning --with-colon --list-sigs " + keyID;
    fp = popen(QFile::encodeName(gpgcmd), "r");
    while (fgets( buffer, sizeof(buffer), fp))
    {
        encResult = buffer;
                if (encResult.startsWith("sig")) {
                        if (encResult.find(message)!=-1)
                                break;
                        signb++;
                } else if (encResult.startsWith("rev"))
                        signb++;
        }
        pclose(fp);
        KProcIO *conprocess=new KProcIO();
        *conprocess<<"gpg"<<"--no-secmem-warning"<<"--no-tty"<<"--command-fd=0"<<"--status-fd=2";
        *conprocess<<"--edit-key"<<keyID<<"uid 1"<<"delsig";
        connect(conprocess,SIGNAL(readReady(KProcIO *)),this,SLOT(delsigprocess(KProcIO *)));
        connect(conprocess, SIGNAL(processExited(KProcess *)),this, SLOT(delsignover(KProcess *)));
        conprocess->start(KProcess::NotifyOnExit,true);
}


void KgpgInterface::delsigprocess(KProcIO *p)//ess *p,char *buf, int buflen)
{

        QString required=QString::null;
        while (p->readln(required,true)!=-1)
        {
                if (required.find("keyedit.delsig")!=-1){

                        if ((sigsearch==signb) && (step==0)) {
                                p->writeStdin(QByteArray("Y"));
                                step=1;
                        } else
                                p->writeStdin(QByteArray("n"));
                        sigsearch++;
                        required=QString::null;
                }
                if ((step==1) && (required.find("keyedit.prompt")!=-1)) {
                        p->writeStdin(QByteArray("save"));
                        required=QString::null;
                        deleteSuccess=true;
                }
                if (required.find("GET_LINE")!=-1) {
                        p->writeStdin(QByteArray("quit"));
                        p->closeWhenDone();
                        deleteSuccess=false;
                }
        }
}

void KgpgInterface::delsignover(KProcess *)
{
        emit delsigfinished(deleteSuccess);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  key revocation

void KgpgInterface::KgpgRevokeKey(QString keyID,QString revokeUrl,int reason,QString description)
{
        revokeReason=reason;
        revokeSuccess=false;
        revokeDescription=description;
        certificateUrl=revokeUrl;
        output=QString::null;
        KProcIO *conprocess=new KProcIO();
        *conprocess<< "gpg"<<"--no-tty"<<"--status-fd=2"<<"--logger-fd=2"<<"--command-fd=0";
        if (!revokeUrl.isEmpty())
                *conprocess<<"-o"<<revokeUrl;
        *conprocess<<"--gen-revoke"<<keyID;
        QObject::connect(conprocess, SIGNAL(processExited(KProcess *)),this, SLOT(revokeover(KProcess *)));
        QObject::connect(conprocess, SIGNAL(readReady(KProcIO *)),this, SLOT(revokeprocess(KProcIO *)));
        conprocess->start(KProcess::NotifyOnExit,true);
}

void KgpgInterface::revokeover(KProcess *)
{
        if (!revokeSuccess)
                KMessageBox::detailedSorry(0,i18n("Creation of the revocation certificate failed..."),output);
        else {
                output=output.section("-----BEGIN",1);
                output.prepend("-----BEGIN");
                output=output.section("BLOCK-----",0);
                emit revokecertificate(output);
                if (!certificateUrl.isEmpty())
                        emit revokeurl(certificateUrl);
        }
}

void KgpgInterface::revokeprocess(KProcIO *p)
{
        QString required=QString::null;
        while (p->readln(required,true)!=-1) {
                output+=required+"\n";

                if (required.find("USERID_HINT",0,false)!=-1)
        updateIDs(required);

                if ((required.find("GOOD_PASSPHRASE")!=-1))
                        revokeSuccess=true;

                if ((required.find("gen_revoke.okay")!=-1) || (required.find("ask_revocation_reason.okay")!=-1) || (required.find("openfile.overwrite.okay")!=-1)) {
                        p->writeStdin(QByteArray("YES"));
                        required=QString::null;
                }

                if (required.find("ask_revocation_reason.code")!=-1) {
                        p->writeStdin(QString::number(revokeReason));
                        required=QString::null;
                }

                if (required.find("passphrase.enter")!=-1)
                {
                    if (sendPassphrase(i18n("<qt>Enter passphrase for <b>%1</b>:</qt>").arg(checkForUtf8bis(userIDs)), p))
                    {
                                expSuccess=3;  /////  aborted by user mode
                                p->writeStdin(QByteArray("quit"));
                                p->closeWhenDone();
                                return;
                        }
                        required=QString::null;

                }
                if (required.find("ask_revocation_reason.text")!=-1) {
                        //      kdDebug(2100)<<"description"<<endl;
                        p->writeStdin(revokeDescription);
                        revokeDescription=QString::null;
                        required=QString::null;
                }
                if ((required.find("GET_")!=-1)) /////// gpg asks for something unusal, turn to konsole mode
                {
                        kdDebug(2100)<<"unknown request"<<endl;
                        expSuccess=1;  /////  switching to console mode
                        p->writeStdin(QByteArray("quit"));
                        p->closeWhenDone();

                }
        }
}

#include "kgpginterface.moc"
