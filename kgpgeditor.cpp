/***************************************************************************
                          kgpgeditor.cpp  -  description
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

#include <QTextStream>
#include <QCloseEvent>
#include <QTextCodec>
#include <QPainter>
#include <ktoggleaction.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <QtDBus>
#include <kicon.h>

#include <kencodingfiledialog.h>
#include <kio/netaccess.h>
#include <kio/renamedialog.h>
#include <kapplication.h>
#include <kmessagebox.h>
#include <ktemporaryfile.h>
#include <kprinter.h>
#include <kaction.h>
#include <klocale.h>
#include <kdebug.h>

#include "selectsecretkey.h"
#include "kgpgmd5widget.h"
#include "kgpgsettings.h"
#include "sourceselect.h"
#include "kgpglibrary.h"
#include "kgpgeditor.h"
#include "keyservers.h"
#include "kgpgview.h"
#include "kgpg.h"

KgpgEditor::KgpgEditor(QWidget *parent, Qt::WFlags f, KShortcut gohome, bool mainwindow)
          : KXmlGuiWindow(parent, f)
{
    m_ismainwindow = mainwindow;
    m_textencoding = QString();
    m_godefaultkey = gohome;

    // call inits to invoke all other construction parts
    initActions();
    initView();

    setObjectName("editor");
    slotSetFont(KGpgSettings::font());
    setupGUI((ToolBar | Keys | StatusBar | Save | Create), "kgpg.rc");
    setAutoSaveSettings("Editor", true);

    connect(view, SIGNAL(textChanged()), this, SLOT(modified()));
    connect(view, SIGNAL(newText()), this, SLOT(newText()));
    connect(view->editor, SIGNAL(undoAvailable(bool)), this, SLOT(slotUndoAvailable(bool)));
    connect(view->editor, SIGNAL(redoAvailable(bool)), this, SLOT(slotRedoAvailable(bool)));
    connect(view->editor, SIGNAL(copyAvailable(bool)), this, SLOT(slotCopyAvailable(bool)));
}

KgpgEditor::~KgpgEditor()
{
    delete view;
}

void KgpgEditor::openDocumentFile(const KUrl& url, const QString &encoding)
{
    QString tempopenfile;
    if(KIO::NetAccess::download(url, tempopenfile, this))
    {
        QFile qfile(tempopenfile);
        if (qfile.open(QIODevice::ReadOnly))
        {
            QTextStream t(&qfile);
            t.setCodec(encoding.toAscii());
            view->editor->setPlainText(t.readAll());
            qfile.close();
            m_docname = url;
            m_textchanged = false;
            setCaption(url.fileName(), false);
        }
        KIO::NetAccess::removeTempFile(tempopenfile);
    }
}

void KgpgEditor::openEncryptedDocumentFile(const KUrl& url)
{
    view->editor->slotDroppedFile(url);
}

void KgpgEditor::slotSetFont(QFont myFont)
{
    view->editor->setFont(myFont);
}

void KgpgEditor::closeWindow()
{
    close();
}

void KgpgEditor::saveOptions()
{
    KGpgSettings::setFirstRun(false);
    KGpgSettings::writeConfig();
}

void KgpgEditor::initActions()
{
    KStandardAction::openNew(this, SLOT(slotFileNew()), actionCollection());
    KStandardAction::open(this, SLOT(slotFileOpen()), actionCollection());
    KStandardAction::save(this, SLOT(slotFileSave()), actionCollection());
    KStandardAction::saveAs(this, SLOT(slotFileSaveAs()), actionCollection());
    KStandardAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    KStandardAction::paste(this, SLOT(slotEditPaste()), actionCollection());
    KStandardAction::print(this, SLOT(slotFilePrint()), actionCollection());
    KStandardAction::selectAll(this, SLOT(slotSelectAll()), actionCollection());
    actionCollection()->addAction(KStandardAction::Preferences, "kgpg_config",
                                  this, SLOT(slotOptions()));

    m_editundo = KStandardAction::undo(this, SLOT(slotundo()), actionCollection());
    m_editredo = KStandardAction::redo(this, SLOT(slotredo()), actionCollection());
    m_editcopy = KStandardAction::copy(this, SLOT(slotEditCopy()), actionCollection());
    m_editcut  = KStandardAction::cut(this, SLOT(slotEditCut()), actionCollection());

    QAction *action = actionCollection()->addAction("file_encrypt");
    action->setIcon(KIcon("encrypted"));
    action->setText(i18n("&Encrypt File..."));
    connect(action, SIGNAL(triggered(bool)), SLOT(slotFilePreEnc()));
    action = actionCollection()->addAction("file_decrypt");
    action->setIcon(KIcon("decrypted"));
    action->setText(i18n("&Decrypt File..."));
    connect(action, SIGNAL(triggered(bool)), SLOT(slotFilePreDec()));
    action = actionCollection()->addAction("key_manage");
    action->setIcon(KIcon("kgpg"));
    action->setText(i18n("&Open Key Manager"));
    connect(action, SIGNAL(triggered(bool)), SLOT(slotKeyManager()));
    action = actionCollection()->addAction("sign_generate");
    action->setText(i18n("&Generate Signature..."));
    connect(action, SIGNAL(triggered(bool) ), SLOT(slotPreSignFile()));
    action = actionCollection()->addAction("sign_verify");
    action->setText(i18n("&Verify Signature..."));
    connect(action, SIGNAL(triggered(bool) ), SLOT(slotPreVerifyFile()));
    action = actionCollection()->addAction("sign_check");
    action->setText(i18n("&Check MD5 Sum..."));
    connect(action, SIGNAL(triggered(bool) ), SLOT(slotCheckMd5()));

    m_encodingaction = actionCollection()->add<KToggleAction>("charsets");
    m_encodingaction->setText(i18n("&Unicode (utf-8) Encoding"));
    connect(m_encodingaction, SIGNAL(triggered(bool) ), SLOT(slotSetCharset()));
}

void KgpgEditor::initView()
{
    view = new KgpgView(this);
    connect(view, SIGNAL(resetEncoding(bool)), this, SLOT(slotResetEncoding(bool)));
    setCentralWidget(view);
    setCaption(i18n("Untitled"), false);
    m_editredo->setEnabled(false);
    m_editundo->setEnabled(false);
    m_editcopy->setEnabled(false);
    m_editcut->setEnabled(false);
    m_textchanged = false;
}

void KgpgEditor::closeEvent (QCloseEvent *e)
{
    if (!m_ismainwindow)
    {
        KGlobal::ref();
        KXmlGuiWindow::closeEvent(e);
    }
    else
        e->accept();
}

bool KgpgEditor::queryClose()
{
    return saveBeforeClear();
}

bool KgpgEditor::saveBeforeClear()
{
    if (m_textchanged)
    {
        QString fname;
        if (m_docname.fileName().isEmpty())
            fname = i18n("Untitled");
        else
            fname = m_docname.fileName();

        QString msg = i18n("The document \"%1\" has changed.\nDo you want to save it?", fname);
        QString caption = i18n("Close the document");
        int res = KMessageBox::warningYesNoCancel(this, msg, caption, KStandardGuiItem::save(), KStandardGuiItem::discard());
        if (res == KMessageBox::Yes)
            return slotFileSave();
        else
        if (res == KMessageBox::No)
            return true;
        else
            return false;
    }

    return true;
}

void KgpgEditor::slotFileNew()
{
    if (saveBeforeClear())
    {
        view->editor->clear();
        newText();
    }
}

void KgpgEditor::slotFileOpen()
{
    if (saveBeforeClear())
    {
        KEncodingFileDialog::Result loadResult;
        loadResult = KEncodingFileDialog::getOpenUrlAndEncoding(QString(), QString(), QString(), this);
        KUrl url = loadResult.URLs.first();
        m_textencoding = loadResult.encoding;

        if(!url.isEmpty())
            openDocumentFile(url, m_textencoding);
    }
}

bool KgpgEditor::slotFileSave()
{
    QString filn = m_docname.path();
    if (filn.isEmpty())
        return slotFileSaveAs();

    QTextCodec *cod = QTextCodec::codecForName(m_textencoding.toAscii());

    if (!checkEncoding(cod))
    {
        KMessageBox::sorry(this, i18n("The document could not been saved, as the selected encoding cannot encode every unicode character in it."));
        return false;
    }

    if (m_docname.isLocalFile())
    {
        QFile f(filn);
        if (!f.open(QIODevice::WriteOnly))
        {
            KMessageBox::sorry(this, i18n("The document could not be saved, please check your permissions and disk space."));
            return false;
        }

        QTextStream t(&f);
        t.setCodec(cod);
        t << view->editor->toPlainText();
        f.close();
    }
    else
    {
        KTemporaryFile tmpfile;
        tmpfile.open();
        QTextStream stream(&tmpfile);
        stream.setCodec(cod);
        stream << view->editor->toPlainText();

        if(!KIO::NetAccess::upload(tmpfile.fileName(), m_docname, this))
        {
            KMessageBox::sorry(this, i18n("The document could not be saved, please check your permissions and disk space."));
            return false;
        }
    }

    m_textchanged = false;
    setCaption(m_docname.fileName(), false);
    return true;
}

bool KgpgEditor::slotFileSaveAs()
{
    KEncodingFileDialog::Result saveResult;
    saveResult = KEncodingFileDialog::getSaveUrlAndEncoding(QString(), QString(), QString(), this);
    KUrl url = saveResult.URLs.first();
    QString selectedEncoding = saveResult.encoding;

    if(!url.isEmpty())
    {
        if (url.isLocalFile())
        {
            QString filn = url.path();
            QFile f(filn);
            if (f.exists())
            {
                QString message = i18n("Overwrite existing file %1?", url.fileName());
                int result = KMessageBox::warningContinueCancel(this, QString(message), i18n("Warning"), KStandardGuiItem::overwrite());
                if (result == KMessageBox::Cancel)
                    return false;
            }
            f.close();
        }
        else
        if (KIO::NetAccess::exists(url, false, this))
        {
            QString message = i18n("Overwrite existing file %1?", url.fileName());
            int result = KMessageBox::warningContinueCancel(this, QString(message), i18n("Warning"), KStandardGuiItem::overwrite());
            if (result == KMessageBox::Cancel)
                return false;
        }

        m_docname = url;
        m_textencoding = selectedEncoding;
        slotFileSave();
        return true;
    }
    return false;
}

void KgpgEditor::slotFilePrint()
{
    KPrinter prt;
    if (prt.setup(this))
    {
        int width = prt.width();
        int height = prt.height();
        QPainter painter(&prt);
        painter.drawText(0, 0, width, height, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, view->editor->toPlainText());
    }
}

void KgpgEditor::slotFilePreEnc()
{
    QStringList opts;
    KUrl::List urls = KFileDialog::getOpenUrls(KUrl(), i18n("*|All Files"), this, i18n("Open File to Encode"));
    if (urls.isEmpty())
        return;
    emit encryptFiles(urls);
}

void KgpgEditor::slotFilePreDec()
{
    KUrl url = KFileDialog::getOpenUrl(KUrl(), i18n("*|All Files"), this, i18n("Open File to Decode"));
    if (url.isEmpty())
        return;

    QString oldname = url.fileName();
    QString newname;

    if (oldname.endsWith(".gpg") || oldname.endsWith(".asc") || oldname.endsWith(".pgp"))
        oldname.truncate(oldname.length() - 4);
    else
        oldname.append(".clear");
    oldname.prepend(url.directory(KUrl::IgnoreTrailingSlash));

    KDialog *popn = new KDialog(this );
    popn->setCaption(  i18n("Decrypt File To") );
    popn->setButtons( KDialog::Ok | KDialog::Cancel );
    popn->setDefaultButton( KDialog::Ok );
    popn->setModal( true );

    SrcSelect *page = new SrcSelect();
    popn->setMainWidget(page);
    page->newFilename->setUrl(oldname);
    page->newFilename->setMode(KFile::File);
    page->newFilename->setWindowTitle(i18n("Save File"));

    page->checkClipboard->setText(i18n("Editor"));
    page->resize(page->minimumSize());
    popn->resize(popn->minimumSize());
    if (popn->exec() == QDialog::Accepted)
    {
        if (page->checkFile->isChecked())
            newname = page->newFilename->url().path();
    }
    else
    {
        delete popn;
        return;
    }
    delete popn;

    if (!newname.isEmpty())
    {
        QFile fgpg(newname);
        if (fgpg.exists())
        {
            KIO::RenameDialog over(0, i18n("File Already Exists"), KUrl(), KUrl::fromPath(newname), KIO::M_OVERWRITE);
            if (over.exec() == QDialog::Rejected)
            {
                return;
            }
            newname = over.newDestUrl().path();
        }

        KgpgLibrary *lib = new KgpgLibrary(this);
        lib->slotFileDec(url, KUrl(newname), m_customdecrypt);
        connect(lib, SIGNAL(importOver(QStringList)), this, SIGNAL(refreshImported(QStringList)));
    }
    else
        openEncryptedDocumentFile(url);
}

void KgpgEditor::slotKeyManager()
{
    QDBusInterface kgpg( "org.kde.kgpg", "/KeyInterface", "org.kde.kgpg.Key" );
    QDBusReply<void> reply =kgpg.call( "showKeyManager" );
    if (!reply.isValid())
        kDebug(2100) << "there was some error using dbus." << endl;
}

void KgpgEditor::slotFileQuit()
{
    saveOptions();
#ifdef __GNUC__
#warning "kde4: port it"
#endif
    //KApplication::quit();
}

void KgpgEditor::slotundo()
{
    view->editor->undo();
}

void KgpgEditor::slotredo()
{
    view->editor->redo();
}

void KgpgEditor::slotEditCut()
{
    view->editor->cut();
}

void KgpgEditor::slotEditCopy()
{
    view->editor->copy();
}

void KgpgEditor::slotEditPaste()
{
    view->editor->paste();
}

void KgpgEditor::slotSelectAll()
{
    view->editor->selectAll();
}

void KgpgEditor::slotSetCharset()
{
    if (!m_encodingaction->isChecked())
        view->editor->setPlainText(QString::fromUtf8(view->editor->toPlainText().toAscii()));
    else
    {
        if (checkEncoding(QTextCodec::codecForLocale()))
            return;
        view->editor->setPlainText(view->editor->toPlainText().toUtf8());
    }
}

bool KgpgEditor::checkEncoding(QTextCodec *codec)
{
    return codec->canEncode(view->editor->toPlainText());
}

void KgpgEditor::slotResetEncoding(bool enc)
{
    m_encodingaction->setChecked(enc);
}

void KgpgEditor::slotPreSignFile()
{
    // create a detached signature for a chosen file
    KUrl url = KFileDialog::getOpenUrl(KUrl(), i18n("*|All Files"), this, i18n("Open File to Sign"));
    if (!url.isEmpty())
        slotSignFile(url);
}

void KgpgEditor::slotSignFile(const KUrl &url)
{
    // create a detached signature for a chosen file
    if (!url.isEmpty())
    {
        QString signKeyID;
        KgpgSelectSecretKey *opts = new KgpgSelectSecretKey(this, "select_secret");
        if (opts->exec() == QDialog::Accepted)
            signKeyID = opts->getKeyID();
        else
        {
            delete opts;
            return;
        }

        delete opts;

        QStringList Options;
        if (KGpgSettings::asciiArmor())
            Options << QString::fromLocal8Bit("--armor");
        if (KGpgSettings::pgpCompatibility())
            Options << QString::fromLocal8Bit("--pgp6");

        KgpgInterface *interface = new KgpgInterface();
        //TODO connect(interface, SIGNAL(...), this, SLOT(slotSignFileFin(KgpgInterface *interface)));
        interface->KgpgSignFile(signKeyID, url, Options);
    }
}

void KgpgEditor::slotSignFileFin(KgpgInterface *interface)
{
    delete interface;
}

void KgpgEditor::slotPreVerifyFile()
{
    KUrl url = KFileDialog::getOpenUrl(KUrl(), i18n("*|All Files"), this, i18n("Open File to Verify"));
    slotVerifyFile(url);
}

void KgpgEditor::slotVerifyFile(const KUrl &url)
{
    if (!url.isEmpty())
    {
        QString sigfile = QString();
        if (!url.fileName().endsWith(".sig"))
        {
            sigfile = url.path() + ".sig";
            QFile fsig(sigfile);
            if (!fsig.exists())
            {
                sigfile = url.path() + ".asc";
                QFile fsig(sigfile);
                // if no .asc or .sig signature file included, assume the file is internally signed
                if (!fsig.exists())
                    sigfile.clear();
            }
        }

        // pipe gpg command
        KgpgInterface *interface = new KgpgInterface();
        interface->KgpgVerifyFile(url, KUrl(sigfile));
        connect(interface, SIGNAL(verifyquerykey(QString)), this, SLOT(importSignatureKey(QString)));
    }
}

void KgpgEditor::slotCheckMd5()
{
    // display md5 sum for a chosen file
    KUrl url = KFileDialog::getOpenUrl(KUrl(), i18n("*|All Files"), this, i18n("Open File to Verify"));
    if (!url.isEmpty())
    {
        Md5Widget *mdwidget = new Md5Widget(this, url);
        mdwidget->exec();
        delete mdwidget;
    }
}

void KgpgEditor::importSignatureKey(const QString &id)
{
    KeyServer *kser = new KeyServer(0, false);
    kser->slotSetText(id);
    kser->slotImport();
}

void KgpgEditor::slotOptions()
{
    QDBusInterface kgpg( "org.kde.kgpg", "/KeyInterface", "org.kde.kgpg.Key" );
    QDBusReply<void> reply =kgpg.call( "showOptions" );
    if (!reply.isValid())
        kDebug(2100) << "there was some error using dbus." << endl;
}

void KgpgEditor::modified()
{
    if (!m_textchanged)
    {
        QString capt = m_docname.fileName();
        if (capt.isEmpty())
            capt = i18n("Untitled");
        setCaption(capt, true);
        m_textchanged = true;
    }
}

void KgpgEditor::slotUndoAvailable(const bool &v)
{
    m_editundo->setEnabled(v);
}

void KgpgEditor::slotRedoAvailable(const bool &v)
{
    m_editredo->setEnabled(v);
}

void KgpgEditor::slotCopyAvailable(const bool &v)
{
    m_editcopy->setEnabled(v);
    m_editcut->setEnabled(v);
}

void KgpgEditor::newText()
{
    m_textchanged = false;
    m_docname.clear();
    setCaption(i18n("Untitled"), false);
    slotResetEncoding(false);
}

#include "kgpgeditor.moc"
