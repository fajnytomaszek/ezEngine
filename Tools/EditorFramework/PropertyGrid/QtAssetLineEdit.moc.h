#pragma once

#include <EditorFramework/Plugin.h>
#include <GuiFoundation/PropertyGrid/Implementation/PropertyWidget.moc.h>
#include <QModelIndex>
#include <QLineEdit>

/// \brief A QLineEditor that adds support for asset specific operations and display
class EZ_EDITORFRAMEWORK_DLL ezQtAssetLineEdit : public QLineEdit
{
  Q_OBJECT

public:

  explicit ezQtAssetLineEdit(QWidget* parent = nullptr);
  virtual void dragMoveEvent(QDragMoveEvent *e) override;
  virtual void dragEnterEvent(QDragEnterEvent * e) override;
  virtual void dropEvent(QDropEvent* e) override;
};

