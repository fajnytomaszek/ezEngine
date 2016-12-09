#include <PCH.h>
#include <EditorFramework/Panels/LogPanel/LogPanel.moc.h>
#include <QSettings>
#include <CoreUtils/Localization/TranslationLookup.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>
#include <QThread>

EZ_IMPLEMENT_SINGLETON(ezQtLogPanel);

ezQtLogPanel::ezQtLogPanel()
  : ezQtApplicationPanel("Panel.Log")
  , m_SingletonRegistrar(this)
{
  setupUi(this);

  setWindowIcon(ezQtUiServices::GetCachedIconResource(":/EditorFramework/Icons/Log.png"));
  setWindowTitle(QString::fromUtf8(ezTranslate("Panel.Log")));

  ezGlobalLog::AddLogWriter(ezMakeDelegate(&ezQtLogPanel::LogWriter, this));
  ezEditorEngineProcessConnection::s_Events.AddEventHandler(ezMakeDelegate(&ezQtLogPanel::EngineProcessMsgHandler, this));

  ListViewEditorLog->setModel(&m_EditorLog);
  ListViewEditorLog->setUniformItemSizes(true);
  connect(&m_EditorLog, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last)
  {
    ScrollToBottomIfAtEnd(ListViewEditorLog, first);
  });

  ListViewEngineLog->setModel(&m_EngineLog);
  ListViewEngineLog->setUniformItemSizes(true);
  connect(&m_EngineLog, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last)
  {
    ScrollToBottomIfAtEnd(ListViewEngineLog, first);
  });

  ButtonClearSearch->setEnabled(false);

  QSettings Settings;
  Settings.beginGroup(QLatin1String("LogPanel"));
  {
    splitter->restoreState(Settings.value("Splitter", splitter->saveState()).toByteArray());
  }
  Settings.endGroup();
}

ezQtLogPanel::~ezQtLogPanel()
{
  QSettings Settings;
  Settings.beginGroup(QLatin1String("LogPanel"));
  {
    Settings.setValue("Splitter", splitter->saveState());
  }
  Settings.endGroup();

  ezGlobalLog::RemoveLogWriter(ezMakeDelegate(&ezQtLogPanel::LogWriter, this));
  ezEditorEngineProcessConnection::s_Events.RemoveEventHandler(ezMakeDelegate(&ezQtLogPanel::EngineProcessMsgHandler, this));
}

void ezQtLogPanel::ToolsProjectEventHandler(const ezToolsProjectEvent& e)
{
  switch (e.m_Type)
  {
  case ezToolsProjectEvent::Type::ProjectClosing:
    {
      m_EditorLog.Clear();
      m_EngineLog.Clear();
      LineSearch->clear();
      ComboFilter->setCurrentIndex(0);
      // fallthrough

  case ezToolsProjectEvent::Type::ProjectOpened:
      setEnabled(e.m_Type == ezToolsProjectEvent::Type::ProjectOpened);
    }
    break;
  }

  ezQtApplicationPanel::ToolsProjectEventHandler(e);
}

void ezQtLogPanel::LogWriter(const ezLoggingEventData& e)
{
  // Can be called from a different thread, but AddLogMsg is thread safe.
  ezQtLogModel::LogMsg msg;
  msg.m_sMsg = e.m_szText;
  msg.m_sTag = e.m_szTag;
  msg.m_Type = e.m_EventType;
  msg.m_uiIndentation = e.m_uiIndentation;

  m_EditorLog.AddLogMsg(msg);
}

void ezQtLogPanel::ScrollToBottomIfAtEnd(QListView* pView, int iNumElements)
{
  if (pView->selectionModel()->hasSelection())
  {
    if (pView->selectionModel()->selectedIndexes()[0].row() + 1 >= iNumElements)
    {
      pView->selectionModel()->clearSelection();
      pView->scrollToBottom();
    }
  }
  else
    pView->scrollToBottom();
}

