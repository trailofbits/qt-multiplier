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
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/MacroVAOpt.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {
namespace {

static constexpr auto kBoldMask = 0b10u;
static constexpr auto kItalicMask = 0b01u;
static constexpr auto kFormatMask = kBoldMask | kItalicMask;
static constexpr auto kFormatShift = 2u;
static constexpr qreal kCursorWidth = 2;
static constexpr qreal kCursorDisp = -0.5;

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

  // Offset of the beginning of this entity in the total text of the document.
  int document_offset;
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
  // The complete, (nearly) original document.
  QString document;

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

  // Keeps track of which macros were and weren't expanded.
  std::unordered_map<RawEntityId, bool> expanded_macros;

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
  int expansion_depth{0};
  int document_offset{0};
  unsigned token_index{0u};
  bool added_anything{false};
  RawEntityId related_entity_id{kInvalidEntityId};

  QString token_data;

  inline SceneBuilder(void) {
    scene.logical_line_index.emplace_back(0u);
  }

  void BeginToken(const Token &tok) {
    related_entity_id = tok.related_entity_id().Pack();
    document_offset = static_cast<int>(scene.document.size());
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
    e.document_offset = document_offset;

    token_start_column = 0u;
    token_length = 0;
    document_offset = static_cast<int>(scene.document.size());
  }

  Scene TakeScene(void) & {
    scene.logical_line_index.resize(static_cast<unsigned>(scene.num_lines + 1));
    scene.logical_line_index.back()
        = static_cast<unsigned>(scene.entities.size());
    std::sort(scene.related_entity_ids.begin(), scene.related_entity_ids.end());
    return std::move(scene);
  }
};

// Dummy model to expose tokens to other stuff.
class TokenModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

 public:
  QString model_id;
  Token token;
  QString text;

  virtual ~TokenModel(void) = default;

  TokenModel(const QString &model_id_, QObject *parent = nullptr)
      : IModel(parent),
        model_id(model_id_) {}

  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL {
    if (!hasIndex(row, column, parent)) {
      return {};
    }
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
          return model_id;
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

  // A single character string. Sometimes we have to draw one character at a
  // time, or measure the width of a single character, so we end up just
  // reusing this string as a single character sized buffer.
  QString monospace;

  // The font we're currently painting with.
  QFont font;

  QColor theme_cursor_color;
  QColor theme_foreground_color;
  QColor theme_background_color;

  // Calculated shape and width of a single space in this font. Generally we
  // make this the size of a bold and italic space.
  QRectF space_rect;
  qreal space_width{0};
  
  QRect canvas_rect;
  QTextOption to;

  // The scene is a linearization of the current configuration of the
  // `TokenTree`, where we represent things in terms of entities, and we know
  // which bits of data each entity is associated with, etc. The scene also
  // caches the shapes/locations of each textual element. When we have
  // requests for things like macro expansions or entity renamings, we need
  // to signal to the next `paintEvent` that the scene has changed, and thus
  // must be recomputed.
  bool scene_changed{true};

  // The canvas is really a mix of things, but as a whole it represents what
  // we're trying to paint in order to render code. The canvas is made of a mix
  // of cached layers (`background_canvas`, `foreground_canvas`) and layers
  // that are (re)generated on every `paintEvent`. If the scene or theme changes
  // then we generally need to recompute the canvas.
  bool canvas_changed{true};

  // The current DPI ratio for the app. The viewport of the codewidget is
  // expressed in pixels, and we need to multiply by the DPI ratio to get the
  // actual width that we're painting.
  qreal dpi_ratio{1.0};

  // Current scroll x/y positions.
  int scroll_x{0};
  int scroll_y{0};

  // Location of the cursor (accounting for `scroll_x` and `scroll_y`).
  std::optional<QPointF> cursor;

  // The starting cursor of the selection.
  std::optional<QPointF> selection_start_cursor;

  bool click_was_primary{false};
  bool click_was_secondary{false};

  bool tracking_selection{false};

  // Determined during painting.
  int line_height{0};
  int max_char_width{0};
  bool is_monospaced{false};

  // The index of the current line to highlight.
  int current_line_index{-1};

  // The current entity under the cursor.
  const Entity *current_entity{nullptr};

  TokenModel token_model;

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

  inline PrivateData(const QString &model_id)
      : monospace(" "),
        to(Qt::AlignLeft),
        dpi_ratio(qApp->devicePixelRatio()),
        token_model(model_id) {}

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

  const Entity *EntityUnderPoint(QPointF point) const;

  std::pair<int, qreal> CharacterPosition(
      QPointF point, const Entity *entity) const;
  std::pair<int, qreal> CharacterPositionVariable(
      QPointF point, const Entity *entity) const;
  std::pair<int, qreal> CharacterPositionFixed(
      QPointF point, const Entity *entity) const;

  QPointF CursorPosition(QPointF point) const;
  QPointF CursorPositionFixed(QPointF point) const;
  QPointF CursorPositionVariable(QPointF point) const;
  QPointF NextCursorPosition(QPointF curr_cursor, qreal dir_x,
                             qreal dir_y) const;
  QPointF NextCursorPositionFixed(QPointF curr_cursor, qreal dir_x,
                                  qreal dir_y, qreal char_width) const;
  QPointF NextCursorPositionVariable(QPointF curr_cursor, qreal dir_x,
                                     qreal dir_y) const;
  QPointF ClampCursorPosition(QPointF point) const;

  QModelIndex CreateModelIndex(const Entity *entity);
};

// Fills `model` (e.g. `PrivateData::token_model`) with information from
// `entity` sufficient to satisfy the `IModel` interface, so that we can
// publish `QModelIndex`es in our signals, e.g. `RequestPrimaryClick`.
QModelIndex CodeWidget::PrivateData::CreateModelIndex(const Entity *entity) {
  if (!entity) {
    return {};
  }

  token_model.token = scene.tokens[entity->token_index];
  token_model.text
      = scene.data[entity->data_index_and_config >> kFormatShift].text;
  return token_model.index(0, 0, {});
}

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
  RawEntityId def_id = kInvalidEntityId;
  auto sub = node.macro();
  if (std::holds_alternative<MacroSubstitution>(sub)) {
    macro_id = std::get<MacroSubstitution>(sub).id().Pack();

    // Global expansion of a macro is based on the definition ID.
    if (auto exp = MacroExpansion::from(std::get<MacroSubstitution>(sub))) {
      if (auto def = exp->definition()) {
        def_id = def->id().Pack();
      }
    }
  } else {
    macro_id = std::get<MacroVAOpt>(sub).id().Pack();
  }

  auto expanded = macros_to_expand.contains(macro_id);
  if (def_id != kInvalidEntityId) {
    expanded = expanded || macros_to_expand.contains(def_id);
    b.scene.expanded_macros.emplace(def_id, expanded);
  }
  b.scene.expanded_macros.emplace(macro_id, expanded);

  if (expanded) {
    ++b.expansion_depth;
    ImportNode(b, node.after());
    --b.expansion_depth;
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
        b.scene.document.append(QChar::Tabulation);
        if (token.kind() == TokenKind::WHITESPACE) {
          b.AddColumn(kTabWidth);
        } else {
          for (auto i = 0u; i < kTabWidth; ++i) {
            b.AddChar(QChar::Space);
          }
        }
        break;

      case QChar::Space:
      case QChar::Nbsp:
        b.scene.document.append(QChar::Space);
        if (token.kind() == TokenKind::WHITESPACE) {
          b.AddColumn();
        } else {
          b.AddChar(QChar::Space);
        }
        break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator: {
        b.scene.document.append(QChar::LineFeed);
        b.AddNewLine();
        break;
      }

      case QChar::CarriageReturn:
        continue;

      default:
        b.scene.document.append(ch);
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

  UpdateScrollbars();
}

// Return the character offset (`-1` if invalid) to the right of `point` (the
// cursor), and the width of the data of `entity` to the left of `point`. 
std::pair<int, qreal> CodeWidget::PrivateData::CharacterPosition(
    QPointF point, const Entity *entity) const {
  if (is_monospaced) {
    return CharacterPositionFixed(point, entity);
  } else {
    return CharacterPositionVariable(point, entity);
  }
}

// Return the character offset (`-1` if invalid) to the right of `point` (the
// cursor), and the width of the data of `entity` to the left of `point` for a
// variable/proportional-sized font.
std::pair<int, qreal> CodeWidget::PrivateData::CharacterPositionVariable(
    QPointF point, const Entity *entity) const {

  auto text_data_index = entity->data_index_and_config >> kFormatShift;
  auto text_config_index = entity->data_index_and_config & kFormatMask;
  const Data *data = &(scene.data[text_data_index]);

  qreal x = point.x();

  // The cursor comes before `entity`.
  if (entity->x > x) {
    return {-1, 0.0};
  }

  qreal line_index = entity->logical_line_number - 1;
  QRectF text_rect = data->bounding_rect[text_config_index];
  qreal entity_y = line_index * line_height;

  text_rect.moveTo(QPointF(entity->x, entity_y));
  if (!text_rect.contains(point)) {
    return {-1, 0.0};
  }

  QPixmap dummy_pixmap(static_cast<int>(text_rect.width()),
                       static_cast<int>(text_rect.height()));
  QPainter p(&dummy_pixmap);

  // Configure the font based on the formatting of the entity. The bold/italic
  // affects character sizes.
  QFont entity_font = font;
  if (text_config_index & kBoldMask) {
    entity_font.setWeight(QFont::DemiBold);
  }
  if (text_config_index & kItalicMask) {
    entity_font.setItalic(true);
  }
  p.setFont(entity_font);

  // The cursor falls inside of an existing entity text. We'll calculate the
  // width of the text. We go one character at a time, building up
  // successively longer prefixes of `data->text` until we find that we've
  // moved just beyond where the user clicked.
  qreal prev_width = 0;
  for (auto k = 1; k <= data->text.size(); ++k) {
    auto prefix = data->text.sliced(0, k);
    auto prefix_rect = p.boundingRect(text_rect, prefix, to);

    if (prefix_rect.contains(point)) {
      auto half = (prefix_rect.width() - prev_width) / 2;

      // Falls to the left of this letter.
      if ((entity->x + prev_width + half) > x) {
        return {k - 1, prev_width};

      // Falls to the right of this letter.
      } else {
        return {k, prefix_rect.width()};
      }
    }

    prev_width = prefix_rect.width();
  }

  Q_ASSERT(false);
  return {-1, 0.0};
}

// Return the character offset (`-1` if invalid) to the right of `point` (the
// cursor), and the width of the data of `entity` to the left of `point` for a
// fixed-width-sized font.
std::pair<int, qreal> CodeWidget::PrivateData::CharacterPositionFixed(
    QPointF point, const Entity *entity) const {

  auto text_data_index = entity->data_index_and_config >> kFormatShift;
  auto text_config_index = entity->data_index_and_config & kFormatMask;
  const Data *data = &(scene.data[text_data_index]);

  qreal x = point.x();

  // The cursor comes before `entity`.
  if (entity->x > x) {
    return {-1, 0.0};
  }

  qreal line_index = entity->logical_line_number - 1;
  QRectF text_rect = data->bounding_rect[text_config_index];
  qreal entity_y = line_index * line_height;

  text_rect.moveTo(QPointF(entity->x, entity_y));
  if (!text_rect.contains(point)) {
    return {-1, 0.0};
  }

  auto diff = x - text_rect.x();

  qreal half_width = space_width / 2;
  auto col_count = std::floor((diff + half_width) / space_width);
  
  if (col_count < 1) {
    return {0, 0.0};
  }

  return {static_cast<int>(col_count), col_count * space_width};
}

// Locates the top-left corner of a cursor that should be placed under/near
// `point`.
//
// NOTE(pag): `point` should already be translated by the `scroll_x` and
//            `scroll_y`.
QPointF CodeWidget::PrivateData::CursorPosition(QPointF point) const {  
  if (is_monospaced) {
    return ClampCursorPosition(CursorPositionFixed(point));
  } else {
    return ClampCursorPosition(CursorPositionVariable(point));
  }
}

// Locates the top-left corner of a cursor that should be placed under/near
// `point`, knowing that the font has a fixed width.
//
// NOTE(pag): `point` should already be translated by the `scroll_x` and
//            `scroll_y`.
QPointF CodeWidget::PrivateData::CursorPositionFixed(QPointF point) const {
  qreal half_width = space_width / 2;
  auto col_count = std::floor((point.x() + half_width) / space_width);
  
  // NOTE(pag): We always have an extra column of whitespace just before the
  //            first character of each line.
  return QPointF(
      col_count * space_width,
      std::floor(point.y() / line_height) * line_height);
}

// Locates the top-left corner of a cursor that should be placed under/near
// `point`, knowing that the font does not have a fixed width.
//
// NOTE(pag): `point` should already be translated by the `scroll_x` and
//            `scroll_y`.
QPointF CodeWidget::PrivateData::CursorPositionVariable(QPointF point) const {

  if (scene.entities.empty()) {
    return CursorPositionFixed(point);
  }

  auto x = point.x();
  auto y = point.y();

  auto line_index = static_cast<unsigned>(std::floor(y / line_height));
  if ((line_index + 1u) >= scene.logical_line_index.size()) {
    return CursorPositionFixed(point);
  }

  auto i = scene.logical_line_index[line_index];
  auto max_i = scene.logical_line_index[line_index + 1u];

  const Entity *prev_entity = nullptr;
  const Data *prev_data = nullptr;
  const Entity *entity = nullptr;
  const Data *data = nullptr;

  for (; i < max_i; ++i, prev_entity = entity, prev_data = data) {
    entity = &(scene.entities[i]);
    Q_ASSERT(entity->logical_line_number == static_cast<int>(line_index + 1u));

    // The cursor comes before `entity`.
    if (entity->x > x) {
      break;
    }

    auto [k, prefix_width] = CharacterPositionVariable(point, entity);
    if (k != -1) {
      return QPointF(entity->x + prefix_width,
                     static_cast<qreal>(line_index) * line_height);
    }
  }

  // There are no entities on this line, or there is no previous entity, and so
  // the cursor is before it.
  if (!entity || !prev_entity) {
    return CursorPositionFixed(point);
  }

  // The cursor is between two entities. Translate the point so that it's
  // as though there is no previous entity, then it's just about whitespace
  // calculation.
  QRectF r = prev_data->bounding_rect[
      prev_entity->data_index_and_config & kFormatMask];

  r.moveTo(prev_entity->x, 0);

  auto adj_pos = CursorPositionFixed(
      QPointF(x - (prev_entity->x + r.width()), y));

  // Readjust to account for the size of `prev_entity`.
  return QPointF(adj_pos.x() + r.width() + prev_entity->x, adj_pos.y());
}

// Always have one empty space on both sides margin, and keep the cursor in-
// bounds.
QPointF CodeWidget::PrivateData::ClampCursorPosition(QPointF point) const {
  qreal c_width = foreground_canvas.width() / dpi_ratio;
  qreal v_width = viewport.width();

  qreal c_height = foreground_canvas.height() / dpi_ratio;
  qreal v_height = viewport.height();
  return QPointF(
      std::max(
          space_width,
          std::min(point.x(), std::max<qreal>(c_width, v_width) - space_width)),
      std::max<qreal>(
          0,
          std::min(std::max(c_height - line_height, v_height - line_height),
                   std::min<qreal>(line_height * scene.num_lines, point.y()))));
}

// Locate the next cursor position (left or right, up or down).
QPointF CodeWidget::PrivateData::NextCursorPosition(
    QPointF curr_cursor, qreal dir_x, qreal dir_y) const {

  if (is_monospaced) {
    return NextCursorPositionFixed(curr_cursor, dir_x, dir_y, space_width);
  } else {
    return NextCursorPositionVariable(curr_cursor, dir_x, dir_y);
  }
}

// Locate the next cursor position (left or right, up or down) for a fixed-
// width font, or where the next character in the relevant direction has width
// `char_width`.
QPointF CodeWidget::PrivateData::NextCursorPositionFixed(
    QPointF curr_cursor, qreal dir_x, qreal dir_y, qreal char_width) const {

  qreal new_x = curr_cursor.x();
  if (dir_x != 0) {
    new_x += (dir_x * char_width);
  }
  
  qreal new_y = curr_cursor.y();
  if (dir_y != 0) {
    new_y += (dir_y * line_height);
  }
  
  return CursorPosition(QPointF(new_x, new_y));
}

QPointF CodeWidget::PrivateData::NextCursorPositionVariable(
    QPointF curr_cursor, qreal dir_x, qreal dir_y) const {

  // Hopefully eighths of the space width are smaller than the smallest
  // horizontal advance of a character in the font.
  auto incr = (space_width / 8.0) * dir_x;

  // Opportunistically search for the next X position of the cursor.
  qreal char_width = space_width;
  if (dir_x != 0) {
    auto curr_x = curr_cursor.x();
    auto guess_x = curr_x;
    for (; guess_x >= space_width; ) {
      guess_x += incr;
      auto new_x = CursorPositionVariable(QPointF(guess_x, curr_cursor.y())).x();
      if (new_x != curr_x) {
        char_width = std::abs(new_x - curr_x);
        break;
      }
    }
  }

  return NextCursorPositionFixed(curr_cursor, dir_x, dir_y, char_width);
}

// Locate the entity underneath `point`. Point corresponds to a viewport
// position.
//
// NOTE(pag): `point` should already be translated by the `scroll_x` and
//            `scroll_y`.
const Entity *CodeWidget::PrivateData::EntityUnderPoint(QPointF point) const {
  if (scene.entities.empty()) {
    return nullptr;
  }

  auto x = point.x();
  auto y = point.y();

  auto line_index = static_cast<unsigned>(std::floor(y / line_height));
  if ((line_index + 1u) >= scene.logical_line_index.size()) {
    return nullptr;
  }

  auto i = scene.logical_line_index[line_index];
  auto max_i = scene.logical_line_index[line_index + 1u];

  for (; i < max_i; ++i) {
    const Entity &e = scene.entities[i];
    if (e.x > x) {
      continue;
    }

    const Data &data = scene.data[e.data_index_and_config >> kFormatShift];
    QRectF r = data.bounding_rect[e.data_index_and_config & kFormatMask];

    qreal e_y = static_cast<qreal>(line_index) * line_height;

    r.moveTo(QPointF(e.x, e_y));
    if (r.contains(point)) {
      return &e;
    }
  }

  return nullptr;
}

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(const ConfigManager &config_manager,
                       const QString &model_id,
                       QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData(model_id)) {

  d->vertical_scrollbar = new QScrollBar(Qt::Vertical, this);
  d->vertical_scrollbar->setSingleStep(1);
  connect(d->vertical_scrollbar, &QScrollBar::valueChanged, this,
          &CodeWidget::OnVerticalScroll);

  d->horizontal_scrollbar = new QScrollBar(Qt::Horizontal, this);
  d->horizontal_scrollbar->setSingleStep(1);
  connect(d->horizontal_scrollbar, &QScrollBar::valueChanged, this,
          &CodeWidget::OnHorizontalScroll);

  auto vertical_layout = new QVBoxLayout;
  vertical_layout->setContentsMargins(0, 0, 0, 0);
  vertical_layout->setSpacing(0);
  vertical_layout->addSpacerItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
  vertical_layout->addStretch();
  vertical_layout->addWidget(d->horizontal_scrollbar);
  
  auto horizontal_layout = new QHBoxLayout;
  horizontal_layout->setContentsMargins(0, 0, 0, 0);
  horizontal_layout->setSpacing(0);
  horizontal_layout->addLayout(vertical_layout);
  horizontal_layout->addWidget(d->vertical_scrollbar);

  setContentsMargins(0, 0, 0, 0);
  setLayout(horizontal_layout);

  d->vertical_scrollbar->hide();
  d->horizontal_scrollbar->hide();

  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto &theme_manager = config_manager.ThemeManager();

  OnThemeChanged(theme_manager);  // Calls `RecomputeScene`.

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &CodeWidget::OnIndexChanged);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &CodeWidget::OnThemeChanged);
}

