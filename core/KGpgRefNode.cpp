/* Copyright 2008,2009,2010,2013 Rolf Eike Beer <kde@opensource.sf-tec.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "KGpgRefNode.h"

#include <KLocalizedString>

#include "KGpgExpandableNode.h"
#include "KGpgRootNode.h"

KGpgRefNode::KGpgRefNode(KGpgExpandableNode *parent, const QString &keyid)
	: KGpgNode(parent),
	m_id(keyid)
{
	Q_ASSERT(!keyid.isEmpty());

	KGpgRootNode *root = getRootNode();
	KGpgExpandableNode *pnd = parent;

	do {
		m_selfsig = (pnd->getId().right(keyid.length()) == keyid);
		if (m_selfsig)
			m_keynode = pnd->toKeyNode();
		else
			pnd = pnd->getParentKeyNode();
	} while (!m_selfsig && (pnd != root));

	// Self signatures do net need to get notified by their key: if the key is changed
	// the key node is deleted, then those refnode would be deleted anyway. This avoids
	// crashes when they would try to find the root node by iterating over their parent
	// when the parents destructor is already called (see bug 208659).
	if (!m_selfsig) {
		m_keynode = root->findKey(keyid);

		if (m_keynode != Q_NULLPTR) {
			m_keynode->addRef(this);
		} else {
			connect(root, &KGpgRootNode::newKeyNode, this, &KGpgRefNode::keyUpdated);
		}
	}

	parent->children.append(this);
}

KGpgRefNode::KGpgRefNode(KGpgExpandableNode *parent, KGpgKeyNode *key)
	: KGpgNode(parent),
	m_id(key->getId()),
	m_keynode(key)
{
	Q_ASSERT(key != Q_NULLPTR);
	Q_ASSERT(parent != Q_NULLPTR);
	m_keynode->addRef(this);

	parent->children.append(this);
}

KGpgRefNode::~KGpgRefNode()
{
	if (m_keynode && !m_selfsig)
		m_keynode->delRef(this);
}

KGpgRootNode *
KGpgRefNode::getRootNode() const
{
	KGpgExpandableNode *root;
	KGpgExpandableNode *pt = m_parent;

	do {
		root = pt;
		pt = pt->getParentKeyNode();
	} while (pt != Q_NULLPTR);

	return root->toRootNode();
}

void
KGpgRefNode::keyUpdated(KGpgKeyNode *nkey)
{
	Q_ASSERT(m_keynode == Q_NULLPTR);
	Q_ASSERT(nkey != Q_NULLPTR);

	if (nkey->compareId(m_id)) {
		disconnect(sender(), Q_NULLPTR, this, SLOT(keyUpdated(KGpgKeyNode*)));
		m_keynode = nkey;
		m_keynode->addRef(this);
	}
}

void
KGpgRefNode::unRef(KGpgRootNode *root)
{
	if (root != Q_NULLPTR)
		connect(root, &KGpgRootNode::newKeyNode, this, &KGpgRefNode::keyUpdated);

	m_keynode = Q_NULLPTR;
}

QString
KGpgRefNode::getId() const
{
	if (m_keynode != Q_NULLPTR)
		return m_keynode->getFingerprint();
	else
		return m_id;
}

QString
KGpgRefNode::getName() const
{
	if (m_keynode != Q_NULLPTR)
		return m_keynode->getName();
	return i18n("[No user id found]");
}

QString
KGpgRefNode::getEmail() const
{
	if (m_keynode != Q_NULLPTR)
		return m_keynode->getEmail();
	return QString();
}

bool
KGpgRefNode::isUnknown() const
{
	return (m_keynode == Q_NULLPTR);
}

KGpgKeyNode *
KGpgRefNode::getRefNode() const
{
	return m_keynode;
}
