/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/ISearchWidget.h>

#include <multiplier/Token.h>

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
  //! \copybrief ICodeView::Model
  virtual ICodeModel *Model() override;

  //! \copybrief ICodeView::SetTheme
  virtual void SetTheme(const CodeViewTheme &theme) override;

  //! \copybrief ICodeView::SetTabWidth
  virtual void SetTabWidth(std::size_t width) override;

  //! Destructor
  virtual ~CodeView() override;

  //! \copybrief ICodeView::GetCursorPosition
  virtual int GetCursorPosition() const override;

  //! \copybrief ICodeView::SetCursorPosition
  virtual bool SetCursorPosition(int start,
                                 std::optional<int> opt_end) const override;

  //! \copybrief ICodeView::Text
  virtual QString Text() const override;

  //! \copybrief ICodeView::SetWordWrapping
  virtual void SetWordWrapping(bool enabled) override;

  //! Scrolls the view to the specified entity id
  virtual bool ScrollToLineNumber(unsigned line) const override;

 protected:
  //! Constructor
  CodeView(ICodeModel *model, QWidget *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Used to capture certain events from the gutter/text edit viewport
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

  //! Installs the given module, updating its parent
  void InstallModel(ICodeModel *model);

  //! Initializes all the widgets in this component
  void InitializeWidgets();

  //! Returns the code model index for the token at the given coordinates
  std::optional<CodeModelIndex>
  GetCodeModelIndexFromMousePosition(const QPoint &pos);

  //! Implements support for token hover notifications
  void OnTextEditViewportMouseMoveEvent(QMouseEvent *event);

  //! Utility function used to handle mouse press events
  void OnTextEditViewportMouseButtonPress(QMouseEvent *event);

  //! Utility function used to handle key press events
  void OnTextEditViewportKeyboardButtonPress(QKeyEvent *event);

  //! Updates the font size when using mouse wheel + cmd
  void OnTextEditTextZoom(QWheelEvent *event);

  //! Updates the tab stop distance based on the current font/settings
  void UpdateTabStopDistance();

  //! Updates the gutter's minimum width based on the highest line number
  void UpdateGutterWidth();

  // Reapplies token group colors using the QPlainTextEdit extra selections
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

    //! This maps a block number to a list of unique token identifiers
    std::unordered_map<int, std::vector<std::uint64_t>>
        block_number_to_unique_token_id_list;

    //! This maps a line number to a block number
    std::unordered_map<Count, int> line_number_to_block_number;

    //! This maps a block number to a line number
    std::unordered_map<int, Count> block_number_to_line_number;

    //! This maps a token group to a list of unique token identifiers
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>
        token_group_id_to_unique_token_id_list;

    //! The highest line number that we have encountered
    unsigned highest_line_number{};
  };

  //! Creates a unique token identifier from the given code model index
  static std::uint64_t GetUniqueTokenIdentifier(const CodeModelIndex &index);

  //! Returns the line number for the specified block number
  static std::optional<std::size_t>
  GetLineNumberFromBlockNumber(const TokenMap &token_map, int block_number);

  //! Returns the block number for the specified line number
  static std::optional<std::size_t>
  GetBlockNumberFromLineNumber(const TokenMap &token_map, unsigned line_number);

  //! Returns a code model index from the specified text cursor
  static std::optional<CodeModelIndex>
  GetCodeModelIndexFromTextCursor(const TokenMap &token_map,
                                  const QTextCursor &cursor);

  //! Used with CreateTextDocument to display graphical progress
  using CreateTextDocumentProgressCallback = std::function<bool(int)>;

  //! Creates a new text document from the given model
  static QTextDocument *
  CreateTextDocument(TokenMap &token_map, const ICodeModel &model,
                     const CodeViewTheme &theme,
                     const std::optional<CreateTextDocumentProgressCallback>
                         &opt_progress_callback = std::nullopt);

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

  //! This slot receives tells us when a request for a model reset is was
  //! skipped because the current model is already the right model to use.
  void OnModelResetDone();

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
  void OnGoToLineTriggered();

  //! Called by the GoToLineWidget when a valid line number has been requested
  void OnGoToLine(unsigned line_number);

  //! Called when the cursor position has changed.
  void OnCursorMoved(void);

  friend class ICodeView;
};

}  // namespace mx::gui
