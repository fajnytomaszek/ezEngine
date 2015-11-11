#include <PCH.h>
#include <EditorPluginScene/Panels/ScenegraphPanel/ScenegraphModel.moc.h>
#include <Foundation/Reflection/Implementation/StaticRTTI.h>
#include <Core/World/GameObject.h>
#include <ToolsFoundation/Document/Document.h>
#include <EditorPluginScene/Scene/SceneDocument.h>
#include <EditorFramework/Assets/AssetCurator.h>

ezQtScenegraphModel::ezQtScenegraphModel(ezSceneDocument* pDocument)
  : ezQtDocumentTreeModel(pDocument->GetObjectManager(), ezGetStaticRTTI<ezGameObject>(), "Children")
{
  m_pSceneDocument = pDocument;

  m_pSceneDocument->m_ObjectMetaData.m_DataModifiedEvent.AddEventHandler(ezMakeDelegate(&ezQtScenegraphModel::ObjectMetaDataEventHandler, this));
}

ezQtScenegraphModel::~ezQtScenegraphModel()
{
  m_pSceneDocument->m_ObjectMetaData.m_DataModifiedEvent.RemoveEventHandler(ezMakeDelegate(&ezQtScenegraphModel::ObjectMetaDataEventHandler, this));
}

void ezQtScenegraphModel::DetermineNodeName(const ezDocumentObject* pObject, const ezUuid& prefabGuid, ezStringBuilder& out_Result) const
{
  // tries to find a good name for a node by looking at the attached components and their properties

  if (prefabGuid.IsValid())
  {
    auto pInfo = ezAssetCurator::GetInstance()->GetAssetInfo(prefabGuid);

    if (pInfo)
    {
      ezStringBuilder sPath = pInfo->m_sRelativePath;
      sPath = sPath.GetFileName();

      out_Result.Set("Prefab: ", sPath);
    }
    else
      out_Result = "Prefab: Invalid Asset";

    return;
  }

  bool bHasChildren = false;

  /// \todo Iterate over children in a way that returns the proper order (this is random)

  for (auto pChild : pObject->GetChildren())
  {
    // search for components
    if (pChild->GetTypeAccessor().GetType()->IsDerivedFrom<ezComponent>())
    {
      // take the first components name
      if (out_Result.IsEmpty())
      {
        out_Result = pChild->GetTypeAccessor().GetType()->GetTypeName();

        // clean up the component name
        if (out_Result.EndsWith_NoCase("Component"))
          out_Result.Shrink(0, 9);
        if (out_Result.StartsWith("ez"))
          out_Result.Shrink(2, 0);
      }

      const auto& properties = pChild->GetTypeAccessor().GetType()->GetProperties();

      for (auto pProperty : properties)
      {
        // search for string properties that also have an asset browser property -> they reference an asset, so this is most likely the most relevant property
        if (pProperty->GetCategory() == ezPropertyCategory::Member && 
            (pProperty->GetSpecificType() == ezGetStaticRTTI<const char*>() ||
             pProperty->GetSpecificType() == ezGetStaticRTTI<ezString>()) &&
            pProperty->GetAttributeByType<ezAssetBrowserAttribute>() != nullptr)
        {
          ezStringBuilder sValue = pChild->GetTypeAccessor().GetValue(ezToolsReflectionUtils::CreatePropertyPath(pProperty->GetPropertyName())).ConvertTo<ezString>();

          // if the property is a full asset guid reference, convert it to a file name
          if (ezConversionUtils::IsStringUuid(sValue))
          {
            const ezUuid AssetGuid = ezConversionUtils::ConvertStringToUuid(sValue);

            auto pAsset = ezAssetCurator::GetInstance()->GetAssetInfo(AssetGuid);

            if (pAsset)
              sValue = pAsset->m_sRelativePath;
            else
              sValue = "<unknown>";
          }

          // only use the file name for our display
          sValue = sValue.GetFileName();

          out_Result.Append(": ", sValue);
          return;
        }
      }
    }
    else
    {
      // must be ezGameObject children
      bHasChildren = true;
    }
  }

  if (bHasChildren)
    out_Result = "Group";
  else
    out_Result = "Entity";
}

