#include <PCH.h>
#include <EditorFramework/Panels/AssetBrowserPanel/AssetBrowserPanel.moc.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <QSettings>
#include <CoreUtils/Localization/TranslationLookup.h>

ezQtAssetBrowserPanel* ezQtAssetBrowserPanel::s_pInstance = nullptr;

ezQtAssetBrowserPanel::ezQtAssetBrowserPanel() : ezQtApplicationPanel("PanelAssetBrowser")
{
  EZ_ASSERT_DEV(s_pInstance == nullptr, "ezQtAssetBrowserPanel panel is not a singleton anymore");

  s_pInstance = this;

  setupUi(this);

  setWindowIcon(QIcon(QString::fromUtf8(":/GuiFoundation/Icons/Asset16.png")));
  setWindowTitle(QString::fromUtf8(ezTranslate("PanelAssetBrowser")));

  EZ_VERIFY(connect(AssetBrowserWidget, SIGNAL(ItemChosen(QString, QString, QString)), this, SLOT(SlotAssetChosen(QString, QString, QString))) != nullptr, "signal/slot connection failed");

  AssetBrowserWidget->RestoreState("AssetBrowserPanel2");
  
}

ezQtAssetBrowserPanel::~ezQtAssetBrowserPanel()
{
  AssetBrowserWidget->SaveState("AssetBrowserPanel2");

  s_pInstance = nullptr;

  
}

void ezQtAssetBrowserPanel::SlotAssetChosen(QString sAssetGuid, QString sAssetPathRelative, QString sAssetPathAbsolute)
{
  ezQtEditorApp::GetInstance()->OpenDocument(sAssetPathAbsolute.toUtf8().data());
}