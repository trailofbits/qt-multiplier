/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Token.h>
#include <multiplier/ui/ICodeView.h>

#include <QTextCursor>
#include <QTextDocument>
#include <QPlainTextEdit>

#include <unordered_map>
#include <functional>
#include <optional>
#include <vector>

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
  virtual ~CodeView() override;

  //! \copybrief ICodeView::GetEntityCursorPosition
  virtual std::optional<int>
  GetEntityCursorPosition(RawEntityId entity_id) const override;

  //! \copybrief ICodeView::GetCursorPosition
  virtual int GetCursorPosition() const override;

  //! \copybrief ICodeView::SetCursorPosition
  virtual bool SetCursorPosition(int start,
                                 std::optional<int> opt_end) const override;

  //! \copybrief ICodeView::Text
  virtual QString Text() const override;

  //! \copybrief ICodeView::SetWordWrapping
  virtual void SetWordWrapping(bool enabled) override;

  //! \copybrief ICodeView::ScrollToEntityId
  virtual bool ScrollToEntityId(RawEntityId entity_id) const override;

  //! \copybrief ICodeView::ScrollToToken
  virtual bool ScrollToToken(const Token &token) const override;

  //! \copybrief ICodeView::ScrollToTokenRange
  virtual bool ScrollToTokenRange(const TokenRange &token_range) const override;

 protected:
  //! Constructor
  CodeView(ICodeModel *model, QWidget *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Used to capture certain events from the gutter/text edit viewport
  virtual bool eventFilter(QObject *, QEvent *) override;

  //! Installs the given module, updating its parent
  void InstallModel(ICodeModel *model);

  //! Initializes all the widgets in this component
  void InitializeWidgets();

  //! Returns the code model index for the token at the given coordinates
  std::optional<CodeModelIndex>
  GetCodeModelIndexFromMousePosition(const QPoint &pos);

  //! Implements support for token hover notifications
  void OnTextEditViewportMouseMoveEvent(QMouseEvent *event);

  //! Utility function used to handle single/double click events
  void OnTextEditViewportMouseButtonEvent(QMouseEvent *event,
                                          bool double_click);

  //! Updates the font size when using mouse wheel + cmd
  void OnTextEditTextZoom(QWheelEvent *event);

  //! Updates the tab stop distance based on the current font/settings
  void UpdateTabStopDistance();

  //! Updates the gutter's minimum width based on the highest line number
  void UpdateGutterWidth();

  void UpdateTokenGroupColors();

 public:
  //! Contains all the tokens that we have imported from the model
  struct TokenMap final {
    //! A single token map entry
    struct Entry final {
      int cursor_start{};
      int cursor_end{};
      CodeModelIndex model_index;
    };

    //! The key is unique and derived from a CodeModelIndex
    std::unordered_map<std::uint64_t, Entry> data;

    //! This maps an entity id to one or more internal unique token IDs
    std::unordered_map<RawEntityId, std::vector<std::uint64_t>>
        entity_id_to_unique_token_id_list;

    //! This maps a block number to a list of unique token identifiers
    std::unordered_map<int, std::vector<std::uint64_t>>
        block_number_to_unique_token_id_list;

    //! This maps a line number to a block number
    std::unordered_map<std::size_t, int> line_number_to_block_number;

    //! This maps a block number to a line number
    std::unordered_map<int, std::size_t> block_number_to_line_number;

    //! This maps a token group to a list of unique token identifiers
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>
        token_group_id_to_unique_token_id_list;

    //! The highest line number that we have encountered
    std::size_t highest_line_number{};
  };

  //! Creates a unique token identifier from the given code model index
  static std::uint64_t GetUniqueTokenIdentifier(const CodeModelIndex &index);

  //! Returns the line number for the specified block number
  static std::optional<std::size_t>
  GetLineNumberFromBlockNumber(const TokenMap &token_map, int block_number);

  //! Returns a code model index from the specified text cursor
  static std::optional<CodeModelIndex>
  GetCodeModelIndexFromTextCursor(const TokenMap &token_map,
                                  const QTextCursor &cursor);

  //! Returns zero or more cursor positions that match the given entity id
  static std::vector<int>
  GetTextCursorListFromRawEntityId(const TokenMap &token_map,
                                   const RawEntityId &entity_id);

  //! Used with CreateTextDocument to display graphical progress
  using CreateTextDocumentProgressCallback = std::function<bool(int)>;

  //! Creates a new text document from the given model
  static QTextDocument *
  CreateTextDocument(TokenMap &token_map, const ICodeModel &model,
                     const CodeViewTheme &theme,
                     const std::optional<CreateTextDocumentProgressCallback>
                         &opt_progress_callback = std::nullopt);

  //! Returns the color map entry that matches with the given token category
  static std::optional<QColor> GetTextColorMapEntryFromTheme(
      const QVariant &token_category_var,
      const std::unordered_map<TokenCategory, QColor> &color_map);

  //! Returns the correct background color from the code view theme
  static std::optional<QColor>
  GetTextBackgroundColorFromTheme(const CodeViewTheme &theme,
                                  const QVariant &token_category_var);

  //! Returns the correct foreground color from the code view theme
  static QColor
  GetTextForegroundColorFromTheme(const CodeViewTheme &theme,
                                  const QVariant &token_category_var);

  //! Returns the correct text style options from the code view theme
  static CodeViewTheme::Style
  GetTextStyleFromTheme(const CodeViewTheme &theme,
                        const QVariant &token_category_var);

  //! Initializes the given text_format object according to the code view theme
  static void ConfigureTextFormatFromTheme(QTextCharFormat &text_format,
                                           const CodeViewTheme &theme,
                                           const QVariant &token_category_var);

  //! Creates a list of extra selections used to highlight token groups
  static QList<QTextEdit::ExtraSelection>
  GenerateExtraSelections(const TokenMap &token_map, QPlainTextEdit &text_edit,
                          const ICodeModel &model, const CodeViewTheme &theme);

 private slots:
  //! This slot regenerates the code view contents using CreateTextDocument
  void OnModelReset();

  //! Used to track single click events and emit the TokenClicked signal
  void OnTextEditViewportMouseButtonReleaseEvent(QMouseEvent *event);

  //! Used to track double click events and emit the TokenClicked signal
  void OnTextEditViewportMouseButtonDblClick(QMouseEvent *event);

  //! Repaints the line numbers on the gutter
  void OnGutterPaintEvent(QPaintEvent *event);

  //! Used to sync the scroll area of the QTextWidget with the gutter's state
  void OnTextEditUpdateRequest(const QRect &rect, int dy);

  friend class ICodeView;
};

}  // namespace mx::gui
