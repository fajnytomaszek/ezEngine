#pragma once

#include <Foundation/Basics.h>
#include <Foundation/Time/Time.h>
#include <Foundation/Containers/Deque.h>
#include <Foundation/Containers/Map.h>
#include <Foundation/Strings/String.h>
#include <Inspector/ui_TimeWidget.h>
#include <QGraphicsView>
#include <QListWidgetItem>
#include <ads/DockWidget.h>

class ezQtTimeWidget : public ads::CDockWidget, public Ui_TimeWidget
{
public:
  Q_OBJECT

public:
  static const ezUInt8 s_uiMaxColors = 9;

  ezQtTimeWidget(QWidget* parent = 0);

  static ezQtTimeWidget* s_pWidget;

private Q_SLOTS:

  void on_ListClocks_itemChanged(QListWidgetItem* item);
  void on_ComboTimeframe_currentIndexChanged(int index);

public:
  static void ProcessTelemetry(void* pUnuseed);

  void ResetStats();
  void UpdateStats();

private:

  QGraphicsPathItem* m_pPath[s_uiMaxColors];
  QGraphicsPathItem* m_pPathMax;
  QGraphicsScene m_Scene;

  ezUInt32 m_uiMaxSamples;

  ezUInt8 m_uiColorsUsed;
  bool m_bClocksChanged;

  ezTime m_MaxGlobalTime;
  ezTime m_DisplayInterval;
  ezTime m_LastUpdatedClockList;

  struct TimeSample
  {
    ezTime m_AtGlobalTime;
    ezTime m_Timestep;
  };

  struct ClockData
  {
    ezDeque<TimeSample> m_TimeSamples;
  
    bool m_bDisplay;
    ezInt8 m_iColor;
    ezTime m_MinTimestep;
    ezTime m_MaxTimestep;
    QListWidgetItem* m_pListItem;

    ClockData()
    {
      m_bDisplay = true;
      m_iColor = -1;
      m_MinTimestep = ezTime::Seconds(60.0);
      m_pListItem = nullptr;
    }
  };

  ezMap<ezString, ClockData> m_ClockData;
};


