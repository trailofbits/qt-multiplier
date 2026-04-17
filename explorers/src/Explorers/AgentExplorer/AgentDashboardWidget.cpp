// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentDashboardWidget.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include <multiplier/GUI/Managers/ConfigManager.h>

namespace mx::gui {
namespace {

// Latency thresholds for bar chart coloring.
static constexpr int kFastMs = 500;
static constexpr int kMediumMs = 2000;

static constexpr int kMarginLeft = 60;
static constexpr int kMarginRight = 20;
static constexpr int kMarginTop = 20;
static constexpr int kMarginBottom = 30;

static constexpr int kChartHeight = 180;
static constexpr int kBarRowHeight = 28;
static constexpr int kSparkRowHeight = 24;
static constexpr int kSparkBuckets = 20;

static QColor latency_color(int avg_ms) {
  if (avg_ms <= kFastMs) {
    return QColor(0x22, 0xC5, 0x5E);   // Green.
  } else if (avg_ms <= kMediumMs) {
    return QColor(0xF5, 0x9E, 0x0B);   // Amber.
  }
  return QColor(0xEF, 0x44, 0x44);     // Red.
}

static QFrame *create_summary_card(const QString &label, const QString &value,
                                   QWidget *parent) {
  auto *frame = new QFrame(parent);
  frame->setFrameShape(QFrame::StyledPanel);
  frame->setMinimumWidth(120);

  auto *layout = new QVBoxLayout(frame);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(2);

  auto *val_label = new QLabel(value, frame);
  auto font = val_label->font();
  font.setPointSize(font.pointSize() + 6);
  font.setBold(true);
  val_label->setFont(font);
  val_label->setAlignment(Qt::AlignCenter);

  auto *desc_label = new QLabel(label, frame);
  desc_label->setAlignment(Qt::AlignCenter);
  auto small_font = desc_label->font();
  small_font.setPointSize(small_font.pointSize() - 1);
  desc_label->setFont(small_font);

  layout->addWidget(val_label);
  layout->addWidget(desc_label);

  return frame;
}

}  // namespace

// ---------------------------------------------------------------------------
// AgentDashboardWidget
// ---------------------------------------------------------------------------

struct AgentDashboardWidget::PrivateData {
  QLabel *title_label{nullptr};
  QHBoxLayout *summary_layout{nullptr};
  CostTimelineWidget *cost_timeline{nullptr};
  ToolBarsWidget *tool_bars{nullptr};
  ToolSparklineWidget *tool_sparklines{nullptr};

  // Summary card labels (updated on refresh).
  QLabel *total_cost_val{nullptr};
  QLabel *llm_calls_val{nullptr};
  QLabel *tool_calls_val{nullptr};
  QLabel *duration_val{nullptr};
  QLabel *avg_cost_val{nullptr};
};

AgentDashboardWidget::~AgentDashboardWidget(void) {}

AgentDashboardWidget::AgentDashboardWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *container = new QWidget(scroll);
  auto *root_layout = new QVBoxLayout(container);
  root_layout->setContentsMargins(8, 8, 8, 8);
  root_layout->setSpacing(12);

  // Title.
  d->title_label = new QLabel(tr("Agent Dashboard"), container);
  auto title_font = d->title_label->font();
  title_font.setPointSize(title_font.pointSize() + 2);
  title_font.setBold(true);
  d->title_label->setFont(title_font);
  root_layout->addWidget(d->title_label);

  // Summary cards row.
  auto *summary_widget = new QWidget(container);
  d->summary_layout = new QHBoxLayout(summary_widget);
  d->summary_layout->setContentsMargins(0, 0, 0, 0);
  d->summary_layout->setSpacing(8);

  auto add_card = [&](const QString &label) -> QFrame * {
    auto *card = create_summary_card(label, QStringLiteral("--"), summary_widget);
    d->summary_layout->addWidget(card);
    return card;
  };

  auto *cost_card = add_card(tr("Total Cost"));
  d->total_cost_val = cost_card->findChild<QLabel *>();

  auto *llm_card = add_card(tr("LLM Calls"));
  d->llm_calls_val = llm_card->findChild<QLabel *>();

  auto *tool_card = add_card(tr("Tool Calls"));
  d->tool_calls_val = tool_card->findChild<QLabel *>();

  auto *dur_card = add_card(tr("Duration"));
  d->duration_val = dur_card->findChild<QLabel *>();

  auto *avg_card = add_card(tr("Avg Cost/Call"));
  d->avg_cost_val = avg_card->findChild<QLabel *>();