void ezQtLogPanel::EngineProcessMsgHandler(const ezEditorEngineProcessConnection::Event& e)
{
  switch (e.m_Type)
  {
  case ezEditorEngineProcessConnection::Event::Type::ProcessMessage:
    {
      if (e.m_pMsg->GetDynamicRTTI()->IsDerivedFrom<ezLogMsgToEditor>())
      {
        const ezLogMsgToEditor* pMsg = static_cast<const ezLogMsgToEditor*>(e.m_pMsg);

        ezQtLogModel::LogMsg msg;
        msg.m_sMsg = pMsg->m_sText;
        msg.m_sTag = pMsg->m_sTag;
        msg.m_Type = (ezLogMsgType::Enum)pMsg->m_iMsgType;
        msg.m_uiIndentation = pMsg->m_uiIndentation;

        m_EngineLog.AddLogMsg(msg);
      }
    }
    break;

  default:
    return;
  }
}

void ezQtLogPanel::on_ButtonClearEditorLog_clicked()
{
  m_EditorLog.Clear();
}

void ezQtLogPanel::on_ButtonClearEngineLog_clicked()
{
  m_EngineLog.Clear();
}

void ezQtLogPanel::on_ButtonClearSearch_clicked()
{
  LineSearch->clear();
}

void ezQtLogPanel::on_LineSearch_textChanged(const QString& text)
{
  ButtonClearSearch->setEnabled(!text.isEmpty());

  m_EditorLog.SetSearchText(text.toUtf8().data());
  m_EngineLog.SetSearchText(text.toUtf8().data());
}

void ezQtLogPanel::on_ComboFilter_currentIndexChanged(int index)
{
  const ezLogMsgType::Enum LogLevel = (ezLogMsgType::Enum) (ezLogMsgType::All - index);

  m_EditorLog.SetLogLevel(LogLevel);
  m_EngineLog.SetLogLevel(LogLevel);
}

ezQtLogModel::ezQtLogModel()
{
  m_bIsValid = true;
  m_LogLevel = ezLogMsgType::All;
}

void ezQtLogModel::Invalidate()
{
  if (!m_bIsValid)
    return;

  m_bIsValid = false;
  dataChanged(QModelIndex(), QModelIndex());
}

void ezQtLogModel::Clear()
{
  if (m_AllMessages.IsEmpty())
    return;

  m_AllMessages.Clear();
  Invalidate();
}

void ezQtLogModel::SetLogLevel(ezLogMsgType::Enum LogLevel)
{
  if (m_LogLevel == LogLevel)
    return;

  m_LogLevel = LogLevel;
  Invalidate();
}

void ezQtLogModel::SetSearchText(const char* szText)
{
  if (m_sSearchText == szText)
    return;

  m_sSearchText = szText;
  Invalidate();
}

void ezQtLogModel::AddLogMsg(const LogMsg& msg)
{
  {
    EZ_LOCK(m_NewMessagesMutex);
    m_NewMessages.PushBack(msg);
  }

  if (QThread::currentThread() == thread())
  {
    ProcessNewMessages();
  }
  else
  {
    QMetaObject::invokeMethod(this, "ProcessNewMessages", Qt::ConnectionType::QueuedConnection);
  }

  return;
}

bool ezQtLogModel::IsFiltered(const LogMsg& lm) const
{
  if (lm.m_Type < ezLogMsgType::None)
    return false;

  if (lm.m_Type > m_LogLevel)
    return true;

  if (m_sSearchText.IsEmpty())
    return false;

  if (lm.m_sMsg.FindSubString_NoCase(m_sSearchText.GetData()))
    return false;

  return true;
}

////////////////////////////////////////////////////////////////////////
// ezQtLogModel QAbstractItemModel functions
////////////////////////////////////////////////////////////////////////

