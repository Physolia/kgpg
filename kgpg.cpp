/***************************************************************************
                          kgpg.cpp  -  description
                             -------------------
    begin                : Mon Nov 18 2002
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


#include <qlabel.h>
#include <qpixmap.h>
#include <qclipboard.h>
#include <qfile.h>
#include <kglobal.h>
#include <kdeversion.h>
#include <klocale.h>
#include <kconfig.h>
#include <kapplication.h>
#include <kmessagebox.h>
#include <kcombobox.h>
#include <kwin.h>
#include <kprocess.h>
#include <kprocio.h>
#include <qcursor.h>
#include <qwidget.h>
#include <qtooltip.h>
#include <kaboutapplication.h>
#include <qpopupmenu.h>
#include <kaction.h>
#include <kurlrequester.h>
#include <ktip.h>
#include <qregexp.h>
#include <kurldrag.h>
#include <kdebug.h>

#include "kgpg.h"


MyView::MyView( QWidget *parent, const char *name )
                : QLabel( parent, name )
{
        setBackgroundMode(  X11ParentRelative );

        KAction *saveDecrypt = new KAction(i18n("&Decrypt && Save File"),"decrypted",0,this, SLOT(decryptDroppedFile()),this,"decrypt_file");
        KAction *showDecrypt = new KAction(i18n("&Show Decrypted File"),"edit",0,this, SLOT(showDroppedFile()),this,"show_file");
        KAction *encrypt = new KAction(i18n("&Encrypt File"),"encrypted",0,this, SLOT(encryptDroppedFile()),this,"encrypt_file");
        KAction *sign = new KAction(i18n("&Sign File"), "signature",0,this, SLOT(signDroppedFile()),this,"sign_file");
        //QToolTip::add(this,i18n("KGpg drag & drop encryption applet"));

        ksConfig=kapp->config();
        readOptions();

        if (tipofday)
                KTipDialog::showTip(this, "kgpg/tips", true);

        setPixmap(KGlobal::iconLoader()->loadIcon("kgpg",KIcon::User,22));
        resize(24,24);
        setAcceptDrops(true);

        droppopup=new QPopupMenu();
        showDecrypt->plug(droppopup);
        saveDecrypt->plug(droppopup);

        udroppopup=new QPopupMenu();
        encrypt->plug(udroppopup);
        sign->plug(udroppopup);
}

MyView::~MyView()
{

    delete droppopup;
    droppopup = 0;
    delete udroppopup;
    udroppopup = 0;
}


void MyView::showPopupMenu( QPopupMenu *menu )
{
        Q_ASSERT( menu != 0L );

        menu->move(-1000,-1000);
        menu->show();
        menu->hide();

        QPoint g = QCursor::pos();
        if ( menu->height() < g.y() )
                menu->popup(QPoint( g.x(), g.y() - menu->height()));
        else
                menu->popup(QPoint(g.x(), g.y()));
}


void  MyView::openKeyServer()
{
        if(!m_keyServer) {
                //keyServer *ks
                m_keyServer=new keyServer(0,"server_dialog",false,WDestructiveClose);
                connect( m_keyServer , SIGNAL( destroyed() ) , this, SLOT( slotKeyServerClosed()));
        }
        m_keyServer->show();
        KWin::setOnDesktop( m_keyServer->winId() , KWin::currentDesktop() );  //set on the current desktop
        KWin::deIconifyWindow( m_keyServer->winId());  //de-iconify window
        m_keyServer->raise();  // set on top
}


void  MyView::clipEncrypt()
{
        popupPublic *dialoguec=new popupPublic(this, "public_keys", 0,false);
        connect(dialoguec,SIGNAL(selectedKey(QString &,QString,bool,bool)),this,SLOT(encryptClipboard(QString &,QString)));
        dialoguec->show();
}

void  MyView::clipDecrypt()
{
        QString clippie=kapp->clipboard()->text().stripWhiteSpace();
        if (clippie.startsWith("-----BEGIN PGP MESSAGE")) {
                KgpgApp *kgpgtxtedit = new KgpgApp(0, "editor",WDestructiveClose);
                kgpgtxtedit->view->editor->setText(clippie);
                kgpgtxtedit->view->slotdecode();
                kgpgtxtedit->show();
        } else
                KMessageBox::sorry(this,i18n("No encrypted text found."));
}


void  MyView::openEditor()
{
        KgpgApp *kgpgtxtedit = new KgpgApp(0, "editor",WType_Dialog);
        kgpgtxtedit->show();
}

void  MyView::encryptDroppedFile()
{
        QString opts="";
        KgpgLibrary *lib=new KgpgLibrary(pgpExtension);
        if (encryptfileto) {
                if (untrusted)
                        opts=" --always-trust ";
                if (ascii)
                        opts+=" --armor ";
                if (hideid)
                        opts+=" --throw-keyid ";
                if (pgpcomp)
                        opts+=" --pgp6 ";
                lib->slotFileEnc(droppedUrls,opts,filekey);
        } else
                lib->slotFileEnc(droppedUrls);
}


void  MyView::shredDroppedFile()
{
        if (KMessageBox::warningContinueCancelList(0,i18n("Do you really want to shred these files"),droppedUrls.toStringList())!=KMessageBox::Continue)
                return;
        KURL::List::iterator it;
        for ( it = droppedUrls.begin(); it != droppedUrls.end(); ++it ) {
                if (!KURL(*it).isLocalFile())
                        KMessageBox::sorry(0,i18n("Cannot shred remote files!"));
                else {
                        kgpgShredWidget *sh=new kgpgShredWidget(0,"shred");
                        sh->setCaption(i18n("Shredding %1").arg(KURL(*it).filename()));
                        sh->show();
                        sh->kgpgShredFile(KURL(*it));
                }
        }
}


void  MyView::slotVerifyFile()
{
        ///////////////////////////////////   check file signature
        if (droppedUrl.isEmpty())
                return;

        QString sigfile="";
        //////////////////////////////////////       try to find detached signature.
        if (!droppedUrl.filename().endsWith(".sig")) {
                sigfile=droppedUrl.path()+".sig";
                QFile fsig(sigfile);
                if (!fsig.exists()) {
                        sigfile=droppedUrl.path()+".asc";
                        QFile fsig(sigfile);
                        //////////////   if no .asc or .sig signature file included, assume the file is internally signed
                        if (!fsig.exists())
                                sigfile="";
                }
        } else {
                sigfile=droppedUrl.path();
                droppedUrl=KURL(sigfile.left(sigfile.length()-4));
        }

        ///////////////////////// pipe gpg command
        KgpgInterface *verifyFileProcess=new KgpgInterface();
        verifyFileProcess->KgpgVerifyFile(droppedUrl,KURL(sigfile));
        connect (verifyFileProcess,SIGNAL(verifyquerykey(QString)),this,SLOT(importSignature(QString)));
}

void  MyView::importSignature(QString ID)
{
        keyServer *kser=new keyServer(0,"server_dialog",false,WDestructiveClose);
        kser->kLEimportid->setText(ID);
        kser->slotImport();
}

void  MyView::signDroppedFile()
{

        //////////////////////////////////////   create a detached signature for a chosen file
        if (droppedUrl.isEmpty())
                return;

        QString signKeyID;
        //////////////////   select a private key to sign file --> listkeys.cpp
        KgpgSelKey *opts=new KgpgSelKey(0,"select_secret",false);
        if (opts->exec()==QDialog::Accepted)
                signKeyID=opts->getkeyID();
        else {
                delete opts;
                return;
        }
        delete opts;
        QString Options;
        if (ascii)
                Options=" --armor ";
        if (pgpcomp)
                Options+=" --pgp6 ";
        KgpgInterface *signFileProcess=new KgpgInterface();
        signFileProcess->KgpgSignFile(signKeyID,droppedUrl,Options);
}

void  MyView::decryptDroppedFile()
{
        if (!droppedUrl.isLocalFile()) {
                showDroppedFile();
                return;
        }
        QString oldname=droppedUrl.filename();
        if (oldname.endsWith(".gpg") || oldname.endsWith(".asc") || oldname.endsWith(".pgp"))
                oldname.truncate(oldname.length()-4);
        else
                oldname.append(".clear");
        KURL swapname(droppedUrl.directory(0,0)+oldname);
        QFile fgpg(swapname.path());
        if (fgpg.exists()) {
                KgpgOverwrite *over=new KgpgOverwrite(0,"overwrite",swapname);
                if (over->exec()==QDialog::Accepted)
                    swapname=KURL(swapname.directory(0,0)+over->getfname());
                else
                {
                    delete over;
                    return;
                }
                delete over;
        }

        KgpgLibrary *lib=new KgpgLibrary();
        lib->slotFileDec(droppedUrl,swapname,customDecrypt);
}

void  MyView::showDroppedFile()
{
        KgpgApp *kgpgtxtedit = new KgpgApp(0, "editor",WDestructiveClose);
        kgpgtxtedit->view->editor->droppedfile(droppedUrl);
        kgpgtxtedit->show();
}


void  MyView::droppedfile (KURL::List url)
{
        droppedUrls=url;
        droppedUrl=url.first();
        if (KMimeType::findByURL(droppedUrl)->name()=="inode/directory") {
                KMessageBox::sorry(0,i18n("Sorry, only file operations are currently supported."));
                return;
        }
        if (!droppedUrl.isLocalFile()) {
                showDroppedFile();
                return;
        }
        if ((droppedUrl.path().endsWith(".asc")) || (droppedUrl.path().endsWith(".pgp")) || (droppedUrl.path().endsWith(".gpg"))) {
                switch (efileDropEvent) {
                case 0:
                        decryptDroppedFile();
                        break;
                case 1:
                        showDroppedFile();
                        break;
                case 2:
                        droppopup->exec(QCursor::pos ());
                        break;
                }
        } else if (droppedUrl.path().endsWith(".sig")) {
                slotVerifyFile();
        } else
                switch (ufileDropEvent) {
                case 0:
                        encryptDroppedFile();
                        break;
                case 1:
                        signDroppedFile();
                        break;
                case 2:
                        udroppopup->exec(QCursor::pos ());
                        break;
                }
}


void  MyView::droppedtext (QString inputText)
{

        QClipboard *cb = QApplication::clipboard();
        cb->setText(inputText);
        if (inputText.startsWith("-----BEGIN PGP MESSAGE")) {
                clipDecrypt();
                return;
        }
        if (inputText.startsWith("-----BEGIN PGP PUBLIC KEY")) {
                int result=KMessageBox::warningContinueCancel(0,i18n("<p>The dropped text is a public key.<br>Do you want to import it ?</p>"),i18n("Warning"));
                if (result==KMessageBox::Cancel)
                        return;
                else {
                        KgpgInterface *importKeyProcess=new KgpgInterface();
                        importKeyProcess->importKey(inputText,false);
                        return;
                }
        }
        clipEncrypt();
}


void  MyView::dragEnterEvent(QDragEnterEvent *e)
{
        e->accept (KURLDrag::canDecode(e) || QTextDrag::canDecode (e));
}


void  MyView::dropEvent (QDropEvent *o)
{
        KURL::List list;
        QString text;
        if ( KURLDrag::decode( o, list ) )
                droppedfile(list);
        else if ( QTextDrag::decode(o, text) )
                droppedtext(text);
}


void  MyView::readOptions()
{
        ksConfig->setGroup("Applet");
        ufileDropEvent=ksConfig->readNumEntry("unencrypted drop event",0);
        efileDropEvent=ksConfig->readNumEntry("encrypted drop event",2);

        ksConfig->setGroup("General Options");
        encryptfileto=ksConfig->readBoolEntry("encrypt files to",false);
        filekey=ksConfig->readEntry("file key");
        ascii=ksConfig->readBoolEntry("Ascii armor",true);
        untrusted=ksConfig->readBoolEntry("Allow untrusted keys",false);
        hideid=ksConfig->readBoolEntry("Hide user ID",false);
        pgpcomp=ksConfig->readBoolEntry("PGP compatibility",false);
	pgpExtension=ksConfig->readBoolEntry("Pgp extension",false);
        customDecrypt=ksConfig->readEntry("custom decrypt");
        if (ksConfig->readBoolEntry("selection clip",false)) {
                if (kapp->clipboard()->supportsSelection())
                        kapp->clipboard()->setSelectionMode(true);
        } else
                kapp->clipboard()->setSelectionMode(false);

        if (ksConfig->readBoolEntry("First run",true))
                firstRun();
        if (ksConfig->readPathEntry("gpg config path").isEmpty())
                startWizard();



        ksConfig->setGroup("TipOfDay");
        tipofday=ksConfig->readBoolEntry("RunOnStart",true);
}


void  MyView::firstRun()
{
        FILE *fp;
        QString tst;
        char line[200];
        bool found=false;

        kgpgOptions *opts=new kgpgOptions(this,0);
        opts->checkMimes();
        delete opts;

        fp = popen("gpg --no-tty --with-colon --list-secret-keys", "r");
        while ( fgets( line, sizeof(line), fp)) {
                tst=line;
                if (tst.startsWith("sec")) {
                        found=true;
                        break;
                }
        }
        pclose(fp);
        startWizard();
}


void  MyView::startWizard()
{
        if (wiz)
                return;
        wiz=new KgpgWizard(0,"wizard");
        QString confPath=QDir::homeDirPath()+"/.gnupg/options";
        if (!QFile(confPath).exists()) {
                confPath=QDir::homeDirPath()+"/.gnupg/gpg.conf";
                if (!QFile(confPath).exists()) {
                        wiz->text_optionsfound->setText(i18n("<qt><b>The GnuPG configuration file was not found</b>. Please make sure you have GnuPG installed and give the path to the file.</qt>"));
                        confPath="";
                }
        }
        wiz->kURLRequester1->setURL(confPath);
        wiz->kURLRequester2->setURL(QString(QDir::homeDirPath()+"/Desktop"));
        wiz->kURLRequester2->setMode(2);
        connect(wiz->pushButton4,SIGNAL(clicked()),this,SLOT(slotGenKey()));
        connect(wiz->finishButton(),SIGNAL(clicked()),this,SLOT(slotSaveOptionsPath()));
        connect( wiz , SIGNAL( destroyed() ) , this, SLOT( slotWizardClose()));

        wiz->setFinishEnabled(wiz->page_4,true);
        wiz->show();
}

void  MyView::slotSaveOptionsPath()
{

        if (wiz->checkBox1->isChecked()) {
                KURL path;
                path.addPath(wiz->kURLRequester2->url());
                path.adjustPath(1);
                path.setFileName("shredder.desktop");
                KDesktopFile configl2(path.path(), false);
                if (configl2.isImmutable() ==false) {
                        configl2.setGroup("Desktop Entry");
                        configl2.writeEntry("Type", "Application");
                        configl2.writeEntry("Name",i18n("Shredder"));
                        configl2.writeEntry("Icon","shredder");
                        configl2.writeEntry("Exec","kgpg -X %U");
                }
        }

        ksConfig->setGroup("Applet");
        ksConfig->writeEntry("AutoStart", wiz->checkBox2->isChecked());

        ksConfig->setGroup("General Options");
#if KDE_IS_VERSION(3,1,3)
        ksConfig->writePathEntry("gpg config path",wiz->kURLRequester1->url());
#else
        ksConfig->writeEntry("gpg config path",wiz->kURLRequester1->url());
#endif
        ksConfig->writeEntry("First run",false);
        ksConfig->sync();
        if (wiz)
                delete wiz;
}

void  MyView::slotWizardClose()
{
        wiz=0L;
}

void  MyView::slotGenKey()
{
        listKeys *creat=new listKeys(0);
        creat->slotgenkey();
        delete creat;
}

void  MyView::about()
{
        KAboutApplication dialog(kapp->aboutData());//_aboutData);
        dialog.exec();
}

void  MyView::help()
{
        kapp->invokeHelp(0,"kgpg");
}


void  MyView::preferences()
{
        kgpgOptions *opts=new kgpgOptions();
        opts->exec();
        delete opts;
        readOptions();
}


void MyView::slotKeyServerClosed()
{
    delete m_keyServer;
    m_keyServer=0L;
}


kgpgapplet::kgpgapplet(QWidget *parent, const char *name)
                : KSystemTray(parent,name)
{
        w=new MyView(this);
        w->show();
        KPopupMenu *conf_menu=contextMenu();
        KAction *KgpgEncryptClipboard = new KAction(i18n("&Encrypt Clipboard"), 0, 0,this, SLOT(slotencryptclip()),actionCollection(),"clip_encrypt");
        KAction *KgpgDecryptClipboard = new KAction(i18n("&Decrypt Clipboard"), 0, 0,this, SLOT(slotdecryptclip()),actionCollection(),"clip_decrypt");
        KAction *KgpgOpenEditor = new KAction(i18n("&Open Editor"), "edit", 0,this, SLOT(sloteditor()),actionCollection(),"kgpg_editor");
        KAction *KgpgPreferences=KStdAction::preferences(this, SLOT(slotOptions()), actionCollection());
        KgpgEncryptClipboard->plug(conf_menu);
        KgpgDecryptClipboard->plug(conf_menu);
        KgpgOpenEditor->plug(conf_menu);
        conf_menu->insertSeparator();
        KgpgPreferences->plug(conf_menu);
}

kgpgapplet::~kgpgapplet()
{
    delete w;
    w = 0L;
}

void kgpgapplet::sloteditor()
{
        w->openEditor();
}

void kgpgapplet::slotencryptclip()
{
        w->clipEncrypt();
}

void kgpgapplet::slotdecryptclip()
{
        w->clipDecrypt();
}

void kgpgapplet::slotOptions()
{
        w->preferences();
}

KgpgAppletApp::KgpgAppletApp()
                : KUniqueApplication()//, kgpg_applet( 0 )
{

}


KgpgAppletApp::~KgpgAppletApp()
{
    delete s_keyManager;
    s_keyManager=0L;
    delete kgpg_applet;
    kgpg_applet = 0L;
}

void KgpgAppletApp::slotHandleQuit()
{

        int autoStart = KMessageBox::questionYesNoCancel( 0, i18n("Should KGpg start automatically\nwhen you login?"), i18n("Automatically Start KGpg?"),KStdGuiItem::yes(),KStdGuiItem::no(),"Autostartup");
        kgpg_applet->w->ksConfig->setGroup("Applet");
        if ( autoStart == KMessageBox::Yes )
                kgpg_applet->w->ksConfig->writeEntry("AutoStart", true);
        else if ( autoStart == KMessageBox::No) {
                kgpg_applet->w->ksConfig->writeEntry("AutoStart", false);
        } else  // cancel chosen don't quit
                return;
        kgpg_applet->w->ksConfig->sync();
        quit();
}

int KgpgAppletApp::newInstance()
{
        args = KCmdLineArgs::parsedArgs();
        if ( kgpg_applet ) {
                kgpg_applet->show();
        } else {
kdDebug() << "Starting KGpg\n";
               s_keyManager=new listKeys(0, "key_manager");

                s_keyManager->refreshkey();
                kgpg_applet=new kgpgapplet(s_keyManager,"kgpg_systrayapplet");
                connect( kgpg_applet, SIGNAL(quitSelected()), this, SLOT(slotHandleQuit()));
                kgpg_applet->show();
                kgpg_applet->w->ksConfig->setGroup("General Options");
                QString gpgPath=kgpg_applet->w->ksConfig->readPathEntry("gpg config path");

                if (gpgPath.isEmpty())
                        KMessageBox::sorry(0,"<qt>You did not set a path to your GnuPG config file.<br>This may bring some surprising results in KGpg's execution...</qt>");
                else {
                        if ((KgpgInterface::getGpgBoolSetting("use-agent",gpgPath)) && (!getenv("GPG_AGENT_INFO")))
                                KMessageBox::sorry(0,i18n("<qt>The use of <b>gpg-agent</b> is enabled in GnuPG's configuration file (%1).<br>"
                                                          "However, the agent doesn't seem to run. Please either disable the agent in config file or fix it. You will otherwise have problems with signing/decryption.").arg(gpgPath));
                }
        }

        ////////////////////////   parsing of command line args
        if (args->isSet("k")!=0) {
                s_keyManager->show();
                KWin::setOnDesktop( s_keyManager->winId() , KWin::currentDesktop() );  //set on the current desktop
                KWin::deIconifyWindow( s_keyManager->winId());  //de-iconify window
                s_keyManager->raise();  // set on top
                s_keyManager->refreshkey();
        } else
                if (args->count()>0) {
		kdDebug() << "KGpg: found files";

                        urlList.clear();

                        for (int ct=0;ct<args->count();ct++)
                                urlList.append(args->url(ct));

                        if (urlList.empty())
                                return 0;

                        kgpg_applet->w->droppedUrl=urlList.first();
                        if (KMimeType::findByURL(urlList.first())->name()=="inode/directory") {
                               KMessageBox::sorry(0,i18n("Sorry, only file operations are currently supported."));
                                return 0;
                        }
                        kgpg_applet->w->droppedUrls=urlList;

			if (args->isSet("e")!=0)
                                kgpg_applet->w->encryptDroppedFile();
                        else if (args->isSet("X")!=0)
                                kgpg_applet->w->shredDroppedFile();
                        else if (args->isSet("s")!=0)
                                kgpg_applet->w->showDroppedFile();
                        else if (args->isSet("S")!=0)
                                kgpg_applet->w->signDroppedFile();
                        else if (args->isSet("V")!=0)
                                kgpg_applet->w->slotVerifyFile();
                        else if (kgpg_applet->w->droppedUrl.filename().endsWith(".sig"))
                                kgpg_applet->w->slotVerifyFile();
                        else
                                kgpg_applet->w->decryptDroppedFile();
                }
	return 0;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void MyView::encryptClipboard(QString &selec,QString encryptOptions)
{
        QString clipContent=kapp->clipboard()->text();//=cb->text(QClipboard::Clipboard);   ///   QT 3.1 only

        if (!clipContent.isEmpty()) {
                if (pgpcomp)
                        encryptOptions+=" --pgp6 ";
                encryptOptions+=" --armor ";

                if (selec==NULL) {
                        KMessageBox::sorry(0,i18n("You have not chosen an encryption key."));
                }//exit(0);}

                KgpgInterface *txtEncrypt=new KgpgInterface();
                connect (txtEncrypt,SIGNAL(txtencryptionfinished(QString)),this,SLOT(slotSetClip(QString)));
                txtEncrypt->KgpgEncryptText(clipContent,selec,encryptOptions);
                QString cryptedClipboard;
                if (clipContent.length()>300)
                        cryptedClipboard=QString(clipContent.left(250).stripWhiteSpace())+"...\n"+QString(clipContent.right(40).stripWhiteSpace());
                else
                        cryptedClipboard=clipContent;


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
                QTimer::singleShot( 3200, this, SLOT(killDisplayClip()));
                //KMessageBox::information(this,i18n("<b>Encrypted following text</b>:<br>")+cryptedClipboard);
#endif

        } else {
                KMessageBox::sorry(0,i18n("Clipboard is empty."));
                //exit(0);
        }
}

void MyView::slotSetClip(QString newtxt)
{
        if (!newtxt.isEmpty()) {
                QClipboard *clip=QApplication::clipboard();
                clip->setText(newtxt);//,QClipboard::Clipboard);    QT 3.1 only
                //connect(kapp->clipboard(),SIGNAL(dataChanged ()),this,SLOT(expressQuit()));
        }
        //else expressQuit();
}

void MyView::killDisplayClip()
{
#if (KDE_VERSION < 310)
        delete clippop;
#endif
}



/////////////////////////////////////////////////////////////////////////////

#include "kgpg.moc"


