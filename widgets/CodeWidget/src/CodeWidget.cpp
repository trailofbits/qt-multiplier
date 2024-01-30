/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/CodeWidget.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QRectF>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>
#include <multiplier/AST/AddrLabelExpr.h>
#include <multiplier/AST/DeclRefExpr.h>
#include <multiplier/AST/LabelStmt.h>
#include <multiplier/AST/MemberExpr.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/MacroVAOpt.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include "GoToLineWidget.h"

#ifdef __APPLE__
# include "MacosUtils.h"
#endif

namespace mx::gui {
namespace {

static const QKeySequence kCopyKeqSequence("Ctrl+C");
static const QKeySequence kFindKeqSequence("Ctrl+F");
static const QKeySequence kGotoLineKeqSequence("Ctrl+L");
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
};

struct Data {
  QString text;

  QString selection;

  bool bounding_rect_valid[4u];

  // Normal, bold, italic, and bold+italic.
  QRectF bounding_rect[4u];
};

struct Scene {
  // The complete, (nearly) original document.
  QString document;

  // Sorted list of entities in this scene. This is sorted by
  // `(Entity::x, Entity::y)` positions.
  std::vector<Entity> entities;

  // For logical (one-based) line number `N`, `logical_line_index[N - 1]` is
  // the index into `entities` of the first entity on that line.
  std::vector<unsigned> logical_line_index;

  // The file line number associated with the `N`th entity. `0` if invalid,
  // negative if in a macro.
  std::vector<int> file_line_number;

  // Offset of the beginning of each entity in the total text of the document.
  std::vector<int> begin_of_entity_in_document;

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

  // Maps things like fragments to where they should/could logically begin.
  std::unordered_map<RawEntityId, unsigned> entity_begin_offset;

  // Maps displayed fragments to where they should/could logically begin.
  std::unordered_map<RawEntityId, unsigned> fragment_begin_offset;

  // Given that `N` is a logical line number, `physical_line_number[N - 1]` is
  // a physical line number. These can have repeats, and negative numbers, and
  // can't be relied upon as being in 
  std::vector<int> physical_line_number;

  // Maximum number of characters on any given line.
  int max_logical_columns{1};

  // Number of logical lines in this scene.
  int num_lines{1};

  // Number file line number seen.
  int num_file_lines{1};
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
  int line_number{0};
  unsigned token_index{0u};
  bool added_anything{false};
  RawEntityId related_entity_id{kInvalidEntityId};
  FileLocationCache file_cache;
  QString token_data;
  TokenRange macro_use_tokens;

  inline SceneBuilder(void) {
    scene.logical_line_index.emplace_back(0u);
  }

  void BeginToken(const Token &tok) {
    related_entity_id = tok.related_entity_id().Pack();
    document_offset = static_cast<int>(scene.document.size());
    line_number = 0;

    TokenRange file_toks;
    if (expansion_depth) {
      file_toks = macro_use_tokens;
    } else {
      file_toks = TokenRange(tok).file_tokens();
    }
    auto first_file_loc = file_toks.front().location(file_cache);
    auto last_file_loc = file_toks.back().location(file_cache);
    if (!first_file_loc) {
      return;
    }

    Q_ASSERT(last_file_loc.has_value());

    auto first_line_num = static_cast<int>(first_file_loc->first);
    auto last_line_num = static_cast<int>(last_file_loc->first);
    
    // The token, or the extent of the use of the macro, are all on one line.
    if (first_line_num == last_line_num) {
      line_number = first_line_num;
    }

    scene.num_file_lines = std::max(scene.num_file_lines, line_number);

    // If we're in an expansion, then negate the line number to signal that it
    // should be specially colored.
    if (expansion_depth) {
      line_number = -line_number;
    }
  }

  void AddNewLine(void) {
    AddChar(QChar::LineFeed);
    AddEntity();
    logical_column_number = 1u;
    scene.num_lines += 1;
    scene.logical_line_index.emplace_back(
        static_cast<unsigned>(scene.entities.size()));

    if (0 < line_number) {
      line_number += 1;
    } 
  }

  void AddChar(QChar ch) {
    if (!token_start_column) {
      token_start_column = logical_column_number;
    }
    scene.document.append(ch);
    token_data.append(ch);
    token_length += 1;
    logical_column_number += 1u;
    added_anything = true;
  }

  void EndToken(Token tok) {
    AddEntity();
    if (added_anything) {

      // The `TokenTree` API often gives us macro tokens, but if we can get a
      // parsed token, then we actually want that, as it makes scroll to entity
      // much easier.
      if (auto parsed_tok = tok.parsed_token()) {
        scene.tokens.emplace_back(std::move(parsed_tok));
      } else {
        scene.tokens.emplace_back(std::move(tok));
      }

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

      for (auto &v : d.bounding_rect_valid) {
        v = false;
      }

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

    scene.file_line_number.push_back(line_number);
    scene.begin_of_entity_in_document.push_back(document_offset);

    token_start_column = 0u;
    token_length = 0;
    document_offset = static_cast<int>(scene.document.size());
  }

  Scene TakeScene(void) & {
    scene.logical_line_index.resize(static_cast<unsigned>(scene.num_lines + 1));
    scene.logical_line_index.back()
        = static_cast<unsigned>(scene.entities.size());
    std::sort(scene.related_entity_ids.begin(), scene.related_entity_ids.end());

    auto max_i = scene.logical_line_index.size() - 1u;
    int last_line_num = -1;

    // Paint the line numbers.
    for (auto i = 0u; i < max_i; ++i) {
      line_number = 0;
      auto backup_line_number = 0;
      auto max_e = scene.logical_line_index[i + 1u];
      
      // Go get the minimum line number. Some might be negative because of a
      // macro expansion on the line, so we want to highlight that an expansion
      // happened somewhere on the line.
      for (auto e = scene.logical_line_index[i]; e < max_e; ++e) {
        if (auto ln = scene.file_line_number[e]) {
          if (!line_number) {
            line_number = ln;
          } else if (std::abs(ln) >= std::abs(last_line_num)) {
            line_number = std::min(line_number, ln);
          
          // TODO(pag): Sometimes see these; diagnose their source.
          } else if (!backup_line_number) {
            backup_line_number = ln;
          } else {
            backup_line_number = std::min(backup_line_number, ln);
          }
        }
      }

      if (!line_number) {
        line_number = backup_line_number ? backup_line_number : last_line_num;
      }

      scene.physical_line_number.emplace_back(line_number);
      last_line_num = -std::abs(line_number);
    }

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
  QString selection;

  bool HasTokenOrSelection(void) const noexcept {
    return token || !selection.isEmpty();
  }

  virtual ~TokenModel(void) = default;

  TokenModel(const QString &model_id_, QObject *parent = nullptr)
      : IModel(parent),
        model_id(model_id_) {}

  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL {
    if (!hasIndex(row, column, parent)) {
      return {};
    }
    if (!row && !column && !parent.isValid() && HasTokenOrSelection()) {
      return createIndex(0, 0, token.id().Pack());
    }
    return {};
  }

  QModelIndex parent(const QModelIndex &) const Q_DECL_FINAL {
    return {};
  }

  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL {
    return parent.isValid() || !HasTokenOrSelection() ? 0 : 1;
  }

  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL {
    return parent.isValid() || !HasTokenOrSelection() ? 0 : 1;
  }

  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL {
    if (index.column() != 0 || index.row() != 0 || !HasTokenOrSelection()) {
      return {};
    }

    switch (role) {
      case IModel::EntityRole:
        if (index.internalId() == token.id().Pack()) {
          return QVariant::fromValue<VariantEntity>(token);
        }
      case IModel::TokenRangeDisplayRole:
        if (index.internalId() == token.id().Pack()) {
          return QVariant::fromValue<TokenRange>(token);
        }
      case IModel::ModelIdRole:
        return model_id;
      case Qt::DisplayRole:
        return text;
      case CodeWidget::SelectedTextRole:
        return selection;
    }
    return {};
  }

  //! Returns the column names for the tree view header
  QVariant headerData(int, Qt::Orientation, int) const Q_DECL_FINAL {
    return {};
  }
};

// Consistent way to initialize our `QPainter`s.
inline static void InitializePainterOptions(QPainter &p) {
  p.setRenderHints(QPainter::Antialiasing |
                   QPainter::TextAntialiasing |
                   QPainter::SmoothPixmapTransform);
}

struct Position {
  qreal scale{0};
  int logical{-1};
  int relative{-1};
  int physical{-1};
};

}  // namespace

struct CodeWidget::PrivateData {

  uint64_t version_number{0};

  // Should browse mode be enabled or disabled? That is, when a user clicks on
  // something, should it trigger the `GoToEntity` action or not?
  bool browse_mode{false};

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

  // Theme defaults.
  QFont theme_font;
  QColor theme_cursor_color;
  QColor theme_foreground_color;
  QColor theme_background_color;

