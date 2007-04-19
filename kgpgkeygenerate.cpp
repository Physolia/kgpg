/***************************************************************************
                          keygen.cpp  -  description
                             -------------------
    begin                : Mon Jul 8 2002
    copyright            : (C) 2002 by Jean-Baptiste Mardelle
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

#include <QVBoxLayout>
#include <QWhatsThis>
#include <QGroupBox>
#include <QWidget>
#include <QLabel>

#include <kmessagebox.h>
#include <kcombobox.h>
#include <klineedit.h>
#include <klocale.h>
#include <kdebug.h>
#include <khbox.h>

#include "kgpgkeygenerate.h"

KgpgKeyGenerate::KgpgKeyGenerate(QWidget *parent)
               : KDialog(parent)
{
    setCaption(i18n("Key Generation"));
    setButtons(User1 | Ok | Cancel);
    setDefaultButton(Cancel);

    setButtonText(User1, i18n("&Expert Mode"));
    setButtonToolTip(User1, i18n("Go to the expert mode"));
    setButtonWhatsThis(User1, "If you go to the expert mode, you will use the command line to create your key.");

    m_expert = false;

    QGroupBox *vgroup = new QGroupBox(i18n("Generate Key Pair"), this);

    QLabel *nameLabel = new QLabel(i18n("&Name:"), vgroup);
    m_kname = new KLineEdit("", vgroup);
    nameLabel->setBuddy(m_kname);
    m_kname->setFocus();
    connect(m_kname, SIGNAL(textChanged(const QString&)), this, SLOT(slotEnableOk()));

    QLabel *emailLabel = new QLabel(i18n("E&mail:"), vgroup);
    m_mail = new KLineEdit("", vgroup);
    emailLabel->setBuddy(m_mail);

    QLabel *commentLabel = new QLabel(i18n("Commen&t (optional):"), vgroup);
    m_comment = new KLineEdit("", vgroup);
    commentLabel->setBuddy(m_comment);

    QLabel *expLabel = new QLabel(i18n("Expiration:"), vgroup);
    KHBox *hgroup = new KHBox(vgroup);
    hgroup->setFrameShape(QFrame::StyledPanel);
    hgroup->setMargin(marginHint());
    hgroup->setSpacing(spacingHint());
    m_days = new KLineEdit("0", hgroup);
    m_days->setMaxLength(4);
    m_days->setDisabled(true);

    m_keyexp = new KComboBox(hgroup);
    m_keyexp->addItem(i18n("Never"), 0);
    m_keyexp->addItem(i18n("Days"), 1);
    m_keyexp->addItem(i18n("Weeks"), 2);
    m_keyexp->addItem(i18n("Months"), 3);
    m_keyexp->addItem(i18n("Years"), 4);
    m_keyexp->setMinimumSize(m_keyexp->sizeHint());
    connect(m_keyexp, SIGNAL(activated(int)), this, SLOT(slotEnableDays(int)));

    QLabel *sizeLabel = new QLabel(i18n("&Key size:"), vgroup);
    m_keysize = new KComboBox(vgroup);
    m_keysize->addItem("768");
    m_keysize->addItem("1024");
    m_keysize->addItem("2048");
    m_keysize->addItem("4096");
    m_keysize->setCurrentIndex(1); // 1024
    m_keysize->setMinimumSize(m_keysize->sizeHint());
    sizeLabel->setBuddy(m_keysize);

    QLabel *algoLabel = new QLabel(i18n("&Algorithm:"), vgroup);
    m_keykind = new KComboBox(vgroup);
    m_keykind->addItem("DSA & ElGamal");
    m_keykind->addItem("RSA");
    m_keykind->setMinimumSize(m_keykind->sizeHint());
    algoLabel->setBuddy(m_keykind);

    QVBoxLayout *vlayout = new QVBoxLayout(vgroup);
    vlayout->addWidget(nameLabel);
    vlayout->addWidget(m_kname);
    vlayout->addWidget(emailLabel);
    vlayout->addWidget(m_mail);
    vlayout->addWidget(commentLabel);
    vlayout->addWidget(m_comment);
    vlayout->addWidget(expLabel);
    vlayout->addWidget(hgroup);
    vlayout->addWidget(sizeLabel);
    vlayout->addWidget(m_keysize);
    vlayout->addWidget(algoLabel);
    vlayout->addWidget(m_keykind);
    vlayout->addStretch();
    vgroup->setLayout(vlayout);

    setMainWidget(vgroup);

    slotEnableOk();
    updateGeometry();
    show();
    connect(this,SIGNAL(okClicked()),this,SLOT(slotOk()));
    connect(this,SIGNAL(user1Clicked()),this,SLOT(slotUser1()));

}

void KgpgKeyGenerate::slotButtonClicked(int button)
{
    if (button == Ok)
        slotOk();
    else
    if (button == User1)
        slotUser1();
    else
    if (button == Cancel)
        reject();
}

void KgpgKeyGenerate::slotOk()
{
    if (QString(m_kname->text()).simplified().isEmpty())
    {
        KMessageBox::sorry(this, i18n("You must give a name."));
        return;
    }

    if (QString(m_kname->text()).simplified().length() < 5)
    {
        KMessageBox::sorry(this, i18n("The name must have at least 5 characters"));
        return;
    }

    QString vmail = m_mail->text();
    if (vmail.isEmpty())
    {
        int result;
        result = KMessageBox::warningContinueCancel(this, i18n("You are about to create a key with no email address"));
        if (result != KMessageBox::Continue)
            return;
    }
    else
    if (vmail.contains(' ') || !vmail.contains('.') || !vmail.contains('@'))
    {
        KMessageBox::sorry(this, i18n("Email address not valid"));
        return;
    }

    accept();
}

void KgpgKeyGenerate::slotUser1()
{
    m_expert = true;
    accept();
}

void KgpgKeyGenerate::slotEnableDays(const int &state)
{
    if (state == 0)
        m_days->setDisabled(true);
    else
        m_days->setDisabled(false);
}

void KgpgKeyGenerate::slotEnableOk()
{
    enableButtonOk(!QString(m_kname->text()).simplified().isEmpty());
}

bool KgpgKeyGenerate::isExpertMode() const
{
    return m_expert;
}

KgpgCore::KgpgKeyAlgo KgpgKeyGenerate::algo() const
{
    if (m_keykind->currentText() == "RSA")
        return KgpgCore::ALGO_RSA;
    else
        return KgpgCore::ALGO_DSA_ELGAMAL;
}

uint KgpgKeyGenerate::size() const
{
    return m_keysize->currentText().toUInt();
}

uint KgpgKeyGenerate::expiration() const
{
    return m_keyexp->currentIndex();
}

uint KgpgKeyGenerate::days() const
{
    if (!m_days->text().isEmpty())
        return m_days->text().toUInt();
    else
        return 0;
}

QString KgpgKeyGenerate::name() const
{
    if (!m_kname->text().isEmpty())
        return(m_kname->text());
    else
        return QString();
}

QString KgpgKeyGenerate::email() const
{
    if (!m_mail->text().isEmpty())
        return(m_mail->text());
    else
        return QString();
}

QString KgpgKeyGenerate::comment() const
{
    if (!m_comment->text().isEmpty())
        return(m_comment->text());
    else
        return QString();
}

#include "kgpgkeygenerate.moc"
