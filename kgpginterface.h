/***************************************************************************
                          kgpginterface.h  -  description
                             -------------------
    begin                : Sat Jun 29 2002
    copyright            : (C) 2002 by
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KGPGINTERFACE_H
#define KGPGINTERFACE_H


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <qstring.h>
#include <qfile.h>
#include <qobject.h>
#include <qlabel.h>

#include <kled.h>
#include <kprocess.h>
#include <kprocio.h>
#include <kdialogbase.h>
#include <kurl.h>
#include <kiconloader.h>
#include <kio/netaccess.h>

#include "detailedconsole.h"
#include "kgpgfast.h"

/*
#include <kdeversion.h>
#if (KDE_VERSION >= 310)
#include <kpassivepopup.h>
#else
#include <qtimer.h>
#endif
*/

/**
 * Encrypt a file using gpg.
 */
//class KgpgEncryptFile : public QObject {
class KgpgInterface : public QObject
{

        Q_OBJECT

public:
        /**
         * Initialize the class
         */
        KgpgInterface();

        /**Encrypt file function
         * @param userIDs the recipients key id's.
         * @param srcUrl Kurl of the file to encrypt.
         * @param destUrl Kurl for the encrypted file.
         * @param Options String with the wanted gpg options. ex: "--armor"
         * @param symetrical bool whether the encryption should be symmetrical.
         */
        void KgpgEncryptFile(QString encuserIDs,KURL srcUrl,KURL destUrl,QString Options="",bool symetrical=false);

        /**Encrypt file function
         * @param userIDs the key user identification.
         * @param srcUrl Kurl of the file to decrypt.
         * @param destUrl Kurl for the decrypted file.
         * @param chances int number of trials left for decryption (used only as an info displayed in the password dialog)
         */
        void KgpgDecryptFile(KURL srcUrl=0,KURL destUrl=0,QString Options="");

        /**Sign file function
         * @param keyID QString the signing key ID.
         * @param srcUrl Kurl of the file to sign.
         * @param Options String with the wanted gpg options. ex: "--armor"
         */
        void KgpgSignFile(QString keyID="",KURL srcUrl=0,QString Options="");

        /**Verify file function
         * @param sigUrl Kurl of the signature file.
         * @param srcUrl Kurl of the file to be verified. If empty, gpg will try to find it using the signature file name (by removing the .sig extensio)
         */
        void KgpgVerifyFile(KURL sigUrl,KURL srcUrl=NULL) ;

        /**Import key function
         * @param url Kurl the url of the key file. Allows public & secret key import.
         */
        void importKeyURL(KURL url, bool importSecret=false);
        /**Import key function
         * @param keystr QString containing th key. Allows public & secret key import.
        */
        void importKey(QString keystr, bool importSecret=false);

        /**Key signature function
         * @param keyID QString the ID of the key to be signed
         * @param signKeyID QString the ID of the signing key
         * @param signKeyMail QString the name of the signing key (only used to prompt user for passphrase)
         * @param local bool should the signature be local
         */
        void KgpgSignKey(QString keyID="",QString signKeyID="",QString signKeyMail="",bool local=false);

        /**Key signature deletion function
         * @param keyID QString the ID of the key
         * @param signKeyID QString the ID of the signature key
         */
        void KgpgDelSignature(QString keyID="",QString signKeyID="");

        /**Encrypt text function
         * @param text QString text to be encrypted.
         * @param userIDs the recipients key id's.
         * @param Options String with the wanted gpg options. ex: "--armor"
         * returns the encrypted text or empty string if encyption failed
         */
        //static
        void KgpgEncryptText(QString text,QString userIDs, QString Options="");

        /**Decrypt text function
        * @param text QString text to be decrypted.
        * @param userID QString the name of the decryption key (only used to prompt user for passphrase)
        */
        static QString KgpgDecryptText(QString text,QString userID);
        static QString KgpgDecryptFileToText(KURL srcUrl,QString userID);

        static QString extractKeyName(QString txt="");
        static QString extractKeyName(KURL url=0);
        static QString getGpgSetting(QString name,QString configFile);
        static void setGpgSetting(QString name,QString ID,QString url);
	static QString checkForUtf8(QString txt);

