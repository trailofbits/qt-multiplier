/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/ICodeView.h>
#include <multiplier/GUI/ISearchWidget.h>
#include <multiplier/GUI/ThemeManager.h>

#include <multiplier/Frontend/Token.h>

#include <QTextCursor>
#include <QTextDocument>
#include <QPlainTextEdit>

#include <unordered_map>
#include <functional>
#include <optional>
#include <vector>
#include <unordered_set>

namespace mx::gui {

//! The main class implementing the ICodeView interface
class CodeView final : public ICodeView {
  Q_OBJECT

 public:
  //! \copybrief ICodeView::SetTheme
  virtual void SetTheme(const CodeViewTheme &theme) override;

  //! \copybrief ICodeView::SetTabWidth
  virtual void SetTabWidth(std::size_t width) override;

  //! Destructor
  virtual ~CodeView(void) override;

  //! \copybrief ICodeView::GetCursorPosition
  virtual int GetCursorPosition(void) const override;

  //! \copybrief ICodeView::SetCursorPosition
  virtual bool SetCursorPosition(int start,
                                 std::optional<int> opt_end) override;

  //! \copybrief ICodeView::Text
  virtual QString Text(void) const override;

  //! \copybrief ICodeView::SetWordWrapping
  virtual void SetWordWrapping(bool enabled) override;

  //! Scrolls the view to the specified entity id
  virtual bool ScrollToLineNumber(unsigned line) override;

  //! \copybrief ICodeView::SetBrowserMode
  virtual void SetBrowserMode(const bool &enabled) override;

