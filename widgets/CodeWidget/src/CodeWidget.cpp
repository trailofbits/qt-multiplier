/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/CodeWidget.h>

#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSpacerItem>
#include <QVBoxLayout>

#include <cmath>
#include <multiplier/Frontend/MacroSubstitution.h>
#include <multiplier/Frontend/MacroVAOpt.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

// #include "CodeModel.h"

namespace mx::gui {

// TODO(pag): Don't hardcode this. Investigate `QStackTextEngine`, the
//            `QPainter` uses this internally. It seems that
//            `QPainter::boundingRect` can take a `QTextOption` that can be
//            configured with tab info.
static constexpr auto kTabWidth = 4u;

struct Entity {

  // Beginning `(x, y)` positions of where some text data exists in the scene.
  // The origin `(0, 0)` represents the top-left of the viewport. The unit size
  // of the `x` coordinate system is in terms of the font width, and the unit
  // size of the `y` coordinate system is in terms of the font height.
  //
  // NOTE(pag): These are overwritten by the painter.
  qreal x;
  qreal y;

  // Index of this entity's data in `Scene::token_data`.
  unsigned data_index;

  // Index of htis entity's token in `Scene::tokens`.
  unsigned token_index;

  // The logical (one-indexed) line number of this token.
  unsigned logical_line_number;

  // The logical (one-indexed) column number of this token.
  unsigned logical_column_number;

  // inline auto operator<=>(const Entity &that) const noexcept {
  //   return position <=> that.position;
  // }

  // bool operator==(const Entity &that) const noexcept = default;
};

// struct DecisionRange {
//   std::pair<unsigned, unsigned> begin_position;
//   std::pair<unsigned, unsigned> end_position;
//   std::pair<unsigned, unsigned> token_range;

//   // An entity ID, such as a fragment, or a 
//   RawEntityId decision_id{kInvalidEntityId};

//   // Indexes into `Scene::tokens` on the range of 
//   unsigned begin;
//   unsigned end;
// };

struct Scene {

  // Sorted list of entities in this scene. This is sorted by
  // `(Entity::x, Entity::y)` positions.
  std::vector<Entity> entities;

  // A linear representation of the token data. If a token spans multiple lines
  // then it is split into multiple entries in `token_data`. If a token only
  // contributes pure whitespace then it is not included in `token_data`.
  std::vector<QString> token_data;

  // The underlying tokens.
  std::vector<Token> tokens;

  // Maximum number of characters on any given line.
  unsigned max_logical_columns{1u};

  // Number of logical lines in this scene.
  unsigned num_lines{1u};

  void AddEntity(unsigned column_number, QString data) {
    auto &e = entities.emplace_back();
    e.logical_line_number = num_lines;
    e.logical_column_number = column_number;
    e.data_index = static_cast<unsigned>(token_data.size());
    e.token_index = static_cast<unsigned>(tokens.size());
    token_data.emplace_back(std::move(data));
  }
};

struct CodeWidget::PrivateData {

  // The theme being used.
  IThemePtr theme;

  // Source of data that we're rendering.
  const TokenTree token_tree;

  // Size of the visible viewport area for this widget.
  QSize viewport_size;

  // Current scroll x/y positions.
  int scroll_x{0};
  int scroll_y{0};

  // Data structure keeping track of the logical things to render, and
  // where to render them. The act of rendering updates `Entity`s in the
  // scene to keep track of their physical locations within `canvas`.
  Scene scene;

  // Actual "picture" of what is rendered.
  QPixmap canvas;

  // Sets of entities that configure what gets shown from `token_tree`.
  QSet<RawEntityId> macros_to_expand;
  QMap<RawEntityId, QString> new_entity_names;
  QSet<RawEntityId> scene_overrides;

  QScrollBar *horizontal_scrollbar{nullptr};
  QScrollBar *vertical_scrollbar{nullptr};

  inline PrivateData(const TokenTree &token_tree_)
      : token_tree(token_tree_) {}