  d->summary_layout->addStretch();
  root_layout->addWidget(summary_widget);

  // Section: Cost timeline.
  auto *cost_label = new QLabel(tr("Cumulative Cost Over Time"), container);
  auto section_font = cost_label->font();
  section_font.setBold(true);
  cost_label->setFont(section_font);
  root_layout->addWidget(cost_label);

  d->cost_timeline = new CostTimelineWidget(container);
  root_layout->addWidget(d->cost_timeline);

  // Section: Tool usage bars.
  auto *bars_label = new QLabel(tr("Tool Call Frequency"), container);
  bars_label->setFont(section_font);
  root_layout->addWidget(bars_label);

  d->tool_bars = new ToolBarsWidget(container);
  root_layout->addWidget(d->tool_bars);

  // Section: Tool sparklines.
  auto *spark_label = new QLabel(tr("Tool Usage Timeline"), container);
  spark_label->setFont(section_font);
  root_layout->addWidget(spark_label);

  d->tool_sparklines = new ToolSparklineWidget(container);
  root_layout->addWidget(d->tool_sparklines);

  root_layout->addStretch();

  scroll->setWidget(container);

  auto *outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);
  outer_layout->addWidget(scroll);
}

void AgentDashboardWidget::refresh(int64_t session_id,
                                   ConfigManager &config) {
  if (session_id < 0) {
    return;
  }

  // Load aggregated data.
  auto summary = config.LoadCostSummary(session_id);
  auto tool_stats = config.LoadToolStatistics(session_id);
  auto cost_nodes = config.LoadCostNodes(session_id);

  // --- Summary cards ---
  d->total_cost_val->setText(
      QStringLiteral("$%1").arg(summary.total_cost_usd, 0, 'f', 4));
  d->llm_calls_val->setText(QString::number(summary.llm_call_count));
  d->tool_calls_val->setText(QString::number(summary.tool_call_count));

  // Duration.
  int secs = summary.total_duration_ms / 1000;
  int hrs = secs / 3600;
  int mins = (secs % 3600) / 60;
  int s = secs % 60;
  d->duration_val->setText(
      QStringLiteral("%1:%2:%3")
          .arg(hrs, 2, 10, QLatin1Char('0'))
          .arg(mins, 2, 10, QLatin1Char('0'))
          .arg(s, 2, 10, QLatin1Char('0')));

  double avg_cost = summary.llm_call_count > 0
      ? summary.total_cost_usd / summary.llm_call_count
      : 0.0;
  d->avg_cost_val->setText(
      QStringLiteral("$%1").arg(avg_cost, 0, 'f', 4));

  // --- Cost timeline ---
  // Build cumulative cost points from cost nodes sorted by start time.
  // Only include LLM calls (node_type == "llm_call") for the timeline.
  struct NodeEntry {
    double seconds;
    double cost;
  };
  QVector<NodeEntry> entries;

  QDateTime session_start;
  for (const auto &node : cost_nodes) {
    if (!node.started_at.isEmpty()) {
      auto dt = QDateTime::fromString(node.started_at, Qt::ISODate);
      if (dt.isValid() && (!session_start.isValid() || dt < session_start)) {
        session_start = dt;
      }
    }
  }

  if (session_start.isValid()) {
    for (const auto &node : cost_nodes) {
      if (node.node_type != QStringLiteral("llm_call")) {
        continue;
      }
      if (node.completed_at.isEmpty()) {
        continue;
      }
      auto dt = QDateTime::fromString(node.completed_at, Qt::ISODate);
      if (!dt.isValid()) {
        continue;
      }
      double sec = session_start.msecsTo(dt) / 1000.0;
      entries.push_back({sec, node.cost_usd});
    }

    std::sort(entries.begin(), entries.end(),
              [](const NodeEntry &a, const NodeEntry &b) {
                return a.seconds < b.seconds;
              });

    QVector<QPointF> points;
    double cumulative = 0.0;
    points.push_back(QPointF(0.0, 0.0));
    for (const auto &e : entries) {
      cumulative += e.cost;
      points.push_back(QPointF(e.seconds, cumulative));
    }
    d->cost_timeline->set_data(points);
  } else {
    d->cost_timeline->set_data({});
  }

  // --- Tool bars ---
  QVector<ToolBarsWidget::ToolBar> bars;
  for (const auto &ts : tool_stats) {
    ToolBarsWidget::ToolBar bar;
    bar.name = ts.tool_name;
    bar.calls = ts.call_count;
    bar.avg_ms = ts.avg_duration_ms;
    bar.cost = ts.total_cost_usd;
    bars.push_back(bar);
  }
  // Sort by call count descending.
  std::sort(bars.begin(), bars.end(),
            [](const ToolBarsWidget::ToolBar &a,
               const ToolBarsWidget::ToolBar &b) {
              return a.calls > b.calls;
            });
  d->tool_bars->set_data(bars);

  // --- Tool sparklines ---
  // Build per-tool time buckets from cost nodes.
  QVector<ToolSparklineWidget::SparkData> spark_data;

  if (session_start.isValid()) {
    // Find session end.
    double max_seconds = 1.0;
    for (const auto &node : cost_nodes) {
      if (!node.completed_at.isEmpty()) {
        auto dt = QDateTime::fromString(node.completed_at, Qt::ISODate);
        if (dt.isValid()) {
          double sec = session_start.msecsTo(dt) / 1000.0;
          if (sec > max_seconds) {
            max_seconds = sec;
          }
        }
      }
    }

    double bucket_width = max_seconds / kSparkBuckets;

    // Collect tool names preserving order from tool_stats.
    QHash<QString, int> tool_index;
    for (const auto &ts : tool_stats) {
      int idx = static_cast<int>(spark_data.size());
      tool_index[ts.tool_name] = idx;
      ToolSparklineWidget::SparkData sd;
      sd.tool_name = ts.tool_name;
      sd.buckets.resize(kSparkBuckets, 0);
      spark_data.push_back(sd);
    }

    for (const auto &node : cost_nodes) {
      if (node.tool_name.isEmpty()) {
        continue;
      }
      auto it = tool_index.find(node.tool_name);
      if (it == tool_index.end()) {
        continue;
      }
      QString ts_str = node.started_at.isEmpty() ? node.completed_at
                                                   : node.started_at;
      if (ts_str.isEmpty()) {
        continue;
      }
      auto dt = QDateTime::fromString(ts_str, Qt::ISODate);
      if (!dt.isValid()) {
        continue;
      }
      double sec = session_start.msecsTo(dt) / 1000.0;
      int bucket = static_cast<int>(sec / bucket_width);
      if (bucket >= kSparkBuckets) {
        bucket = kSparkBuckets - 1;
      }
      if (bucket < 0) {
        bucket = 0;
      }
      spark_data[it.value()].buckets[bucket]++;
    }
  }
  d->tool_sparklines->set_data(spark_data);
}