void CodeWidget::focusOutEvent(QFocusEvent *) {
  // Requests for context menus trigger `focusOutEvent`s prior to
  // `mouseReleaseEvent`.
  if (d->cursor && !d->click_was_secondary) {
    d->cursor.reset();
    d->selection_start_cursor.reset();
    d->tracking_selection = false;
    update();
  }
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

    vertical_pixel_delta =
        d->line_height * static_cast<int>(vertical_angle_delta * 1.0 / 120.0);

    horizontal_pixel_delta =
      d->line_height * static_cast<int>(horizontal_angle_delta * 1.0 / 120.0);
  }

  d->ScrollBy(horizontal_pixel_delta * -1, vertical_pixel_delta * -1);
  update();
}

void CodeWidget::paintEvent(QPaintEvent *) {
  d->RecomputeCanvas();

  QPainter blitter(this);
  blitter.setRenderHints(QPainter::Antialiasing);

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
        const Data &data = d->scene.data[e.data_index_and_config >> kFormatShift];
        unsigned rect_config = e.data_index_and_config & kFormatMask;
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

    // NOTE(pag): By setting `cs.background_color = QColor();` below, we make
    //            sure this painter won't be used, and so we don't anticipate
    //            Qt reporting painter device errors.
    QPainter dummy_bg_painter;

    for (auto it = re_it; it != re_end_it && it->first == related_entity_id;
         ++it) {
      const Entity &e = d->scene.entities[it->second];
      const Token &t = d->scene.tokens[e.token_index];
      Data &data = d->scene.data[e.data_index_and_config >> kFormatShift];
      unsigned rect_config = e.data_index_and_config & kFormatMask;

      qreal e_y = static_cast<qreal>(e.logical_line_number - 1) *
                  d->line_height;
      qreal x = e.x - d->scroll_x;
      qreal y = e_y - d->scroll_y;

      QRectF bounding_rect = data.bounding_rect[rect_config];
      bounding_rect.moveTo(x, y);

      ITheme::ColorAndStyle cs = d->theme->TokenColorAndStyle(t);
      cs.background_color = QColor();
      cs.foreground_color = highlight_foreground_color;

      d->PaintToken(blitter, dummy_bg_painter, data, rect_config, cs, x, y);
    }
  }

  // Draw the selection background, if any.
  std::optional<QRectF> selections[3u];
  QColor selection_color;
  QColor selected_text_color;
  if (d->cursor && d->selection_start_cursor) {

    QPointF top_left = d->cursor.value();
    QPointF bottom_right = d->selection_start_cursor.value();
    if (top_left.y() > bottom_right.y()) {
      std::swap(top_left, bottom_right);
    }

    // Selection contained within one line; the polygon is a rectange.
    if (top_left.y() == bottom_right.y()) {
      auto left_point = std::min(top_left.x(), bottom_right.x());
      auto right_point = std::max(top_left.x(), bottom_right.x());

      selections[0] = QRectF(
          left_point - d->scroll_x, top_left.y() - d->scroll_y,
          right_point - left_point, d->line_height);

    // Selection cross multiple lines.
    } else {
      selections[0] = QRectF(
          top_left.x() - d->scroll_x, top_left.y() - d->scroll_y,
          d->canvas_rect.width() + d->viewport.width(),
          d->line_height);

      auto bottom_left_y = top_left.y() + d->line_height;

      if (bottom_left_y < bottom_right.y()) {
        selections[1] = QRectF(
            -d->scroll_x, bottom_left_y - d->scroll_y,
            d->canvas_rect.width() + d->viewport.width(),
            bottom_right.y() - bottom_left_y);
      }

      selections[2] = QRectF(
          -d->scroll_x, bottom_right.y() - d->scroll_y,
          bottom_right.x(), d->line_height);
    }

    selection_color = d->theme->SelectionColor();
    selected_text_color = ITheme::ContrastingColor(selection_color);

    // Fill in up to three rectangles for the selection.
    for (auto &sel : selections) {
      if (sel) {
        blitter.fillRect(sel.value(), selection_color);
      }
    }

    // Go find all entities bounded by the selections, and repaint them as being
    // selected.
    auto start_index = static_cast<unsigned>(
        (top_left.y() + (d->line_height / 2)) / d->line_height);

    auto stop_index = static_cast<unsigned>(
        (bottom_right.y() + (d->line_height / 2)) / d->line_height);

    // Go through only the entities on the relevant lines. 
    for (auto l = start_index; l <= stop_index; ++l) {
      auto i = d->scene.logical_line_index[l];
      auto max_i = d->scene.logical_line_index[l + 1];

      // Inspect each entity on the line.
      for (; i < max_i; ++i) {
        const Entity &e = d->scene.entities[i];
        unsigned data_index = e.data_index_and_config >> kFormatShift;
        unsigned rect_config = e.data_index_and_config & kFormatMask;
        Data &data = d->scene.data[data_index];

        const Token &t = d->scene.tokens[e.token_index];
        ITheme::ColorAndStyle cs = d->theme->TokenColorAndStyle(t);
        cs.background_color = selection_color;
        cs.foreground_color = selected_text_color;

        qreal e_y = static_cast<qreal>(e.logical_line_number - 1) *
                    d->line_height;
        qreal x = e.x - d->scroll_x;
        qreal y = e_y - d->scroll_y;

        QRectF bounding_rect = data.bounding_rect[rect_config];
        bounding_rect.moveTo(x, y);

        for (auto &sel : selections) {
          if (!sel) {
            continue;
          }
          
          // The selection fully contains this entity; paint it.
          if (sel->contains(bounding_rect)) {
            d->PaintToken(blitter, blitter, data, rect_config, cs, x, y);
            break;

          // The selection is unrelated to this entity.
          } else if (!sel->intersects(bounding_rect)) {
            continue;
          }

          Data new_data;

          qsizetype start_k = 0;
          qsizetype stop_k = data.text.size();

          // Top-left intersection case (highlight a suffix of `data`).
          if (bounding_rect.x() < sel->x()) {
            auto [index, width] = d->CharacterPosition(
                QPointF(sel->x(), e_y), &e);
            Q_ASSERT(0 < index);
            x += width;
            start_k = index;
          }

          // Bottom-right intersection case (highlight a prefix of `data`).
          if (sel->topRight().x() < bounding_rect.topRight().x()) {
            stop_k = d->CharacterPosition(
                QPointF(sel->topRight().x(), e_y), &e).first;
            Q_ASSERT(0 < stop_k);
          }

          for (auto k = start_k; 0 <= k && k < stop_k; ++k) {
            new_data.text += data.text[k];
          }

          d->PaintToken(blitter, blitter, new_data, rect_config, cs, x, y);
        }
      }
    }
  }

  // Paint the cursor.
  if (d->cursor && d->theme_cursor_color.isValid()) {
    QRectF cursor(d->cursor->x() + kCursorDisp - d->scroll_x,
                  d->cursor->y() - d->scroll_y, kCursorWidth, d->line_height);
    blitter.fillRect(cursor, d->theme_cursor_color);
  }

  blitter.end();
}

