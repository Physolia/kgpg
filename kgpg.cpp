/***************************************************************************
                        kgpg.cpp  -  description
                           -------------------
  begin                : Tue Jul  2 12:31:38 GMT 2002
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

// include files for QT
#include <qstring.h>
#include <qtimer.h>
#include <qfile.h>
#include <stdlib.h>
#include <qwidgetlist.h>
#include <unistd.h>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kshred.h>

#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kpopupmenu.h>
#include <ktip.h>
#include <kapplication.h>
#include <qclipboard.h>
#include <qdialog.h>
#include <qregexp.h>


// application specific includes

#include "kgpgfast.h"

//#include <qfont.h>
#include "kgpg.h"
#include "kgpginterface.h"

#define ID_STATUS_MSG 1

KgpgApp::KgpgApp(const char* name,KURL fileToOpen,QString opmode):KMainWindow(0, name)
{  
  config=kapp->config();
  readOptions();
commandLineMode=false;
  if ((opmode.isEmpty()) || (opmode=="show") || (opmode=="clipboard"))
    {
	  if (tipofday) slotTip();
	  
	  KIconLoader *loader = KGlobal::iconLoader();
      fileEnc=loader->loadIcon("kgpg",KIcon::Small);
      fileDec=loader->loadIcon("kgpg2",KIcon::Small);
      
	  // call inits to invoke all other construction parts
      initActions();
      initView();
      
	  if (opmode=="clipboard") {commandLineMode=true;slotClip();} /// decrypt & open clipboard content
      
      fileSave->setEnabled(false);
      editRedo->setEnabled(false);
      editUndo->setEnabled(false);
	  
	  if (opmode=="show") openEncryptedDocumentFile(fileToOpen);//checkEncryptedDocumentFile(fileToOpen);
    createGUI("kgpg.rc");
	}
  else
  	if (opmode=="clipboardEnc")
	{
popupPublic *dialoguec=new popupPublic(this, "public_keys", 0,false);
connect(dialoguec,SIGNAL(selectedKey(QString &,QString,bool,bool)),this,SLOT(encryptClipboard(QString &,QString)));
if (dialoguec->exec()==QDialog::Rejected ) exit(0);
	}
else if (!fileToOpen.filename().isEmpty())
    {
      urlselected=fileToOpen;
	  commandLineMode=true;
      if (opmode=="encrypt") ///// user asked for encryption in command line arguments
        {
          if (!encryptfileto)
            {
              popupPublic *dialogue=new popupPublic(this,"public_keys",fileToOpen.filename(),true);
              connect(dialogue,SIGNAL(selectedKey(QString &,QString,bool,bool)),this,SLOT(fastencode(QString &,QString,bool,bool)));
              if (dialogue->exec()==QDialog::Rejected ) exit(0);
            }
          else
		  {
		  QString opts;
		  	if (untrusted) opts=" --always-trust ";
			if (ascii) opts+=" --armor ";
            fastencode(filekey,opts,false,false);
		  }
          return;
        }
      else if (opmode=="decrypt") fastdecode(true);  ///// user asked for decryption in command line arguments
     else if (opmode=="verify") slotVerifyFile(urlselected);
	  else if (opmode=="sign") slotSignFile(urlselected);
	  else view->editor->droppedfile(urlselected);
    }
}

KgpgApp::~KgpgApp()
{
}


void KgpgApp::encryptClipboard(QString &selec,QString encryptOptions)
{
QString clipContent=kapp->clipboard()->text();//=cb->text(QClipboard::Clipboard);   ///   QT 3.1 only

  if (clipContent)
  {
  if (pgpcomp) encryptOptions+=" --pgp6 ";
  encryptOptions+=" --armor ";
    
 if (selec==NULL) {KMessageBox::sorry(0,i18n("You have not chosen an encryption key."));exit(0);}

 
 KgpgInterface *txtEncrypt=new KgpgInterface();
connect (txtEncrypt,SIGNAL(txtencryptionfinished(QString)),this,SLOT(slotSetClip(QString)));
 txtEncrypt->KgpgEncryptText(clipContent,selec,encryptOptions);

     QWidgetList  *list = QApplication::topLevelWidgets();
    QWidgetListIt it( *list );          // iterate over the widgets
    while ( it.current() ) {            // for each top level widget...
        if ( it.current()->isVisible() )
            it.current()->hide();
        ++it;
    }
    delete list;

QString cryptedClipboard;
if (clipContent.length()>300) cryptedClipboard=QString(clipContent.left(250).stripWhiteSpace())+"...\n"+QString(clipContent.right(40).stripWhiteSpace());
else cryptedClipboard=clipContent;


cryptedClipboard.replace(QRegExp("<"),"&lt;");   /////   disable html tags
cryptedClipboard.replace(QRegExp("\n"),"<br>");

#if (KDE_VERSION >= 310)
pop = new KPassivePopup( this);
pop->setView(i18n("Encrypted following text:"),cryptedClipboard,KGlobal::iconLoader()->loadIcon("kgpg",KIcon::Desktop));
		pop->setTimeout(3200);
	  	pop->show();	  
	  	QRect qRect(QApplication::desktop()->screenGeometry());
		int iXpos=qRect.width()/2-pop->width()/2;
		int iYpos=qRect.height()/2-pop->height()/2;
      	pop->move(iXpos,iYpos);
#else
	clippop = new QDialog( this,0,false,WStyle_Customize | WStyle_NormalBorder);
              QVBoxLayout *vbox=new QVBoxLayout(clippop,3);
              QLabel *tex=new QLabel(clippop);
              tex->setText(i18n("<b>Encrypted following text:</b>"));
			  QLabel *tex2=new QLabel(clippop);
			  //tex2->setTextFormat(Qt::PlainText);
			  tex2->setText(cryptedClipboard);
              vbox->addWidget(tex);
			  vbox->addWidget(tex2);
              clippop->setMinimumWidth(250);
              clippop->adjustSize();
			  clippop->show();
 QTimer::singleShot( 3000, this, SLOT(killDisplayClip()));
//KMessageBox::information(this,i18n("<b>Encrypted following text</b>:<br>")+cryptedClipboard);
#endif
 }
 else
{
KMessageBox::sorry(0,i18n("Clipboard is empty."));
exit(0);
}
}

void KgpgApp::slotSetClip(QString newtxt)
{
 if (!newtxt.isEmpty())
 {
 QClipboard *clip=QApplication::clipboard();
 clip->setText(newtxt);//,QClipboard::Clipboard);    QT 3.1 only
 connect(kapp->clipboard(),SIGNAL(dataChanged ()),this,SLOT(expressQuit()));
 }
 else expressQuit();
}

void KgpgApp::killDisplayClip()
{
#if (KDE_VERSION < 310)
delete clippop;
#endif
}


void KgpgApp::expressQuit()
{
exit(0);
}


void KgpgApp::slotman()
{
   kapp->startServiceByDesktopName("khelpcenter", QString("man:/gpg"), 0, 0, 0, "", true);
}

void KgpgApp::slotTip()
{
    KTipDialog::showTip(this, "kgpg/tips", true);
}


void KgpgApp::initActions()
{
  KStdAction::openNew(this, SLOT(slotFileNew()), actionCollection());
  KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
  KStdAction::saveAs(this, SLOT(slotFileSaveAs()), actionCollection());
  KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
  KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
  KStdAction::copy(this, SLOT(slotEditCopy()), actionCollection());
  KStdAction::paste(this, SLOT(slotEditPaste()), actionCollection());
  KStdAction::preferences(this, SLOT(slotOptions()), actionCollection());
  
  fileSave = KStdAction::save(this, SLOT(slotFileSave()), actionCollection());
  fileEncrypt = new KAction(i18n("&Encrypt File..."), fileEnc, 0,this, SLOT(slotFileEnc()), actionCollection(),"file_encrypt");
  fileDecrypt = new KAction(i18n("&Decrypt File..."), fileDec, 0,this, SLOT(slotFileDec()), actionCollection(),"file_decrypt");
  editUndo = KStdAction::undo(this, SLOT(slotundo()), actionCollection());
  editRedo = KStdAction::redo(this, SLOT(slotredo()), actionCollection());  
  keysManage = new KAction(i18n("&Manage Keys"), "kgpg_manage", 0,this, SLOT(slotManageKey()), actionCollection(),"keys_manage");
  signGenerate = new KAction(i18n("&Generate Signature..."),0, this, SLOT(slotPreSignFile()), actionCollection(), "sign_generate");
  signVerify = new KAction(i18n("&Verify Signature..."),0, this, SLOT(slotPreVerifyFile()), actionCollection(), "sign_verify");
  signCheck = new KAction(i18n("&Check MD5 Sum..."), 0,this, SLOT(slotCheckMd5()), actionCollection(), "sign_check");
  helptips = new KAction(i18n("Tip of the &Day"), "idea", 0,this, SLOT(slotTip()), actionCollection(),"help_tipofday");
  manpage = new KAction(i18n("View GnuPG Manual"), "contents", 0,this, SLOT(slotman()),actionCollection(),"gpg_man");
}

void KgpgApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
  //  statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}








void KgpgApp::initView()
{
  ////////////////////////////////////////////////////////////////////
  // create the main widget here that is managed by KTMainWindow's view-region and
  // connect the widget to your document to display document contents.

  view = new KgpgView(this,0);
  //  doc->addView(view);
  setCentralWidget(view);
  setCaption(i18n("untitled"),false); ///   doc->URL().fileName(),false);

}


void KgpgApp::openEncryptedDocumentFile(const KURL& url)
{
QString userIDs=KgpgInterface::extractKeyName(url);
  QFile qfile(url.path());
  QString encryptedText;
  if (qfile.open(IO_ReadOnly))
    {
      QTextStream t( &qfile );
      encryptedText=t.read();
      qfile.close();
	  QString decrypted=KgpgInterface::KgpgDecryptText(encryptedText,userIDs);
      if (!decrypted.isEmpty())
	  {
	  view->editor->setText(decrypted);
      fileSave->setEnabled(false);
      editRedo->setEnabled(false);
      editUndo->setEnabled(false);
	  }
	  else KMessageBox::sorry(this,i18n("Decryption failed"));
    }
    else KMessageBox::sorry(0,i18n("Unable to read file."));
}



void KgpgApp::openDocumentFile(const KURL& url)
{

  /////////////////////////////////////////////////

  QFile qfile(url.path());

  if (qfile.open(IO_ReadOnly))
    {
      QTextStream t( &qfile );
      view->editor->setText(t.read());
      qfile.close();
      fileSave->setEnabled(false);
      editRedo->setEnabled(false);
      editUndo->setEnabled(false);
    }

}


void KgpgApp::saveOptions()
{
  config->setGroup("General Options");
  config->writeEntry("Geometry", size());
  config->writeEntry("First run",false);
  config->writeEntry("gpg version",version);
}

void KgpgApp::readOptions(bool doresize)
{
  config->setGroup("General Options");

  ascii=config->readBoolEntry("Ascii armor",true);
  untrusted=config->readBoolEntry("Allow untrusted keys",false);
  hideid=config->readBoolEntry("Hide user ID",false);
  pgpcomp=config->readBoolEntry("PGP compatibility",false);
  encrypttodefault=config->readBoolEntry("encrypt to default key",false);
  defaultkey=config->readEntry("default key");
  encryptfileto=config->readBoolEntry("encrypt files to",false);
  filekey=config->readEntry("file key");
customDecrypt=config->readEntry("custom decrypt");

if (config->readBoolEntry("selection clip",false))
{
// support clipboard selection (if possible)
if (kapp->clipboard()->supportsSelection())
kapp->clipboard()->setSelectionMode(true);
}
else kapp->clipboard()->setSelectionMode(false);

  if (doresize)
    {
      QSize size=config->readSizeEntry("Geometry");
      bool frun=config->readBoolEntry("First run",true);
      config->setGroup("TipOfDay");
      tipofday=config->readBoolEntry("RunOnStart",true);
      if (frun) firstrun();
	else
      if (!size.isEmpty())
        resize(size);
    }

}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

void KgpgApp::firstrun()
{
  FILE *fp;
  QString tst;
  char line[200];
  bool found=false;

kgpgOptions *opts=new kgpgOptions(this,0);
opts->checkMimes();
delete opts;

  fp = popen("gpg --no-tty --with-colon --list-secret-keys", "r");
  while ( fgets( line, sizeof(line), fp))
    {
      tst=line;
      if (tst.startsWith("sec"))
      found=true;
    }
  pclose(fp);
  if (!found)
    {
      int result=KMessageBox::questionYesNo(0,i18n("Welcome to KGPG.\nNo secret key was found on your computer.\nWould you like to create one now?"));
      if (result==3)
        {
          listKeys *creat=new listKeys(this);
          creat->slotgenkey();
          delete creat;
        }
    }
}




void KgpgApp::slotOptions()
{
  /////////////////////// open option dialog box --> kgpgoptions.cpp
  kgpgOptions *opts=new kgpgOptions(this,0);
  opts->exec();
  delete opts;
  readOptions(false);
}


void KgpgApp::slotundo()
{
  view->editor->undo();
  editRedo->setEnabled(true);
}

void KgpgApp::slotredo()
{
  view->editor->redo();
}

/////////////    file signature slots


void KgpgApp::slotCheckMd5()
{
  /////////////////////////////////////////////////////////////////////////  display md5 sum for a chosen file

  KURL url=KFileDialog::getOpenURL(QString::null,
                                   i18n("*|All Files"), this, i18n("Open File to Verify"));
  if (!url.isEmpty())
    {

      Md5Widget *mdwidget=new Md5Widget(this,0,url);
      mdwidget->exec();
      delete mdwidget;
      //      KMessageBox::information(this,QString("MD5 sum for "+url.filename()+" is:\n"+checkfile.hexDigest().data()));
    }
}


void KgpgApp::slotPreSignFile()
{
  //////////////////////////////////////   create a detached signature for a chosen file
  KURL url=KFileDialog::getOpenURL(QString::null,i18n("*|All Files"), this, i18n("Open File to Sign"));
 if (!url.isEmpty()) slotSignFile(url);
}

void KgpgApp::slotSignFile(KURL url)
{
  //////////////////////////////////////   create a detached signature for a chosen file
  QString signKeyID;
 if (!url.isEmpty())
    {
      //////////////////   select a private key to sign file --> listkeys.cpp
	  KgpgSelKey *opts=new KgpgSelKey(this,"select_secret",false);
      if (opts->exec()==QDialog::Accepted) signKeyID=opts->getkeyID();
      else
        {
          delete opts;
		  if (commandLineMode) slotExpressQuit();
          return;
        }
      delete opts;
  QString Options;
  if (pgpcomp) Options=" --pgp6 ";
 KgpgInterface *signFileProcess=new KgpgInterface();
 if (commandLineMode) connect (signFileProcess,SIGNAL(signfinished()),this,SLOT(slotExpressQuit()));
 signFileProcess->KgpgSignFile(signKeyID,url,Options);
  }
  else if (commandLineMode) slotExpressQuit();
}

void KgpgApp::slotPreVerifyFile()
{
 KURL url=KFileDialog::getOpenURL(QString::null,
                                   i18n("*|All Files"), this, i18n("Open File to Verify"));
 slotVerifyFile(url);
}

void KgpgApp::slotVerifyFile(KURL url)
{
  ///////////////////////////////////   check file signature
  QString sigfile="";
 if (!url.isEmpty())
    {
      //////////////////////////////////////       try to find detached signature.
if (!url.filename().endsWith(".sig"))
{
      sigfile=url.path()+".sig";
      QFile fsig(sigfile);
      if (!fsig.exists())
        {
          sigfile=url.path()+".asc";
          QFile fsig(sigfile);
          //////////////   if no .asc or .sig signature file included, assume the file is internally signed
          if (!fsig.exists())
            sigfile="";
        }
}
      ///////////////////////// pipe gpg command
      KgpgInterface *verifyFileProcess=new KgpgInterface();
	  if (commandLineMode) connect (verifyFileProcess,SIGNAL(verifyfinished()),this,SLOT(slotExpressQuit()));
 verifyFileProcess->KgpgVerifyFile(url,KURL(sigfile));
 }
 else if (commandLineMode) slotExpressQuit();
}

///////////////////////////////////   key slots


void KgpgApp::slotClip()
{

  QString text;

  // Copy text from the clipboard (paste)
  text = kapp->clipboard()->text();
  if ( !text.isEmpty())
  {
  view->editor->setText(text);
  view->slotdecode();
  commandLineMode=false;
  }
  else {KMessageBox::sorry(this,i18n("Clipboard is empty."));exit(0);}
}




void KgpgApp::slotManageKey()
{
    /////////// open key management window --> listkeys.cpp
   listKeys * keydialogue = new listKeys(this, "key_manager",WShowModal | WType_Dialog);
   keydialogue->show();
}


void KgpgApp::slotFileNew()
{
  //////  delete all text from editor

  view->editor->setText("");
  editRedo->setEnabled(false);
  editUndo->setEnabled(false);
  setCaption(i18n("Untitled"), false);
  fileSave->setEnabled(false);
  Docname="";
}


void KgpgApp::slotFileOpen()
{

  KURL url=KFileDialog::getOpenURL(QString::null,
                                   i18n("*|All Files"), this, i18n("Open File"));
  if(!url.isEmpty())
    {
      openDocumentFile(url);
      Docname=url;
      fileSave->setEnabled(false);
      //fileSaveAs->setEnabled(true);
      setCaption(url.fileName(), false);
    }

}



void KgpgApp::slotFileSave()
{
  // slotStatusMsg(i18n("Saving file..."));

  QString filn=Docname.path();
  if (filn=="")
    {
      slotFileSaveAs();
      return;
    }
  QFile f(filn);
  if ( !f.open( IO_WriteOnly ) )
    {
      return;
    }

  QTextStream t( &f );
  t << view->editor->text();
  f.close();
  fileSave->setEnabled(false);
  setCaption(Docname.fileName(),false);
}


void KgpgApp::slotFileSaveAs()
{

  KURL url=KFileDialog::getSaveURL(QDir::currentDirPath(),
                                   i18n("*|All Files"), this, i18n("Save As"));
  if(!url.isEmpty())
    {

      QString filn=url.path();
      QFile f(filn);
      if (f.exists())
        {
          QString message=i18n("Overwrite existing file %1?").arg(url.filename());
          int result=KMessageBox::warningContinueCancel(this,QString(message),i18n("Warning"),i18n("Overwrite"));
          if (result==KMessageBox::Cancel)
            return;
        }

      if ( !f.open( IO_WriteOnly ) )
        {
          return;
        }

      QTextStream t( &f );
      t << view->editor->text();
      f.close();
      Docname=url;
      fileSave->setEnabled(false);
      setCaption(url.fileName(),false);
    }
}

void KgpgApp::slotFilePrint()
{}

void KgpgApp::slotExpressQuit()
{
  //kapp->
  exit(0);
}

void KgpgApp::slotFileQuit()
{
  saveOptions();
  // close the first window, the list makes the next one the first again.
  // This ensures that queryClose() is called on each window to ask for closing
  /*
    KMainWindow* w;
    if(memberList)
    {
      for(w=memberList->first(); w!=0; w=memberList->first())
      {
        // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
        // the window and the application stay open.
        if(!w->close())
  	break;
      }
    }
    */
  kapp->exit(0);
}

