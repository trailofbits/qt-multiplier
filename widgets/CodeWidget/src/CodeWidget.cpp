/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/CodeWidget.h>

#include <QApplication>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
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
#include <QWheelEvent>

#include <cmath>
#include <multiplier/Frontend/MacroSubstitution.h>
#include <multiplier/Frontend/MacroVAOpt.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

// #include "CodeModel.h"

namespace mx::gui {
namespace {

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

  // Index of this entity's data in `Scene::token_data`. The low two bits are
  // the "configuration" of this entity, i.e. selects which bounding rect
  // applies. These low two bits get updated by the painter.
  unsigned data_index_and_config;

  // Index of this entity's token in `Scene::tokens`.
  unsigned token_index;

  // The logical (one-indexed) line number of this token.
  int logical_line_number;

  // The logical (one-indexed) column number of this token.
  int logical_column_number;

  // inline auto operator<=>(const Entity &that) const noexcept {
  //   return position <=> that.position;
  // }

  // bool operator==(const Entity &that) const noexcept = default;
};

struct Data {
  QString text;

  bool bounding_rect_valid[4u];

  // Normal, bold, italic, and bold+italic.
  QRectF bounding_rect[4u];
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

  // For logical (one-based) line number `N`, `logical_line_index[N - 1]` is
  // the index into `entities` of the first entity on that line.
  std::vector<unsigned> logical_line_index;

  // A linear representation of the token data. If a token spans multiple lines
  // then it is split into multiple entries in `token_data`. If a token only
  // contributes pure whitespace then it is not included in `token_data`.
  std::vector<Data> data;

  // The underlying tokens.
  std::vector<Token> tokens;

  // A sorted list of related entity IDs and the into `entities`.
  std::vector<std::pair<RawEntityId, unsigned>> related_entity_ids;

  // Maximum number of characters on any given line.
  int max_logical_columns{1};

  // Number of logical lines in this scene.
  int num_lines{1};
};

// The scene builder helps to populate a given `Scene`. It keeps track of state
// that doesn't need to persist past the creation of a `Scene`.
struct SceneBuilder {
  Scene scene;

  // Maps unique `QString`s to an index in `Scene::data`.
  QMap<QString, unsigned> data_to_index;

  int logical_column_number{1};
  int token_start_column{0};
  int token_length{0};
  unsigned token_index{0u};
  bool added_anything{false};
  RawEntityId related_entity_id{kInvalidEntityId};

  QString token_data;

  inline SceneBuilder(void) {
    scene.logical_line_index.emplace_back(0u);
  }

  void BeginToken(const Token &tok) {
    related_entity_id = tok.related_entity_id().Pack();
  }

  void AddColumn(unsigned num_columns = 1u) {
    if (token_length) {
      AddEntity();
    }
    logical_column_number += num_columns;
  }

  void AddNewLine(void) {
    if (token_length) {
      AddEntity();
    }

    logical_column_number = 1u;
    scene.num_lines += 1;
    scene.logical_line_index.emplace_back(
        static_cast<unsigned>(scene.entities.size()));
  }

  void AddChar(QChar ch) {
    if (!token_start_column) {
      token_start_column = logical_column_number;
    }
    token_data += ch;
    token_length += 1;
    logical_column_number += 1u;
    added_anything = true;
  }

  void EndToken(Token tok) {
    AddEntity();
    if (added_anything) {
      scene.tokens.emplace_back(std::move(tok));
      added_anything = false;
      token_index += 1u;
    }
  }