QVariant ezQtLogModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.column() != 0)
    return QVariant();

  UpdateVisibleEntries();

  const ezInt32 iRow = index.row();
  if (iRow < 0 || iRow >= (ezInt32)m_VisibleMessages.GetCount())
    return QVariant();

  const LogMsg& msg = *m_VisibleMessages[iRow];

  switch (role)
  {
  case Qt::DisplayRole:
  case Qt::ToolTipRole:
    {
      return QString::fromUtf8(msg.m_sMsg.GetData());
    }
  case Qt::TextColorRole:
    {
      switch (msg.m_Type)
      {
      case ezLogMsgType::BeginGroup:          return QColor::fromRgb(160, 90, 255);
      case ezLogMsgType::EndGroup:            return QColor::fromRgb(110, 60, 185);
      case ezLogMsgType::ErrorMsg:            return QColor::fromRgb(255, 0, 0);
      case ezLogMsgType::SeriousWarningMsg:   return QColor::fromRgb(255, 64, 0);
      case ezLogMsgType::WarningMsg:          return QColor::fromRgb(255, 140, 0);
      case ezLogMsgType::SuccessMsg:          return QColor::fromRgb(0, 128, 0);
      case ezLogMsgType::DevMsg:              return QColor::fromRgb(128, 128, 128);
      case ezLogMsgType::DebugMsg:            return QColor::fromRgb(255, 0, 255);
      default:
        return QVariant();
      }
    }

  default:
    return QVariant();
  }
}

Qt::ItemFlags ezQtLogModel::flags(const QModelIndex& index) const
{
  if (!index.isValid())
    return 0;

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant ezQtLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  return QVariant();
}

QModelIndex ezQtLogModel::index(int row, int column, const QModelIndex& parent) const
{
  if (parent.isValid() || column != 0)
    return QModelIndex();

  return createIndex(row, column, row);
}

QModelIndex ezQtLogModel::parent(const QModelIndex& index) const
{
  return QModelIndex();
}

int ezQtLogModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;

  UpdateVisibleEntries();

  return (int)m_VisibleMessages.GetCount();
}

int ezQtLogModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}


void ezQtLogModel::ProcessNewMessages()
{
  EZ_LOCK(m_NewMessagesMutex);
  ezStringBuilder s;
  for (const auto& msg : m_NewMessages)
  {
    m_AllMessages.PushBack(msg);

    if (msg.m_Type == ezLogMsgType::BeginGroup || msg.m_Type == ezLogMsgType::EndGroup)
    {
      s.Printf("%*s<<< %s", msg.m_uiIndentation, "", msg.m_sMsg.GetData());

      if (!msg.m_sTag.IsEmpty())
        s.Append(" (", msg.m_sTag, ") >>>");
      else
        s.Append(" >>>");

      m_AllMessages.PeekBack().m_sMsg = s;
    }
    else
    {
      s.Printf("%*s%s", 4 * msg.m_uiIndentation, "", msg.m_sMsg.GetData());
      m_AllMessages.PeekBack().m_sMsg = s;
    }


    // if the message would not be shown anyway, don't trigger an update
    if (IsFiltered(msg))
      continue;

    beginInsertRows(QModelIndex(), m_VisibleMessages.GetCount(), m_VisibleMessages.GetCount());
    m_VisibleMessages.PushBack(&m_AllMessages.PeekBack());
    endInsertRows();
  }

  m_NewMessages.Clear();
}

void ezQtLogModel::UpdateVisibleEntries() const
{
  if (m_bIsValid)
    return;

  m_bIsValid = true;
  m_VisibleMessages.Clear();

  for (const auto& msg : m_AllMessages)
  {
    if (IsFiltered(msg))
      continue;

    if (msg.m_Type == ezLogMsgType::EndGroup)
    {
      if (!m_VisibleMessages.IsEmpty())
      {
        if (m_VisibleMessages.PeekBack()->m_Type == ezLogMsgType::BeginGroup)
          m_VisibleMessages.PopBack();
        else
          m_VisibleMessages.PushBack(&msg);
      }
    }
    else
    {
      m_VisibleMessages.PushBack(&msg);
    }
  }
}