void CodeWidget::mouseReleaseEvent(QMouseEvent *event) {
  auto click_was_primary = d->click_was_primary;
  auto click_was_secondary = d->click_was_secondary;
  d->click_was_primary = false;
  d->click_was_secondary = false;

  if (event->buttons() & Qt::LeftButton) {
    return;
  }

  // Web browsers and SciTools Understand's Browse Mode all enact the change of
  // website on mouse release rather than press. This has inspired a habit in
  // Jay to use selections as a way of getting his cursor on an entity without
  // having the view change. 
  if (!d->tracking_selection && click_was_primary && !click_was_secondary) {
    d->selection_start_cursor.reset();

    if (d->current_entity) {
      emit RequestPrimaryClick(d->CreateModelIndex(d->current_entity));
    }
    return;
  }

  d->tracking_selection = false;

  Q_ASSERT(d->selection_start_cursor.has_value());
  Q_ASSERT(d->cursor.has_value());
  if (d->selection_start_cursor.value() == d->cursor.value()) {
    d->selection_start_cursor.reset();
    return;
  }

  update();
}

void CodeWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton)) {
    return;
  }

  if (!d->selection_start_cursor && d->cursor && d->click_was_primary) {
    d->tracking_selection = true;
    d->selection_start_cursor = d->cursor;
  }

  mousePressEvent(event);

  QPointF rel_position = event->position();
  auto x = d->scroll_x + rel_position.x();
  auto y = d->scroll_y + rel_position.y();

  QPointF scrolled_xy(x, y);
  auto curr_cursor = d->CursorPosition(scrolled_xy);
  if (curr_cursor == d->cursor.value()) {
    return;
  }

  d->cursor = curr_cursor;
  update();
}

