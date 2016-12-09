#include <GuiFoundation/PCH.h>
#include <GuiFoundation/PropertyGrid/PropertyBaseWidget.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <GuiFoundation/PropertyGrid/Implementation/TypeWidget.moc.h>
#include <GuiFoundation/PropertyGrid/Implementation/AddSubElementButton.moc.h>
#include <GuiFoundation/PropertyGrid/Implementation/ElementGroupButton.moc.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>
#include <GuiFoundation/Widgets/CollapsibleGroupBox.moc.h>
#include <ToolsFoundation/Reflection/IReflectedTypeAccessor.h>
#include <ToolsFoundation/Object/DocumentObjectBase.h>
#include <ToolsFoundation/Command/TreeCommands.h>
#include <ToolsFoundation/Document/Document.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>
#include <CoreUtils/Localization/TranslationLookup.h>
#include <algorithm>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStringBuilder>
#include <QLabel>
#include <QMenu>
#include <QClipboard>
#include <QMimeData>

/// *** BASE ***
ezQtPropertyWidget::ezQtPropertyWidget() : QWidget(nullptr), m_pGrid(nullptr), m_pProp(nullptr)
{
  m_bUndead = false;
  m_bIsDefault = true;
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

ezQtPropertyWidget::~ezQtPropertyWidget()
{
}

void ezQtPropertyWidget::Init(ezQtPropertyGridWidget* pGrid, const ezAbstractProperty* pProp)
{
  m_pGrid = pGrid;
  m_pProp = pProp;

  if (pProp->GetAttributeByType<ezReadOnlyAttribute>() != nullptr || pProp->GetFlags().IsSet(ezPropertyFlags::ReadOnly))
    setEnabled(false);

  OnInit();
}

void ezQtPropertyWidget::SetSelection(const ezHybridArray<Selection, 8>& items)
{
  m_Items = items;
}

const ezRTTI* ezQtPropertyWidget::GetCommonBaseType(const ezHybridArray<ezQtPropertyWidget::Selection, 8>& items)
{
  const ezRTTI* pSubtype = nullptr;

  for (const auto& item : items)
  {
    const auto& accessor = item.m_pObject->GetTypeAccessor();

    if (pSubtype == nullptr)
      pSubtype = accessor.GetType();
    else
    {
      pSubtype = ezReflectionUtils::GetCommonBaseType(pSubtype, accessor.GetType());
    }
  }

  return pSubtype;
}


void ezQtPropertyWidget::PrepareToDie()
{
  EZ_ASSERT_DEBUG(!m_bUndead, "Object has already been marked for cleanup");

  m_bUndead = true;

  DoPrepareToDie();
}


void ezQtPropertyWidget::OnCustomContextMenu(const QPoint& pt)
{
  QMenu m;

  // revert
  {
    QAction* pRevert = m.addAction("Revert to Default");
    pRevert->setEnabled(!m_bIsDefault);
    connect(pRevert, &QAction::triggered, this, [this]()
    {
      ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
      pObjectAccessor->StartTransaction("Revert to Default");
      for (const Selection& sel : m_Items)
      {
        ezVariant defaultValue = m_pGrid->GetDocument()->GetDefaultValue(sel.m_pObject, m_pProp->GetPropertyName());
        pObjectAccessor->SetValue(sel.m_pObject, m_pProp, defaultValue, sel.m_Index);
      }
      pObjectAccessor->FinishTransaction();
    });
  }

  // copy internal name
  {
    auto lambda = [this]()
    {
      QClipboard* clipboard = QApplication::clipboard();
      QMimeData* mimeData = new QMimeData();
      mimeData->setText(m_pProp->GetPropertyName());
      clipboard->setMimeData(mimeData);
    };

    QAction* pAction = m.addAction("Copy Internal Property Name:");
    connect(pAction, &QAction::triggered, this, lambda);

    QAction* pAction2 = m.addAction(m_pProp->GetPropertyName());
    connect(pAction2, &QAction::triggered, this, lambda);
  }

  ExtendContextMenu(m);
  m.exec(pt); // pt is already in global space, because we fixed that
}

void ezQtPropertyWidget::Broadcast(Event::Type type)
{
  Event ed;
  ed.m_Type = type;
  ed.m_pProperty = m_pProp;

  m_Events.Broadcast(ed);
}


/// *** ezQtUnsupportedPropertyWidget ***

ezQtUnsupportedPropertyWidget::ezQtUnsupportedPropertyWidget(const char* szMessage)
  : ezQtPropertyWidget()
{
  m_pLayout = new QHBoxLayout(this);
  m_pLayout->setMargin(0);
  setLayout(m_pLayout);

  m_pWidget = new QLabel(this);
  m_pWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_pLayout->addWidget(m_pWidget);
  m_sMessage = szMessage;
}

void ezQtUnsupportedPropertyWidget::OnInit()
{
  QtScopedBlockSignals bs(m_pWidget);

  QString sMessage = QStringLiteral("Unsupported Type: ") % QString::fromUtf8(m_pProp->GetSpecificType()->GetTypeName());
  if (!m_sMessage.IsEmpty())
    sMessage += QStringLiteral(" (") % QString::fromUtf8(m_sMessage) % QStringLiteral(")");
  m_pWidget->setText(sMessage);
  m_pWidget->setToolTip(sMessage);
}


/// *** ezQtStandardPropertyWidget ***

ezQtStandardPropertyWidget::ezQtStandardPropertyWidget()
  : ezQtPropertyWidget()
{

}

void ezQtStandardPropertyWidget::SetSelection(const ezHybridArray<Selection, 8>& items)
{
  ezQtPropertyWidget::SetSelection(items);
  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();

  ezVariant value;
  // check if we have multiple values
  for (const auto& item : items)
  {
    if (!value.IsValid())
    {
      pObjectAccessor->GetValue(item.m_pObject, m_pProp, value, item.m_Index);
    }
    else
    {
      ezVariant valueNext;
      pObjectAccessor->GetValue(item.m_pObject, m_pProp, valueNext, item.m_Index);
      if (value != valueNext)
      {
        value = ezVariant();
        break;
      }
    }
  }

  m_OldValue = value;
  InternalSetValue(value);
}

void ezQtStandardPropertyWidget::BroadcastValueChanged(const ezVariant& NewValue)
{
  if (NewValue == m_OldValue)
    return;

  m_OldValue = NewValue;

  Event ed;
  ed.m_Type = Event::Type::ValueChanged;
  ed.m_pProperty = m_pProp;
  ed.m_Value = NewValue;
  ed.m_pItems = &m_Items;

  m_Events.Broadcast(ed);
}


/// *** ezQtPropertyPointerWidget ***

ezQtPropertyPointerWidget::ezQtPropertyPointerWidget()
  : ezQtPropertyWidget()
{
  m_pLayout = new QHBoxLayout(this);
  m_pLayout->setMargin(0);
  setLayout(m_pLayout);

  m_pGroup = new ezQtCollapsibleGroupBox(this);
  m_pGroupLayout = new QHBoxLayout(nullptr);
  m_pGroupLayout->setSpacing(1);
  m_pGroupLayout->setContentsMargins(5, 0, 0, 0);
  m_pGroup->setInnerLayout(m_pGroupLayout);

  m_pLayout->addWidget(m_pGroup);

  m_pAddButton = new ezQtAddSubElementButton();
  m_pGroup->HeaderLayout->addWidget(m_pAddButton);

  m_pDeleteButton = new ezQtElementGroupButton(m_pGroup->Header, ezQtElementGroupButton::ElementAction::DeleteElement, this);
  m_pGroup->HeaderLayout->addWidget(m_pDeleteButton);
  connect(m_pDeleteButton, &QToolButton::clicked, this, &ezQtPropertyPointerWidget::OnDeleteButtonClicked);

  m_pTypeWidget = nullptr;
}

ezQtPropertyPointerWidget::~ezQtPropertyPointerWidget()
{
  m_pGrid->GetDocument()->GetObjectManager()->m_StructureEvents.RemoveEventHandler(ezMakeDelegate(&ezQtPropertyPointerWidget::StructureEventHandler, this));
}

void ezQtPropertyPointerWidget::OnInit()
{
  m_pGroup->setTitle(ezTranslate(m_pProp->GetPropertyName()));
  m_pGrid->SetCollapseState(m_pGroup);
  connect(m_pGroup, &ezQtCollapsibleGroupBox::CollapseStateChanged, m_pGrid, &ezQtPropertyGridWidget::OnCollapseStateChanged);

  // Add Buttons
  auto pAttr = m_pProp->GetAttributeByType<ezContainerAttribute>();
  m_pAddButton->setVisible(!pAttr || pAttr->CanAdd());
  m_pDeleteButton->setVisible(!pAttr || pAttr->CanDelete());

  m_pAddButton->Init(m_pGrid, m_pProp);
  m_pGrid->GetDocument()->GetObjectManager()->m_StructureEvents.AddEventHandler(ezMakeDelegate(&ezQtPropertyPointerWidget::StructureEventHandler, this));
}

void ezQtPropertyPointerWidget::SetSelection(const ezHybridArray<Selection, 8>& items)
{
  QtScopedUpdatesDisabled _(this);

  ezQtPropertyWidget::SetSelection(items);

  if (m_pTypeWidget)
  {
    m_pGroupLayout->removeWidget(m_pTypeWidget);
    delete m_pTypeWidget;
    m_pTypeWidget = nullptr;
  }

  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
  ezHybridArray<ezQtPropertyWidget::Selection, 8> emptyItems;
  ezHybridArray<ezQtPropertyWidget::Selection, 8> subItems;
  for (const auto& item : m_Items)
  {
    ezUuid ObjectGuid = pObjectAccessor->Get<ezUuid>(item.m_pObject, m_pProp, item.m_Index);
    if (!ObjectGuid.IsValid())
    {
      emptyItems.PushBack(item);
    }
    else
    {
      ezQtPropertyWidget::Selection sel;
      sel.m_pObject = pObjectAccessor->GetObject(ObjectGuid);

      subItems.PushBack(sel);
    }
  }

  auto pAttr = m_pProp->GetAttributeByType<ezContainerAttribute>();
  if (!pAttr || pAttr->CanAdd())
    m_pAddButton->setVisible(!emptyItems.IsEmpty());
  if (!pAttr || pAttr->CanDelete())
    m_pDeleteButton->setVisible(!subItems.IsEmpty());

  if (!emptyItems.IsEmpty())
  {
    m_pAddButton->SetSelection(emptyItems);
  }

  if (!subItems.IsEmpty())
  {
    const ezRTTI* pCommonType = ezQtPropertyWidget::GetCommonBaseType(subItems);

    m_pTypeWidget = new ezQtTypeWidget(m_pGroup->Content, m_pGrid, pCommonType);
    m_pTypeWidget->SetSelection(subItems);

    m_pGroupLayout->addWidget(m_pTypeWidget);
  }
}


void ezQtPropertyPointerWidget::DoPrepareToDie()
{
  if (m_pTypeWidget)
  {
    m_pTypeWidget->PrepareToDie();
  }
}

void ezQtPropertyPointerWidget::OnDeleteButtonClicked()
{
  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
  pObjectAccessor->StartTransaction("Delete Object");

  ezStatus res;
  const ezHybridArray<ezQtPropertyWidget::Selection, 8> selection = m_pTypeWidget->GetSelection();
  for (auto& item : selection)
  {
    res = pObjectAccessor->RemoveObject(item.m_pObject);
    if (res.m_Result.Failed())
      break;
  }

  if (res.m_Result.Failed())
    pObjectAccessor->CancelTransaction();
  else
    pObjectAccessor->FinishTransaction();

  ezQtUiServices::GetSingleton()->MessageBoxStatus(res, "Removing sub-element from the property failed.");
}

void ezQtPropertyPointerWidget::StructureEventHandler(const ezDocumentObjectStructureEvent& e)
{
  if (IsUndead())
    return;

  switch (e.m_EventType)
  {
  case ezDocumentObjectStructureEvent::Type::AfterObjectAdded:
  case ezDocumentObjectStructureEvent::Type::AfterObjectMoved:
  case ezDocumentObjectStructureEvent::Type::AfterObjectRemoved:
    {
      if (!e.m_sParentProperty.IsEqual(m_pProp->GetPropertyName()))
        return;

      if (std::none_of(cbegin(m_Items), cend(m_Items),
                       [&](const ezQtPropertyWidget::Selection& sel) { return e.m_pNewParent == sel.m_pObject || e.m_pPreviousParent == sel.m_pObject; }
      ))
        return;

      SetSelection(m_Items);
    }
    break;
  }
}


/// *** ezQtPropertyTypeWidget ***

ezQtPropertyTypeWidget::ezQtPropertyTypeWidget(bool bAddCollapsibleGroup)
  : ezQtPropertyWidget()
{
  m_pLayout = new QHBoxLayout(this);
  m_pLayout->setMargin(0);
  setLayout(m_pLayout);
  m_pGroup = nullptr;
  m_pGroupLayout = nullptr;

  if (bAddCollapsibleGroup)
  {
    m_pGroup = new ezQtCollapsibleGroupBox(this);
    m_pGroupLayout = new QHBoxLayout(nullptr);
    m_pGroupLayout->setSpacing(1);
    m_pGroupLayout->setContentsMargins(5, 0, 0, 0);
    m_pGroup->setInnerLayout(m_pGroupLayout);

    m_pLayout->addWidget(m_pGroup);
  }
  m_pTypeWidget = nullptr;
}

ezQtPropertyTypeWidget::~ezQtPropertyTypeWidget()
{
}

void ezQtPropertyTypeWidget::OnInit()
{
  if (m_pGroup)
  {
    m_pGroup->setTitle(ezTranslate(m_pProp->GetPropertyName()));
    m_pGrid->SetCollapseState(m_pGroup);
    connect(m_pGroup, &ezQtCollapsibleGroupBox::CollapseStateChanged, m_pGrid, &ezQtPropertyGridWidget::OnCollapseStateChanged);
  }
}

void ezQtPropertyTypeWidget::SetSelection(const ezHybridArray<Selection, 8>& items)
{
  QtScopedUpdatesDisabled _(this);

  ezQtPropertyWidget::SetSelection(items);

  QHBoxLayout* pLayout = m_pGroup != nullptr ? m_pGroupLayout : m_pLayout;
  QWidget* pOwner = m_pGroup != nullptr ? m_pGroup->Content : this;
  if (m_pTypeWidget)
  {
    pLayout->removeWidget(m_pTypeWidget);
    delete m_pTypeWidget;
    m_pTypeWidget = nullptr;
  }
  const ezRTTI* pCommonType = nullptr;
  if (m_pProp->GetFlags().IsSet(ezPropertyFlags::EmbeddedClass))
  {
    // If we create a widget for a member struct we already determined the common base type at the parent type widget.
    // As we are not dealing with a pointer in this case the type must match the property exactly.
    pCommonType = m_pProp->GetSpecificType();
  }
  else
  {
    pCommonType = ezQtPropertyWidget::GetCommonBaseType(m_Items);
  }
  m_pTypeWidget = new ezQtTypeWidget(pOwner, m_pGrid, pCommonType);
  m_pTypeWidget->SetSelection(m_Items);

  pLayout->addWidget(m_pTypeWidget);
}


void ezQtPropertyTypeWidget::DoPrepareToDie()
{
  if (m_pTypeWidget)
  {
    m_pTypeWidget->PrepareToDie();
  }
}

/// *** ezQtPropertyContainerWidget ***

ezQtPropertyContainerWidget::ezQtPropertyContainerWidget()
  : ezQtPropertyWidget()
  , m_pAddButton(nullptr)
{
  m_pLayout = new QHBoxLayout(this);
  m_pLayout->setMargin(0);
  setLayout(m_pLayout);

  m_pGroup = new ezQtCollapsibleGroupBox(this);
  m_pGroupLayout = new QVBoxLayout(nullptr);
  m_pGroupLayout->setSpacing(1);
  m_pGroupLayout->setContentsMargins(5, 0, 0, 0);
  m_pGroup->setInnerLayout(m_pGroupLayout);

  m_pLayout->addWidget(m_pGroup);
}

ezQtPropertyContainerWidget::~ezQtPropertyContainerWidget()
{
  Clear();
}

void ezQtPropertyContainerWidget::SetSelection(const ezHybridArray<Selection, 8>& items)
{
  ezQtPropertyWidget::SetSelection(items);

  UpdateElements();

  if (m_pAddButton)
  {
    m_pAddButton->SetSelection(m_Items);
  }
}


void ezQtPropertyContainerWidget::DoPrepareToDie()
{
  for (const auto& e : m_Elements)
  {
    e.m_pWidget->PrepareToDie();
  }
}

void ezQtPropertyContainerWidget::OnElementButtonClicked()
{
  ezQtElementGroupButton* pButton = qobject_cast<ezQtElementGroupButton*>(sender());
  const ezAbstractProperty* pProp = pButton->GetGroupWidget()->GetProperty();
  ezHybridArray<Selection, 8> items = pButton->GetGroupWidget()->GetSelection();

  switch (pButton->GetAction())
  {
  case ezQtElementGroupButton::ElementAction::MoveElementUp:
    {
      MoveItems(items, -1);
    }
    break;
  case ezQtElementGroupButton::ElementAction::MoveElementDown:
    {
      MoveItems(items, 2);
    }
    break;
  case ezQtElementGroupButton::ElementAction::DeleteElement:
    {
      DeleteItems(items);
    }
    break;
  }
}

ezQtPropertyContainerWidget::Element& ezQtPropertyContainerWidget::AddElement(ezUInt32 index)
{
  ezQtCollapsibleGroupBox* pSubGroup = new ezQtCollapsibleGroupBox(m_pGroup);
  connect(pSubGroup, &ezQtCollapsibleGroupBox::CollapseStateChanged, m_pGrid, &ezQtPropertyGridWidget::OnCollapseStateChanged);

  pSubGroup->SetFillColor(palette().window().color());

  QVBoxLayout* pSubLayout = new QVBoxLayout(nullptr);
  pSubLayout->setContentsMargins(5, 0, 0, 0);
  pSubLayout->setSpacing(1);
  pSubGroup->setInnerLayout(pSubLayout);

  m_pGroupLayout->insertWidget((int)index, pSubGroup);

  bool bST = m_pProp->GetFlags().IsSet(ezPropertyFlags::StandardType);
  ezQtPropertyWidget* pNewWidget = bST ? ezQtPropertyGridWidget::CreateMemberPropertyWidget(m_pProp) : new ezQtPropertyTypeWidget();

  pNewWidget->setParent(pSubGroup);
  pSubLayout->addWidget(pNewWidget);

  pNewWidget->Init(m_pGrid, m_pProp);

  {
    // Add Buttons
    auto pAttr = m_pProp->GetAttributeByType<ezContainerAttribute>();
    if (!pAttr || pAttr->CanMove())
    {
      ezQtElementGroupButton* pUpButton = new ezQtElementGroupButton(pSubGroup->Header, ezQtElementGroupButton::ElementAction::MoveElementUp, pNewWidget);
      pSubGroup->HeaderLayout->addWidget(pUpButton);
      connect(pUpButton, &QToolButton::clicked, this, &ezQtPropertyContainerWidget::OnElementButtonClicked);

      ezQtElementGroupButton* pDownButton = new ezQtElementGroupButton(pSubGroup->Header, ezQtElementGroupButton::ElementAction::MoveElementDown, pNewWidget);
      pSubGroup->HeaderLayout->addWidget(pDownButton);
      connect(pDownButton, &QToolButton::clicked, this, &ezQtPropertyContainerWidget::OnElementButtonClicked);
    }

    if (!pAttr || pAttr->CanDelete())
    {
      ezQtElementGroupButton* pDeleteButton = new ezQtElementGroupButton(pSubGroup->Header, ezQtElementGroupButton::ElementAction::DeleteElement, pNewWidget);
      pSubGroup->HeaderLayout->addWidget(pDeleteButton);
      connect(pDeleteButton, &QToolButton::clicked, this, &ezQtPropertyContainerWidget::OnElementButtonClicked);
    }
  }

  m_Elements.Insert(Element(pSubGroup, pNewWidget), index);
  return m_Elements[index];
}

void ezQtPropertyContainerWidget::RemoveElement(ezUInt32 index)
{
  Element& elem = m_Elements[index];

  m_pGroupLayout->removeWidget(elem.m_pSubGroup);
  delete elem.m_pSubGroup;
  m_Elements.RemoveAt(index);
}

void ezQtPropertyContainerWidget::UpdateElements()
{
  QtScopedUpdatesDisabled _(this);

  ezUInt32 iElements = GetRequiredElementCount();

  while (m_Elements.GetCount() > iElements)
  {
    RemoveElement(m_Elements.GetCount() - 1);
  }
  while (m_Elements.GetCount() < iElements)
  {
    AddElement(m_Elements.GetCount());
  }

  for (ezUInt32 i = 0; i < iElements; ++i)
  {
    UpdateElement(i);
  }

  // Force re-layout of parent hierarchy to prevent flicker.
  QWidget* pCur = m_pGroup;
  while (pCur != nullptr && qobject_cast<QScrollArea*>(pCur) == nullptr)
  {
    pCur->updateGeometry();
    pCur = pCur->parentWidget();
  }
}

ezUInt32 ezQtPropertyContainerWidget::GetRequiredElementCount() const
{
  ezInt32 iElements = 0x7FFFFFFF;
  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
  for (const auto& item : m_Items)
  {
    ezInt32 iCount = 0;
    EZ_VERIFY(pObjectAccessor->GetCount(item.m_pObject, m_pProp, iCount).m_Result.Succeeded(), "GetCount should always succeed.");
    iElements = ezMath::Min(iElements, iCount);
  }
  EZ_ASSERT_DEV(iElements >= 0, "Mismatch between storage and RTTI (%i)", iElements);
  return ezUInt32(iElements);
}

void ezQtPropertyContainerWidget::Clear()
{
  while (m_Elements.GetCount() > 0)
  {
    RemoveElement(m_Elements.GetCount() - 1);
  }

  m_Elements.Clear();
}

void ezQtPropertyContainerWidget::OnInit()
{
  m_pGroup->setTitle(ezTranslate(m_pProp->GetPropertyName()));

  const ezContainerAttribute* pArrayAttr = m_pProp->GetAttributeByType<ezContainerAttribute>();
  if (!pArrayAttr || pArrayAttr->CanAdd())
  {
    m_pAddButton = new ezQtAddSubElementButton();
    m_pAddButton->Init(m_pGrid, m_pProp);
    m_pGroup->HeaderLayout->addWidget(m_pAddButton);
  }

  m_pGrid->SetCollapseState(m_pGroup);
  connect(m_pGroup, &ezQtCollapsibleGroupBox::CollapseStateChanged, m_pGrid, &ezQtPropertyGridWidget::OnCollapseStateChanged);
}

void ezQtPropertyContainerWidget::DeleteItems(ezHybridArray<Selection, 8>& items)
{
  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
  pObjectAccessor->StartTransaction("Delete Object");

  ezStatus res;
  if (m_pProp->GetFlags().IsSet(ezPropertyFlags::StandardType))
  {
    for (auto& item : items)
    {
      res = pObjectAccessor->RemoveValue(item.m_pObject, m_pProp, item.m_Index);
      if (res.m_Result.Failed())
        break;
    }
  }
  else
  {
    ezRemoveObjectCommand cmd;

    for (auto& item : items)
    {
      res = pObjectAccessor->RemoveObject(item.m_pObject);
      if (res.m_Result.Failed())
        break;
    }
  }

  if (res.m_Result.Failed())
    pObjectAccessor->CancelTransaction();
  else
    pObjectAccessor->FinishTransaction();

  ezQtUiServices::GetSingleton()->MessageBoxStatus(res, "Removing sub-element from the property failed.");
}

void ezQtPropertyContainerWidget::MoveItems(ezHybridArray<Selection, 8>& items, ezInt32 iMove)
{
  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
  pObjectAccessor->StartTransaction("Reparent Object");

  ezStatus res(EZ_SUCCESS);

  if (m_pProp != nullptr && m_pProp->GetFlags().IsSet(ezPropertyFlags::StandardType))
  {
    for (auto& item : items)
    {
      ezInt32 iCurIndex = item.m_Index.ConvertTo<ezInt32>() + iMove;
      if (iCurIndex < 0 || iCurIndex > pObjectAccessor->GetCount(item.m_pObject, m_pProp))
        continue;

      res = pObjectAccessor->MoveValue(item.m_pObject, m_pProp, item.m_Index, iCurIndex);
      if (res.m_Result.Failed())
        break;
    }
  }
  else
  {
    ezMoveObjectCommand cmd;

    for (auto& item : items)
    {
      ezInt32 iCurIndex = item.m_pObject->GetPropertyIndex().ConvertTo<ezInt32>() + iMove;
      if (iCurIndex < 0 || iCurIndex > pObjectAccessor->GetCount(item.m_pObject->GetParent(), item.m_pObject->GetParentPropertyType()))
        continue;

      res = pObjectAccessor->MoveObject(item.m_pObject, item.m_pObject->GetParent(), item.m_pObject->GetParentPropertyType(), iCurIndex);
      if (res.m_Result.Failed())
        break;
    }
  }

  if (res.m_Result.Failed())
    pObjectAccessor->CancelTransaction();
  else
    pObjectAccessor->FinishTransaction();

  ezQtUiServices::GetSingleton()->MessageBoxStatus(res, "Moving sub-element failed.");
}


/// *** ezQtPropertyStandardTypeContainerWidget ***

ezQtPropertyStandardTypeContainerWidget::ezQtPropertyStandardTypeContainerWidget()
  : ezQtPropertyContainerWidget()
{
}

ezQtPropertyStandardTypeContainerWidget::~ezQtPropertyStandardTypeContainerWidget()
{
}

ezQtPropertyContainerWidget::Element& ezQtPropertyStandardTypeContainerWidget::AddElement(ezUInt32 index)
{
  ezQtPropertyContainerWidget::Element& elem = ezQtPropertyContainerWidget::AddElement(index);
  elem.m_pWidget->m_Events.AddEventHandler(ezMakeDelegate(&ezQtPropertyStandardTypeContainerWidget::PropertyChangedHandler, this));

  return elem;
}

void ezQtPropertyStandardTypeContainerWidget::RemoveElement(ezUInt32 index)
{
  {
    Element& elem = m_Elements[index];
    elem.m_pWidget->m_Events.RemoveEventHandler(ezMakeDelegate(&ezQtPropertyStandardTypeContainerWidget::PropertyChangedHandler, this));
  }
  ezQtPropertyContainerWidget::RemoveElement(index);
}

void ezQtPropertyStandardTypeContainerWidget::UpdateElement(ezUInt32 index)
{
  Element& elem = m_Elements[index];

  ezHybridArray<ezQtPropertyWidget::Selection, 8> SubItems;

  for (const auto& item : m_Items)
  {
    ezQtPropertyWidget::Selection sel;
    sel.m_pObject = item.m_pObject;
    sel.m_Index = index;

    SubItems.PushBack(sel);
  }

  ezStringBuilder sTitle;
  sTitle.Printf("[%i]", index);

  elem.m_pSubGroup->setTitle(sTitle);
  m_pGrid->SetCollapseState(elem.m_pSubGroup);
  elem.m_pWidget->SetSelection(SubItems);
}

void ezQtPropertyStandardTypeContainerWidget::PropertyChangedHandler(const ezQtPropertyWidget::Event& ed)
{
  if (IsUndead())
    return;

  // Forward from child widget to parent ezQtTypeWidget.
  m_Events.Broadcast(ed);
}


/// *** ezQtPropertyTypeContainerWidget ***

ezQtPropertyTypeContainerWidget::ezQtPropertyTypeContainerWidget()
{
}

ezQtPropertyTypeContainerWidget::~ezQtPropertyTypeContainerWidget()
{
  m_pGrid->GetDocument()->GetObjectManager()->m_StructureEvents.RemoveEventHandler(ezMakeDelegate(&ezQtPropertyTypeContainerWidget::StructureEventHandler, this));
}

void ezQtPropertyTypeContainerWidget::OnInit()
{
  ezQtPropertyContainerWidget::OnInit();
  m_pGrid->GetDocument()->GetObjectManager()->m_StructureEvents.AddEventHandler(ezMakeDelegate(&ezQtPropertyTypeContainerWidget::StructureEventHandler, this));

}

void ezQtPropertyTypeContainerWidget::UpdateElement(ezUInt32 index)
{
  Element& elem = m_Elements[index];
  ezObjectAccessorBase* pObjectAccessor = m_pGrid->GetObjectAccessor();
  ezHybridArray<ezQtPropertyWidget::Selection, 8> SubItems;

  for (const auto& item : m_Items)
  {
    ezUuid ObjectGuid = pObjectAccessor->Get<ezUuid>(item.m_pObject, m_pProp, index);

    ezQtPropertyWidget::Selection sel;
    sel.m_pObject = pObjectAccessor->GetObject(ObjectGuid);
    //sel.m_Index = // supposed to be invalid;

    SubItems.PushBack(sel);
  }

  const ezRTTI* pCommonType = ezQtPropertyWidget::GetCommonBaseType(SubItems);

  ezStringBuilder sTitle;
  sTitle.Printf("[%i] - %s", index, ezTranslate(pCommonType->GetTypeName()));
  elem.m_pSubGroup->setTitle(sTitle);

  {
    ezStringBuilder sIconName;
    sIconName.Set(":/TypeIcons/", pCommonType->GetTypeName());
    elem.m_pSubGroup->Icon2->setPixmap(ezQtUiServices::GetCachedPixmapResource(sIconName.GetData()));
  }

  m_pGrid->SetCollapseState(elem.m_pSubGroup);
  elem.m_pWidget->SetSelection(SubItems);
}

void ezQtPropertyTypeContainerWidget::StructureEventHandler(const ezDocumentObjectStructureEvent& e)
{
  if (IsUndead())
    return;

  switch (e.m_EventType)
  {
  case ezDocumentObjectStructureEvent::Type::AfterObjectAdded:
  case ezDocumentObjectStructureEvent::Type::AfterObjectMoved:
  case ezDocumentObjectStructureEvent::Type::AfterObjectRemoved:
    {
      if (!e.m_sParentProperty.IsEqual(m_pProp->GetPropertyName()))
        return;

      if (std::none_of(cbegin(m_Items), cend(m_Items),
                       [&](const ezQtPropertyWidget::Selection& sel) { return e.m_pNewParent == sel.m_pObject || e.m_pPreviousParent == sel.m_pObject; }
      ))
        return;

      UpdateElements();
    }
    break;
  }
}