        /*
         * Destructor for the class.
         */
        ~KgpgInterface();



private slots:

        void openSignConsole();
        /**
              * Checks output of the signature process
              */
        void signover(KProcess *);
        /**
                * Read output of the signature process
                */
        void sigprocess(KProcIO *p);

        /**
         * Checks if the encrypted file was saved.
         */
        void encryptfin(KProcess *);

        /**
                * Checks if the decrypted file was saved.
                */
        void decryptfin(KProcess *);

        /**
                * Checks if the signing was successfull.
                */
        void signfin(KProcess *p);

        /**
                * Checks the number of uid's for a key-> if greater than one, key signature will switch to konsole mode
                */
        int checkuid(QString KeyID);

        /**
                * Reads output of the delete signature process
                */
        void delsigprocess(KProcIO *p);
        /**
                * Checks output of the delete signature process
                */
        void delsignover(KProcess *p);
        /**
                * Checks output of the import process
                */
        void importURLover(KProcess *);
        void importover(KProcess *);
        /**
                 * Read output of the import process
                 */
        void importprocess(KProcIO *p);
        /**
                 * Reads output of the current process + allow overwriting of a file
                 */
        void readprocess(KProcIO *p);
        /**
                * Reads output of the current encryption process + allow overwriting of a file
                */
        void readencprocess(KProcIO *p);
        /**
                * Reads output of the current signing process + allow overwriting of a file
                */
        void readsignprocess(KProcIO *p);
        /**
                * Reads output of the current decryption process + allow overwriting of a file
                */
        void readdecprocess(KProcIO *p);
        /**
                 * Checks output of the verify process
                 */
        void verifyfin(KProcess *p);

        void txtreadencprocess(KProcIO *p);
        void txtencryptfin(KProcess *);
        void signkillDisplayClip();
        //void txtreaddecprocess(KProcIO *p);
        //void txtdecryptfin(KProcess *);
signals:
        /**
               *  emitted when an txt encryption finished
               */
        void txtencryptionfinished(QString);
        /**
               *  emitted when an error occured
               */
        void errormessage(QString);
        /**
                *  true if encryption successfull, false on error.
                */
        void encryptionfinished(KURL);
        /**
                *  true if key signature deletion successfull, false on error.
                */
        void delsigfinished(bool);

        /**
                * Signature process result: 0=successfull, 1=error, 2=bad passphrase
                */
        void signatureFinished(int);
        /**
                *  emitted when user cancels process
                */
        void processaborted(bool);
        /**
                *  emitted when the process starts
                */
        void processstarted();
        /**
                *  true if decryption successfull, false on error.
                */
        void decryptionfinished();
        /**
                * emitted if bad passphrase was giver
                */
        void badpassphrase(bool);
        /**
                *  true if import successfull, false on error.
                */
        void importfinished();
        /**
                *  true if verify successfull, false on error.
                */
        void verifyfinished();
        /**
                *  emmitted if signature key is missing & user want to import it from keyserver
                */
        void verifyquerykey(QString ID);
        /**
                *  true if signature successfull, false on error.
                */
        void signfinished();


private:
        /**
        * @internal structure for communication
        */
        QString message,tempKeyFile,userIDs,txtprocess,output;
        QCString passphrase;
        bool deleteSuccess,konsLocal,anonymous,txtsent,decfinished,decok,badmdc;
        int signSuccess;
        int step,signb,sigsearch;
        QString konsSignKey, konsKeyID;
        KURL sourceFile;

        /*
        #if (KDE_VERSION >= 310)
        KPassivePopup *pop;
        #else
        QDialog *clippop;
        #endif
        */

        /**
         * @internal structure for the file information
         */
        KURL file;
        /**
         * @internal structure to send signal only once on error.
         */
        bool encError;
};

class  Md5Widget :public KDialogBase
{
        Q_OBJECT
public:
        Md5Widget(QWidget *parent=0, const char *name=0,KURL url=0);
        ~Md5Widget();
public slots:
        void slotApply();
private:
        QString mdSum;
        KLed *KLed1;
        QLabel *TextLabel1_2;
};

#endif