  // Calculated shape and width of a single space in this font. Generally we
  // make this the size of a bold and italic space.
  QRectF space_rect;
  qreal space_width{0};

  // Left and right margin.
  qreal left_margin{0};
  qreal right_margin{0};
  
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

  int selection_start_offset{-1};
  int selection_end_offset{-1};

  bool click_was_primary{false};
  bool click_was_secondary{false};

  bool tracking_selection{false};

  // Determined during painting.
  int line_height{0};
  int max_char_width{0};
  bool is_monospaced{false};

  // The index of the current line to highlight. This is a logical line index.
  int current_line_index{-1};

  // The current entity under the cursor.
  const Entity *current_entity{nullptr};

  // The previous highlighted entity.
  const Entity *prev_highlighted_entity{nullptr};

  TokenModel token_model;

  // Data structure keeping track of the logical things to render, and
  // where to render them. The act of rendering updates `Entity`s in the
  // scene to keep track of their physical locations within `*_canvas`.
  Scene scene;

  // Semi-persistent layers of the "picture" of what is rendered.
  QImage background_canvas;
  QImage foreground_canvas;
  QImage highlight_canvas;
  QImage line_number_canvas;

  // Sets of entities that configure what gets shown from `token_tree`.
  QSet<RawEntityId> macros_to_expand;
  QMap<RawEntityId, QString> new_entity_names;
  QSet<RawEntityId> scene_overrides;

  // Search results.
  std::vector<std::pair<qsizetype, qsizetype>> search_result_list;

  QWidget *code_area{nullptr};
  QScrollBar *horizontal_scrollbar{nullptr};
  QScrollBar *vertical_scrollbar{nullptr};
  SearchWidget *search_widget{nullptr};
  GoToLineWidget *goto_line_widget{nullptr};

  std::optional<OpaqueLocation> last_location;
  VariantEntity last_entity_for_location;

  inline PrivateData(const QString &model_id)
      : monospace(" "),
        to(Qt::AlignLeft),
        dpi_ratio(qApp->devicePixelRatio()),
        token_model(model_id) {}

  void UpdateScrollbars(void);
  void RecomputeScene(void);
  void RecomputeCanvas(void);
  void RecomputeLineNumbers(void);
  void RecomputeHighlights(void);
  void RecomputeSelection(QPainter &blitter);
  void ScrollToPoint(CodeWidget *self, QPointF, bool take_focus,
                     LocationChangeReason reason);
  void ScrollToEntityOffset(CodeWidget *self, unsigned offset,
                            bool take_focus, LocationChangeReason reason);

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

  std::pair<const Entity *, int> EntityAtDocumentOffset(int offset) const;

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

  CodeWidget::OpaquePosition YDimensionToPosition(qreal y) const;
  qreal PositionToYDimension(CodeWidget::OpaquePosition pos) const;

  // Capture an "opaque" representation of the current location in the code.
  CodeWidget::OpaqueLocation Location(void);

  void ClampScrollXY(void);

  void SetLocation(CodeWidget::OpaqueLocation loc);
  void SetCursor(CodeWidget::OpaqueLocation loc);

