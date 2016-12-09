#include <PCH.h>
#include <Inspector/InputWidget.moc.h>
#include <qlistwidget.h>
#include <Foundation/Communication/Telemetry.h>
#include <MainWindow.moc.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>

ezQtInputWidget* ezQtInputWidget::s_pWidget = nullptr;

ezQtInputWidget::ezQtInputWidget(QWidget* parent) : QDockWidget (parent)
{
  s_pWidget = this;

  setupUi (this);

  ResetStats();
}

void ezQtInputWidget::ResetStats()
{
  ClearSlots();
  ClearActions();
}

void ezQtInputWidget::ClearSlots()
{
  m_InputSlots.Clear();
  TableInputSlots->clear();

  {
    QStringList Headers;
    Headers.append("");
    Headers.append(" Slot ");
    Headers.append(" State ");
    Headers.append(" Value ");
    Headers.append(" Dead Zone ");
    Headers.append(" Flags (Binary) ");

    TableInputSlots->setColumnCount(Headers.size());

    TableInputSlots->setHorizontalHeaderLabels(Headers);
    TableInputSlots->horizontalHeader()->show();
  }
}

void ezQtInputWidget::ClearActions()
{
  m_InputActions.Clear();
  TableInputActions->clear();

  {
    QStringList Headers;
    Headers.append("");
    Headers.append(" Action ");
    Headers.append(" State ");
    Headers.append(" Value ");

    for (ezInt32 slot = 0; slot < ezInputActionConfig::MaxInputSlotAlternatives; ++slot)
      Headers.append(QString(" Slot %1 ").arg(slot + 1));

    TableInputActions->setColumnCount(Headers.size());

    TableInputActions->setHorizontalHeaderLabels(Headers);
    TableInputActions->horizontalHeader()->show();
  }
}

void ezQtInputWidget::ProcessTelemetry(void* pUnuseed)
{
  if (!s_pWidget)
    return;

  ezTelemetryMessage msg;

  bool bUpdateSlotTable = false;
  bool bFillSlotTable = false;
  bool bUpdateActionTable = false;
  bool bFillActionTable = false;

  while (ezTelemetry::RetrieveMessage('INPT', msg) == EZ_SUCCESS)
  {
    if (msg.GetMessageID() == 'SLOT')
    {
      ezString sSlotName;
      msg.GetReader() >> sSlotName;

      SlotData& sd = s_pWidget->m_InputSlots[sSlotName];

      msg.GetReader() >> sd.m_uiSlotFlags;

      ezUInt8 uiKeyState = 0;
      msg.GetReader() >> uiKeyState;
      sd.m_KeyState = (ezKeyState::Enum) uiKeyState;

      msg.GetReader() >> sd.m_fValue;
      msg.GetReader() >> sd.m_fDeadZone;

      if (sd.m_iTableRow == -1)
        bUpdateSlotTable = true;

      bFillSlotTable = true;
    }

    if (msg.GetMessageID() == 'ACTN')
    {
      ezString sInputSetName;
      msg.GetReader() >> sInputSetName;

      ezString sActionName;
      msg.GetReader() >> sActionName;

      ezStringBuilder sFinalName = sInputSetName.GetData();
      sFinalName.Append("::");
      sFinalName.Append(sActionName.GetData());

      ActionData& sd = s_pWidget->m_InputActions[sFinalName.GetData()];

      ezUInt8 uiKeyState = 0;
      msg.GetReader() >> uiKeyState;
      sd.m_KeyState = (ezKeyState::Enum) uiKeyState;

      msg.GetReader() >> sd.m_fValue;
      msg.GetReader() >> sd.m_bUseTimeScaling;

      for (ezUInt32 i = 0; i < ezInputActionConfig::MaxInputSlotAlternatives; ++i)
      {
        msg.GetReader() >> sd.m_sTrigger[i];
        msg.GetReader() >> sd.m_fTriggerScaling[i];
      }

      if (sd.m_iTableRow == -1)
        bUpdateActionTable = true;

      bFillActionTable = true;
    }
  }

  if (bUpdateSlotTable)
    s_pWidget->UpdateSlotTable(true);
  else
  if (bFillSlotTable)
    s_pWidget->UpdateSlotTable(false);

  if (bUpdateActionTable)
    s_pWidget->UpdateActionTable(true);
  else
  if (bFillActionTable)
    s_pWidget->UpdateActionTable(false);}

