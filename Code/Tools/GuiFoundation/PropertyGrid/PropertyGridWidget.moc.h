#pragma once

#include <GuiFoundation/Basics.h>
#include <Foundation/Containers/HybridArray.h>
#include <ToolsFoundation/Object/DocumentObjectBase.h>
#include <ToolsFoundation/Selection/SelectionManager.h>
#include <GuiFoundation/PropertyGrid/PropertyBaseWidget.moc.h>
#include <GuiFoundation/PropertyGrid/Implementation/TypeWidget.moc.h>
#include <ToolsFoundation/Factory/RttiMappedObjectFactory.h>
#include <ToolsFoundation/Reflection/PhantomRttiManager.h>
#include <QWidget>

class QSpacerItem;
class QVBoxLayout;
class QScrollArea;

class ezCollapsibleGroupBox;
class ezDocument;
struct ezDocumentObjectPropertyEvent;

class EZ_GUIFOUNDATION_DLL ezPropertyGridWidget : public QWidget
{
  Q_OBJECT
public:
  ezPropertyGridWidget(ezDocument* pDocument, QWidget* pParent);
  ~ezPropertyGridWidget();

  void ClearSelection();
  void SetSelection(const ezDeque<const ezDocumentObject*>& selection);
  const ezDocument* GetDocument() const;

  static ezRttiMappedObjectFactory<ezQtPropertyWidget>& GetFactory();
  static ezQtPropertyWidget* CreateMemberPropertyWidget(const ezAbstractProperty* pProp);
  static ezQtPropertyWidget* CreatePropertyWidget(const ezAbstractProperty* pProp);

  void SetCollapseState(ezCollapsibleGroupBox* pBox);

public slots:
  void OnCollapseStateChanged(bool bCollapsed);

private:
  static ezRttiMappedObjectFactory<ezQtPropertyWidget> s_Factory;
  EZ_MAKE_SUBSYSTEM_STARTUP_FRIEND(GuiFoundation, PropertyGrid);

private:
  void SelectionEventHandler(const ezSelectionManager::Event& e);
  void FactoryEventHandler(const ezRttiMappedObjectFactory<ezQtPropertyWidget>::Event& e);
  void TypeEventHandler(const ezPhantomRttiManager::Event& e);
  ezUInt32 GetGroupBoxHash(ezCollapsibleGroupBox* pBox) const;

private:
  ezDocument* m_pDocument;
  ezDeque<const ezDocumentObject*> m_Selection;
  ezMap<ezUInt32, bool> m_CollapseState;

  QVBoxLayout* m_pLayout;
  QScrollArea* m_pScroll;
  QWidget* m_pContent;
  QVBoxLayout* m_pContentLayout;

  ezTypeWidget* m_pTypeWidget;
  QSpacerItem* m_pSpacer;
};


