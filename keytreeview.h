#ifndef KEYTREEVIEW_H
#define KEYTREEVIEW_H

#include <QTreeView>

#include "core/kgpgkey.h"

class KGpgNode;
class KGpgItemModel;
class KeyListProxyModel;
class KConfigGroup;

class KeyTreeView: public QTreeView
{
private:
	KeyListProxyModel *m_proxy;

public:
	explicit KeyTreeView(QWidget *parent = 0, KeyListProxyModel *model = 0);

	QList<KGpgNode *> selectedNodes(bool *psame = NULL, KgpgCore::KgpgItemType *pt = NULL) const;
	KGpgNode *selectedNode() const;

	void restoreLayout(KConfigGroup &cg);
	void saveLayout(KConfigGroup &cg) const;

public Q_SLOTS:
	void selectNode(KGpgNode *nd);
};

#endif