void ezQtInputWidget::UpdateSlotTable(bool bRecreate)
{
  TableInputSlots->blockSignals(true);

  if (bRecreate)
  {
    TableInputSlots->clear();
    TableInputSlots->setRowCount(m_InputSlots.GetCount());

    QStringList Headers;
    Headers.append("");
    Headers.append(" Slot ");
    Headers.append(" State ");
    Headers.append(" Value ");
    Headers.append(" Dead Zone ");
    Headers.append(" Flags (Binary) ");

    TableInputSlots->setColumnCount(Headers.size());

    TableInputSlots->setHorizontalHeaderLabels(Headers);
    TableInputSlots->horizontalHeader()->show();

    ezStringBuilder sTemp;

    ezInt32 iRow = 0;
    for (ezMap<ezString, SlotData>::Iterator it = m_InputSlots.GetIterator(); it.IsValid(); ++it)
    {
      it.Value().m_iTableRow = iRow;

      sTemp.Printf("  %s  ", it.Key().GetData());

      QLabel* pIcon = new QLabel();
      pIcon->setPixmap(ezQtUiServices::GetCachedPixmapResource(":/Icons/Icons/InputSlots.png"));
      pIcon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
      TableInputSlots->setCellWidget(iRow, 0, pIcon);

      TableInputSlots->setCellWidget(iRow, 1, new QLabel(sTemp.GetData()));
      TableInputSlots->setCellWidget(iRow, 2, new QLabel("????????????"));
      TableInputSlots->setCellWidget(iRow, 3, new QLabel("????????"));
      TableInputSlots->setCellWidget(iRow, 4, new QLabel("????????"));
      TableInputSlots->setCellWidget(iRow, 5, new QLabel("????????????????"));

      const ezUInt32 uiFlags = it.Value().m_uiSlotFlags;

      // Flags
      {
        ezStringBuilder sFlags;
        sFlags.Printf("  %16b  ", uiFlags);

        QLabel* pFlags = (QLabel*) TableInputSlots->cellWidget(iRow, 5);
        pFlags->setAlignment(Qt::AlignRight);
        pFlags->setText(sFlags.GetData());
      }

      // Flags Tooltip
      {
        // in VS 2012 at least the snprintf fails when "yes" and "no" are passed directly, instead of as const char* variables
        const char* szYes = "<b>yes</b>";
        const char* szNo  = "no";

        ezStringBuilder tt("<p>");
        tt.AppendPrintf("ReportsRelativeValues: %s<br>",      (uiFlags & ezInputSlotFlags::ReportsRelativeValues)     ? szYes : szNo);
        tt.AppendPrintf("ValueBinaryZeroOrOne: %s<br>",       (uiFlags & ezInputSlotFlags::ValueBinaryZeroOrOne)      ? szYes : szNo);
        tt.AppendPrintf("ValueRangeZeroToOne: %s<br>",        (uiFlags & ezInputSlotFlags::ValueRangeZeroToOne)       ? szYes : szNo);
        tt.AppendPrintf("ValueRangeZeroToInf: %s<br>",        (uiFlags & ezInputSlotFlags::ValueRangeZeroToInf)       ? szYes : szNo);
        tt.AppendPrintf("Pressable: %s<br>",                  (uiFlags & ezInputSlotFlags::Pressable)                 ? szYes : szNo);
        tt.AppendPrintf("Holdable: %s<br>",                   (uiFlags & ezInputSlotFlags::Holdable)                  ? szYes : szNo);
        tt.AppendPrintf("HalfAxis: %s<br>",                   (uiFlags & ezInputSlotFlags::HalfAxis)                  ? szYes : szNo);
        tt.AppendPrintf("FullAxis: %s<br>",                   (uiFlags & ezInputSlotFlags::FullAxis)                  ? szYes : szNo);
        tt.AppendPrintf("RequiresDeadZone: %s<br>",           (uiFlags & ezInputSlotFlags::RequiresDeadZone)          ? szYes : szNo);
        tt.AppendPrintf("ValuesAreNonContinuous: %s<br>",     (uiFlags & ezInputSlotFlags::ValuesAreNonContinuous)    ? szYes : szNo);
        tt.AppendPrintf("ActivationDependsOnOthers: %s<br>",  (uiFlags & ezInputSlotFlags::ActivationDependsOnOthers) ? szYes : szNo);
        tt.AppendPrintf("NeverTimeScale: %s<br>",             (uiFlags & ezInputSlotFlags::NeverTimeScale)            ? szYes : szNo);
        tt.Append("</p>");

        TableInputSlots->cellWidget(iRow, 5)->setToolTip(tt.GetData());
      }

      ++iRow;
    }

    TableInputSlots->resizeColumnsToContents();
  }

  {
    ezStringBuilder sTemp;

    ezInt32 iRow = 0;
    for (ezMap<ezString, SlotData>::Iterator it = m_InputSlots.GetIterator(); it.IsValid(); ++it)
    {
      QLabel* pState = (QLabel*) TableInputSlots->cellWidget(iRow, 2);
      pState->setAlignment(Qt::AlignHCenter);

      switch (it.Value().m_KeyState)
      {
      case ezKeyState::Down:
        pState->setText("Down");
        break;
      case ezKeyState::Pressed:
        pState->setText("Pressed");
        break;
      case ezKeyState::Released:
        pState->setText("Released");
        break;
      case ezKeyState::Up:
        pState->setText("");
        break;
      }

      // Value
      {
        QLabel* pValue = (QLabel*) TableInputSlots->cellWidget(iRow, 3);
        pValue->setAlignment(Qt::AlignHCenter);

        if (it.Value().m_fValue == 0.0f)
          pValue->setText("");
        else
        {
          sTemp.Printf(" %.4f ", it.Value().m_fValue);
          pValue->setText(sTemp.GetData());
        }
      }

      // Dead-zone
      {
        QLabel* pDeadZone = (QLabel*) TableInputSlots->cellWidget(iRow, 4);
        pDeadZone->setAlignment(Qt::AlignHCenter);

        if (it.Value().m_fDeadZone == 0.0f)
          pDeadZone->setText("");
        else
          pDeadZone->setText(QString::number(it.Value().m_fDeadZone, 'f', 2));
      }

      ++iRow;
    }
  }

  TableInputSlots->blockSignals(false);
}