  void AddEntity(void) {
    if (!token_length) {
      return;
    }

    scene.max_logical_columns = std::max(
        scene.max_logical_columns, token_start_column + token_length);

    // Get or create an index in `Scene::data` for the actual token data.
    auto data_index_it = data_to_index.find(token_data);
    unsigned data_index = 0u;
    if (data_index_it == data_to_index.end()) {
      data_index = static_cast<unsigned>(scene.data.size());

      Data &d = scene.data.emplace_back();
      d.text = std::move(token_data);

    } else {
      data_index = data_index_it.value();
    }
    token_data.clear();

    // Keep track of the related entity ID associated with this entity.
    if (related_entity_id != kInvalidEntityId) {
      scene.related_entity_ids.emplace_back(
        related_entity_id, static_cast<unsigned>(scene.entities.size()));
    }

    // Add the entity.
    Entity &e = scene.entities.emplace_back();
    e.logical_line_number = scene.num_lines;
    e.logical_column_number = token_start_column;
    e.data_index_and_config = data_index << 2u;
    e.token_index = token_index;

    token_start_column = 0u;
    token_length = 0;
  }

  Scene TakeScene(void) & {
    std::sort(scene.related_entity_ids.begin(), scene.related_entity_ids.end());
    return std::move(scene);
  }
};

// Dummy model to expose tokens to other stuff.
class TokenModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

 public:
  Token token;
  QString text;

  virtual ~TokenModel(void) = default;

  TokenModel(QObject *parent = nullptr)
      : IModel(parent) {}

  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL {
    if (!row && !column && !parent.isValid() && token) {
      return createIndex(0, 0, token.id().Pack());
    }
    return {};
  }

  //! Returns the parent of the given model index
  QModelIndex parent(const QModelIndex &) const Q_DECL_FINAL {
    return {};
  }

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL {
    return parent.isValid() || !token ? 0 : 1;
  }

  //! Returns the amount of columns in the model
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL {
    return parent.isValid() || !token ? 0 : 1;
  }

  //! Returns the index data for the specified role
  //! \todo Fix role=TokenCategoryRole support
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL {
    if (index.column() != 0 || index.row() != 0 ||
        index.internalId() != token.id().Pack()) {
      return {};
    }

    if (token) {
      switch (role) {
        case IModel::EntityRole:
          return QVariant::fromValue<VariantEntity>(token);
        case IModel::TokenRangeDisplayRole:
          return QVariant::fromValue<TokenRange>(token);
        case IModel::ModelIdRole:
          return "com.trailofbits.model.CodeWidget.TokenModel";
        case Qt::DisplayRole:
          return text;
      }
    }
    return {};
  }

  //! Returns the column names for the tree view header
  QVariant headerData(int, Qt::Orientation, int) const Q_DECL_FINAL {
    return {};
  }
};

}  // namespace

struct CodeWidget::PrivateData {

  // The theme being used.
  IThemePtr theme;

  // Source of data that we're rendering.
  TokenTree token_tree;

  // Size of the visible viewport area for this widget.
  QRect viewport;

  // A single character string.
  QString monospace;

  // The font we're currently painting with.
  QFont font;

  QColor theme_foreground_color;
  QColor theme_background_color;
  QRectF space_rect;
  QRect canvas_rect;
  QTextOption to;

  bool scene_changed{true};
  bool canvas_changed{true};

  // The current DPI ratio for the app.
  qreal dpi_ratio{1.0};

  // Current scroll x/y positions.
  int scroll_x{0};
  int scroll_y{0};

  // Determined during painting.
  int line_height{0};
  int max_char_width{0};
  bool is_monospaced{false};

  // The index of the current line to highlight.
  int current_line_index{-1};

  // The current entity under the cursor.
  const Entity *current_entity{nullptr};

  TokenModel primary_click_model;
  TokenModel secondary_click_model;
  TokenModel key_press_model;

  // Data structure keeping track of the logical things to render, and
  // where to render them. The act of rendering updates `Entity`s in the
  // scene to keep track of their physical locations within `*_canvas`.
  Scene scene;

  // Actual "picture" of what is rendered.
  QImage background_canvas;
  QImage foreground_canvas;

  // Sets of entities that configure what gets shown from `token_tree`.
  QSet<RawEntityId> macros_to_expand;
  QMap<RawEntityId, QString> new_entity_names;
  QSet<RawEntityId> scene_overrides;