  void UpdateScrollbars(void);
  void ResetScene(void);
  void ResetCanvas(void);
  void ImportChoiceNode(Scene &s, unsigned &logical_column_number,
                        ChoiceTokenTreeNode node);
  void ImportSubstitutionNode(Scene &s, unsigned &logical_column_number,
                              SubstitutionTokenTreeNode node);
  void ImportSequenceNode(Scene &s, unsigned &logical_column_number,
                          SequenceTokenTreeNode node);
  void ImportTokenNode(Scene &s, unsigned &logical_column_number,
                       TokenTokenTreeNode node);
  void ImportNode(Scene &s, unsigned &logical_column_number,
                  TokenTreeNode node);
};

// Import a choice node.
void CodeWidget::PrivateData::ImportChoiceNode(
    Scene &s, unsigned &logical_column_number, ChoiceTokenTreeNode node) {

  std::optional<TokenTreeNode> chosen_node;

  for (auto &item : node.children()) {
    RawEntityId fragment_id = item.first.id().Pack();
    if (scene_overrides.contains(fragment_id) || !chosen_node) {
      chosen_node.reset();
      chosen_node.emplace(std::move(item.second));
    }
  }

  if (chosen_node) {
    ImportNode(s, logical_column_number, std::move(chosen_node.value()));
  }
}

// Import a substitution node.
void CodeWidget::PrivateData::ImportSubstitutionNode(
    Scene &s, unsigned &logical_column_number, SubstitutionTokenTreeNode node) {

  RawEntityId macro_id = kInvalidEntityId;
  auto sub = node.macro();
  if (std::holds_alternative<MacroSubstitution>(sub)) {
    macro_id = std::get<MacroSubstitution>(sub).id().Pack();
  } else {
    macro_id = std::get<MacroVAOpt>(sub).id().Pack();
  }

  if (macros_to_expand.contains(macro_id)) {
    ImportNode(s, logical_column_number, node.after());
  } else {
    ImportNode(s, logical_column_number, node.before());
  }
}

// Import a sequence of nodes.
void CodeWidget::PrivateData::ImportSequenceNode(
    Scene &s, unsigned &logical_column_number, SequenceTokenTreeNode node) {
  for (auto child_node : node.children()) {
    ImportNode(s, logical_column_number, std::move(child_node));
  }
}

// Import a node containing a token.
void CodeWidget::PrivateData::ImportTokenNode(
    Scene &s, unsigned &logical_column_number, TokenTokenTreeNode node) {

  // Get the data of this token in Qt's native format.
  Token token = node.token();
  std::string_view utf8_data = token.data();
  if (utf8_data.empty()) {
    return;
  }

  QString utf16_data = QString::fromUtf8(
      utf8_data.data(), static_cast<qsizetype>(utf8_data.size()));

  // Support entity renaming.
  RawEntityId related_entity_id = token.related_entity_id().Pack();
  if (related_entity_id != kInvalidEntityId) {
    if (auto rename_it = new_entity_names.find(related_entity_id);
        rename_it != new_entity_names.end()) {
      utf16_data = rename_it.value();
    }
  }

  // The new name is empty. That's weird.
  if (utf16_data.isEmpty()) {
    Q_ASSERT(false);
    return;
  }

  QString data;
  bool seen_non_whitespace = false;

  auto start_x = logical_column_number;

  auto add_entity = [&] (void) {
    if (!data.isEmpty()) {
      s.max_logical_columns = std::max(
          s.max_logical_columns, start_x + static_cast<unsigned>(data.size()));
      s.AddEntity(start_x, std::move(data));  // Reads `s.num_lines`.
      data.clear();
      seen_non_whitespace = true;
    }
    start_x = logical_column_number;
  };

  for (QChar ch : utf16_data) {
    switch (ch.unicode()) {

      // TODO(pag): Configurable tab width; tab stops
      case QChar::Tabulation:
        logical_column_number += kTabWidth;
        add_entity();
        break;

      case QChar::Space:
      case QChar::Nbsp:
        logical_column_number += 1;
        add_entity();
        break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator: {
        logical_column_number = 1;
        add_entity();
        s.num_lines += 1u;
        break;
      }

      case QChar::CarriageReturn:
        add_entity();
        continue;

      default:
        logical_column_number += 1;
        data.append(ch);
        break;
    }
  }

  add_entity();

  if (seen_non_whitespace) {
    s.tokens.emplace_back(std::move(token));
  }
}

// Import a generic node, dispatching to the relevant node.
void CodeWidget::PrivateData::ImportNode(
    Scene &s, unsigned &logical_column_number, TokenTreeNode node) {
  switch (node.kind()) {
    case TokenTreeNodeKind::EMPTY:
      break; 
    case TokenTreeNodeKind::TOKEN:
      ImportTokenNode(
          s, logical_column_number,
          std::move(reinterpret_cast<TokenTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::CHOICE:
      ImportChoiceNode(
          s, logical_column_number,
          std::move(reinterpret_cast<ChoiceTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SUBSTITUTION:
      ImportSubstitutionNode(
          s, logical_column_number,
          std::move(reinterpret_cast<SubstitutionTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SEQUENCE:
      ImportSequenceNode(
          s, logical_column_number,
          std::move(reinterpret_cast<SequenceTokenTreeNode &&>(node)));
      break;
  }
}

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(const ConfigManager &config_manager,
                       const TokenTree &token_tree,
                       QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(token_tree)) {

  // d->vertical_scrollbar = new QScrollBar(Qt::Vertical, this);
  // d->vertical_scrollbar->setSingleStep(1);
  // connect(d->vertical_scrollbar, &QScrollBar::valueChanged, this,
  //         &CodeWidget::OnVerticalScroll);

  // d->horizontal_scrollbar = new QScrollBar(Qt::Horizontal, this);
  // d->horizontal_scrollbar->setSingleStep(1);
  // connect(d->horizontal_scrollbar, &QScrollBar::valueChanged, this,
  //         &CodeWidget::OnHorizontalScroll);

  // auto vertical_layout = new QVBoxLayout(this);
  // vertical_layout->setContentsMargins(0, 0, 0, 0);
  // vertical_layout->setSpacing(0);
  // vertical_layout->addSpacerItem(
  //     new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
  // vertical_layout->addStretch();
  // vertical_layout->addWidget(d->horizontal_scrollbar);
  
  // auto horizontal_layout = new QHBoxLayout();
  // horizontal_layout->setContentsMargins(0, 0, 0, 0);
  // horizontal_layout->setSpacing(0);
  // horizontal_layout->addLayout(vertical_layout);
  // horizontal_layout->addWidget(d->vertical_scrollbar);

  // setLayout(horizontal_layout);

  setContentsMargins(0, 0, 0, 0);

  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto &media_manager = config_manager.MediaManager();
  auto &theme_manager = config_manager.ThemeManager();

  OnThemeChanged(theme_manager);  // Calls `ResetScene`.
  OnIconsChanged(media_manager);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &CodeWidget::OnIndexChanged);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &CodeWidget::OnThemeChanged);

  connect(&media_manager, &MediaManager::IconsChanged,
          this, &CodeWidget::OnIconsChanged);
}

void CodeWidget::resizeEvent(QResizeEvent *event) {
  d->viewport_size = event->size();
  d->UpdateScrollbars();
  

}

void CodeWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.drawPixmap(0, 0, d->canvas);

  (void) event;
}

void CodeWidget::mousePressEvent(QMouseEvent *event) {

  qDebug() << event;

  QPointF rel_position = event->position();
  (void) rel_position;
  // QPointF mouse_position(static_cast<qreal>(event->x() + d->viewport.x()),
  //                        static_cast<qreal>(event->y() + d->viewport.y()));
  // (void) mouse_position;
}

void CodeWidget::PrivateData::UpdateScrollbars(void) {

}

void CodeWidget::PrivateData::ResetScene(void) {
  Scene s;
  unsigned logical_column_number = 1;
  ImportNode(s, logical_column_number, token_tree.root());
  scene = std::move(s);
}

void CodeWidget::PrivateData::ResetCanvas(void) {
  ResetScene();

  QFont font = theme->Font();

  font.setStyleStrategy(QFont::NoSubpixelAntialias);

  QFont bold_font = font;
  QFont italic_font = font;
  QFont bold_italic_font = font;

  bold_font.setWeight(QFont::DemiBold);
  italic_font.setItalic(true);
  bold_italic_font.setWeight(QFont::DemiBold);
  bold_italic_font.setItalic(true);

  QFontMetricsF font_metrics(font);
  QFontMetricsF font_metrics_bi(bold_italic_font);
  QFontMetricsF font_metrics_b(bold_font);
  QFontMetricsF font_metrics_i(italic_font);

  int line_height = static_cast<int>(std::ceil(std::max(
      {font_metrics_bi.height(), font_metrics_b.height(),
       font_metrics_i.height(), font_metrics.height()})));

  auto max_char_width = static_cast<int>(std::ceil(std::max(
      {font_metrics_bi.horizontalAdvance("W"),
       font_metrics_b.horizontalAdvance("W"),
       font_metrics_i.horizontalAdvance("W"),
       font_metrics.horizontalAdvance("W")})));

  qreal space_width = font_metrics.horizontalAdvance(QChar::Space);

  QRect canvas_rect(
      0, 0,
      (max_char_width * static_cast<int>(scene.max_logical_columns)) + 100,
      line_height * static_cast<int>(std::max(1u, scene.num_lines)));

  qDebug() << scene.num_lines << scene.max_logical_columns << canvas_rect;

  QPixmap c(canvas_rect.width(), canvas_rect.height());
  QPainter blitter(&c);
  // blitter.setRenderHint(QPainter::Antialiasing);
  blitter.setRenderHint(QPainter::SmoothPixmapTransform);
  blitter.fillRect(canvas_rect, QBrush(theme->DefaultBackgroundColor()));

  qreal x = 0;
  qreal y = 0;
  unsigned logical_column_number = 1;
  unsigned logical_line_number = 1;

  for (Entity &e : scene.entities) {
    const QString &data = scene.token_data[e.data_index];
    const Token &token = scene.tokens[e.token_index];
    auto cs = theme->TokenColorAndStyle(token);

    // Synchronize our logical and physical positions. This ends up accounting
    // for whitespace.
    while (logical_line_number < e.logical_line_number) {
      y += line_height;
      x = 0;
      logical_line_number += 1;
      logical_column_number = 1;
    }

    while (logical_column_number < e.logical_column_number) {
      logical_column_number += 1;
      x += space_width;
    }

    // NOTE(pag): Mutate these in-place so that we always know where each
    //            entity is. This is required for click and selection managent.
    e.x = x;
    e.y = y;

    QPointF token_pos(x, y);

    font.setItalic(cs.italic);
    font.setUnderline(cs.underline);
    font.setStrikeOut(cs.strikeout);
    font.setWeight(cs.bold ? QFont::DemiBold : QFont::Normal);

    blitter.setPen(cs.foreground_color);
    blitter.setFont(font);

    QRectF token_rect = blitter.boundingRect(canvas_rect, Qt::AlignLeft, data);
    token_rect.moveTo(token_pos);

    blitter.fillRect(token_rect, cs.background_color);
    blitter.drawText(token_rect, Qt::AlignLeft, data);

    x += token_rect.width();
    logical_column_number += static_cast<unsigned>(data.size());
  }

  blitter.end();
  canvas = std::move(c);
}

void CodeWidget::OnIndexChanged(const ConfigManager &config_manager) {
  // d->model->Reset();

  (void) config_manager;
}

void CodeWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  d->theme = theme_manager.Theme();

  auto theme = d->theme.get();
  QPalette p = palette();
  p.setColor(QPalette::Window, theme->DefaultBackgroundColor());
  p.setColor(QPalette::WindowText, theme->DefaultForegroundColor());
  p.setColor(QPalette::Base, theme->DefaultBackgroundColor());
  p.setColor(QPalette::Text, theme->DefaultForegroundColor());
  p.setColor(QPalette::AlternateBase, theme->DefaultBackgroundColor());
  setPalette(p);

  auto font = theme->Font();

  setFont(font);

  d->ResetCanvas();
  update();
}

void CodeWidget::OnIconsChanged(const MediaManager &media_manager) {
  (void) media_manager;
}

// Invoked when the set of macros to be expanded changes.
void CodeWidget::OnExpandMacros(const QSet<RawEntityId> &macros_to_expand) {
  d->macros_to_expand = macros_to_expand;
  d->ResetCanvas();
  update();
}

// Invoked when the set of entities to be renamed changes.
void CodeWidget::OnRenameEntities(
    const QMap<RawEntityId, QString> &new_entity_names) {
  d->new_entity_names = new_entity_names;
  d->ResetCanvas();
  update();
}

void CodeWidget::OnVerticalScroll(int change) {
  d->scroll_y += change;
  update();
}

void CodeWidget::OnHorizontalScroll(int change) {
  d->scroll_x += change;
  update();
}

// Invoked when we want to scroll to a specific entity.
void CodeWidget::OnGoToEntity(const VariantEntity &entity) {
  d->scene_overrides.clear();

  if (auto frag = Fragment::containing(entity)) {
    d->scene_overrides.insert(frag->id().Pack());
  }

  update();
}

}  // namespace mx::gui