QVariant ezQtScenegraphModel::data(const QModelIndex &index, int role) const
{
  const ezDocumentObject* pObject = (const ezDocumentObject*)index.internalPointer();

  switch (role)
  {
  case Qt::DisplayRole:
    {
      ezStringBuilder sName = pObject->GetTypeAccessor().GetValue(ezToolsReflectionUtils::CreatePropertyPath("Name")).ConvertTo<ezString>();

      auto pMeta = m_pSceneDocument->m_ObjectMetaData.BeginReadMetaData(pObject->GetGuid());
      const ezUuid prefabGuid = pMeta->m_CreateFromPrefab;

      if (sName.IsEmpty())
        sName = pMeta->m_CachedNodeName;

      m_pSceneDocument->m_ObjectMetaData.EndReadMetaData();

      if (sName.IsEmpty())
      {
        // the cached node name is only determined once
        // after that only a node rename (EditRole) will currently trigger a cache cleaning and thus a reevaluation
        // this is to prevent excessive recomputation of the name, which is quite involved

        DetermineNodeName(pObject, prefabGuid, sName);

        auto pMetaWrite = m_pSceneDocument->m_ObjectMetaData.BeginModifyMetaData(pObject->GetGuid());
        pMetaWrite->m_CachedNodeName = sName;
        m_pSceneDocument->m_ObjectMetaData.EndModifyMetaData(0); // no need to broadcast this change
      }

      const QString sQtName = QString::fromUtf8(sName.GetData());

      if (prefabGuid.IsValid())
        return QStringLiteral("[") + sQtName + QStringLiteral("]");

      return sQtName;
    }
    break;

  case Qt::EditRole:
    {
      ezStringBuilder sName = pObject->GetTypeAccessor().GetValue(ezToolsReflectionUtils::CreatePropertyPath("Name")).ConvertTo<ezString>();

      if (sName.IsEmpty())
      {
        auto pMeta = m_pSceneDocument->m_ObjectMetaData.BeginReadMetaData(pObject->GetGuid());
        sName = pMeta->m_CachedNodeName;
        m_pSceneDocument->m_ObjectMetaData.EndReadMetaData();
      }

      return QString::fromUtf8(sName.GetData());
    }
    break;

  case Qt::ToolTipRole:
    {
      auto pMeta = m_pSceneDocument->m_ObjectMetaData.BeginReadMetaData(pObject->GetGuid());
      const ezUuid prefab = pMeta->m_CreateFromPrefab;
      m_pSceneDocument->m_ObjectMetaData.EndReadMetaData();

      if (prefab.IsValid())
      {
        auto pInfo = ezAssetCurator::GetInstance()->GetAssetInfo(prefab);

        if (pInfo)
          return QString::fromUtf8(pInfo->m_sRelativePath);

        return QStringLiteral("Prefab asset could not be found");
      }

    }
    break;

  case Qt::FontRole:
    {
      auto pMeta = m_pSceneDocument->m_ObjectMetaData.BeginReadMetaData(pObject->GetGuid());
      const bool bHidden = pMeta->m_bHidden;
      m_pSceneDocument->m_ObjectMetaData.EndReadMetaData();

      const bool bHasName = !pObject->GetTypeAccessor().GetValue(ezToolsReflectionUtils::CreatePropertyPath("Name")).ConvertTo<ezString>().IsEmpty();

      if (bHidden || bHasName)
      {
        QFont font;

        if (bHidden)
          font.setStrikeOut(true);
        if (bHasName)
          font.setBold(true);

        return font;
      }
    }
    break;

  case Qt::ForegroundRole:
    {
      ezStringBuilder sName = pObject->GetTypeAccessor().GetValue(ezToolsReflectionUtils::CreatePropertyPath("Name")).ConvertTo<ezString>();
      
      auto pMeta = m_pSceneDocument->m_ObjectMetaData.BeginReadMetaData(pObject->GetGuid());
      const bool bPrefab = pMeta->m_CreateFromPrefab.IsValid();
      m_pSceneDocument->m_ObjectMetaData.EndReadMetaData();

      if (bPrefab)
      {
        return QColor(0, 128, 196);
      }

      if (sName.IsEmpty())
      {
        // uses an auto generated name
        return QColor(128, 128, 128);
      }

    }
    break;
  }

  return ezQtDocumentTreeModel::data(index, role);
}

bool ezQtScenegraphModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (role == Qt::EditRole)
  {
    const ezDocumentObject* pObject = (const ezDocumentObject*)index.internalPointer();
 
    auto pMetaWrite = m_pSceneDocument->m_ObjectMetaData.BeginModifyMetaData(pObject->GetGuid());

    ezStringBuilder sNewValue = value.toString().toUtf8().data();

    const ezStringBuilder sOldValue = pMetaWrite->m_CachedNodeName;

    pMetaWrite->m_CachedNodeName.Clear();
    m_pSceneDocument->m_ObjectMetaData.EndModifyMetaData(0); // no need to broadcast this change
    
    if (sOldValue == sNewValue && !sOldValue.IsEmpty())
      return false;

    sNewValue.Trim("[]{}() \t\r"); // forbid these

    return ezQtDocumentTreeModel::setData(index, QString::fromUtf8(sNewValue.GetData()), role);
  }

  return false;
}


void ezQtScenegraphModel::ObjectMetaDataEventHandler(const ezObjectMetaData<ezUuid, ezSceneObjectMetaData>::EventData& e)
{
  auto pObject = m_pSceneDocument->GetObjectManager()->GetObject(e.m_ObjectKey);

  auto index = ComputeModelIndex(pObject);

  QVector<int> v;
  v.push_back(Qt::FontRole);
  dataChanged(index, index, v);
}


