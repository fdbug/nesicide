
#include "model/cprojectmodel.h"
#include "model/projectsearcher.h"

#include "model/cattributemodel.h"
#include "model/cbinaryfilemodel.h"
#include "model/cfiltermodel.h"
#include "model/cgraphicsbankmodel.h"
#include "model/csourcefilemodel.h"
#include "model/ctilestampmodel.h"

#include "cnesicideproject.h"
#include "iprojecttreeviewitem.h"

//--------------------------------------------------------------------------------------
// CProjectModel implementation
//--------------------------------------------------------------------------------------
CProjectModel::CProjectModel()
{
   m_pProject = NULL;

   m_pAttributeModel    = new CAttributeModel();
   m_pBinaryFileModel   = new CBinaryFileModel();
   m_pFilterModel       = new CFilterModel(this);
   m_pGraphicsBankModel = new CGraphicsBankModel();
   m_pSourceFileModel   = new CSourceFileModel();
   m_pTileStampModel    = new CTileStampModel();

   // Connect to source events
   //QObject::connect(m_pSourceFileModel, SIGNAL(sourceFileAdded(QUuid)), this, SIGNAL(itemAdded(QUuid)));
   //QObject::connect(m_pSourceFileModel, SIGNAL(sourceFileRemoved(QUuid)), this, SIGNAL(itemRemoved(QUuid)));
}

CProjectModel::~CProjectModel()
{
   delete m_pAttributeModel;
   delete m_pBinaryFileModel;
   delete m_pFilterModel;
   delete m_pGraphicsBankModel;
   delete m_pSourceFileModel;
   delete m_pTileStampModel;
}

void CProjectModel::setProject(CNesicideProject *project)
{
   m_pProject = project;

   // Propagate data changes through all child models.
   m_pSourceFileModel->setProject(project);
   m_pAttributeModel->setProject(project);
   m_pBinaryFileModel->setProject(project);
   m_pFilterModel->setProject(project);
   m_pGraphicsBankModel->setProject(project);
   m_pSourceFileModel->setProject(project);
   m_pTileStampModel->setProject(project);

   emit reset();
}

QList<QUuid> CProjectModel::getUuids() const
{
   QList<QUuid> uuids;
   if (m_pProject == NULL)
      return uuids;

   // Extract the uuids of all project items, but
   // exclude "Filters" like CSourceFiles, CGraphicsBanks, etc.
   uuids.append(ProjectSearcher::findUuidsOfType<CAttributeTable>(m_pProject));
   uuids.append(ProjectSearcher::findUuidsOfType<CBinaryFile>(m_pProject));
   uuids.append(ProjectSearcher::findUuidsOfType<CGraphicsBank>(m_pProject));
   uuids.append(ProjectSearcher::findUuidsOfType<CSourceItem>(m_pProject));
   uuids.append(ProjectSearcher::findUuidsOfType<CTileStamp>(m_pProject));

   // Add filters; they're not present in the Project right now.
   uuids.append(m_pFilterModel->getUuids());

   return uuids;
}

void CProjectModel::visitDataItem(const QUuid &uuid, IUuidVisitor &visitor)
{
   if (m_pProject == NULL)
      return;

   // If a filter, not in the project tree; search for it first.
   if (this->getFilterModel()->isFilter(uuid))
   {
      CFilterUuid id(uuid);
      visitor.visit(id);
      return;
   }

   // Find project item with target uuid
   IProjectTreeViewItemIterator iter(m_pProject);
   while ( iter.current() != NULL )
   {
      if (iter.current()->uuid() == uuid)
      {
         // Convert item to ProjectBase.
         CProjectBase* item = dynamic_cast<CProjectBase*>(iter.current());
         if (item == NULL)
            return;

         // Construct Uuid of correct type depending on the class name.
         const char* className = item->metaObject()->className();
         if (strcmp(className,"CBinaryFile") == 0)
         {
            CBinaryFileUuid id(uuid);
            visitor.visit(id);
         }
         else if(strcmp(className,"CSourceItem") == 0)
         {
            CSourceFileUuid id(uuid);
            visitor.visit(id);
         }
         else if (strcmp(className,"CAttributeTable") == 0)
         {
            CAttributeUuid id(uuid);
            visitor.visit(id);
         }
         else if (strcmp(className,"CGraphicsBank") == 0)
         {
            CGraphicsBankUuid id(uuid);
            visitor.visit(id);
         }
         else if (strcmp(className,"CTileStamp") == 0)
         {
            CTileStampUuid id(uuid);
            visitor.visit(id);
         }
         return;
      }
      iter.next();
   }
}
