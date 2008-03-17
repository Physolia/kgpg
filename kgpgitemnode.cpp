#include "kgpgitemnode.h"

#include <KLocale>
#include "kgpginterface.h"
#include "kgpgsettings.h"
#include "convert.h"
#include "kgpgitemmodel.h"

KGpgNode::KGpgNode(KGpgExpandableNode *parent)
	: QObject(), m_parent(parent)
{
	if (parent == NULL)
		m_model = NULL;
	else
		m_model = parent->m_model;
}

KGpgNode::~KGpgNode()
{
	Q_ASSERT(m_model);
	m_model->invalidateIndexes(this);

	if (m_parent != NULL)
		m_parent->deleteChild(this);
}

QString
KGpgNode::getNameComment() const
{
	if (getComment().isEmpty())
		return getName();
	else
		return i18nc("Name of uid (comment)", "%1 (%2)", getName(), getComment());
}

KGpgExpandableNode::KGpgExpandableNode(KGpgExpandableNode *parent)
	: KGpgNode(parent)
{
	if (parent != NULL)
		parent->children.append(this);
}

KGpgExpandableNode::~KGpgExpandableNode()
{
	for (int i = children.count() - 1; i >= 0; i--)
		delete children[i];
}

KGpgNode *
KGpgExpandableNode::getChild(const int &index) const
{
	if ((index < 0) || (index > children.count()))
		return NULL;
	return children.at(index);
}

int
KGpgExpandableNode::getChildCount()
{
	if (children.count() == 0)
		readChildren();

	return children.count();
}

KGpgRootNode::KGpgRootNode(KGpgItemModel *model)
	: KGpgExpandableNode(NULL), m_groups(0)
{
	m_model = model;
	addGroups();
}

void
KGpgRootNode::addGroups()
{
	QStringList groups = KgpgInterface::getGpgGroupNames(KGpgSettings::gpgConfigPath());

	for (QStringList::Iterator it = groups.begin(); it != groups.end(); ++it)
		new KGpgGroupNode(this, QString(*it));
}

void
KGpgRootNode::addKeys(const QStringList &ids)
{
	KgpgInterface *interface = new KgpgInterface();

	KgpgKeyList publiclist = interface->readPublicKeys(true, ids);

	KgpgKeyList secretlist = interface->readSecretKeys();
	QStringList issec = secretlist;

	delete interface;

	for (int i = 0; i < publiclist.size(); ++i)
	{
		KgpgKey key = publiclist.at(i);

		int index = issec.indexOf(key.fullId());
		if (index != -1)
		{
			key.setSecret(true);
			issec.removeAt(index);
			secretlist.removeAt(index);
		}

		new KGpgKeyNode(this, key);
	}

	for (int i = 0; i < secretlist.count(); ++i)
	{
		KgpgKey key = secretlist.at(i);

		new KGpgOrphanNode(this, key);
	}
}

KGpgKeyNode *
KGpgRootNode::findKey(const QString &keyId)
{
	int i = findKeyRow(keyId);
	if (i >= 0) {
		KGpgNode *nd = children[i];
		Q_ASSERT(!(nd->getType() & ~ITYPE_PAIR));
		return static_cast<KGpgKeyNode *>(nd);
	}

	return NULL;
}

int
KGpgRootNode::findKeyRow(const QString &keyId)
{
	for (int i = 0; i < children.count(); i++) {
		KGpgNode *node = children[i];
		if ((node->getType() & ITYPE_PAIR) == 0)
			continue;

		KGpgKeyNode *key = static_cast<KGpgKeyNode *>(node);

		if (keyId == key->getId().right(keyId.length()))
			return i;
	}
	return -1;
}

KGpgKeyNode::KGpgKeyNode(KGpgExpandableNode *parent, const KgpgKey &k)
	: KGpgExpandableNode(parent), m_key(new KgpgKey(k))
{
	m_signs = 0;
}

KGpgKeyNode::~KGpgKeyNode()
{
}

KgpgItemType
KGpgKeyNode::getType() const
{
	return getType(m_key);
}

KgpgItemType
KGpgKeyNode::getType(const KgpgKey *k)
{
	if (k->secret())
		return ITYPE_PAIR;

	return ITYPE_PUBLIC;
}

QString
KGpgKeyNode::getSize() const
{
	return QString::number(m_key->size());
}