void CodeWidget::mousePressEvent(QMouseEvent *event) {

  QPointF rel_position = event->position();
  auto x = d->scroll_x + rel_position.x();
  auto y = d->scroll_y + rel_position.y();

  QPointF scrolled_xy(x, y);
  QPointF new_cursor = d->CursorPosition(scrolled_xy);

  const Entity *entity = d->EntityUnderPoint(scrolled_xy);

  d->token_model.token = {};
  d->click_was_primary = event->buttons() & Qt::LeftButton;
  d->click_was_secondary = event->buttons() & Qt::RightButton;

  if (d->selection_start_cursor && !d->tracking_selection &&
      (d->click_was_primary || !d->click_was_secondary)) {
    d->selection_start_cursor.reset();
  }

  // When we click, we want to set the cursor. If we right click and we have a
  // selection, then we don't want to change the cursor or the current line. If
  // we right click and don't have a selection, when we want to move the cursor.
  if (d->click_was_primary ||
      ((d->click_was_secondary && !d->selection_start_cursor))) {
    d->cursor = new_cursor;

    // Calculate the current line index based on the clamped cursor.
    auto new_current_line_index = static_cast<int>(
        std::floor(d->cursor->y() / d->line_height));
    if (new_current_line_index != d->current_line_index) {
      d->current_line_index = new_current_line_index;
    }
  }

  d->current_entity = entity;

  // Calculate the index of the current line.
  if (!d->tracking_selection) {
    if (!d->click_was_primary && d->click_was_secondary) {
      emit RequestSecondaryClick(d->CreateModelIndex(entity));
    }
  }

  update();
}