void KgpgApp::slotEditCut()
{
  view->editor->cut();
}

void KgpgApp::slotEditCopy()
{
  view->editor->copy();
}

void KgpgApp::slotEditPaste()
{
  view->editor->paste();
}


void KgpgApp::fastencode(QString &selec,QString encryptOptions,bool shred,bool symetric)
{
  //////////////////              encode from file
  if ((selec==NULL) && (!symetric))
    {
      KMessageBox::sorry(0,i18n("You have not chosen an encryption key."));
      return;
    }

  KURL dest;

  if (encryptOptions.find("--armor")!=-1) dest.setPath(urlselected.path()+".asc");
  else dest.setPath(urlselected.path()+".gpg");

  QFile fgpg(dest.path());

  if (fgpg.exists())
    {
      KgpgOverwrite *over=new KgpgOverwrite(0,"overwrite",dest);
      if (over->exec()==QDialog::Accepted)
        dest.setFileName(over->getfname());
      else
        return;
    }
  if (pgpcomp)
    encryptOptions+=" --pgp6 ";
  KgpgInterface *cryptFileProcess=new KgpgInterface();
  cryptFileProcess->KgpgEncryptFile(selec,urlselected,dest,encryptOptions,symetric);
  //KgpgEncryptFile *cryptFileProcess=new KgpgEncryptFile(selec,urlselected,dest,symetric,encryptOptions);
  
  connect(cryptFileProcess,SIGNAL(processaborted(bool)),this,SLOT(processenc(bool)));
  connect(cryptFileProcess,SIGNAL(processstarted()),this,SLOT(processpopup2()));
  if (shred)
    connect(cryptFileProcess,SIGNAL(encryptionfinished()),this,SLOT(shredprocessenc()));
  else
    connect(cryptFileProcess,SIGNAL(encryptionfinished()),this,SLOT(processenc()));
	connect(cryptFileProcess,SIGNAL(errormessage(QString)),this,SLOT(processencerror(QString)));
}

