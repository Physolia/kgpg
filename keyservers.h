/***************************************************************************
                          keyservers.h  -  description
                             -------------------
    begin                : Tue Nov 26 2002
    copyright            : (C) 2002 by y0k0
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
 
  #ifndef KEYSERVERS_H
#define KEYSERVERS_H

#include <qlayout.h>
#include <qlabel.h>
#include <qdialog.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qhbuttongroup.h>
#include <qvbuttongroup.h>
#include <qregexp.h>
#include <qfile.h>

#include <kdialogbase.h>
#include <klocale.h>
#include <kconfig.h>
#include <klineedit.h>
#include <kprocess.h>
#include <kprocio.h>
#include <klistview.h>
#include <kstatusbar.h>
#include <kurl.h>

#include "keyserver.h"
#include "searchres.h"
#include "detailedconsole.h"

class keyServer : public Keyserver
{
  Q_OBJECT
  public:
 keyServer(QWidget *parent=0, const char *name=0,bool modal=false, WFlags f = 0);
 ~keyServer();
  QDialog *importpop;


 public slots:
 void abortImport();
 void abortSearch();
 void transferKeyID();
 void slotsearchread(KProcIO *p);
 void slotAddServer();
 void slotRemoveServer();
 void slotOk();
void syncCombobox();
void slotImport();
void slotSearch();
void slotimportresult(KProcess*);
void slotimportread(KProcIO *p);
void slotprocread(KProcIO *p);
void slotExport();
void slotEdit(QListViewItem *);
void slotEditServer();
void preimport();
void slotsearchresult(KProcess *);
void slotEnableProxyI(bool on);
void slotEnableProxyE(bool on);
void handleQuit();

   private:

  KConfig *config;
  uint keyNumbers;
  QString readmessage;
  KProcIO *importproc;

  KeyServer *listpop;
  QStatusBar *sBar;
QLabel *statusmsg;
  int count;
  bool cycle;
  KListViewItem *kitem;
};

#endif