void CodeWidget::keyPressEvent(QKeyEvent *event) {
  qreal dx = 0;
  qreal dy = 0;

  switch (event->key()) {

    // Pressing the up arrow moves the current line up, and maybe triggers
    // a scroll.
    case Qt::Key_Up:
      dy = -1;
      break;

    // Pressing the down arrow moves the current line down, and maybe triggers
    // a scroll.
    case Qt::Key_Down:
      dy = 1;
      break;

    // Pressing the left arrow moves the cursor left, and maybe triggers a
    // scroll.
    case Qt::Key_Left:
      dx = -1;
      break;

    // Pressing the right arrow moves the cursor left, and maybe triggers a
    // scroll.
    case Qt::Key_Right:
      dx = 1;
      break;

    default:
      if (d->current_entity && d->cursor) {
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
            d->CreateModelIndex(d->current_entity));
      }
      break;
  }

  auto need_repaint = false;
  if (d->selection_start_cursor) {
    d->selection_start_cursor.reset();
    d->tracking_selection = false;
    need_repaint = true;
  }

  if (!d->cursor || (dx == 0 && dy == 0)) {
    if (need_repaint) {
      update();
    }
    return;
  }

  // Figure out the next cursor position.
  auto new_cursor = d->NextCursorPosition(d->cursor.value(), dx, dy);
  auto new_current_line_index = static_cast<int>(
      std::floor(new_cursor.y() / d->line_height));

  // Set the current line, and possibly scroll us up or down.
  if (new_current_line_index != d->current_line_index) {
    need_repaint = true;
    d->current_line_index = new_current_line_index;

    // Detect if we need to scroll down to follow the current line.
    if (dy == 1 && (((new_current_line_index + 1) * d->line_height) >
                    (d->scroll_y + d->viewport.height()))) {
      d->ScrollBy(0, d->line_height);

    // Detect if we need to scroll up to follow the current line.
    } else if (dy == -1 && ((new_current_line_index * d->line_height) <
                            d->scroll_y)) {
      d->ScrollBy(0, -d->line_height);
    }
  }

  // Set the current cursor, and possible scroll us left or right.
  if (new_cursor != d->cursor.value()) {
    need_repaint = true;
    d->cursor = new_cursor;
    d->current_entity = d->EntityUnderPoint(new_cursor);

    if (dx == -1 && (new_cursor.x() - d->scroll_x) < 0) {
      d->ScrollBy(
          static_cast<int>(-std::ceil(d->cursor->x() - new_cursor.x())), 0);

      // If we get to the right edge of our left margin, then put us back at
      // zero.
      if (d->scroll_x <= d->space_width) {
        d->scroll_x = 0;
      }

    } else if (dx == 1 && ((new_cursor.x() - d->scroll_x) >=
                           d->viewport.width())) {
      d->ScrollBy(
          static_cast<int>(std::ceil(new_cursor.x() - d->cursor->x())), 0);
    }
  }

  if (need_repaint) {
    update();
  }
}

