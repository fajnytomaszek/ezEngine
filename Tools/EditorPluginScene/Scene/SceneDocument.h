#pragma once

#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/DocumentObjectManager.h>
#include <EditorFramework/DocumentWindow3D/DocumentWindow3D.moc.h>
#include <CoreUtils/DataStructures/ObjectMetaData.h>

enum class ActiveGizmo
{
  None,
  Translate,
  Rotate,
  Scale,
  DragToPosition,
};

struct ezSceneObjectMetaData
{
  enum ModifiedFlags
  {
    HiddenFlag = EZ_BIT(0),

    AllFlags = 0xFFFFFFFF
  };

  ezSceneObjectMetaData()
  {
    m_bHidden = false;
  }

  bool m_bHidden;
};

class ezSceneDocument : public ezDocument
{
  EZ_ADD_DYNAMIC_REFLECTION(ezSceneDocument);

public:
  ezSceneDocument(const char* szDocumentPath);
  ~ezSceneDocument();

  virtual const char* GetDocumentTypeDisplayString() const override { return "Scene"; }

  virtual ezStatus InternalSaveDocument() override;

  void SetActiveGizmo(ActiveGizmo gizmo);
  ActiveGizmo GetActiveGizmo() const;

  enum class ShowOrHide
  {
    Show,
    Hide
  };

  void TriggerShowSelectionInScenegraph();
  void TriggerFocusOnSelection(bool bAllViews);
  void TriggerSnapPivotToGrid();
  void TriggerSnapEachObjectToGrid();
  void GroupSelection();
  void DuplicateSelection();
  void ShowOrHideSelectedObjects(ShowOrHide action);
  void ShowOrHideAllObjects(ShowOrHide action);
  void HideUnselectedObjects();
  
  void SetGizmoWorldSpace(bool bWorldSpace);
  bool GetGizmoWorldSpace() const;

  virtual bool Copy(ezAbstractObjectGraph& out_objectGraph) override;
  virtual bool Paste(const ezArrayPtr<PasteInfo>& info) override;
  bool Duplicate(const ezArrayPtr<PasteInfo>& info);
  bool Copy(ezAbstractObjectGraph& graph, ezMap<ezUuid, ezUuid>* out_pParents);
  bool PasteAt(const ezArrayPtr<PasteInfo>& info, const ezVec3& vPos);
  bool PasteAtOrignalPosition(const ezArrayPtr<PasteInfo>& info);

  const ezTransform& GetGlobalTransform(const ezDocumentObject* pObject);
  void SetGlobalTransform(const ezDocumentObject* pObject, const ezTransform& t);

  void SetPickingResult(const ezObjectPickingResult& res) { m_PickingResult = res; }

  static ezTransform QueryLocalTransform(const ezDocumentObject* pObject);
  static ezTransform ComputeGlobalTransform(const ezDocumentObject* pObject);

  struct SceneEvent
  {
    enum class Type
    {
      ActiveGizmoChanged,
      ShowSelectionInScenegraph,
      FocusOnSelection_Hovered,
      FocusOnSelection_All,
      SnapSelectionPivotToGrid,
      SnapEachSelectedObjectToGrid,
    };

    Type m_Type;
  };

  ezEvent<const SceneEvent&> m_SceneEvents;
  ezObjectMetaData<ezUuid, ezSceneObjectMetaData> m_ObjectMetaData;

protected:
  virtual void InitializeAfterLoading() override;

  virtual ezDocumentInfo* CreateDocumentInfo() override { return EZ_DEFAULT_NEW(ezDocumentInfo); }

  template<typename Func>
  void ApplyRecursive(const ezDocumentObject* pObject, Func f)
  {
    f(pObject);

    for (auto pChild : pObject->GetChildren())
    {
      ApplyRecursive<Func>(pChild, f);
    }
  }

private:
  void ObjectPropertyEventHandler(const ezDocumentObjectPropertyEvent& e);
  void ObjectStructureEventHandler(const ezDocumentObjectStructureEvent& e);

  void InvalidateGlobalTransformValue(const ezDocumentObject* pObject);

  bool m_bGizmoWorldSpace; // whether the gizmo is in local/global space mode
  ActiveGizmo m_ActiveGizmo;
  ezObjectPickingResult m_PickingResult;

  ezHashTable<const ezDocumentObject*, ezTransform> m_GlobalTransforms;
};