void KgpgApp::shredprocessenc()
{
#if (KDE_VERSION >= 310)
delete pop;
#else
delete clippop;
#endif
      //KMessageBox::sorry(0,"shred");
      KShred *shredres=new KShred(urlselected.path());
      if (shredres->shred()==false) KMessageBox::sorry(0,i18n("The source file could not be shredded.\nCheck your permissions."));
if (commandLineMode) kapp->exit(0);
    
}


void KgpgApp::processenc()
{
#if (KDE_VERSION >= 310)
delete pop;
#else
delete clippop;
#endif
  if (commandLineMode)
    kapp->exit(0);
}

void KgpgApp::processencerror(QString mssge)
{
#if (KDE_VERSION >= 310)
delete pop;
#else
delete clippop;
#endif
  KMessageBox::detailedSorry(0,i18n("Encryption failed."),mssge);
  if (commandLineMode) kapp->exit(0);
}

void KgpgApp::fastdecode(bool quit)
{
  //////////////////////////////////////////////////////////////////    decode file from konqueror or menu

  fastact=quit;
  QString oldname=urlselected.filename();

QString newname;

  if (oldname.endsWith(".gpg") || oldname.endsWith(".asc") || oldname.endsWith(".pgp"))
    oldname.truncate(oldname.length()-4);
  else oldname.append(".clear");
  KURL swapname(urlselected.directory(0,0)+oldname);

  if (!fastact)
    {
      popupName *popn=new popupName(i18n("Decryption To"), this, "decryption to", swapname);
      if (popn->exec()==QDialog::Accepted)
        {
          if (popn->checkFile->isChecked())
            newname=popn->newFilename->text();
        }
      else {delete popn;return;}
      delete popn;
    }
  else 
  newname=swapname.path();  ////////////////// fast mode
      
    
  if (!newname.isEmpty())
    {
      QFile fgpg(newname);
      if (fgpg.exists())
        {
          KgpgOverwrite *over=new KgpgOverwrite(0,"overwrite",KURL(newname));
          if (over->exec()==QDialog::Accepted)
            {
              if (fastact)
                newname=swapname.directory(0,0)+over->getfname();
              else
                newname=KURL(newname).directory(0,0)+over->getfname();
            }
          else
            {
              if (fastact) exit(1);
              else
                return;
            }
        }
    }
  KgpgInterface *decryptFileProcess=new KgpgInterface();
  decpasssrc=urlselected;
  if (!newname.isEmpty()) ////////////////////   decrypt to file
  {	
  decpassdest=KURL(newname);
  decryptFileProcess->KgpgDecryptFile(decpasssrc,decpassdest,customDecrypt);
  connect(decryptFileProcess,SIGNAL(processaborted(bool)),this,SLOT(processdecover()));
  connect(decryptFileProcess,SIGNAL(processstarted()),this,SLOT(processpopup()));
  connect(decryptFileProcess,SIGNAL(decryptionfinished()),this,SLOT(processdecover()));
  connect(decryptFileProcess,SIGNAL(errormessage(QString)),this,SLOT(processdecerror(QString)));
  }
  else openEncryptedDocumentFile(urlselected);
}