  QScrollBar *horizontal_scrollbar{nullptr};
  QScrollBar *vertical_scrollbar{nullptr};

  inline PrivateData(void)
      : monospace(" "),
        to(Qt::AlignLeft),
        dpi_ratio(qApp->devicePixelRatio()) {}

  void UpdateScrollbars(void);
  void RecomputeScene(void);
  void RecomputeCanvas(void);

  void PaintToken(
      QPainter &fg_painter, QPainter &bg_painter, Data &data,
      unsigned rect_config, ITheme::ColorAndStyle cs, qreal &x, qreal &y);

  void ImportChoiceNode(SceneBuilder &b,
                        ChoiceTokenTreeNode node);
  void ImportSubstitutionNode(SceneBuilder &b,
                              SubstitutionTokenTreeNode node);
  void ImportSequenceNode(SceneBuilder &b,
                          SequenceTokenTreeNode node);
  void ImportTokenNode(SceneBuilder &b,
                       TokenTokenTreeNode node);
  void ImportNode(SceneBuilder &b, TokenTreeNode node);

  void ScrollBy(int horizontal_pixel_delta, int vertical_pixel_delta);

  const Entity *EntityUnderCursor(QPointF point) const;

  QModelIndex CreateModelIndex(TokenModel &model, const Entity *entity) {
    if (!entity) {
      return {};
    }

    model.token = scene.tokens[entity->token_index];
    model.text = scene.data[entity->data_index_and_config >> 2u].text;
    return model.index(0, 0, {});
  }
};

// Import a choice node.
void CodeWidget::PrivateData::ImportChoiceNode(
    SceneBuilder &b, ChoiceTokenTreeNode node) {

  std::optional<TokenTreeNode> chosen_node;

  for (auto &item : node.children()) {
    RawEntityId fragment_id = item.first.id().Pack();
    if (scene_overrides.contains(fragment_id) || !chosen_node) {
      chosen_node.reset();
      chosen_node.emplace(std::move(item.second));
    }
  }

  if (chosen_node) {
    ImportNode(b, std::move(chosen_node.value()));
  }
}

// Import a substitution node.
void CodeWidget::PrivateData::ImportSubstitutionNode(
    SceneBuilder &b, SubstitutionTokenTreeNode node) {

  RawEntityId macro_id = kInvalidEntityId;
  auto sub = node.macro();
  if (std::holds_alternative<MacroSubstitution>(sub)) {
    macro_id = std::get<MacroSubstitution>(sub).id().Pack();
  } else {
    macro_id = std::get<MacroVAOpt>(sub).id().Pack();
  }

  if (macros_to_expand.contains(macro_id)) {
    ImportNode(b, node.after());
  } else {
    ImportNode(b, node.before());
  }
}

// Import a sequence of nodes.
void CodeWidget::PrivateData::ImportSequenceNode(
    SceneBuilder &b, SequenceTokenTreeNode node) {
  for (auto child_node : node.children()) {
    ImportNode(b, std::move(child_node));
  }
}

// Import a node containing a token.
void CodeWidget::PrivateData::ImportTokenNode(
    SceneBuilder &b, TokenTokenTreeNode node) {

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

  b.BeginToken(token);

  for (QChar ch : utf16_data) {
    switch (ch.unicode()) {

      // TODO(pag): Configurable tab width; tab stops
      case QChar::Tabulation:
        b.AddColumn(kTabWidth);
        break;

      case QChar::Space:
      case QChar::Nbsp:
        b.AddColumn();
        break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator: {
        b.AddNewLine();
        break;
      }

      case QChar::CarriageReturn:
        continue;

      default:
        b.AddChar(ch);
        break;
    }
  }

  b.EndToken(std::move(token));
}