QString
KGpgKeyNode::getName() const
{
	return m_key->name();
}

QString
KGpgKeyNode::getEmail() const
{
	return m_key->email();
}

QDate
KGpgKeyNode::getExpiration() const
{
	return m_key->expirationDate();
}

QDate
KGpgKeyNode::getCreation() const
{
	return m_key->creationDate();
}

QString
KGpgKeyNode::getId() const
{
	return m_key->fingerprint();
}

void
KGpgKeyNode::readChildren()
{
	KgpgInterface *interface = new KgpgInterface();
	KgpgKeyList keys = interface->readPublicKeys(true, m_key->fullId(), true);
	KgpgKey key = keys.at(0);

	/********* insertion of sub keys ********/
	for (int i = 0; i < key.subList()->size(); ++i)
	{
		KgpgKeySub sub = key.subList()->at(i);

		KGpgSubkeyNode *n = new KGpgSubkeyNode(this, sub);
		insertSigns(n, sub.signList());
	}

	/********* insertion of users id ********/
	for (int i = 0; i < key.uidList()->size(); ++i)
	{
		KgpgKeyUid uid = key.uidList()->at(i);

		KGpgUidNode *n = new KGpgUidNode(this, uid);
		insertSigns(n, uid.signList());
	}

	/******** insertion of photos id ********/
	QStringList photolist = key.photoList();
	for (int i = 0; i < photolist.size(); ++i)
	{
		KgpgKeyUat uat = key.uatList()->at(i);

		KGpgUatNode *n = new KGpgUatNode(this, uat, photolist.at(i));
		insertSigns(n, uat.signList());
	}
	/****************************************/

	delete interface;

	/******** insertion of signature ********/
	insertSigns(this, key.signList());

	m_signs = key.signList().size();
}

void KGpgKeyNode::insertSigns(KGpgExpandableNode *node, const KgpgKeySignList &list)
{
	for (int i = 0; i < list.size(); ++i)
	{
		(void) new KGpgSignNode(node, list.at(i));
	}
}

QString KGpgKeyNode::getSignCount() const
{
	if (!wasExpanded())
		return QString();
	return i18np("1 signature", "%1 signatures", m_signs);
}

KGpgUidNode::KGpgUidNode(KGpgKeyNode *parent, const KgpgKeyUid &u)
	: KGpgExpandableNode(parent), m_uid(new KgpgKeyUid(u))
{
}

QString
KGpgUidNode::getName() const
{
	return m_uid->name();
}

QString
KGpgUidNode::getEmail() const
{
	return m_uid->email();
}

QString
KGpgUidNode::getSignCount() const
{
	return i18np("1 signature", "%1 signatures", children.count());
}

QString
KGpgUidNode::getId() const
{
	return QString::number(m_uid->index());
}

KGpgSignNode::KGpgSignNode(KGpgExpandableNode *parent, const KgpgKeySign &s)
	: KGpgNode(parent), m_sign(new KgpgKeySign(s))
{
	Q_ASSERT(parent != NULL);
	parent->children.append(this);
}

QString
KGpgSignNode::getName() const
{
	return m_sign->name();
}

QString
KGpgSignNode::getEmail() const
{
	return m_sign->email();
}

QDate
KGpgSignNode::getExpiration() const
{
	return m_sign->expirationDate();
}

QDate
KGpgSignNode::getCreation() const
{
	return m_sign->creationDate();
}

QString
KGpgSignNode::getId() const
{
	return m_sign->fullId();
}

bool
KGpgSignNode::isUnknown() const
{
	// ugly hack to detect unknown keys
	return (m_sign->name().startsWith('[') && m_sign->name().endsWith(']'));
}

KGpgSubkeyNode::KGpgSubkeyNode(KGpgKeyNode *parent, const KgpgKeySub &k)
	: KGpgExpandableNode(parent), m_skey(k)
{
	Q_ASSERT(parent != NULL);
}

QDate
KGpgSubkeyNode::getExpiration() const
{
	return m_skey.expirationDate();
}

QDate
KGpgSubkeyNode::getCreation() const
{
	return m_skey.creationDate();
}

QString
KGpgSubkeyNode::getId() const
{
	return m_skey.id();
}

QString
KGpgSubkeyNode::getName() const
{
	return i18n("%1 subkey", Convert::toString(m_skey.algorithm()));
}

QString
KGpgSubkeyNode::getSize() const
{
	return QString::number(m_skey.size());
}

