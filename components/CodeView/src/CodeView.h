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

#include <unordered_map>
#include <functional>
#include <optional>
#include <vector>

namespace mx::gui {

class CodeView final : public ICodeView {
  Q_OBJECT

 public:
  virtual void SetTheme(const CodeViewTheme &theme) override;
  virtual void SetTabWidth(std::size_t width) override;

  virtual ~CodeView() override;

  virtual std::optional<int>
  GetEntityCursorPosition(RawEntityId entity_id) const override;

  virtual int GetCursorPosition() const override;
  virtual bool SetCursorPosition(int start,
                                 std::optional<int> opt_end) const override;

  virtual QString Text() const override;
  virtual void SetWordWrapping(bool enabled) override;

  virtual bool ScrollToEntityId(RawEntityId entity_id) const override;
  virtual bool ScrollToToken(const Token &token) const override;
  virtual bool ScrollToTokenRange(const TokenRange &token_range) const override;

 protected:
  CodeView(ICodeModel *model, QWidget *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  virtual bool eventFilter(QObject *, QEvent *) override;
  void InstallModel(ICodeModel *model);
  void InitializeWidgets();
  bool IsValidFileToken(RawEntityId file_token_id) const;

  std::optional<CodeModelIndex>
  GetCodeModelIndexFromMousePosition(const QPoint &pos);

  void OnTextEditViewportMouseMoveEvent(QMouseEvent *event);
  void OnTextEditViewportMouseButtonEvent(QMouseEvent *event,
                                          bool double_click);

  void OnTextEditTextZoom(QWheelEvent *event);
  void UpdateTabStopDistance();
  void UpdateGutterWidth();
  void UpdateTokenGroupColors();

 public:
  // Contains all the tokens that we have imported from the model and
  // the necessary indexes to access it
  struct TokenMap final {
    // A single token map entry
    struct Entry final {
      int cursor_start{};
      int cursor_end{};
      CodeModelIndex model_index;
    };

    // The key is derived from a CodeModelIndex and is guaranteed
    // to always be unique (see GetUniqueTokenIdentifier)
    std::unordered_map<std::uint64_t, Entry> data;

    // This maps an entity id to the internal unique token identifier. Since multiple
    // tokens can have the same RawEntityId (because tokens can be split if they
    // contain whitespace) the mapping is one to many
    std::unordered_map<RawEntityId, std::vector<std::uint64_t>>
        entity_id_to_unique_token_id_list;

    // This maps a block number to a list of unique token identifiers
    std::unordered_map<int, std::vector<std::uint64_t>>
        block_number_to_unique_token_id_list;

    // This maps a line number to a block number
    std::unordered_map<std::size_t, int> line_number_to_block_number;

    // This maps a block number to a line number
    std::unordered_map<int, std::size_t> block_number_to_line_number;

    // This maps a token group to a list of unique token identifiers
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>
        token_group_id_to_unique_token_id_list;

    // The highest line number that we have encountered. This is used to set the
    // correct size for the gutter widget
    std::size_t highest_line_number{};
  };

  // Creates a unique token identifier from the given code model index
  static std::uint64_t GetUniqueTokenIdentifier(const CodeModelIndex &index);

  static std::optional<std::size_t>
  GetLineNumberFromBlockNumber(const TokenMap &token_map, int block_number);

  static std::optional<CodeModelIndex>
  GetCodeModelIndexFromTextCursor(const TokenMap &token_map,
                                  const QTextCursor &cursor);

  static std::vector<int>
  GetTextCursorListFromRawEntityId(const TokenMap &token_map,
                                   const RawEntityId &entity_id);

  using CreateTextDocumentProgressCallback = std::function<bool(int)>;

  static QTextDocument *
  CreateTextDocument(TokenMap &token_map, const ICodeModel &model,
                     const CodeViewTheme &theme,
                     const std::optional<CreateTextDocumentProgressCallback>
                         &opt_progress_callback = std::nullopt);

  static std::optional<QColor> GetTextColorMapEntryFromTheme(
      const QVariant &token_category_var,
      const std::unordered_map<TokenCategory, QColor> &color_map);

  static std::optional<QColor>
  GetTextBackgroundColorFromTheme(const CodeViewTheme &theme,
                                  const QVariant &token_category_var);

  static QColor
  GetTextForegroundColorFromTheme(const CodeViewTheme &theme,
                                  const QVariant &token_category_var);

  static CodeViewTheme::Style
  GetTextStyleFromTheme(const CodeViewTheme &theme,
                        const QVariant &token_category_var);

  static void ConfigureTextFormatFromTheme(QTextCharFormat &text_format,
                                           const CodeViewTheme &theme,
                                           const QVariant &token_category_var);

 private slots:
  void OnModelReset();
  void OnTextEditViewportMouseButtonReleaseEvent(QMouseEvent *event);
  void OnTextEditViewportMouseButtonDblClick(QMouseEvent *event);
  void OnGutterPaintEvent(QPaintEvent *event);
  void OnTextEditUpdateRequest(const QRect &rect, int dy);

  friend class ICodeView;
};

}  // namespace mx::gui
