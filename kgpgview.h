/**************************************************************************
                          kgpgview.h  -  description
                             -------------------
    begin                : Tue Jul  2 12:31:38 GMT 2002
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

#ifndef KGPGVIEW_H
#define KGPGVIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


// include files for Qt
#include <qwidget.h>
#include <qtextedit.h>
#include <qlayout.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qdragobject.h>
#include <qevent.h>

#include <ktexteditor/editor.h>

#include <kbuttonbox.h>
#include <kpassdlg.h>
#include <kprocess.h>
#include <kprocio.h>
#include <kmessagebox.h>
#include <kurl.h>
#include <ktextedit.h>



//#include "popuppublic.h"
//#include "listkeys.h"


//class KgpgDoc;

/** The KgpgView class provides the view widget for the KgpgApp instance.
 * The View instance inherits QWidget as a base class and represents the view object of a KTMainWindow. As KgpgView is part of the
 * docuement-view model, it needs a reference to the document object connected with it by the KgpgApp class to manipulate and display
 * the document structure provided by the KgpgDoc class.
 *
 * @author Source Framework Automatically Generated by KDevelop, (c) The KDevelop Team.
 * @version KDevelop version 0.4 code generation
 */

class MyEditor : public KTextEdit
{
        Q_OBJECT

public:
        MyEditor( QWidget *parent = 0, const char *name = 0);
private:
        QString message,messages,tempFile;
public slots:
        void slotDecodeFile(QString);
        void slotDroppedFile(KURL url);
        void slotProcessResult(QStringList iKeys);
	bool slotCheckContent(QString fileToCheck, bool checkForPgpMessage=true);

protected:
        void contentsDragEnterEvent( QDragEnterEvent *e );
        void contentsDropEvent( QDropEvent *e );
	
private slots:
	void editorUpdateDecryptedtxt(QString newtxt);
	void editorFailedDecryptedtxt(QString newtxt);

signals:
	void refreshImported(QStringList);
};


class KgpgView : public QWidget
{
        Q_OBJECT
        friend class MyEditor;
public:
        /** Constructor for the main view */
        KgpgView(QWidget *parent = 0, const char *name=0);
        /** Destructor for the main view */
        ~KgpgView();

        /** returns a pointer to the document connected to the view instance. Mind that this method requires a KgpgApp instance as a parent
         * widget to get to the window document pointer by calling the KgpgApp::getDocument() method.
         *
         * @see KgpgApp#getDocument
         */
        //  KgpgDoc *getDocument() const;

        //        QTextEdit  *editor;

        MyEditor *editor;
        KURL fselected;
	bool windowAutoClose;

        /** contains the implementation for printing functionality */
        //    void print(QPrinter *pPrinter);

        QPushButton *bouton1,*bouton2,*bouton0;
private:
        QString messages;

public slots:
        void slotdecode();
	void clearSign();

private slots:
	void slotVerifyResult(QString mssge);
	void slotAskForImport(QString ID);
        void popuppublic();
        void modified();
        void encodetxt(QStringList selec,QStringList encryptOptions,bool, bool symmetric);
        void updatetxt(QString);
	void updateDecryptedtxt(QString newtxt);
	void failedDecryptedtxt(QString newtxt);
	bool checkForUtf8(QString text);
	
signals:
	void resetEncoding(bool);
	void verifyFinished();
	void verifyDetach();
};

#endif // KGPGVIEW_H