void KgpgApp::processpopup()
{
#if (KDE_VERSION >= 310)
pop = new KPassivePopup( this);
      pop->setView(i18n("Processing decryption"),i18n("Please wait..."),KGlobal::iconLoader()->loadIcon("kgpg",KIcon::Desktop));
	  	pop->show();	  
	  	QRect qRect(QApplication::desktop()->screenGeometry());
		int iXpos=qRect.width()/2-pop->width()/2;
		int iYpos=qRect.height()/2-pop->height()/2;
      	pop->move(iXpos,iYpos);
#else
	clippop = new QDialog( this,0,false,WStyle_Customize | WStyle_NormalBorder);
              QVBoxLayout *vbox=new QVBoxLayout(clippop,3);
              QLabel *tex=new QLabel(clippop);
              tex->setText(i18n("<b>Processing decryption</b>"));
			  QLabel *tex2=new QLabel(clippop);
			  tex2->setText(i18n("Please wait..."));
              vbox->addWidget(tex);
			  vbox->addWidget(tex2);
              clippop->setMinimumWidth(250);
              clippop->adjustSize();
			  clippop->show();
#endif
}

void KgpgApp::processpopup2()
{
#if (KDE_VERSION >= 310)
 pop = new KPassivePopup( this);
      pop->setView(i18n("Processing encryption"),i18n("Please wait..."),KGlobal::iconLoader()->loadIcon("kgpg",KIcon::Desktop));
	  	pop->show();	  
	  	QRect qRect(QApplication::desktop()->screenGeometry());
		int iXpos=qRect.width()/2-pop->width()/2;
		int iYpos=qRect.height()/2-pop->height()/2;
      	pop->move(iXpos,iYpos);
#else
	clippop = new QDialog( this,0,false,WStyle_Customize | WStyle_NormalBorder);
              QVBoxLayout *voxLayout(clippop,3);
              QLabel *tex=new QLabel(clippop);
              tex->setText(i18n("<b>Processing encryption</b>"));
			  QLabel *tex2=new QLabel(clippop);
			  tex2->setText(i18n("Please wait..."));
              vbox->addWidget(tex);
			  vbox->addWidget(tex2);
              clippop->setMinimumWidth(250);
              clippop->adjustSize();
			  clippop->show();
#endif
}