  template <typename CB>
  void TriggerScrollbarUpdate(CB cb);
};

// Apply some change to the `scroll_x` and `scroll_y`, and trigger the
// relevant change in the scrollbars.
template <typename CB>
void CodeWidget::PrivateData::TriggerScrollbarUpdate(CB cb) {
  auto old_scroll_x = horizontal_scrollbar->value();
  auto old_scroll_y = vertical_scrollbar->value();

  // Force the values to be in sync.
  scroll_x = old_scroll_x;
  scroll_y = old_scroll_y;

  cb();

  ClampScrollXY();

  if (horizontal_scrollbar->maximum()) {
    if (auto delta_x = scroll_x - old_scroll_x) {
      horizontal_scrollbar->setValue(old_scroll_x + delta_x);
    }
  }

  if (vertical_scrollbar->maximum()) {
    if (auto delta_y = scroll_y - old_scroll_y) {
      vertical_scrollbar->setValue(old_scroll_y + delta_y);
    }
  }
}

// Fills `model` (e.g. `PrivateData::token_model`) with information from
// `entity` sufficient to satisfy the `IModel` interface, so that we can
// publish `QModelIndex`es in our signals, e.g. `RequestPrimaryClick`.
QModelIndex CodeWidget::PrivateData::CreateModelIndex(const Entity *entity) {
  if (!entity) {
    token_model.token = {};
    token_model.text = token_model.selection;
  } else {
    token_model.token = scene.tokens[entity->token_index];
    token_model.text
        = scene.data[entity->data_index_and_config >> kFormatShift].text;
  }
  return token_model.index(0, 0, {});
}

// Import a choice node.
void CodeWidget::PrivateData::ImportChoiceNode(
    SceneBuilder &b, ChoiceTokenTreeNode node) {

  std::optional<TokenTreeNode> chosen_node;
  RawEntityId chosen_fragment_id = kInvalidEntityId;
  unsigned eo = static_cast<unsigned>(b.scene.entities.size());

  for (auto &item : node.children()) {
    RawEntityId fragment_id = item.first.id().Pack();
    
    // Keep track of fragment locations.
    b.scene.entity_begin_offset.emplace(fragment_id, eo);

    if (scene_overrides.contains(fragment_id) || !chosen_node) {
      chosen_node.reset();
      chosen_node.emplace(std::move(item.second));
      chosen_fragment_id = fragment_id;
    }
  }

  if (chosen_node) {
    b.scene.fragment_begin_offset.emplace(chosen_fragment_id, eo);
    ImportNode(b, std::move(chosen_node.value()));
  }
}

// Import a substitution node.
void CodeWidget::PrivateData::ImportSubstitutionNode(
    SceneBuilder &b, SubstitutionTokenTreeNode node) {

  RawEntityId def_id = kInvalidEntityId;
  std::optional<Macro> macro;
  auto sub = node.macro();
  if (std::holds_alternative<MacroSubstitution>(sub)) {
    macro = std::get<MacroSubstitution>(sub);

    // Global expansion of a macro is based on the definition ID.
    if (auto exp = MacroExpansion::from(macro.value())) {
      if (auto def = exp->definition()) {
        def_id = def->id().Pack();
      }
    }
  } else {
    macro = std::get<MacroVAOpt>(sub);
  }

  const RawEntityId macro_id = macro->id().Pack();

  // Keep track of which macros were expanded.
  auto expanded = macros_to_expand.contains(macro_id);
  if (def_id != kInvalidEntityId) {
    expanded = expanded || macros_to_expand.contains(def_id);
    b.scene.expanded_macros.emplace(def_id, expanded);
  }
  b.scene.expanded_macros.emplace(macro_id, expanded);

  // Keep track of macro locations.
  b.scene.entity_begin_offset.emplace(
      macro_id, static_cast<unsigned>(b.scene.entities.size()));

  if (expanded) {
    if (!b.expansion_depth) {
      b.macro_use_tokens = macro->use_tokens().file_tokens();
    }
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

  RawEntityId related_entity_id = token.related_entity_id().Pack();
  if (related_entity_id != kInvalidEntityId) {

    // Support entity renaming.
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
        for (auto i = 0u; i < kTabWidth; ++i) {
          b.AddChar(QChar::Space);
        }
        break;

      case QChar::Space:
      case QChar::Nbsp:
        b.AddChar(QChar::Space);
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

  const qreal x = point.x();

  // The cursor comes before `entity`.
  if (entity->x > x) {
    return {-1, 0.0};
  }

  Q_ASSERT(entity->x >= left_margin);

  qreal line_index = entity->logical_line_number - 1;
  QRectF text_rect = data->bounding_rect[text_config_index];
  qreal entity_y = line_index * line_height;

  text_rect.moveTo(QPointF(entity->x, entity_y));
  if (!text_rect.contains(point)) {
    return {-1, 0.0};
  }

  QPixmap dummy_pixmap(static_cast<int>(text_rect.width()),
                       static_cast<int>(text_rect.height()));

  // Configure the font based on the formatting of the entity. The bold/italic
  // affects character sizes.
  QFont font = theme_font;
  if (text_config_index & kBoldMask) {
    font.setWeight(QFont::DemiBold);
  }
  if (text_config_index & kItalicMask) {
    font.setItalic(true);
  }

  QPainter p(&dummy_pixmap);
  InitializePainterOptions(p);
  p.setFont(font);

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
  const Entity *entity = nullptr;

  for (; i < max_i; ++i, prev_entity = entity) {
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
  const Data *prev_data = &(scene.data[
      prev_entity->data_index_and_config >> kFormatShift]);
  QRectF r = prev_data->bounding_rect[
      prev_entity->data_index_and_config & kFormatMask];

  r.moveTo(prev_entity->x, 0);

  auto adj_pos = CursorPositionFixed(
      QPointF(x - (prev_entity->x + r.width()), y));

  // Readjust to account for the size of `prev_entity`.
  return QPointF(adj_pos.x() + r.width() + prev_entity->x, adj_pos.y());
}

// Always have margin on both sides margin, and keep the cursor in-
// bounds.
QPointF CodeWidget::PrivateData::ClampCursorPosition(QPointF point) const {
  qreal c_width = foreground_canvas.width() / dpi_ratio;
  qreal v_width = viewport.width();

  qreal c_height = foreground_canvas.height() / dpi_ratio;
  qreal v_height = viewport.height();
  return QPointF(
      std::max(
          left_margin,
          std::min(point.x(), std::max<qreal>(c_width, v_width) - right_margin)),
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

  // Hopefully sixteenths of the space width are smaller than the smallest
  // horizontal advance of a character in the font.
  auto incr = (space_width / 16.0) * dir_x;

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

std::pair<const Entity *, int> CodeWidget::PrivateData::EntityAtDocumentOffset(
    int offset) const {

  auto it_begin = scene.begin_of_entity_in_document.begin();
  auto it_end = scene.begin_of_entity_in_document.end();
  auto it = std::upper_bound(it_begin, it_end, offset);
  if (it == it_begin) {
    return {nullptr, -1};
  }

  --it;

  for (; it != it_end; ++it) {
    auto eo = static_cast<unsigned>(it - it_begin);
    const Entity *entity = &(scene.entities[eo]);
    auto begin_offset = scene.begin_of_entity_in_document[eo];
    if (begin_offset > offset) {
      break;
    }

    const Data &data = scene.data[
        entity->data_index_and_config >> kFormatShift];

    if ((begin_offset + data.text.size()) < offset) {
      continue;
    }

    Q_ASSERT(begin_offset <= offset);
    return {entity, offset - begin_offset};
  }

  return {nullptr, -1};
}

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(const ConfigManager &config_manager,
                       const QString &model_id, bool browse_mode,
                       QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData(model_id)) {

  config_manager.ActionManager().Register(
      this, "com.trailofbits.action.ToggleBrowseMode",
      &CodeWidget::OnToggleBrowseMode);

  d->browse_mode = browse_mode;

  d->vertical_scrollbar = new QScrollBar(Qt::Vertical, this);
  d->vertical_scrollbar->setSingleStep(1);
  connect(d->vertical_scrollbar, &QScrollBar::valueChanged, this,
          &CodeWidget::OnVerticalScroll);

  d->horizontal_scrollbar = new QScrollBar(Qt::Horizontal, this);
  d->horizontal_scrollbar->setSingleStep(1);
  connect(d->horizontal_scrollbar, &QScrollBar::valueChanged, this,
          &CodeWidget::OnHorizontalScroll);

  d->search_widget = new SearchWidget(
      config_manager.MediaManager(), SearchWidget::Mode::Search, this);
  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, &CodeWidget::OnSearchParametersChange);

  connect(d->search_widget, &SearchWidget::ShowSearchResult,
          this, &CodeWidget::OnShowSearchResult);

  d->goto_line_widget = new GoToLineWidget(this);
  connect(d->goto_line_widget, &GoToLineWidget::LineNumberChanged,
          this, &CodeWidget::OnGoToLineNumber);

  d->code_area = new QWidget();
  d->code_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  d->code_area->setMinimumWidth(200);
  d->code_area->setMinimumHeight(100);

  auto vertical_layout = new QVBoxLayout;
  vertical_layout->setContentsMargins(0, 0, 0, 0);
  vertical_layout->setSpacing(0);
  vertical_layout->addWidget(d->code_area, 1);
  vertical_layout->addWidget(d->horizontal_scrollbar);
  
  auto horizontal_layout = new QHBoxLayout;
  horizontal_layout->setContentsMargins(0, 0, 0, 0);
  horizontal_layout->setSpacing(0);
  horizontal_layout->addLayout(vertical_layout, 1);
  horizontal_layout->addWidget(d->vertical_scrollbar);

  auto search_layout = new QVBoxLayout;
  search_layout->setContentsMargins(0, 0, 0, 0);
  search_layout->setSpacing(0);
  search_layout->addLayout(horizontal_layout, 1);
  search_layout->addWidget(d->search_widget);

  setContentsMargins(0, 0, 0, 0);
  setLayout(search_layout);

  d->search_widget->hide();
  d->vertical_scrollbar->hide();
  d->horizontal_scrollbar->hide();
  d->code_area->installEventFilter(this);

  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto &theme_manager = config_manager.ThemeManager();

  OnThemeChanged(theme_manager);  // Calls `RecomputeScene`.

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &CodeWidget::OnIndexChanged);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &CodeWidget::OnThemeChanged);
}

void CodeWidget::focusInEvent(QFocusEvent *) {
  if (d->last_location) {
    d->SetCursor(std::move(d->last_location.value()));
    d->last_location.reset();
  }

  if (0 < d->space_width && 0 < d->line_height) {
    emit LocationChanged(kExternalFocusChange);
  }
}

void CodeWidget::focusOutEvent(QFocusEvent *) {
  d->last_location.reset();
  if (0 < d->space_width && 0 < d->line_height) {
    d->last_location = d->Location();
    emit LocationChanged(kExternalFocusChange);
  }

  // Requests for context menus trigger `focusOutEvent`s prior to
  // `mouseReleaseEvent`.
  if (d->cursor && !d->click_was_secondary) {
    d->cursor.reset();
    d->selection_start_cursor.reset();
    d->tracking_selection = false;
    update();
  }
}

bool CodeWidget::eventFilter(QObject *object, QEvent *event) {
  if (object == d->code_area) {
    if (auto re = dynamic_cast<QResizeEvent *>(event)) {
      QSize new_size = re->size();
      d->viewport.setWidth(new_size.width());
      d->viewport.setHeight(new_size.height());
      d->RecomputeLineNumbers();
      d->UpdateScrollbars();
      update();
    }
  }
  return false;
}

void CodeWidget::wheelEvent(QWheelEvent *event) {

  qreal vertical_pixel_delta = 0;
  qreal horizontal_pixel_delta = 0;

  if (auto pixel_delta_point = event->pixelDelta(); !pixel_delta_point.isNull()) {
    vertical_pixel_delta = pixel_delta_point.y();
    horizontal_pixel_delta = pixel_delta_point.x();

  // High resolution gaming mice are capable of returning fractions of what is
  // usually considered a single mouse wheel turn.
  } else {
    auto vertical_angle_delta = event->angleDelta().y();
    auto horizontal_angle_delta = event->angleDelta().x();

    vertical_pixel_delta =
        d->line_height * (vertical_angle_delta * 1.0 / 120.0);

    horizontal_pixel_delta =
      d->line_height * (horizontal_angle_delta * 1.0 / 120.0);
  }

  d->TriggerScrollbarUpdate([=, this] (void) {
#ifdef __APPLE__
    qreal mult = IsNaturalScroll() ? 1 : -1;
#else
    qreal mult = event->inverted() ? 1 : -1;
#endif

    d->ScrollBy(static_cast<int>(horizontal_pixel_delta * mult),
                static_cast<int>(vertical_pixel_delta * mult));
  });

  emit LocationChanged(kExternalScrollChange);
}

void CodeWidget::paintEvent(QPaintEvent *) {
  
  // Check if the DPI ratio has changed.
  if (auto window = QApplication::activeWindow()) {
    auto window_dpi_ratio = window->devicePixelRatio();
    if (window_dpi_ratio != d->dpi_ratio) {
      d->dpi_ratio = window_dpi_ratio;
      d->canvas_changed = true;
      d->scene_changed = true;
    }
  }

  d->RecomputeCanvas();

  QPainter blitter(this);
  InitializePainterOptions(blitter);

  // ---------------------------------------------------------------------------
  // Fill the viewport with the theme background color.
  blitter.fillRect(d->viewport, d->theme_background_color);

  // ---------------------------------------------------------------------------
  // Render current line within the canvas.
  if (d->current_line_index != -1) {
    QRectF current_line(
        0, (d->current_line_index * d->line_height) - d->scroll_y,
        d->viewport.width(), d->line_height);
    current_line.setWidth(d->viewport.width());
    blitter.fillRect(current_line, d->theme->CurrentLineBackgroundColor());
  }

  // ---------------------------------------------------------------------------
  // Draw the code background layer. If any tokens have background colors then
  // they are in this layer.
  blitter.drawImage(-d->scroll_x, -d->scroll_y, d->background_canvas);

  // ---------------------------------------------------------------------------
  // Draw the entity highlights. If the cursor is on an token, then all tokens
  // with the same related entity IDs are highlighted. Here, we only actually
  // change the background color.
  if (d->current_entity) {
    blitter.drawImage(-d->scroll_x, -d->scroll_y, d->highlight_canvas);
  }

  // ---------------------------------------------------------------------------
  // Draw the selection background colors, and compute the bounds of the
  // currently selected text.
  if (d->cursor && d->selection_start_cursor) {
    d->RecomputeSelection(blitter);
  }

  // ---------------------------------------------------------------------------
  // Draw the line numbers.
  blitter.drawImage(-d->scroll_x, -d->scroll_y, d->line_number_canvas);

  // ---------------------------------------------------------------------------
  // Paint the code.
  blitter.drawImage(-d->scroll_x, -d->scroll_y, d->foreground_canvas);

  // ---------------------------------------------------------------------------
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

    if (d->current_entity &&
        d->browse_mode == !(event->modifiers() & Qt::ControlModifier)) {
      emit RequestPrimaryClick(d->CreateModelIndex(d->current_entity));
    }
    return;
  }

  if (d->tracking_selection) {
    d->tracking_selection = false;
    Q_ASSERT(d->selection_start_cursor);
    Q_ASSERT(d->cursor.has_value());
    if (d->selection_start_cursor.value() == d->cursor.value()) {
      d->selection_start_cursor.reset();
    }
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

  d->last_location.reset();
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

  d->last_location.reset();
  d->last_entity_for_location = {};
  d->token_model.token = {};
  d->click_was_primary = event->buttons() & Qt::LeftButton;
  d->click_was_secondary = event->buttons() & Qt::RightButton;

  if (d->click_was_primary || d->click_was_secondary) {
    d->goto_line_widget->Deactivate();
  }

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

    if (!d->tracking_selection) {
      emit LocationChanged(kExternalMousePress);
    }

    // Calculate the current line index based on the clamped cursor.
    auto new_current_line_index = static_cast<int>(
        std::floor(d->cursor->y() / d->line_height));
    if (new_current_line_index != d->current_line_index) {
      d->current_line_index = new_current_line_index;
    }
  }

  d->current_entity = entity;

  // Update *prior* to rendering the context menu, if any.
  update();

  // Update the selection in the model.
  d->token_model.selection.clear();
  auto sel_size = d->selection_end_offset - d->selection_start_offset;
  if (d->selection_start_cursor && 0 <= d->selection_start_offset &&
      0 <= d->selection_end_offset && 0 < sel_size &&
      (d->selection_start_offset + sel_size) <= d->scene.document.size()) {
    d->token_model.selection
        = d->scene.document.sliced(d->selection_start_offset, sel_size);
  }

  // Calculate the index of the current line.
  if (!d->tracking_selection) {
    if (!d->click_was_primary && d->click_was_secondary) {
      emit RequestSecondaryClick(d->CreateModelIndex(entity));
    }
  }
}

void CodeWidget::keyPressEvent(QKeyEvent *event) {
  qreal dx = 0;
  qreal dy = 0;

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

  QKeySequence ks(modifier + QKeySequence(event->key()).toString());

  switch (event->key()) {

    // Pressing the page up button should shift by the number of lines in the
    // viewport.
    case Qt::Key_PageUp:
      d->selection_start_cursor.reset();
      d->tracking_selection = false;
      dy = -std::floor(d->viewport.height() / d->line_height);
      break;

    // Pressing the page down button should shift by the number of lines in the
    // viewport.
    case Qt::Key_PageDown:
      d->selection_start_cursor.reset();
      d->tracking_selection = false;
      dy = std::floor(d->viewport.height() / d->line_height);
      break;

    // Pressing the up arrow moves the current line up, and maybe triggers
    // a scroll.
    case Qt::Key_Up:
      d->selection_start_cursor.reset();
      d->tracking_selection = false;
      dy = -1;
      break;

    // Pressing the down arrow moves the current line down, and maybe triggers
    // a scroll.
    case Qt::Key_Down:
      d->selection_start_cursor.reset();
      d->tracking_selection = false;
      dy = 1;
      break;

    // Pressing the left arrow moves the cursor left, and maybe triggers a
    // scroll.
    case Qt::Key_Left:
      d->selection_start_cursor.reset();
      d->tracking_selection = false;
      dx = -1;
      break;

    // Pressing the right arrow moves the cursor left, and maybe triggers a
    // scroll.
    case Qt::Key_Right:
      d->selection_start_cursor.reset();
      d->tracking_selection = false;
      dx = 1;
      break;

    default:
      // Support Ctrl-C.
      if (ks == kCopyKeqSequence && d->selection_start_cursor &&
          !d->token_model.selection.isEmpty()) {
        qApp->clipboard()->setText(d->token_model.selection);

      } else if (ks == kFindKeqSequence) {
        d->search_widget->show();

      } else if (ks == kGotoLineKeqSequence && d->scene.num_file_lines) {
        d->goto_line_widget->Activate(
            static_cast<unsigned>(d->scene.num_file_lines));

      // Otherwise, request a generic keypress handler.
      } else if (d->current_entity && d->cursor) {
        emit RequestKeyPress(ks, d->CreateModelIndex(d->current_entity));
      }
      break;
  }

  auto need_repaint = false;
  if (d->selection_start_cursor && !event->modifiers()) {
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

  d->TriggerScrollbarUpdate([=, &need_repaint, this] (void) {
    // Figure out the next cursor position.
    auto new_cursor = d->NextCursorPosition(d->cursor.value(), dx, dy);
    auto new_current_line_index = static_cast<int>(
        std::floor(new_cursor.y() / d->line_height));

    // Set the current line, and possibly scroll us up or down.
    if (new_current_line_index != d->current_line_index) {
      need_repaint = true;
      d->current_line_index = new_current_line_index;

      // Detect if we need to scroll down to follow the current line.
      if (0 < dy && (((new_current_line_index + 1) * d->line_height) >
                     (d->scroll_y + d->viewport.height()))) {
        d->ScrollBy(0, static_cast<int>(dy * d->line_height));

      // Detect if we need to scroll up to follow the current line.
      } else if (0 > dy && ((new_current_line_index * d->line_height) <
                            d->scroll_y)) {
        d->ScrollBy(0, static_cast<int>(dy * d->line_height));
      }
    }

    // Set the current cursor, and possible scroll us left or right.
    if (new_cursor != d->cursor.value()) {
      need_repaint = true;
      d->current_entity = d->EntityUnderPoint(new_cursor);

      if (0 > dx && (new_cursor.x() - d->scroll_x) < 0) {
        d->ScrollBy(
            static_cast<int>(-std::ceil(d->cursor->x() - new_cursor.x())), 0);

        // If we get to the right edge of our left margin, then put us back at
        // zero.
        if (d->scroll_x <= d->left_margin) {
          d->scroll_x = 0;
        }

      } else if (0 < dx && ((new_cursor.x() - d->scroll_x) >=
                            d->viewport.width())) {
        d->ScrollBy(
            static_cast<int>(std::ceil(new_cursor.x() - d->cursor->x())), 0);
      }
    }

    d->last_entity_for_location = {};
    d->last_location.reset();
    d->cursor = new_cursor;
  });

  if (need_repaint) {
    update();
  }

  emit LocationChanged(kExternalKeyPress);
}

void CodeWidget::PrivateData::UpdateScrollbars(void) {

  if (scene.entities.empty()) {
    horizontal_scrollbar->hide();
    vertical_scrollbar->hide();
    return;
  }

  qreal c_width = canvas_rect.width();
  qreal c_height = canvas_rect.height();

  qreal v_width = viewport.width();
  qreal v_height = viewport.height();

  if (0 < v_width && v_width < c_width) {
    horizontal_scrollbar->show();
    horizontal_scrollbar->setMinimum(0);
    horizontal_scrollbar->setMaximum(static_cast<int>(c_width - v_width));

  } else {
    horizontal_scrollbar->hide();
    horizontal_scrollbar->setMaximum(0);
  }

  if (0 < v_height && v_height < c_height) {
    vertical_scrollbar->show();
    vertical_scrollbar->setMinimum(0);
    vertical_scrollbar->setMaximum(static_cast<int>(c_height - v_height));

  } else {
    vertical_scrollbar->hide();
    vertical_scrollbar->setMaximum(0);
  }
}

// Capture an "opaque" representation of the current location in the code.
CodeWidget::OpaqueLocation CodeWidget::LastLocation(void) const {
  if (d->last_location) {
    return d->last_location.value();
  } else if (0 < d->space_width && 0 < d->line_height) {
    return d->Location();
  } else {
    Q_ASSERT(false);
    return {};
  }
}

CodeWidget::OpaquePosition CodeWidget::PrivateData::YDimensionToPosition(
    qreal y) const {

  CodeWidget::OpaquePosition pos;

  // Try to maintain scroll position across scene changes.
  pos.scale = 0.0;
  pos.physical = 0;
  pos.relative = 0;  // Displacement from the first `physical`.
  if (0 < y && !scene.physical_line_number.empty()) {

    pos.scale = y / line_height;
    auto logical = static_cast<int>(std::floor(pos.scale));

    auto line_nums = scene.physical_line_number.data();
    pos.physical = std::abs(line_nums[logical]);

    for (auto i = logical - 1; 0 <= i; --i, ++pos.relative) {
      if (std::abs(line_nums[i]) != pos.physical) {
        break;
      }
    }

    Q_ASSERT(static_cast<unsigned>(logical) <
             scene.logical_line_index.size());
  }

  return pos;
}

qreal CodeWidget::PrivateData::PositionToYDimension(
    CodeWidget::OpaquePosition pos) const {

  if (0 > pos.physical) {
    return 0;
  }

  int found = 0;
  auto new_line_index = 0;  // Logical line index.
  auto new_line_index_rel = 0;
  for (auto new_phy_line : scene.physical_line_number) {
    if (found && found > pos.relative) {
      break;
    }

    auto abs_new_phy_line = std::abs(new_phy_line);
    if (abs_new_phy_line == pos.physical) {
      if (!new_line_index_rel) {
        new_line_index_rel = new_line_index;
      }
      ++found;
    } else if (found && abs_new_phy_line > pos.physical) {
      break;
    }
    ++new_line_index;
  }

  if (found) {

    // We have enough relative lines, e.g. a macro that expanded to multiple
    // lines.
    if (found >= pos.relative) {
      return (new_line_index_rel + pos.relative) * line_height;

    // We have fewer relative lines, e.g. a macro that spanned multiple lines
    // and was unexpanded.
    } else {
      return (new_line_index_rel + (found - 1)) * line_height;
    }
  
  // Backup position. Useful if all that's changed is the theme.
  } else {
    return pos.scale * line_height;
  }
}

// Capture an "opaque" representation of the current location in the code. This
// can be used to maintain scroll and cursor positions across scene changes,
// such as when macros are expanded.
CodeWidget::OpaqueLocation CodeWidget::PrivateData::Location(void) {
  CodeWidget::OpaqueLocation loc;

  loc.entity = last_entity_for_location;
  loc.scroll_y = YDimensionToPosition(scroll_y);
  
  // Figure out of offset within the current logical line in terms of a scaling
  // factor of the line height.  
  auto scaled_y = static_cast<int>(std::floor(
      std::floor(loc.scroll_y.scale) * line_height));
  Q_ASSERT(scaled_y <= scroll_y);
  loc.scroll_y_offset_scale = (scroll_y - scaled_y) / qreal(line_height);

  // Represent the scroll X position in terms of a scaling factor of the font's
  // space width. This is so that if a scene recompute due to theme change,
  // we'll be able to put ourselves back approximately correctly.
  loc.scroll_x_scale = scroll_x / space_width;

  // Try to figure out where the cursor should go.
  if (cursor) {
    loc.cursor_y = YDimensionToPosition(cursor->y());

    // If there is a cursor, then use it as the current line for line
    // highlighting.
    loc.current_y = loc.cursor_y;

    loc.cursor_x_scale = cursor->x() / space_width;

    // Calculate the character index. This is the logical column, minus one.
    if (auto entity = EntityUnderPoint(cursor.value())) {
      loc.cursor_index = CharacterPosition(cursor.value(), entity).first;
      auto li = static_cast<unsigned>(entity->logical_line_number - 1);
      auto lie = scene.entities.data() + scene.logical_line_index[li];
      for (; lie < entity; ++lie) {
        loc.cursor_index += static_cast<int>(
            scene.data[lie->data_index_and_config >> kFormatShift].text.size());
      }
    }

  // If there is no cursor, try to keep track of the current highlighted line.
  } else if (current_line_index != -1) {
    loc.current_y = YDimensionToPosition(current_line_index * line_height);
  }
  return loc;
}

void CodeWidget::PrivateData::SetLocation(CodeWidget::OpaqueLocation loc) {
  scroll_y = static_cast<int>(PositionToYDimension(loc.scroll_y) +
                              (loc.scroll_y_offset_scale * line_height));
  scroll_x = static_cast<int>(loc.scroll_x_scale * space_width);
  
  if (loc.current_y.physical >= 0) {
    current_line_index = static_cast<int>(
        std::floor(PositionToYDimension(loc.current_y) / line_height));
  }

  last_location = loc;
  SetCursor(std::move(loc));
}

void CodeWidget::PrivateData::SetCursor(CodeWidget::OpaqueLocation loc) {
  if (0 > loc.cursor_y.physical) {
    cursor.reset();
    current_entity = nullptr;
    last_entity_for_location = std::move(loc.entity);
    return;
  }

  QPointF pt(left_margin + (loc.scroll_x_scale * space_width),
             PositionToYDimension(loc.cursor_y));

  current_line_index = static_cast<int>(std::floor(pt.y() / line_height));

  auto li = static_cast<unsigned>(std::floor(pt.y() / line_height));
  if (0 < loc.cursor_index && (li + 1u) < scene.logical_line_index.size()) {
    pt.setX(left_margin);
    while (0 < loc.cursor_index) {
      pt = NextCursorPosition(pt, 1, 0);
      --loc.cursor_index;
    }
  }

  cursor = CursorPosition(pt);
  current_entity = EntityUnderPoint(cursor.value());
  last_entity_for_location = std::move(loc.entity);
}

// Clamp the scroll positions.
void CodeWidget::PrivateData::ClampScrollXY(void) {
  auto v_width = viewport.width();
  auto v_height = viewport.height();
  if (v_width && v_height) {
    auto c_width = static_cast<int>(foreground_canvas.width() / dpi_ratio);
    auto c_height = static_cast<int>(foreground_canvas.height() / dpi_ratio);
    
    scroll_y = std::max(0, scroll_y);
    if (c_height > v_height) {
      scroll_y = std::min(scroll_y, c_height - v_height);
    }

    scroll_x = std::max(0, scroll_x);
    if (c_width > v_width) {
      scroll_x = std::min(scroll_x, c_width - v_width);
    }
  }
}

void CodeWidget::PrivateData::RecomputeScene(void) {
  if (!scene_changed) {
    return;
  }

  // Try to maintain scroll position across scene changes.
  std::optional<OpaqueLocation> loc;
  if (0 < space_width && 0 < line_height) {
    loc = Location();
  }

  version_number++;

  SceneBuilder builder;  
  ImportNode(builder, token_tree.root());

  scene = builder.TakeScene();
  scene_changed = false;
  current_entity = nullptr;

  if (loc) {
    SetLocation(std::move(loc.value()));
  }

  // Force a change.
  canvas_changed = true;
}

// Recompute and paint the selection.
void CodeWidget::PrivateData::RecomputeSelection(QPainter &blitter) {
  std::optional<QRectF> selections[3u];

  QPointF top_left = cursor.value();
  QPointF bottom_right = selection_start_cursor.value();
  if (top_left.y() > bottom_right.y()) {
    std::swap(top_left, bottom_right);
  }

  // Selection contained within one line; the polygon is a rectange.
  if (top_left.y() == bottom_right.y()) {
    auto left_point = std::min(top_left.x(), bottom_right.x());
    auto right_point = std::max(top_left.x(), bottom_right.x());

    selections[0] = QRectF(
        left_point - scroll_x, top_left.y() - scroll_y,
        right_point - left_point, line_height);

  // Selection cross multiple lines.
  } else {
    selections[0] = QRectF(
        top_left.x() - scroll_x, top_left.y() - scroll_y,
        canvas_rect.width() + viewport.width(),
        line_height);

    auto bottom_left_y = top_left.y() + line_height;

    if (bottom_left_y < bottom_right.y()) {
      selections[1] = QRectF(
          -scroll_x, bottom_left_y - scroll_y,
          canvas_rect.width() + viewport.width(),
          bottom_right.y() - bottom_left_y);
    }

    selections[2] = QRectF(
        -scroll_x, bottom_right.y() - scroll_y,
        bottom_right.x(), line_height);
  }

  QColor selection_color = theme->SelectionColor();

  // Fill in up to three rectangles for the selection.
  for (auto &sel : selections) {
    if (sel) {
      blitter.fillRect(sel.value(), selection_color);
    }
  }

  // Track what actual characters in the underlying document are being
  // selected for the sake of copy & paste.
  selection_start_offset = -1;
  selection_end_offset = -1;
  int *selection = &(selection_start_offset);

  auto update_selections = [&, this] (auto begin_offset, auto end_offset) {
    *selection = static_cast<int>(begin_offset);
    selection = &(selection_end_offset);
    *selection = static_cast<int>(end_offset);
  };

  // -------------------------------------------------------------------------
  // Go find all entities bounded by the selections, and repaint them as being
  // selected.
  auto start_index = static_cast<unsigned>(
      (top_left.y() + (line_height / 2)) / line_height);

  auto stop_index = static_cast<unsigned>(
      (bottom_right.y() + (line_height / 2)) / line_height);

  // Go through only the entities on the relevant lines.
  for (auto l = start_index; l <= stop_index; ++l) {
    auto i = scene.logical_line_index[l];
    auto max_i = scene.logical_line_index[l + 1];

    QPainter dummy_fg;

    // Inspect each entity on the line.
    for (; i < max_i; ++i) {
      const Entity &e = scene.entities[i];
      unsigned data_index = e.data_index_and_config >> kFormatShift;
      unsigned rect_config = e.data_index_and_config & kFormatMask;
      Data &data = scene.data[data_index];

      const Token &t = scene.tokens[e.token_index];
      ITheme::ColorAndStyle cs = theme->TokenColorAndStyle(t);
      cs.background_color = selection_color;
      cs.foreground_color = QColor();

      int entity_offset = scene.begin_of_entity_in_document[i];
      qreal e_y = static_cast<qreal>(e.logical_line_number - 1) *
                  line_height;

      QRectF bounding_rect = data.bounding_rect[rect_config];
      for (auto &sel : selections) {
        if (!sel) {
          continue;
        }

        qreal x = e.x - scroll_x;
        qreal y = e_y - scroll_y;
        bounding_rect.moveTo(x, y);

        // The selection fully contains this entity; paint it.
        if (sel->contains(bounding_rect)) {
          update_selections(entity_offset + 0,
                            entity_offset + data.text.size());
          PaintToken(dummy_fg, blitter, data, rect_config, cs, x, y);
          break;

        // The selection is unrelated to this entity.
        } else if (!sel->intersects(bounding_rect)) {
          continue;
        }

        qsizetype start_k = 0;
        qsizetype stop_k = data.text.size();

        // Top-left intersection case (highlight a suffix of `data`).
        if (bounding_rect.x() < sel->x()) {
          auto [index, width] = CharacterPosition(
              QPointF(sel->x() + scroll_x, e_y), &e);
          Q_ASSERT(0 < index);
          x += width;
          start_k = index;
        }

        // Bottom-right intersection case (highlight a prefix of `data`).
        if (sel->topRight().x() < bounding_rect.topRight().x()) {
          stop_k = CharacterPosition(
              QPointF(sel->topRight().x() + scroll_x, e_y), &e).first;
          Q_ASSERT(0 < stop_k);
        }

        Data new_data = {};
        new_data.bounding_rect_valid[rect_config] = false;
        for (auto k = start_k; 0 <= k && k < stop_k; ++k) {
          new_data.text += data.text[k];
        }
        
        update_selections(entity_offset + start_k, entity_offset + stop_k);
        PaintToken(dummy_fg, blitter, new_data, rect_config, cs, x, y);
        break;
      }
    }
  }
}

// Recompute the line numbers.
void CodeWidget::PrivateData::RecomputeLineNumbers(void) {
  auto bg_color = theme->GutterBackgroundColor();
  if (!bg_color.isValid()) {
    bg_color = theme_background_color;
  }

  auto fg_color = theme->GutterForegroundColor();
  if (!fg_color.isValid()) {
    fg_color = theme_foreground_color;
  }

  int num_digits = 0;
  for (auto i = scene.num_file_lines; i; ++num_digits) {
    i /= 10;
  }

  QFontMetricsF fm(theme_font);

  auto height = std::max(canvas_rect.height(), viewport.height());
  auto width = (space_width * 3) + (fm.maxWidth() * num_digits);
  left_margin = width;

  QImage bg(static_cast<int>(width * dpi_ratio),
            static_cast<int>(height * dpi_ratio),
            QImage::Format_ARGB32_Premultiplied);

  bg.setDevicePixelRatio(dpi_ratio);

  if (bg_color.isValid()) {
    bg.fill(bg_color);
  } else {
    bg.fill(0);
  }

  QPainter blitter(&bg);
  InitializePainterOptions(blitter);

  QFont font = theme_font;
  blitter.setPen(fg_color);

  QTextOption gutter_to(Qt::AlignRight);

  if (!scene.logical_line_index.empty()) {
    auto max_i = scene.logical_line_index.size() - 1u;
    QRectF bounding_rect(space_width, 0, width - (space_width * 3), line_height);

    int last_line_num = 0;

    // Paint the line numbers.
    for (auto i = 0u; i < max_i; ++i) {
      int line_number = 0;
      auto max_e = scene.logical_line_index[i + 1u];
      
      // Go get the minimum line number. Some might be negative because of a
      // macro expansion on the line, so we want to highlight that an expansion
      // happened somewhere on the line.
      for (auto e = scene.logical_line_index[i]; e < max_e; ++e) {
        if (auto ln = scene.file_line_number[e]) {
          if (!line_number) {
            line_number = ln;
          } else {
            line_number = std::min(line_number, ln);
          }
        }
      }

      if (!line_number) {
        line_number = last_line_num;
      }

      if (line_number) {
        QString text = QString::number(std::abs(line_number));
        font.setUnderline(0 > line_number);
        blitter.setFont(font);
        blitter.drawText(bounding_rect, text, gutter_to);

        last_line_num = -std::abs(line_number);
      }

      bounding_rect.moveTo(
          QPointF(bounding_rect.x(), bounding_rect.y() + line_height));
    }
  }

  // Paint a right margin one space wide.
  QRectF right_margin_rect((space_width * 2) + (fm.maxWidth() * num_digits), 0,
                           space_width, height);
  blitter.fillRect(right_margin_rect, theme_background_color);

  blitter.end();

  line_number_canvas.swap(bg);
}

// Recompute the highlights.
void CodeWidget::PrivateData::RecomputeHighlights(void) {
  if (current_entity == prev_highlighted_entity && !canvas_changed) {
    return;
  }

  prev_highlighted_entity = current_entity;

  QImage bg(static_cast<int>(canvas_rect.width() * dpi_ratio),
            static_cast<int>(canvas_rect.height() * dpi_ratio),
            QImage::Format_ARGB32_Premultiplied);

  bg.setDevicePixelRatio(dpi_ratio);

  // Fill the contents with transparent pixels, rather than leaving them
  // undefined.
  bg.fill(0);

  if (!current_entity) {
    highlight_canvas.swap(bg);
    return;
  }

  const Token &token = scene.tokens[current_entity->token_index];
  RawEntityId related_entity_id = token.related_entity_id().Pack();
  if (related_entity_id == kInvalidEntityId) {
    highlight_canvas.swap(bg);
    return;
  }

  QColor highlight_color = theme->CurrentEntityBackgroundColor(
      token.related_entity());

  // The theme doesn't want to highlight current entities.
  if (!highlight_color.isValid()) {
    highlight_canvas.swap(bg);
    return;
  }

  QPainter fg_painter;
  QPainter bg_painter(&bg);
  InitializePainterOptions(bg_painter);

  auto re_end_it = scene.related_entity_ids.end();
  auto re_it = std::upper_bound(
      scene.related_entity_ids.begin(), re_end_it,
      std::pair<RawEntityId, unsigned>(related_entity_id - 1, ~0u));

  for (auto it = re_it; it != re_end_it && it->first == related_entity_id;
     ++it) {
    const Entity &e = scene.entities[it->second];
    const Token &t = scene.tokens[e.token_index];
    Data &data = scene.data[e.data_index_and_config >> kFormatShift];
    unsigned rect_config = e.data_index_and_config & kFormatMask;

    qreal e_x = e.x;
    qreal e_y = static_cast<qreal>(e.logical_line_number - 1) * line_height;

    QRectF bounding_rect = data.bounding_rect[rect_config];
    bounding_rect.moveTo(e_x, e_y);

    ITheme::ColorAndStyle cs = theme->TokenColorAndStyle(t);
    cs.background_color = highlight_color;
    cs.foreground_color = QColor();

    PaintToken(fg_painter, bg_painter, data, rect_config, cs, e_x, e_y);
  }

  bg_painter.end();
  highlight_canvas.swap(bg);
}

void CodeWidget::PrivateData::RecomputeCanvas(void) {
  RecomputeScene();

  if (!canvas_changed) {
    RecomputeHighlights();
    return;
  }

  canvas_changed = false;

  theme_font = theme->Font();  // Reset (to clear bold/italic).
  theme_font.setStyleStrategy(QFont::NoSubpixelAntialias);

  QFont bold_font = theme_font;
  QFont italic_font = theme_font;
  QFont bold_italic_font = theme_font;

  bold_font.setWeight(QFont::DemiBold);
  italic_font.setItalic(true);
  bold_italic_font.setWeight(QFont::DemiBold);
  bold_italic_font.setItalic(true);

  QFontMetricsF font_metrics(theme_font);
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
    InitializePainterOptions(p);
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

  UpdateScrollbars();

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

  InitializePainterOptions(fg_painter);
  InitializePainterOptions(bg_painter);

  fg_painter.setFont(bold_italic_font);

  monospace[0] = QChar::Space;
  space_rect = fg_painter.boundingRect(canvas_rect, monospace, to);
  space_width = space_rect.width();

  RecomputeLineNumbers();  // Computes `left_margin` using `space_width`.
  right_margin = space_width;

  Q_ASSERT(0 < space_width);

  // Start new lines indented with a single space. This helps us account for
  // italic character writing outside of their minimum-sized bounding box.
  qreal x = left_margin;
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
      x = left_margin;
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

  // TODO(pag): `scroll_x` and `scroll_y` probably don't make sense anymore.
  if (cursor) {
    cursor = CursorPosition(cursor.value());
    current_entity = EntityUnderPoint(cursor.value());
  }

  RecomputeHighlights();
}

void CodeWidget::PrivateData::ScrollToPoint(
    CodeWidget *self, QPointF point, bool take_focus,
    LocationChangeReason reason) {

  int v_width = viewport.width();
  int v_height = viewport.height();

  // If we don't yet have a viewport width/height, then schedule this function
  // to run later.
  if (!v_width || !v_height) {
    QTimer::singleShot(
        10, self, [=, this, vn = version_number] (void) {
          if (vn == version_number) {
            ScrollToPoint(self, point, take_focus, reason);
          }
        });
    return;
  }

  // NOTE(pag): This function calls `ClampScrollXY` to keep them in range.
  TriggerScrollbarUpdate([=, this] (void) {

    // If the entity isn't already visible, then center the window to make it
    // visible.
    //
    // NOTE(pag): The `5` divisor is to say: if less than 1/5th of the viewport
    //            is below the point, then center the point in the viewport.
    //            It can be odd when you go to an entity/point, and it brings
    //            you to the file, but the entity is way at the bottom of the
    //            screen.
    if (point.y() < scroll_y ||
        (point.y() + line_height + (v_height / 5)) > (scroll_y + v_height)) {
      scroll_y = static_cast<int>(point.y() - (v_height / 2));
    }

    if (point.x() > v_width) {
      scroll_x = static_cast<int>(point.x() - (v_width / 2));

    } else {
      scroll_x = 0;
    }
  });

  self->update();

  if (take_focus) {
    self->setFocus();
  }

  self->EmitLocationChanged(reason);
}

void CodeWidget::PrivateData::ScrollToEntityOffset(
    CodeWidget *self, unsigned offset, bool take_focus,
    LocationChangeReason reason) {

  if (offset > scene.entities.size()) {
    return;
  }

  const Entity &entity = scene.entities[offset];
  current_line_index = entity.logical_line_number - 1;
  qreal entity_y = current_line_index * line_height;
  QPointF entity_loc(entity.x, entity_y);
  selection_start_cursor.reset();
  cursor = CursorPosition(entity_loc);
  current_entity = &entity;

  ScrollToPoint(self, QPointF(entity.x, entity_y), take_focus, reason);
}

// Paint a token.
void CodeWidget::PrivateData::PaintToken(
    QPainter &fg_painter, QPainter &bg_painter, Data &data,
    unsigned rect_config, ITheme::ColorAndStyle cs, qreal &x, qreal &y) {

  QFont font = theme_font;
  font.setItalic(cs.italic);
  font.setUnderline(cs.underline);
  font.setStrikeOut(cs.strikeout);
  font.setWeight(cs.bold ? QFont::DemiBold : QFont::Normal);

  QRectF &token_rect = data.bounding_rect[rect_config];
  bool &token_rect_valid = data.bounding_rect_valid[rect_config];
  bool fg_valid = cs.foreground_color.isValid();
  bool bg_valid = cs.background_color.isValid();

  if (fg_valid) {
    fg_painter.setPen(cs.foreground_color);
  }

  if (bg_valid) {
    bg_painter.setFont(font);
  }

  Q_ASSERT(fg_valid || bg_valid);
  QPainter *valid_painter = bg_valid ? &bg_painter : &fg_painter;
  valid_painter->setFont(font);

  // Draw one character at a time. That results in better alignment across
  // lines.
  if (is_monospaced) {
    if (!token_rect_valid) {
      token_rect = space_rect;
      token_rect.setWidth(space_width * static_cast<double>(data.text.size()));
      token_rect_valid = true;
    }

    // Paint the background as a whole, otherwise lines between characters are
    // observable.
    if (bg_valid) {
      token_rect.moveTo(QPointF(x, y));
      bg_painter.fillRect(token_rect, cs.background_color);
    }

    QRectF glyph_rect = space_rect;

    // Allow the glyph rect to be wider so that we don't cut off parts of
    // italic text.
    glyph_rect.setWidth(glyph_rect.width() * 2);

    for (QChar ch : data.text) {
      monospace[0] = ch;
      if (fg_valid) {
        glyph_rect.moveTo(QPointF(x, y));
        fg_painter.drawText(glyph_rect, monospace, to);
      }
      x += space_width;
    }

  // Draw it as one word.
  } else {
    if (!token_rect_valid) {
      token_rect = valid_painter->boundingRect(canvas_rect, data.text, to);
      token_rect_valid = true;
    }

    token_rect.moveTo(QPointF(x, y));
    if (bg_valid) {
      bg_painter.fillRect(token_rect, cs.background_color);
    }
    if (fg_valid) {
      fg_painter.drawText(token_rect, data.text, to);
    }
    x += token_rect.width();
  }
}

void CodeWidget::OnIndexChanged(const ConfigManager &) {
  ChangeScene({}, {});
  close();
}

void CodeWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  QFont old_font;
  if (d->theme) {
    old_font = d->theme->Font();
  }

  d->theme = theme_manager.Theme();
  d->theme_font = d->theme->Font();
  d->theme_cursor_color = d->theme->CursorColor();
  d->theme_foreground_color = d->theme->DefaultForegroundColor();
  d->theme_background_color = d->theme->DefaultBackgroundColor();

  d->scene_changed = true;

  // If the font changed then scale the scroll position.
  if (old_font != d->theme_font) {
    QFontMetricsF old_fm(old_font);
    QFontMetricsF new_fm(d->theme_font);

    d->scroll_x = static_cast<int>(
        (d->scroll_x / old_fm.maxWidth()) * new_fm.maxWidth());
    d->scroll_y = static_cast<int>(
        (d->scroll_y / old_fm.height()) * new_fm.height());
  }

  // Rendering of things like the selection position is entirely dependent
  // on the font sizes, so all of this stuff needs to be cleared out.
  d->click_was_primary = false;
  d->click_was_secondary = false;
  d->current_entity = nullptr;
  d->cursor.reset();
  d->selection_start_cursor.reset();
  d->tracking_selection = false;

  QPalette p = palette();
  p.setColor(QPalette::Window, d->theme_background_color);
  p.setColor(QPalette::WindowText, d->theme_foreground_color);
  p.setColor(QPalette::Base, d->theme_background_color);
  p.setColor(QPalette::Text, d->theme_foreground_color);
  p.setColor(QPalette::AlternateBase, d->theme_background_color);
  setPalette(p);
  setFont(d->theme_font);
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

  if (!d->scene_changed) {
    return;
  }

  d->TriggerScrollbarUpdate([this] (void) {
    d->RecomputeScene();
  });
  update();
}

// Invoked when the set of entities to be renamed changes.
void CodeWidget::OnRenameEntities(
    const QMap<RawEntityId, QString> &new_entity_names) {
  d->scene_changed = true;  // TODO(pag): Be more selective.
  d->new_entity_names = new_entity_names;
  update();
}

void CodeWidget::OnVerticalScroll(int) {
  auto change = d->vertical_scrollbar->value() - d->scroll_y;
  d->ScrollBy(0, change);
  update();
  emit LocationChanged(kExternalScrollChange);
}

void CodeWidget::OnHorizontalScroll(int) {
  auto change = d->horizontal_scrollbar->value() - d->scroll_x;
  d->ScrollBy(change, 0);
  update();
  emit LocationChanged(kExternalScrollChange);
}

// Invoked when we want to scroll to a specific entity.
void CodeWidget::OnGoToEntity(const VariantEntity &entity_, bool take_focus) {
  // Clear out the last location.
  d->last_location.reset();

  // Clear out any selections.
  d->selection_start_cursor.reset();
  d->tracking_selection = false;
  
  d->last_entity_for_location = entity_;
  VariantEntity entity = entity_;

  // TODO(pag): Eventually handle nested fragments.
  RawEntityId frag_id = kInvalidEntityId;
  if (auto frag = Fragment::containing(entity)) {
    frag_id = frag->id().Pack();
    if (!d->scene.fragment_begin_offset.contains(frag_id) &&
        d->scene.entity_begin_offset.contains(frag_id)) {
      d->scene_overrides.clear();
      d->scene_overrides.insert(frag->id().Pack());
      d->scene_changed = true;
    }
  }

  // It's possible we haven't painted anything yet, and so we need a scene to
  // be able to know what entities are even in our scene.
  d->RecomputeCanvas();

  auto from_macro = false;
  auto it_end = d->scene.entity_begin_offset.end();

  // Map to the entity.
  while (!std::holds_alternative<NotAnEntity>(entity)) {

    // Try to find `entity`.
    RawEntityId entity_id = EntityId(entity).Pack();
    auto it = d->scene.entity_begin_offset.find(entity_id);
    if (it != it_end) {
      d->ScrollToEntityOffset(this, it->second, take_focus,
                              kExternalGoToEntity);
      return;
    }

    // We failed to find `entity`, try to compute a findable `entity` by
    // ascending various tree-like representations.

    if (auto tok = std::get_if<Token>(&entity)) {
      if (!tok) {
        break;
      }

      if (!from_macro) {
        if (auto macro = tok->containing_macro()) {
          entity = macro.value();
          continue;
        }
      }
      from_macro = false;

      auto ti = 0u;
      for (const auto &existing_tok : d->scene.tokens) {
        if (existing_tok == *tok) {
          break;
        }
        ++ti;
      }

      // We found the token in the scene; now find the first entity using the
      // token.
      if (ti < d->scene.tokens.size()) {
        const auto &entities = d->scene.entities;
        auto max_eo = entities.size();
        for (auto eo = ti; eo < max_eo; ++eo) {
          if (entities[eo].token_index == ti) {
            d->ScrollToEntityOffset(this, eo, take_focus, kExternalGoToEntity);
            return;
          }
        }
      }

      // Fall back on the fragment.
      break;

    } else if (auto macro = std::get_if<Macro>(&entity)) {

      if (auto dir = MacroDirective::from(*macro)) {
        if (auto hash = dir->hash()) {
          entity = hash;  // Might not have a hash with a `_Pragma` case.
          from_macro = true;
          continue;
        }
      }

      if (auto parent = macro->parent()) {
        entity = parent.value();

      } else {
        entity = Fragment::containing(*macro);
      }

    // Keep us where we are.
    } else if (std::holds_alternative<File>(entity)) {
      update();
      emit LocationChanged(kExternalGoToEntity);
      return;

    } else {
      auto found = false;
      for (auto entity_tok : Tokens(entity)) {
        entity = entity_tok;
        found = true;
        break;
      }

      if (found) {
        continue;
      }

      if (!std::holds_alternative<Fragment>(entity)) {
        if (auto frag = Fragment::containing(entity)) {
          entity = frag.value();
          continue;
        }
      }

      if (!found) {
        break;
      }
    }
  }

  // TODO(pag): Add line-based fallback?

  // We failed to find the entity, hopefully we can find its containing
  // fragment. This code is probably dead.
  auto it = d->scene.entity_begin_offset.find(frag_id);
  if (it != it_end) {
    d->ScrollToEntityOffset(this, it->second, take_focus, kExternalGoToEntity);
    return;
  }

  update();
  emit LocationChanged(kExternalGoToEntity);
}

//! Change the underlying data / model being rendered by this code widget.
void CodeWidget::ChangeScene(const TokenTree &token_tree,
                             const SceneOptions &options) {
  d->version_number++;
  d->scene_changed = true;
  d->canvas_changed = true;
  d->click_was_primary = false;
  d->click_was_secondary = false;
  d->current_entity = nullptr;
  d->cursor.reset();
  d->selection_start_cursor.reset();
  d->tracking_selection = false;
  d->token_model.token = {};
  d->token_model.selection.clear();
  d->scroll_x = 0;
  d->scroll_y = 0;
  d->current_line_index = -1;
  d->current_entity = nullptr;
  d->scene_overrides.clear();
  d->token_tree = token_tree;
  d->goto_line_widget->Deactivate();
  d->search_widget->Deactivate();
  d->search_result_list.clear();
  d->macros_to_expand = options.macros_to_expand;
  d->new_entity_names = options.new_entity_names;
  d->last_entity_for_location = {};
  d->last_location.reset();

  if (d->horizontal_scrollbar->value()) {
    d->horizontal_scrollbar->setValue(0);
  }
  if (d->vertical_scrollbar->value()) {
    d->vertical_scrollbar->setValue(0);
  }
  d->UpdateScrollbars();
  update();
  emit LocationChanged(kExternalSceneChange);
}

void CodeWidget::EmitLocationChanged(LocationChangeReason reason) {
  emit LocationChanged(reason);
}

void CodeWidget::OnGoToLineNumber(unsigned line_) {
  d->current_entity = nullptr;
  d->selection_start_cursor.reset();
  d->tracking_selection = false;
  d->last_location.reset();
  d->last_entity_for_location = {};

  auto line = static_cast<int>(line_);
  auto max_e = d->scene.entities.size();
  for (auto e = 0u; e < max_e; ++e) {
    if (std::abs(d->scene.file_line_number[e]) == line) {
      d->ScrollToEntityOffset(this, e, true  /* take focus */,
                              kInternalGoToLine);
      break;
    }
  }
}

void CodeWidget::OnSearchParametersChange(void) {
  const auto &search_parameters = d->search_widget->Parameters();

  d->search_result_list.clear();
  if (search_parameters.pattern.empty()) {
    return;
  }

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == SearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Q_ASSERT(regex.isValid());

  QRegularExpressionMatchIterator i = regex.globalMatch(d->scene.document);
  while (i.hasNext()) {
    QRegularExpressionMatch match = i.next();
    d->search_result_list.emplace_back(match.capturedStart(), match.capturedLength());
  }

  std::sort(d->search_result_list.begin(), d->search_result_list.end());
  auto it = std::unique(d->search_result_list.begin(),
                        d->search_result_list.end());

  d->search_result_list.erase(it, d->search_result_list.end());

  d->search_widget->UpdateSearchResultCount(d->search_result_list.size());
}

void CodeWidget::OnShowSearchResult(size_t result_index) {
  if (result_index >= d->search_result_list.size()) {
    return;
  }

  auto [begin_, length] = d->search_result_list[result_index];
  auto begin = static_cast<int>(begin_);
  auto end = begin + static_cast<int>(length);
  if (0 > begin || 0 > end || end >= d->scene.document.size()) {
    return;
  }

  auto eo_to_point =
      [this] (const Entity *entity, int entity_offset) -> QPointF {
        const Data &data = d->scene.data[
            entity->data_index_and_config >> kFormatShift];
        QRectF entity_rect = data.bounding_rect[
            entity->data_index_and_config & kFormatMask];

        // Figure out the starting position of the selection.
        qreal entity_y = (entity->logical_line_number - 1) * d->line_height;
        qreal incr = d->is_monospaced ? d->space_width : (d->space_width / 16.0);
        qreal shift = 0;

        for (qreal max_width = entity_rect.width(); ; shift += incr) {
          auto [index, width] = d->CharacterPosition(
              QPointF(entity->x + shift, entity_y), entity);

          if (index >= entity_offset || shift >= max_width) {
            shift = width;
            break;
          }
        }

        return QPointF(entity->x + shift, entity_y);
      };

  auto [begin_entity, begin_offset] = d->EntityAtDocumentOffset(begin);
  if (!begin_entity) {
    return;
  }

  d->token_model.selection = d->scene.document.sliced(begin, length);
  d->current_entity = nullptr;
  d->last_entity_for_location = {};
  d->last_location.reset();
  d->cursor = d->CursorPosition(
      eo_to_point(begin_entity, begin_offset));
  d->selection_start_cursor = d->cursor;
  d->tracking_selection = false;

  auto [end_entity, end_offset] = d->EntityAtDocumentOffset(end);
  if (end_entity) {
    d->selection_start_cursor
        = d->CursorPosition(eo_to_point(end_entity, end_offset));
  }

  d->ScrollToPoint(this, d->cursor.value(), false  /* take focus */,
                   kInternalGoToSearchResult);
}

//! Called when we want to act on the context menu.
void CodeWidget::ActOnContextMenu(IWindowManager *, QMenu *menu,
                                  const QModelIndex &) {
  if (!d->token_model.selection.isEmpty()) {
    auto copy_selection = new QAction(tr("Copy"), menu);
    menu->addAction(copy_selection);
    connect(copy_selection, &QAction::triggered,
            this, [this] (void) {
                    qApp->clipboard()->setText(d->token_model.selection);
                  });
  }
}

void CodeWidget::OnToggleBrowseMode(const QVariant &toggled) {
  d->browse_mode = toggled.toBool();
}

void CodeWidget::TryGoToLocation(const OpaqueLocation &location,
                                 bool take_focus) {

  d->TriggerScrollbarUpdate([&, this] (void) {
    d->SetLocation(location);
  });

  update();

  if (take_focus) {
    setFocus();
  }

  emit LocationChanged(kExternalSetOpaqueLocation);
}

// Returns `0` if not valid.
unsigned CodeWidget::OpaqueLocation::Line(void) const {
  if (cursor_y.physical != -1) {
    return static_cast<unsigned>(std::max(0, cursor_y.physical));
  
  } else if (current_y.physical != -1) {
    return static_cast<unsigned>(std::max(0, current_y.physical));
  
  } else if (scroll_y.physical != -1) {
    return static_cast<unsigned>(std::max(0, current_y.physical));
  
  } else {
    return 0u;
  }
}

// Returns `0` if not valid.
unsigned CodeWidget::OpaqueLocation::Column(void) const {
  return cursor_index >= 0 ? static_cast<unsigned>(cursor_index + 1) : 0u;
}

}  // namespace mx::gui

#include "CodeWidget.moc"
