// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QWidget>

#include <memory>

namespace mx::gui {

class ConfigManager;

class AgentDashboardWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~AgentDashboardWidget(void);

  explicit AgentDashboardWidget(QWidget *parent = nullptr);

  //! Reload all dashboard data for the given session.
  void refresh(int64_t session_id, ConfigManager &config);

  //! Set tool descriptions (name → description) for tooltips.
  void setToolDescriptions(const QHash<QString, QString> &descriptions);
};

// Custom widget: cumulative cost over time as a line chart.
class CostTimelineWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

 public:
  explicit CostTimelineWidget(QWidget *parent = nullptr);

  void set_data(const QVector<QPointF> &points);
  void setCumulative(bool cumulative);
  bool isCumulative(void) const;
  QSize sizeHint(void) const override;

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;

 private:
  QVector<QPointF> m_points;  // (seconds_since_start, cumulative_cost)
  int m_hover_index{-1};
  bool m_cumulative{true};
};

// Custom widget: horizontal bar chart of tool call frequency.
class ToolBarsWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

 public:
  struct ToolBar {
    QString name;
    QString description;
    int calls{0};
    int avg_ms{0};
    double cost{0.0};
  };

  explicit ToolBarsWidget(QWidget *parent = nullptr);

  void set_data(const QVector<ToolBar> &bars);
  QSize sizeHint(void) const override;

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

 private:
  QVector<ToolBar> m_bars;
};

// Custom widget: sparkline heatmap showing when each tool was called.
class ToolSparklineWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

 public:
  struct SparkData {
    QString tool_name;
    QVector<int> buckets;  // ~20 time buckets, each = call count.
  };

  explicit ToolSparklineWidget(QWidget *parent = nullptr);

  void set_data(const QVector<SparkData> &data);
  QSize sizeHint(void) const override;

 protected:
  void paintEvent(QPaintEvent *event) override;

 private:
  QVector<SparkData> m_data;
};

}  // namespace mx::gui
