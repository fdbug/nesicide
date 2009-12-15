#ifndef CPROJECT_H
#define CPROJECT_H

#include "iprojecttreeviewitem.h"
#include "ixmlserializable.h"
#include "iprojecttreeviewitem.h"

class CProject : public IXMLSerializable, public IProjectTreeViewItem
{
public:
    CProject();
    ~CProject();

    // IXMLSerializable Interface Implementation
    virtual bool serialize(QDomDocument &doc, QDomNode &node);
    virtual bool deserialize(QDomDocument &doc, QDomNode &node);

    // IProjectTreeViewItem Interface Implmentation
    QString caption() const;
    virtual void contextMenuEvent(QContextMenuEvent*, QTreeView*) {}
    virtual void openItemEvent(QTabWidget*) {}
};

#endif // CPROJECT_H