// ---------------------------------------------------------------------------
// CostTimelineWidget
// ---------------------------------------------------------------------------

CostTimelineWidget::CostTimelineWidget(QWidget *parent)
    : QWidget(parent) {
  setMinimumHeight(kChartHeight + kMarginTop + kMarginBottom);
  setMouseTracking(true);
}

void CostTimelineWidget::set_data(const QVector<QPointF> &points) {
  m_points = points;
  m_hover_index = -1;
  update();
}

QSize CostTimelineWidget::sizeHint(void) const {
  return QSize(400, kChartHeight + kMarginTop + kMarginBottom);
}

void CostTimelineWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  auto bg = palette().color(QPalette::Base);
  auto fg = palette().color(QPalette::Text);
  auto grid_color = palette().color(QPalette::Mid);

  p.fillRect(rect(), bg);

  int chart_w = width() - kMarginLeft - kMarginRight;
  int chart_h = kChartHeight;

  if (m_points.size() < 2 || chart_w <= 0) {
    p.setPen(fg);
    p.drawText(rect(), Qt::AlignCenter, tr("No cost data"));
    return;
  }

  double max_time = m_points.last().x();
  double max_cost = m_points.last().y();
  if (max_time <= 0.0) {
    max_time = 1.0;
  }
  if (max_cost <= 0.0) {
    max_cost = 0.01;
  }

  // Add 10% headroom.
  max_cost *= 1.1;

  auto to_screen = [&](const QPointF &pt) -> QPointF {
    double x = kMarginLeft + (pt.x() / max_time) * chart_w;
    double y = kMarginTop + chart_h - (pt.y() / max_cost) * chart_h;
    return QPointF(x, y);
  };

  // Grid lines (4 horizontal).
  p.setPen(QPen(grid_color, 1, Qt::DotLine));
  for (int i = 1; i <= 4; ++i) {
    double y_val = (max_cost / 4.0) * i;
    double y = kMarginTop + chart_h - (y_val / max_cost) * chart_h;
    p.drawLine(QPointF(kMarginLeft, y),
               QPointF(kMarginLeft + chart_w, y));

    // Y-axis label.
    p.setPen(fg);
    p.drawText(QRectF(0, y - 8, kMarginLeft - 4, 16),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("$%1").arg(y_val, 0, 'f', 3));
    p.setPen(QPen(grid_color, 1, Qt::DotLine));
  }

  // Axes.
  p.setPen(QPen(fg, 1));
  p.drawLine(kMarginLeft, kMarginTop,
             kMarginLeft, kMarginTop + chart_h);
  p.drawLine(kMarginLeft, kMarginTop + chart_h,
             kMarginLeft + chart_w, kMarginTop + chart_h);

  // X-axis labels (5 ticks).
  for (int i = 0; i <= 4; ++i) {
    double t = (max_time / 4.0) * i;
    double x = kMarginLeft + (t / max_time) * chart_w;

    int total_secs = static_cast<int>(t);
    int mins = total_secs / 60;
    int secs = total_secs % 60;
    auto label = QStringLiteral("%1:%2")
                     .arg(mins)
                     .arg(secs, 2, 10, QLatin1Char('0'));
    p.drawText(QRectF(x - 25, kMarginTop + chart_h + 4, 50, 20),
               Qt::AlignCenter, label);
  }

  // Draw the line.
  QPainterPath path;
  for (int i = 0; i < m_points.size(); ++i) {
    auto sp = to_screen(m_points[i]);
    if (i == 0) {
      path.moveTo(sp);
    } else {
      path.lineTo(sp);
    }
  }
  p.setPen(QPen(QColor(0x3B, 0x82, 0xF6), 2));
  p.setBrush(Qt::NoBrush);
  p.drawPath(path);

  // Draw dots at each data point.
  for (int i = 0; i < m_points.size(); ++i) {
    auto sp = to_screen(m_points[i]);
    bool hovered = (i == m_hover_index);
    double radius = hovered ? 5.0 : 3.0;
    p.setBrush(hovered ? QColor(0xFB, 0xBF, 0x24) : QColor(0x3B, 0x82, 0xF6));
    p.setPen(Qt::NoPen);
    p.drawEllipse(sp, radius, radius);
  }

  // Hover tooltip is handled in mouseMoveEvent via QToolTip.
}

void CostTimelineWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_points.size() < 2) {
    return;
  }

  double max_time = m_points.last().x();
  double max_cost = m_points.last().y() * 1.1;
  if (max_time <= 0.0) max_time = 1.0;
  if (max_cost <= 0.0) max_cost = 0.01;

  int chart_w = width() - kMarginLeft - kMarginRight;

  auto pos = event->pos();
  int best = -1;
  double best_dist = 20.0;  // Max pixel distance for hover.

  for (int i = 0; i < m_points.size(); ++i) {
    double x = kMarginLeft + (m_points[i].x() / max_time) * chart_w;
    double y = kMarginTop + kChartHeight
               - (m_points[i].y() / max_cost) * kChartHeight;
    double dx = pos.x() - x;
    double dy = pos.y() - y;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < best_dist) {
      best_dist = dist;
      best = i;
    }
  }

  if (best != m_hover_index) {
    m_hover_index = best;
    update();
  }

  if (best >= 0) {
    auto &pt = m_points[best];
    int secs = static_cast<int>(pt.x());
    auto tip = QStringLiteral("Time: %1:%2  Cost: $%3")
                   .arg(secs / 60)
                   .arg(secs % 60, 2, 10, QLatin1Char('0'))
                   .arg(pt.y(), 0, 'f', 4);
    QToolTip::showText(event->globalPosition().toPoint(), tip, this);
  } else {
    QToolTip::hideText();
  }
}

void CostTimelineWidget::leaveEvent(QEvent *) {
  if (m_hover_index >= 0) {
    m_hover_index = -1;
    update();
  }
  QToolTip::hideText();
}

// ---------------------------------------------------------------------------
// ToolBarsWidget
// ---------------------------------------------------------------------------

ToolBarsWidget::ToolBarsWidget(QWidget *parent)
    : QWidget(parent) {
  setMinimumHeight(kBarRowHeight * 2);
}

void ToolBarsWidget::set_data(const QVector<ToolBar> &bars) {
  m_bars = bars;
  setMinimumHeight(std::max(kBarRowHeight * 2,
                            kBarRowHeight * static_cast<int>(m_bars.size())));
  updateGeometry();
  update();
}

QSize ToolBarsWidget::sizeHint(void) const {
  int h = std::max(kBarRowHeight * 2,
                   kBarRowHeight * static_cast<int>(m_bars.size()));
  return QSize(400, h);
}

void ToolBarsWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  auto bg = palette().color(QPalette::Base);
  auto fg = palette().color(QPalette::Text);

  p.fillRect(rect(), bg);

  if (m_bars.isEmpty()) {
    p.setPen(fg);
    p.drawText(rect(), Qt::AlignCenter, tr("No tool data"));
    return;
  }

  int max_calls = 1;
  for (const auto &bar : m_bars) {
    if (bar.calls > max_calls) {
      max_calls = bar.calls;
    }
  }

  int label_w = 160;
  int bar_area_w = width() - label_w - 20;
  if (bar_area_w < 50) {
    bar_area_w = 50;
  }

  for (int i = 0; i < m_bars.size(); ++i) {
    const auto &bar = m_bars[i];
    int y = i * kBarRowHeight;

    // Label.
    p.setPen(fg);
    auto label = QStringLiteral("%1: %2 calls (avg %3ms)")
                     .arg(bar.name)
                     .arg(bar.calls)
                     .arg(bar.avg_ms);
    p.drawText(QRectF(4, y + 2, label_w - 8, kBarRowHeight - 4),
               Qt::AlignLeft | Qt::AlignVCenter, label);

    // Bar.
    int bar_w = static_cast<int>(
        (static_cast<double>(bar.calls) / max_calls) * bar_area_w);
    if (bar_w < 4) {
      bar_w = 4;
    }

    auto color = latency_color(bar.avg_ms);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(label_w, y + 4, bar_w, kBarRowHeight - 8),
                      3, 3);

    // Cost label at end of bar.
    if (bar.cost > 0.0) {
      p.setPen(fg);
      auto cost_str = QStringLiteral("$%1").arg(bar.cost, 0, 'f', 4);
      p.drawText(QRectF(label_w + bar_w + 4, y + 2,
                         80, kBarRowHeight - 4),
                 Qt::AlignLeft | Qt::AlignVCenter, cost_str);
    }
  }
}

// ---------------------------------------------------------------------------
// ToolSparklineWidget
// ---------------------------------------------------------------------------

ToolSparklineWidget::ToolSparklineWidget(QWidget *parent)
    : QWidget(parent) {
  setMinimumHeight(kSparkRowHeight * 2);
}

void ToolSparklineWidget::set_data(const QVector<SparkData> &data) {
  m_data = data;
  setMinimumHeight(std::max(kSparkRowHeight * 2,
                            kSparkRowHeight * static_cast<int>(m_data.size())));
  updateGeometry();
  update();
}

QSize ToolSparklineWidget::sizeHint(void) const {
  int h = std::max(kSparkRowHeight * 2,
                   kSparkRowHeight * static_cast<int>(m_data.size()));
  return QSize(400, h);
}

void ToolSparklineWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  auto bg = palette().color(QPalette::Base);
  auto fg = palette().color(QPalette::Text);

  p.fillRect(rect(), bg);

  if (m_data.isEmpty()) {
    p.setPen(fg);
    p.drawText(rect(), Qt::AlignCenter, tr("No timeline data"));
    return;
  }

  // Find the global max bucket value for intensity scaling.
  int global_max = 1;
  for (const auto &sd : m_data) {
    for (int v : sd.buckets) {
      if (v > global_max) {
        global_max = v;
      }
    }
  }

  int label_w = 140;
  int spark_area_w = width() - label_w - 10;
  if (spark_area_w < 50) {
    spark_area_w = 50;
  }
  int cell_w = spark_area_w / kSparkBuckets;
  if (cell_w < 4) {
    cell_w = 4;
  }

  auto accent = QColor(0x3B, 0x82, 0xF6);

  for (int i = 0; i < m_data.size(); ++i) {
    const auto &sd = m_data[i];
    int y = i * kSparkRowHeight;

    // Label.
    p.setPen(fg);
    p.drawText(QRectF(4, y + 2, label_w - 8, kSparkRowHeight - 4),
               Qt::AlignLeft | Qt::AlignVCenter, sd.tool_name);

    // Sparkline cells.
    for (int b = 0; b < sd.buckets.size() && b < kSparkBuckets; ++b) {
      int val = sd.buckets[b];
      if (val <= 0) {
        continue;
      }
      double intensity = static_cast<double>(val) / global_max;
      int alpha = static_cast<int>(40 + intensity * 215);
      if (alpha > 255) {
        alpha = 255;
      }

      auto cell_color = accent;
      cell_color.setAlpha(alpha);

      int cx = label_w + b * cell_w;
      p.setBrush(cell_color);
      p.setPen(Qt::NoPen);
      p.drawRect(cx, y + 3, cell_w - 1, kSparkRowHeight - 6);
    }
  }
}

}  // namespace mx::gui