 protected:
  //! Constructor
  CodeView(QAbstractItemModel *model, QWidget *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Used to capture certain events from the gutter/text edit viewport
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

  //! Installs the given module, updating its parent
  void InstallModel(QAbstractItemModel *model);

  //! Initializes all the widgets in this component
  void InitializeWidgets(void);

  //! Returns the code model index for the token at the given coordinates
  std::optional<QModelIndex> GetModelIndexFromMousePosition(const QPoint &pos);

  //! Starts tracking the mouse position for a possible hover event
  void OnTextEditViewportMouseMoveEvent(QMouseEvent *event);

  //! Stops mouse tracking and optionally emits an hover token action
  void OnHoverTimerTimeout();

  //! Utility function used to handle mouse press events
  bool OnTextEditViewportMouseButtonPress(QMouseEvent *event);

  //! Utility function used to handle key press events
  void OnTextEditViewportKeyboardButtonPress(QKeyEvent *event);

  //! Updates the font size when using mouse wheel + cmd
  void OnTextEditTextZoom(QWheelEvent *event);

  //! Updates the tab stop distance based on the current font/settings
  void UpdateTabStopDistance(void);

  //! Regenerates the extra selections for the highlights
  void UpdateBaseExtraSelections();

  //! Updates the gutter's minimum width based on the highest line number
  void UpdateGutterWidth(void);

  //! Disable cursor change tracking.
  void StopCursorTracking(void);

  //! Re-introduce cursor change tracking.
  void ResumeCursorTracking(void);

  //! Handle a cursor move.
  void HandleNewCursor(const QTextCursor &cursor);

  //! Scrolls the view to the specified entity id (internal)
  bool ScrollToLineNumberInternal(unsigned line);

  //! Sets the given text zoom
  void SetZoom(const qreal &font_point_size);

  //! Sets the given text zoom delta
  void SetZoomDelta(const qreal &font_point_size_delta);

  //! Starts/restarts a delayed DocumentChanged signal
  void StartDelayedDocumentUpdateSignal();

 public:
  //! Contains all the tokens that we have imported from the model
  struct TokenMap final {
    //! A cursor range. This could be relative or absolute
    struct CursorRange final {
      //! Start position of this range
      int start;

      //! End position of this range
      int end;
    };

    //! Represents a single token in the code view
    struct TokenEntry final {
      //! Cursor position of the first character, block-relative
      int cursor_start{};

      //! Cursor position of the last character, block-relative
      int cursor_end{};

      //! Entity ID
      RawEntityId entity_id{kInvalidEntityId};

      //! Related entity ID
      RawEntityId related_entity_id{kInvalidEntityId};
    };

    //! A block entry represents a single line in the QTextDocument object
    struct BlockEntry final {
      //! True if this block entry contains macro expansion tokens
      bool contains_macro_expansion{false};

      //! The list of token entries in this block
      std::vector<TokenEntry> token_entry_list;

      //! The line number, as reported by multiplier
      std::size_t line_number{};
    };

    //! A list of all the blocks in the document
    std::vector<BlockEntry> block_entry_list;

    //! Maps a line number to a block number
    std::unordered_map<std::size_t, std::size_t> line_num_to_block_num_map;

    //! Maps an entity to its related entities
    std::unordered_map<RawEntityId, std::vector<RawEntityId>>
        related_entity_to_entity_list;

    //! Maps an entity to a line and column
    std::unordered_map<RawEntityId, CursorRange> entity_cursor_range_map;

    //! Highest line number encountered, used to determine the gutter size
    std::size_t highest_line_number{};
  };

  //! Creates a unique token identifier from the given code model index
  static std::uint64_t GetUniqueTokenIdentifier(const QModelIndex &index);

  //! Returns the line number for the specified block number
  static std::optional<std::size_t>
  GetLineNumberFromBlockNumber(const TokenMap &token_map,
                               const int &block_number);

  //! Returns the block number for the specified line number
  static std::optional<std::size_t>
  GetBlockNumberFromLineNumber(const TokenMap &token_map,
                               const std::size_t &line_number);

  //! Returns a code model index from the specified text cursor
  static std::optional<QModelIndex>
  GetQModelIndexFromTextCursor(const QAbstractItemModel &model,
                               const TokenMap &token_map,
                               const QTextCursor &cursor);

  //! Used with CreateTextDocument to display graphical progress
  using CreateTextDocumentProgressCallback = std::function<bool(int)>;

  //! Creates a new text document from the given model
  static QTextDocument *
  CreateTextDocument(TokenMap &token_map, const QAbstractItemModel &model,
                     const CodeViewTheme &theme,
                     const std::optional<CreateTextDocumentProgressCallback>
                         &opt_progress_callback = std::nullopt);

  //! Creates a new block associated with the given model index
  static void CreateTextDocumentLine(TokenMap &token_map,
                                     QTextDocument &document,
                                     const CodeViewTheme &theme,
                                     const QModelIndex &row_index);

  //! Removes the specified block number from the document
  static void EraseTextDocumentLine(TokenMap &token_map,
                                    QTextDocument &document,
                                    const int &block_number);

  //! Updates the line number mappings and the highest line number
  static void UpdateTokenDataLineNumbers(TokenMap &token_map);

  //! Updates the entity->cursor position map used by the token highlighter
  static void UpdateTokenMappings(TokenMap &token_map,
                                  const QTextDocument &document);

  //! Initializes the given text_format object according to the code view theme
  static void ConfigureTextFormatFromTheme(QTextCharFormat &text_format,
                                           const CodeViewTheme &theme,
                                           const QVariant &token_category_var);

  //! Adds highlights for tokens to an existing extra selection list
  static void HighlightTokensForRelatedEntityID(
      const TokenMap &token_map, const QTextCursor &text_cursor,
      const QModelIndex &model_index,
      QList<QTextEdit::ExtraSelection> &selection_list,
      const CodeViewTheme &theme);

 private slots:
  //! Connect the cursor changed event. This will also trigger a cursor event.
  void ConnectCursorChangeEvent(void);

  //! One or multiple rows are being removed
  void OnRowsRemoved(const QModelIndex &parent, int first, int last);

  //! One or multiple rows are being inserted
  void OnRowsInserted(const QModelIndex &parent, int first, int last);

  //! Generates new extra selections for highlight changes, or a reset otherwise
  void OnDataChange(const QModelIndex &top_left,
                    const QModelIndex &bottom_right, const QList<int> &roles);

  //! This slot regenerates the code view contents using CreateTextDocument
  void OnModelReset(void);

  //! Used to emit the DocumentChanged signal after a model edit (insertion/removal of rows)
  void DocumentChangedTimedSignal();

  //! Repaints the line numbers on the gutter
  void OnGutterPaintEvent(QPaintEvent *event);

  //! Used to sync the scroll area of the QTextWidget with the gutter's state
  void OnTextEditUpdateRequest(const QRect &rect, int dy);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Called by search widget whenever a search result needs to be shown
  void OnShowSearchResult(const std::size_t &result_index);

  //! Called by the go-to-line QShortcut
  void OnGoToLineTriggered(void);

  //! Called by the GoToLineWidget when a valid line number has been requested
  void OnGoToLine(unsigned line_number);

  //! Called when the cursor position has changed.
  void OnCursorMoved(void);

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  //! Called by the zoom-in shortcut
  void OnZoomIn();

  //! Called by the zoom-out shortcut
  void OnZoomOut();

  //! Called by the reset zoom shortcut
  void OnResetZoom();

  //! Just before model will be loaded, this tells us the location of the
  //! entity corresponding to the last call to `SetEntity`.
  void OnEntityLocation(RawEntityId id, unsigned line, unsigned col);

  friend class ICodeView;
};

}  // namespace mx::gui