void KgpgApp::processdecover()
{
#if (KDE_VERSION >= 310)
delete pop;
#else
delete clippop;
#endif
if (fastact) kapp->exit(0);
}

void KgpgApp::processdecerror(QString mssge)
{
#if (KDE_VERSION >= 310)
delete pop;
#else
delete clippop;
#endif

  KMessageBox::detailedSorry(0,i18n("Decryption failed."),mssge);
  if (fastact) kapp->exit(0);
}


void KgpgApp::slotFileEnc()
{
  /////////////////////////////////////////////////////////////////////////  encode file file

  KURL url=KFileDialog::getOpenURL(QString::null,
                                   i18n("*|All Files"), this, i18n("Open File to Encode"));
  if (!url.isEmpty())
    {
      urlselected=url;
      if (!encryptfileto)
        {
          popupPublic *dialogue=new popupPublic(this,"Public keys",url.filename(),true);
          connect(dialogue,SIGNAL(selectedKey(QString &,QString,bool,bool)),this,SLOT(fastencode(QString &,QString,bool,bool)));
          dialogue->exec();
          delete dialogue;
        }
      else
	  	{
        	QString opts;
		  	if (untrusted) opts=" --always-trust ";
			if (ascii) opts+=" --armor ";
            fastencode(filekey,opts,false,false);
		}
    }
}

void KgpgApp::slotFileDec()
{
  /////////////////////////////////////////////////////////////////////////  decode file file

  KURL url=KFileDialog::getOpenURL(QString::null,
                                   i18n("*|All Files"), this, i18n("Open File to Decode"));
  if (!url.isEmpty())
    {
      urlselected=url;
      fastdecode(false);
    }
}
#include "kgpg.moc"