// Import a generic node, dispatching to the relevant node.
void CodeWidget::PrivateData::ImportNode(SceneBuilder &b, TokenTreeNode node) {
  switch (node.kind()) {
    case TokenTreeNodeKind::EMPTY:
      break; 
    case TokenTreeNodeKind::TOKEN:
      ImportTokenNode(
          b, std::move(reinterpret_cast<TokenTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::CHOICE:
      ImportChoiceNode(
          b, std::move(reinterpret_cast<ChoiceTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SUBSTITUTION:
      ImportSubstitutionNode(
          b, std::move(reinterpret_cast<SubstitutionTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SEQUENCE:
      ImportSequenceNode(
          b, std::move(reinterpret_cast<SequenceTokenTreeNode &&>(node)));
      break;
  }
}

// Scroll the window by a specific delta.
void CodeWidget::PrivateData::ScrollBy(
    int horizontal_pixel_delta, int vertical_pixel_delta) {

  auto c_width = static_cast<int>(foreground_canvas.width() / dpi_ratio);
  auto c_height = static_cast<int>(foreground_canvas.height() / dpi_ratio);

  auto v_width = viewport.width();
  auto v_height = viewport.height();

  if (c_width > v_width) {
    scroll_x = std::min(
        std::max(0, scroll_x + horizontal_pixel_delta),
        c_width - v_width);

  } else {
    scroll_x = 0;
  }

  if (c_height > v_height) {
    scroll_y = std::min(
        std::max(0, scroll_y + vertical_pixel_delta),
        c_height - v_height);

  } else {
    scroll_y = 0;
  }
}

// Locate the entity underneath `point`. Point corresponds to a viewport
// position.
const Entity *CodeWidget::PrivateData::EntityUnderCursor(QPointF point) const {
  if (scene.entities.empty()) {
    return nullptr;
  }

  auto x = scroll_x + point.x();
  auto y = scroll_y + point.y();

  auto line_index = static_cast<unsigned>(std::floor(y / line_height));
  if (line_index >= scene.logical_line_index.size()) {
    return nullptr;
  }

  auto i = scene.logical_line_index[line_index];
  auto max_i = scene.logical_line_index[line_index + 1];

  for (; i < max_i; ++i) {
    const Entity &e = scene.entities[i];
    if (e.x > x) {
      continue;
    }

    const Data &data = scene.data[e.data_index_and_config >> 2u];
    QRectF r = data.bounding_rect[e.data_index_and_config & 0b11u];

    qreal e_y = static_cast<qreal>(line_index) * line_height;

    r.moveTo(QPointF(e.x, e_y));
    if (r.contains(QPointF(x, y))) {
      return &e;
    }
  }

  return nullptr;
}

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(const ConfigManager &config_manager,
                       QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData) {

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

  OnThemeChanged(theme_manager);  // Calls `RecomputeScene`.
  OnIconsChanged(media_manager);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &CodeWidget::OnIndexChanged);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &CodeWidget::OnThemeChanged);

  connect(&media_manager, &MediaManager::IconsChanged,
          this, &CodeWidget::OnIconsChanged);
}

void CodeWidget::resizeEvent(QResizeEvent *event) {
  QSize new_size = event->size();
  d->viewport.setWidth(new_size.width());
  d->viewport.setHeight(new_size.height());
  d->UpdateScrollbars();
}

void CodeWidget::wheelEvent(QWheelEvent *event) {

  int vertical_pixel_delta = 0;
  int horizontal_pixel_delta = 0;

  if (auto pixel_delta_point = event->pixelDelta(); !pixel_delta_point.isNull()) {
    vertical_pixel_delta = pixel_delta_point.y();
    horizontal_pixel_delta = pixel_delta_point.x();

  // High resolution gaming mice are capable of returning fractions of what is
  // usually considered a single mouse wheel turn.
  } else {
    auto vertical_angle_delta = event->angleDelta().y();
    auto horizontal_angle_delta = event->angleDelta().x();

    auto line_height = QFontMetrics(d->theme->Font(), this).height();

    vertical_pixel_delta =
        line_height * static_cast<int>(vertical_angle_delta * 1.0 / 120.0);

    horizontal_pixel_delta =
      line_height * static_cast<int>(horizontal_angle_delta * 1.0 / 120.0);
  }

  d->ScrollBy(horizontal_pixel_delta * -1, vertical_pixel_delta * -1);
  d->UpdateScrollbars();
  update();
}

void CodeWidget::paintEvent(QPaintEvent *) {
  d->RecomputeCanvas();

  QPainter blitter(this);
  blitter.eraseRect(d->viewport);

  // Fill the viewport with the theme background color.
  blitter.fillRect(d->viewport, d->theme_background_color);

  // Render current line within the canvas.
  if (d->current_line_index != -1) {
    QRectF current_line(
        0, (d->current_line_index * d->line_height) - d->scroll_y,
        d->viewport.width(), d->line_height);
    current_line.setWidth(d->viewport.width());
    blitter.fillRect(current_line, d->theme->CurrentLineBackgroundColor());
  }

  // Draw the code background layer.
  blitter.drawImage(-d->scroll_x, -d->scroll_y, d->background_canvas);

  auto re_it = d->scene.related_entity_ids.end();
  auto re_end_it = d->scene.related_entity_ids.end();
  QColor highlight_foreground_color;
  RawEntityId related_entity_id = kInvalidEntityId;

  // Render the related entity ID highlight background colors.
  if (d->current_entity) {
    const Token &token = d->scene.tokens[d->current_entity->token_index];
    related_entity_id = token.related_entity_id().Pack();
    
    QColor highlight_color = d->theme->CurrentEntityBackgroundColor(
        token.related_entity());
    highlight_foreground_color = ITheme::ContrastingColor(highlight_color);

    if (related_entity_id != kInvalidEntityId) {
      re_it = std::upper_bound(
          d->scene.related_entity_ids.begin(), re_end_it,
          std::pair<RawEntityId, unsigned>(related_entity_id - 1, ~0u));
      
      for (auto it = re_it; it != re_end_it && it->first == related_entity_id;
           ++it) {
        const Entity &e = d->scene.entities[it->second];
        const Data &data = d->scene.data[e.data_index_and_config >> 2u];
        unsigned rect_config = e.data_index_and_config & 0b11u;
        QRectF rect = data.bounding_rect[rect_config];

        qreal e_y = static_cast<qreal>(e.logical_line_number - 1) *
                    d->line_height;
        rect.moveTo(QPointF(e.x - d->scroll_x, e_y - d->scroll_y));
        blitter.fillRect(rect, highlight_color);
      }
    }
  }

  // Draw the code foreground layer.
  blitter.drawImage(-d->scroll_x, -d->scroll_y, d->foreground_canvas);

  // Overlay the code foreground layer with contrasting text for the highlighted
  // entities.
  if (d->current_entity && highlight_foreground_color.isValid()) {
    QPainter dummy_bg_painter;
    for (auto it = re_it; it != re_end_it && it->first == related_entity_id;
         ++it) {
      const Entity &e = d->scene.entities[it->second];
      const Token &t = d->scene.tokens[e.token_index];
      Data &data = d->scene.data[e.data_index_and_config >> 2u];
      unsigned rect_config = e.data_index_and_config & 0b11u;

      ITheme::ColorAndStyle cs = d->theme->TokenColorAndStyle(t);
      cs.background_color = QColor();
      cs.foreground_color = highlight_foreground_color;

      qreal e_y = static_cast<qreal>(e.logical_line_number - 1) * d->line_height;
      qreal x = e.x - d->scroll_x;
      qreal y = e_y - d->scroll_y;

      d->PaintToken(blitter, dummy_bg_painter, data, rect_config, cs, x, y);
    }
  }

  blitter.end();
}

void CodeWidget::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) {
    mousePressEvent(event);
    return;
  }
}

void CodeWidget::mousePressEvent(QMouseEvent *event) {

  QPointF rel_position = event->position();
  auto x = d->scroll_x + rel_position.x();
  auto y = d->scroll_y + rel_position.y();

  const Entity *entity = d->EntityUnderCursor(rel_position);

  d->primary_click_model.token = {};
  d->secondary_click_model.token = {};
  d->key_press_model.token = {};

  // Calculate the index of the current line.
  if (event->buttons() & Qt::LeftButton) {
    auto new_current_line_index = static_cast<int>(std::floor(y / d->line_height));
    if (new_current_line_index != d->current_line_index) {
      d->current_line_index = new_current_line_index;
    }

    d->current_entity = entity;
    emit RequestPrimaryClick(
        d->CreateModelIndex(d->primary_click_model, entity));

  } else if (event->buttons() & Qt::RightButton) {
    emit RequestSecondaryClick(
        d->CreateModelIndex(d->secondary_click_model, entity));
  }

  update();

  (void) x;
  // QPointF mouse_position(static_cast<qreal>(event->x() + d->viewport.x()),
  //                        static_cast<qreal>(event->y() + d->viewport.y()));
  // (void) mouse_position;
}

void CodeWidget::keyPressEvent(QKeyEvent *event) {
  auto need_repaint = false;
  switch (event->key()) {

    // Pressing the up arrow moves the current line up, and maybe triggers
    // a scroll.
    case Qt::Key_Up:
      need_repaint = true;
      d->current_line_index = std::max(0, d->current_line_index - 1);

      // Detect if we need to scroll up to follow the current line.
      if ((d->current_line_index * d->line_height) < d->scroll_y) {
        d->ScrollBy(0, -d->line_height);
      }

      break;

    // Pressing the down arrow moves the current line down, and maybe triggers
    // a scroll.
    case Qt::Key_Down:
      need_repaint = true;
      d->current_line_index = std::min(
          d->scene.num_lines - 1, d->current_line_index + 1);

      // Detect if we need to scroll down to follow the current line.
      if (((d->current_line_index + 1) * d->line_height) >
          (d->scroll_y + d->viewport.height())) {
        d->ScrollBy(0, d->line_height);
      }

      break;
    case Qt::Key_Left:
    case Qt::Key_Right:
      break;
    default:
      if (d->current_entity && d->primary_click_model.token) {
        QString modifier;
        if (event->modifiers() & Qt::ShiftModifier) {
          modifier += "Shift+";
        }
        if (event->modifiers() & Qt::ControlModifier) {
          modifier += "Ctrl+";
        }
        if (event->modifiers() & Qt::AltModifier) {
          modifier += "Alt+";
        }
        if (event->modifiers() & Qt::MetaModifier) {
          modifier += "Meta+";
        }

        emit RequestKeyPress(
            QKeySequence(modifier + QKeySequence(event->key()).toString()),
            d->CreateModelIndex(d->key_press_model, d->current_entity));
      }
      break;
  }

  if (need_repaint) {
    update();
  }
}

void CodeWidget::PrivateData::UpdateScrollbars(void) {

}

void CodeWidget::PrivateData::RecomputeScene(void) {
  if (!scene_changed) {
    return;
  }

  SceneBuilder builder;  
  ImportNode(builder, token_tree.root());
  scene = builder.TakeScene();
  scene_changed = false;

  // Force a change.
  current_entity = nullptr;
  canvas_changed = true;
}

void CodeWidget::PrivateData::RecomputeCanvas(void) {
  RecomputeScene();

  if (!canvas_changed) {
    return;
  }

  canvas_changed = false;

  font = theme->Font();  // Reset (to clear bold/italic).
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

  line_height = static_cast<int>(std::ceil(std::max(
      {font_metrics_bi.height(), font_metrics_b.height(),
       font_metrics_i.height(), font_metrics.height()})));

  max_char_width = static_cast<int>(std::ceil(std::max(
      {font_metrics_bi.maxWidth(), font_metrics_b.maxWidth(),
       font_metrics_i.maxWidth(), font_metrics.maxWidth()})));

  // Use a painter for also figuring out the maximum character size, as a
  // bounding rect from a painter could be bigger.
  {
    QPainter p;
    p.setFont(bold_italic_font);
    max_char_width = std::max(
        max_char_width,
        static_cast<int>(std::ceil(
            p.boundingRect(
                QRectF(max_char_width, line_height,
                       max_char_width * 4, line_height * 4),
                "W", to).width())));
  }

  // Figure out the canvas size. This is the maximum number of characters we
  // have, plus a margin for one on the left and one on the right, to allow for
  // italic text to "lean in" and spill over into those margins.
  canvas_rect = QRect(
      0, 0,
      (max_char_width * static_cast<int>(scene.max_logical_columns + 2)),
      line_height * std::max(1, scene.num_lines));

  QImage fg(static_cast<int>(canvas_rect.width() * dpi_ratio),
            static_cast<int>(canvas_rect.height() * dpi_ratio),
            QImage::Format_RGBA8888);

  QImage bg(static_cast<int>(canvas_rect.width() * dpi_ratio),
            static_cast<int>(canvas_rect.height() * dpi_ratio),
            QImage::Format_RGBA8888);

  fg.setDevicePixelRatio(dpi_ratio);
  bg.setDevicePixelRatio(dpi_ratio);

  QPainter fg_painter(&fg);
  QPainter bg_painter(&bg);

  bg_painter.setRenderHints(QPainter::SmoothPixmapTransform);
  fg_painter.setRenderHints(QPainter::TextAntialiasing |
                            QPainter::Antialiasing |
                            QPainter::SmoothPixmapTransform);
  fg_painter.setFont(font);

  monospace[0] = QChar::Space;
  space_rect = fg_painter.boundingRect(canvas_rect, monospace, to);
  qreal space_width = space_rect.width();

  // Start new lines indented with a single space. This helps us account for
  // italic character writing outside of their minimum-sized bounding box.
  qreal x = space_width;
  qreal y = 0;
  int logical_column_number = 1;
  int logical_line_number = 1;

  // Try to detect if the font is monospaced.
  is_monospaced =
      font_metrics_bi.maxWidth() == font_metrics.maxWidth() &&
      font_metrics_bi.horizontalAdvance(".") == font_metrics_bi.maxWidth();

  for (Entity &e : scene.entities) {
    Data &data = scene.data[e.data_index_and_config >> 2u];
    const Token &token = scene.tokens[e.token_index];

    Q_ASSERT(!data.text.isEmpty());

    // Synchronize our logical and physical positions. This ends up accounting
    // for whitespace.
    while (logical_line_number < e.logical_line_number) {
      y += line_height;
      x = space_width;
      logical_line_number += 1;
      logical_column_number = 1;
    }

    while (logical_column_number < e.logical_column_number) {
      logical_column_number += 1;
      x += space_width;
    }

    // NOTE(pag): Mutate this in-place so that we always know where each
    //            entity is. This is required for click and selection managent.
    e.x = x;

    ITheme::ColorAndStyle cs = theme->TokenColorAndStyle(token);

    // Figure out the configuration for this entity, and update it in place.
    unsigned rect_config = (cs.bold ? 2u : 0u) | (cs.italic ? 1u : 0u);
    e.data_index_and_config |= rect_config;

    PaintToken(fg_painter, bg_painter, data, rect_config, cs, x, y);

    logical_column_number += static_cast<int>(data.text.size());
  }

  fg_painter.end();
  bg_painter.end();

  foreground_canvas.swap(fg);
  background_canvas.swap(bg);
}

// Paint a token.
void CodeWidget::PrivateData::PaintToken(
    QPainter &fg_painter, QPainter &bg_painter, Data &data,
    unsigned rect_config, ITheme::ColorAndStyle cs, qreal &x, qreal &y) {

  font.setItalic(cs.italic);
  font.setUnderline(cs.underline);
  font.setStrikeOut(cs.strikeout);
  font.setWeight(cs.bold ? QFont::DemiBold : QFont::Normal);
  fg_painter.setFont(font);

  if (cs.foreground_color.isValid()) {
    fg_painter.setPen(cs.foreground_color);
  } else {
    fg_painter.setPen(theme_foreground_color);
  }

  QRectF &token_rect = data.bounding_rect[rect_config];
  bool &token_rect_valid = data.bounding_rect_valid[rect_config];
  bool bg_valid = cs.background_color.isValid();
  qreal space_width = space_rect.width();

  // Draw one character at a time. That results in better alignment across
  // lines.
  if (is_monospaced) {
    if (!token_rect_valid) {
      token_rect = space_rect;
      token_rect.setWidth(space_width * data.text.size());
      token_rect_valid = true;
    }

    QRectF glyph_rect = space_rect;

    for (QChar ch : data.text) {
      monospace[0] = ch;

      glyph_rect.moveTo(QPointF(x, y));
      if (bg_valid) {
        bg_painter.fillRect(glyph_rect, cs.background_color);
      }
      fg_painter.drawText(glyph_rect, monospace, to);
      x += space_width;
    }

  // Draw it as one word.
  } else {
    if (!token_rect_valid) {
      token_rect = fg_painter.boundingRect(canvas_rect, data.text, to);
      token_rect_valid = true;
    }

    if (bg_valid) {
      bg_painter.fillRect(token_rect, cs.background_color);
    }

    fg_painter.drawText(token_rect, data.text, to);
    x += token_rect.width();
  }
}

void CodeWidget::OnIndexChanged(const ConfigManager &) {
  SetTokenTree({});
  close();
}

void CodeWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  QFont old_font;
  if (d->theme) {
    old_font = d->theme->Font();
  }

  d->theme = theme_manager.Theme();
  d->font = d->theme->Font();
  d->theme_foreground_color = d->theme->DefaultForegroundColor();
  d->theme_background_color = d->theme->DefaultBackgroundColor();

  if (old_font == d->font) {
    d->canvas_changed = true;
  } else {
    d->scene_changed = true;
  }

  QPalette p = palette();
  p.setColor(QPalette::Window, d->theme_background_color);
  p.setColor(QPalette::WindowText, d->theme_foreground_color);
  p.setColor(QPalette::Base, d->theme_background_color);
  p.setColor(QPalette::Text, d->theme_foreground_color);
  p.setColor(QPalette::AlternateBase, d->theme_background_color);
  setPalette(p);
  setFont(d->font);
  update();
}

void CodeWidget::OnIconsChanged(const MediaManager &media_manager) {
  (void) media_manager;
}

// Invoked when the set of macros to be expanded changes.
void CodeWidget::OnExpandMacros(const QSet<RawEntityId> &macros_to_expand) {
  d->scene_changed = true;  // TODO(pag): Be more selective.
  d->macros_to_expand = macros_to_expand;
  update();
}

// Invoked when the set of entities to be renamed changes.
void CodeWidget::OnRenameEntities(
    const QMap<RawEntityId, QString> &new_entity_names) {
  d->scene_changed = true;  // TODO(pag): Be more selective.
  d->new_entity_names = new_entity_names;
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

void CodeWidget::SetTokenTree(const TokenTree &token_tree) {
  d->scene_changed = true;
  d->canvas_changed = true;
  d->current_entity = nullptr;
  d->primary_click_model.token = {};
  d->secondary_click_model.token = {};
  d->key_press_model.token = {};
  d->scroll_x = 0;
  d->scroll_y = 0;
  d->current_line_index = -1;
  d->scene_overrides.clear();
  d->token_tree = token_tree;
  update();
}

}  // namespace mx::gui

#include "CodeWidget.moc"