void ezQtInputWidget::UpdateActionTable(bool bRecreate)
{
  TableInputActions->blockSignals(true);

  if (bRecreate)
  {
    TableInputActions->clear();
    TableInputActions->setRowCount(m_InputActions.GetCount());

    QStringList Headers;
    Headers.append("");
    Headers.append(" Action ");
    Headers.append(" State ");
    Headers.append(" Value ");

    for (ezInt32 slot = 0; slot < ezInputActionConfig::MaxInputSlotAlternatives; ++slot)
      Headers.append(QString(" Slot %1 ").arg(slot + 1));

    TableInputActions->setColumnCount(Headers.size());

    TableInputActions->setHorizontalHeaderLabels(Headers);
    TableInputActions->horizontalHeader()->show();

    ezStringBuilder sTemp;

    ezInt32 iRow = 0;
    for (ezMap<ezString, ActionData>::Iterator it = m_InputActions.GetIterator(); it.IsValid(); ++it)
    {
      it.Value().m_iTableRow = iRow;

      sTemp.Printf("  %s  ", it.Key().GetData());

      QLabel* pIcon = new QLabel();
      pIcon->setPixmap(ezQtUiServices::GetCachedPixmapResource(":/Icons/Icons/InputActions.png"));
      pIcon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
      TableInputActions->setCellWidget(iRow, 0, pIcon);

      TableInputActions->setCellWidget(iRow, 1, new QLabel(sTemp.GetData()));
      TableInputActions->setCellWidget(iRow, 2, new QLabel("????????????"));
      TableInputActions->setCellWidget(iRow, 3, new QLabel("????????????????????????"));
      TableInputActions->setCellWidget(iRow, 4, new QLabel(""));
      TableInputActions->setCellWidget(iRow, 5, new QLabel(""));
      TableInputActions->setCellWidget(iRow, 6, new QLabel(""));

      // Trigger Slots

      for (ezInt32 slot = 0; slot < ezInputActionConfig::MaxInputSlotAlternatives; ++slot)
      {
        if (it.Value().m_sTrigger[slot].IsEmpty())
          sTemp = "  ";
        else
          sTemp.Printf("  [Scale: %.2f] %s  ", it.Value().m_fTriggerScaling[slot], it.Value().m_sTrigger[slot].GetData());

        QLabel* pValue = (QLabel*) TableInputActions->cellWidget(iRow, 4 + slot);
        pValue->setText(sTemp.GetData());
      }

      ++iRow;
    }

    TableInputActions->resizeColumnsToContents();
  }

  {
    ezStringBuilder sTemp;

    ezInt32 iRow = 0;
    for (ezMap<ezString, ActionData>::Iterator it = m_InputActions.GetIterator(); it.IsValid(); ++it)
    {
      QLabel* pState = (QLabel*) TableInputActions->cellWidget(iRow, 2);
      pState->setAlignment(Qt::AlignHCenter);

      switch (it.Value().m_KeyState)
      {
      case ezKeyState::Down:
        pState->setText("Down");
        break;
      case ezKeyState::Pressed:
        pState->setText("Pressed");
        break;
      case ezKeyState::Released:
        pState->setText("Released");
        break;
      case ezKeyState::Up:
        pState->setText("");
        break;
      }

      // Value
      {
        QLabel* pValue = (QLabel*) TableInputActions->cellWidget(iRow, 3);
        pValue->setAlignment(Qt::AlignHCenter);

        if (it.Value().m_fValue == 0.0f)
          pValue->setText("");
        else
        {
          if (it.Value().m_bUseTimeScaling)
            sTemp.Printf(" %.4f (Time-Scaled) ", it.Value().m_fValue);
          else
            sTemp.Printf(" %.4f (Absolute) ", it.Value().m_fValue);

          pValue->setText(sTemp.GetData());
        }
      }


      ++iRow;
    }
  }

  TableInputActions->blockSignals(false);
}

void ezQtInputWidget::on_ButtonClearSlots_clicked()
{
  ClearSlots();
}

void ezQtInputWidget::on_ButtonClearActions_clicked()
{
  ClearActions();
}