QString
KGpgSubkeyNode::getSignCount() const
{
	return i18np("1 signature", "%1 signatures", children.count());
}

KGpgUatNode::KGpgUatNode(KGpgKeyNode *parent, const KgpgKeyUat &k, const QString &index)
	: KGpgExpandableNode(parent), m_uat(k), m_idx(index)
{
	KgpgInterface iface;

	m_pic = iface.loadPhoto(parent->getKeyId(), index, true);
}

QString
KGpgUatNode::getName() const
{
	return i18n("Photo id");
}

QString
KGpgUatNode::getSize() const
{
	return QString::number(m_pic.width()) + "x" + QString::number(m_pic.height());
}

QDate
KGpgUatNode::getCreation() const
{
	return m_uat.creationDate();
}

QString
KGpgUatNode::getSignCount() const
{
	return i18np("1 signature", "%1 signatures", children.count());
}

KGpgGroupNode::KGpgGroupNode(KGpgRootNode *parent, const QString &name)
	: KGpgExpandableNode(parent), m_name(name)
{
	readChildren();
	parent->m_groups++;
}

KGpgGroupNode::KGpgGroupNode(KGpgRootNode *parent, const QString &name, const KGpgKeyNodeList &members)
	: KGpgExpandableNode(parent), m_name(name)
{
	Q_ASSERT(members.count() > 0);
	for (int i = 0; i < members.count(); i++)
		new KGpgGroupMemberNode(this, members.at(i));
	parent->m_groups++;
}

KGpgGroupNode::~KGpgGroupNode()
{
	static_cast<KGpgRootNode *>(m_parent)->m_groups--;
}

QString
KGpgGroupNode::getName() const
{
	return m_name;
}

QString
KGpgGroupNode::getSize() const
{
	return i18np("1 key", "%1 keys", children.count());
}

void
KGpgGroupNode::readChildren()
{
	QStringList keys = KgpgInterface::getGpgGroupSetting(m_name, KGpgSettings::gpgConfigPath());

	children.clear();

	for (QStringList::Iterator it = keys.begin(); it != keys.end(); ++it)
		new KGpgGroupMemberNode(this, QString(*it));
}

KGpgGroupMemberNode::KGpgGroupMemberNode(KGpgGroupNode *parent, const QString &k)
	: KGpgNode(parent)
{
	KgpgInterface *iface = new KgpgInterface();
	KgpgKeyList l = iface->readPublicKeys(true, k);
	delete iface;

	m_key = new KgpgKey(l.at(0));

	parent->children.append(this);
}

KGpgGroupMemberNode::KGpgGroupMemberNode(KGpgGroupNode *parent, const KGpgKeyNode *k)
	: KGpgNode(parent)
{
	m_key = new KgpgKey(*k->m_key);

	parent->children.append(this);
}

QString
KGpgGroupMemberNode::getName() const
{
	return m_key->name();
}

QString
KGpgGroupMemberNode::getEmail() const
{
	return m_key->email();
}

QString
KGpgGroupMemberNode::getSize() const
{
	return QString::number(m_key->size());
}

QDate
KGpgGroupMemberNode::getExpiration() const
{
	return m_key->expirationDate();
}

QDate
KGpgGroupMemberNode::getCreation() const
{
	return m_key->creationDate();
}

QString
KGpgGroupMemberNode::getId() const
{
	return m_key->fingerprint();
}

KGpgOrphanNode::KGpgOrphanNode(KGpgExpandableNode *parent, const KgpgKey &k)
	: KGpgNode(parent), m_key(new KgpgKey(k))
{
}

KGpgOrphanNode::~KGpgOrphanNode()
{
	delete m_key;
}

KgpgItemType
KGpgOrphanNode::getType() const
{
	return ITYPE_SECRET;
}

QString
KGpgOrphanNode::getName() const
{
	return m_key->name();
}

QString
KGpgOrphanNode::getEmail() const
{
	return m_key->email();
}

QString
KGpgOrphanNode::getSize() const
{
	return QString::number(m_key->size());
}

QDate
KGpgOrphanNode::getExpiration() const
{
	return m_key->expirationDate();
}

QDate
KGpgOrphanNode::getCreation() const
{
	return m_key->creationDate();
}

QString
KGpgOrphanNode::getId() const
{
	return m_key->fullId();
}

