/*
 * Copyright (C) 2009,2010 Rolf Eike Beer <kde@opensource.sf-tec.de>
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "caff.h"
#include "caff_p.h"

#include <QDir>

#include <KDebug>
#include <KLocale>
#include <KProcess>
#include <KTempDir>
#include <KTemporaryFile>
#include <KToolInvocation>

#include "kgpgdeluid.h"
#include "kgpgexport.h"
#include "kgpgimport.h"
#include "kgpginterface.h"
#include "KGpgKeyNode.h"
#include "kgpgsettings.h"
#include "KGpgSignableNode.h"
#include "kgpgsignuid.h"
#include "kgpgtextinterface.h"

KGpgCaffPrivate::KGpgCaffPrivate(KGpgCaff *parent, const KGpgSignableNode::List &ids, const QStringList &signers,
		const KGpgCaff::OperationFlags flags, const KGpgSignTransactionHelper::carefulCheck checklevel)
	: QObject(parent),
	q_ptr(parent),
	m_tempdir(NULL),
	m_signers(signers),
	m_textinterface(new KGpgTextInterface(parent)),
	m_flags(flags),
	m_checklevel(checklevel),
	m_allids(ids)
{
	const QString gpgCfg(KGpgSettings::gpgConfigPath());

	const QString secring = KgpgInterface::getGpgSetting("secret-keyring", gpgCfg);

	if (!secring.isEmpty()) {
		m_secringfile = secring;
	} else {
		QDir dir(gpgCfg + "/..");	// simplest way of removing the file name
		dir = dir.absolutePath();
		m_secringfile = dir.toNativeSeparators(dir.filePath("secring.gpg"));
	}

	connect(m_textinterface, SIGNAL(txtEncryptionFinished(QString)), SLOT(slotTextEncrypted(QString)));
}

KGpgCaffPrivate::~KGpgCaffPrivate()
{
	delete m_tempdir;
}

void
KGpgCaffPrivate::reexportKey(const KGpgSignableNode *key)
{
	Q_ASSERT(m_tempdir == NULL);
	m_tempdir = new KTempDir();

	// export all keys necessary for signing
	QStringList exportkeys(m_signers);
	exportkeys << key->getKeyNode()->getId();

	KGpgImport *imp = new KGpgImport(this);

	QStringList expOptions("--export-options");
	expOptions << "export-clean,export-attribute";
	KGpgExport *exp = new KGpgExport(this, exportkeys, expOptions);
	exp->setOutputTransaction(imp);

	imp->setGnuPGHome(m_tempdir->name());

	connect(imp, SIGNAL(done(int)), SLOT(slotReimportDone(int)));
	imp->start();
}

void
KGpgCaffPrivate::slotReimportDone(int result)
{
	KGpgImport *imp = qobject_cast<KGpgImport *>(sender());

	if (result != KGpgTransaction::TS_OK) {
		abortOperation(result);
	} else {
		bool ret = (imp->getImportedIds(0x1).count() == 1 + m_signers.count());

		if (!ret) {
			abortOperation(-1);
		} else {
			KGpgSignUid *signuid = new KGpgSignUid(this, m_signers.first(), m_allids.first(), false, m_checklevel);
			signuid->setGnuPGHome(m_tempdir->name());
			signuid->setSecringFile(m_secringfile);
			connect(signuid, SIGNAL(done(int)), SLOT(slotSigningFinished(int)));

			signuid->start();
		}
	}

	sender()->deleteLater();
}

void
KGpgCaffPrivate::abortOperation(int result)
{
	Q_Q(KGpgCaff);

	kDebug(2100) << "transaction failed, result" << result;
	delete m_tempdir;
	m_tempdir = NULL;

	emit q->aborted();
}

void
KGpgCaffPrivate::checkNextLoop()
{
	Q_Q(KGpgCaff);

	delete m_tempdir;
	m_tempdir = NULL;

	if (m_allids.isEmpty())
		emit q->done();
	else
		reexportKey(m_allids.first());
}

void
KGpgCaffPrivate::slotSigningFinished(int result)
{
	sender()->deleteLater();

	if (result != KGpgTransaction::TS_OK) {
		if ((result == KGpgSignTransactionHelper::TS_ALREADY_SIGNED) && (m_flags & KGpgCaff::IgnoreAlreadySigned)) {
			m_allids.removeFirst();
			checkNextLoop();
		} else {
			abortOperation(result);
		}
		return;
	}

	const KGpgSignableNode *uid = m_allids.first();

	// if there is no email address we can't send this out anyway, so don't bother.
	// could be improved: if this is the only selected uid from this key go and select
	// a proper mail address to send this to
	if (uid->getEmail().isEmpty()) {
		m_allids.removeFirst();
		checkNextLoop();
	}

	const KGpgKeyNode *key = uid->getKeyNode();

	int uidnum;

	if (uid == key) {
		uidnum = -1;
	} else {
		uidnum = -uid->getId().toInt();
	}

	KGpgDelUid::RemoveMode removeMode;
	switch (KGpgSettings::mailUats()) {
	case 0:
		removeMode = KGpgDelUid::RemoveWithEmail;
		break;
	case 1:
		if (uid == key) {
			removeMode = KGpgDelUid::RemoveWithEmail;
		} else {
			// check if this is the first uid with email address
			const KGpgSignableNode *otherUid;
			int index = 1;
			removeMode = KGpgDelUid::RemoveAllOther;

			while ( (otherUid = key->getUid(index++)) != NULL) {
				if (otherUid == uid) {
					removeMode = KGpgDelUid::RemoveWithEmail;
					break;
				}
				if (!otherUid->getEmail().isEmpty())
					break;
			}
		}
		break;
	case 2:
		removeMode = KGpgDelUid::RemoveAllOther;
		break;
	default:
		Q_ASSERT(0);
		return;
	}

	KGpgDelUid *deluid = new KGpgDelUid(this, key, uidnum, removeMode);

	deluid->setGnuPGHome(m_tempdir->name());

	connect(deluid, SIGNAL(done(int)), SLOT(slotDelUidFinished(int)));

	deluid->start();
}

void
KGpgCaffPrivate::slotDelUidFinished(int result)
{
	sender()->deleteLater();

	const KGpgSignableNode *uid = m_allids.first();
	const KGpgKeyNode *key = uid->getKeyNode();

	if ((result != KGpgTransaction::TS_OK)) {
		// it's no error if we tried to delete all other ids but there is no other id
		if ((uid != key) || (result != KGpgDelUid::TS_NO_SUCH_UID)) {
			abortOperation(result);
			return;
		}
	}

	QStringList expOptions("--export-options");
	expOptions << "export-attribute";

	KGpgExport *exp = new KGpgExport(this, QStringList(key->getId()), expOptions);

	exp->setGnuPGHome(m_tempdir->name());

	connect(exp, SIGNAL(done(int)), SLOT(slotExportFinished(int)));

	exp->start();
}

void
KGpgCaffPrivate::slotExportFinished(int result)
{
	sender()->deleteLater();

	if ((result != KGpgTransaction::TS_OK)) {
		abortOperation(result);
		return;
	}

	const KGpgSignableNode *uid = m_allids.first();
	const KGpgKeyNode *key = uid->getKeyNode();

	KGpgExport *exp = qobject_cast<KGpgExport *>(sender());
	Q_ASSERT(exp != NULL);

	QString body = KGpgSettings::emailTemplate();
	body.replace('%' + i18nc("Email template placeholder for key id", "KEYID") + '%', key->getId());
	body.replace('%' + i18nc("Email template placeholder for key id", "UIDNAME") + '%', uid->getNameComment());

	body += '\n' + exp->getOutputData();

	m_textinterface->encryptText(body, QStringList(key->getId()), QStringList("--armor"));
}

void
KGpgCaffPrivate::slotTextEncrypted(const QString &text)
{
	Q_ASSERT(sender() == m_textinterface);

	const KGpgSignableNode *uid = m_allids.takeFirst();

	const QString email = uid->getEmail();
	const QString keyid = uid->getKeyNode()->getId();

	KToolInvocation::invokeMailer(email, QString(), QString(), "your key " + keyid, text);

	checkNextLoop();
}

KGpgCaff::KGpgCaff(QObject *parent, const KGpgSignableNode::List &ids, const QStringList &signids,
		const int checklevel, const OperationFlags flags)
	: QObject(parent),
	d_ptr(new KGpgCaffPrivate(this, ids, signids, flags, static_cast<KGpgSignTransactionHelper::carefulCheck>(checklevel)))
{
}

void
KGpgCaff::run()
{
	Q_D(KGpgCaff);

	d->reexportKey(d->m_allids.first());
}

#include "caff.moc"
#include "caff_p.moc"