void CodeWidget::PrivateData::UpdateScrollbars(void) {
  return;
  if (scene.entities.empty()) {
    horizontal_scrollbar->hide();
    vertical_scrollbar->hide();
    return;
  }

  auto s_width = canvas_rect.width();
  auto s_height = canvas_rect.height();

  auto v_width = viewport.width();
  auto v_height = viewport.height();

  if (v_width < s_width) {
    horizontal_scrollbar->show();
    horizontal_scrollbar->setMaximum(static_cast<int>(s_width - v_width));

  } else {
    horizontal_scrollbar->hide();
    horizontal_scrollbar->setMaximum(0);
  }

  if (v_height < s_height) {
    vertical_scrollbar->show();
    vertical_scrollbar->setMaximum(static_cast<int>(s_height - v_height));

  } else {
    vertical_scrollbar->hide();
    vertical_scrollbar->setMaximum(0);
  }
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

  // TODO(pag): `scroll_x` and `scroll_y` probably don't make sense anymore.
  if (cursor) {
    cursor = CursorPosition(cursor.value());
    current_entity = EntityUnderPoint(cursor.value());
  }
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

  Q_ASSERT(0 < line_height);

  max_char_width = static_cast<int>(std::ceil(std::max(
      {font_metrics_bi.maxWidth(), font_metrics_b.maxWidth(),
       font_metrics_i.maxWidth(), font_metrics.maxWidth()})));

  Q_ASSERT(0 < max_char_width);

  // Use a painter for also figuring out the maximum character size, as a
  // bounding rect from a painter could be bigger.
  {
    QPixmap dummy_pixmap(max_char_width * 4, line_height * 4);
    QPainter p(&dummy_pixmap);
    p.setFont(bold_italic_font);
    auto r = p.boundingRect(QRectF(max_char_width, line_height,
                                   max_char_width * 3, line_height * 3),
                            "W", to);
    
    max_char_width = std::max(
        max_char_width, static_cast<int>(std::ceil(r.width())));

    line_height = std::max(
        line_height, static_cast<int>(std::ceil(r.height())));
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
            QImage::Format_ARGB32_Premultiplied);

  QImage bg(static_cast<int>(canvas_rect.width() * dpi_ratio),
            static_cast<int>(canvas_rect.height() * dpi_ratio),
            QImage::Format_ARGB32_Premultiplied);

  fg.setDevicePixelRatio(dpi_ratio);
  bg.setDevicePixelRatio(dpi_ratio);

  // Fill the contents with transparent pixels, rather than leaving them
  // undefined.
  fg.fill(0);
  bg.fill(0);

  QPainter fg_painter(&fg);
  QPainter bg_painter(&bg);

  bg_painter.setRenderHints(QPainter::SmoothPixmapTransform);
  fg_painter.setRenderHints(QPainter::TextAntialiasing |
                            QPainter::Antialiasing |
                            QPainter::SmoothPixmapTransform);
  fg_painter.setFont(bold_italic_font);

  monospace[0] = QChar::Space;
  space_rect = fg_painter.boundingRect(canvas_rect, monospace, to);
  space_width = space_rect.width();

  Q_ASSERT(0 < space_width);

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
    Data &data = scene.data[e.data_index_and_config >> kFormatShift];
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
    unsigned rect_config = (cs.bold ? kBoldMask : 0u) |
                           (cs.italic ? kItalicMask : 0u);
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

    token_rect.moveTo(QPointF(x, y));
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
  d->theme_cursor_color = d->theme->CursorColor();
  d->theme_foreground_color = d->theme->DefaultForegroundColor();
  d->theme_background_color = d->theme->DefaultBackgroundColor();

  // If the old font is the same as the new font, then the entity positions
  // and data rects remain the same, so we don't need to recompute the scene.
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

// Invoked when the set of macros to be expanded changes.
void CodeWidget::OnExpandMacros(const QSet<RawEntityId> &macros_to_expand) {

  // Look for macros that weren't expanded before, but are now requested to be
  // expanded.
  for (auto macro_id : macros_to_expand) {
    auto it = d->scene.expanded_macros.find(macro_id);
    if (it != d->scene.expanded_macros.end() && !(it->second)) {
      d->scene_changed = true;
      break;
    }
  }

  // Look for macros that were expanded before, and now aren't being expanded.
  if (!d->scene_changed) {
    for (auto [macro_id, expanded] : d->scene.expanded_macros) {
      if (expanded && !macros_to_expand.contains(macro_id)) {
        d->scene_changed = true;
        break;
      }
    }
  }

  d->macros_to_expand = macros_to_expand;

  if (d->scene_changed) {
    update();
  }
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
  d->click_was_primary = false;
  d->click_was_secondary = false;
  d->current_entity = nullptr;
  d->cursor.reset();
  d->selection_start_cursor.reset();
  d->tracking_selection = false;
  d->token_model.token = {};
  d->scroll_x = 0;
  d->scroll_y = 0;
  d->current_line_index = -1;
  d->scene_overrides.clear();
  d->token_tree = token_tree;
  update();
}

}  // namespace mx::gui

#include "CodeWidget.moc"
