/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeView.h>

namespace mx::gui {

class CodeView final : public ICodeView {
  Q_OBJECT

 public:
  void setTheme(const CodeViewTheme &theme) override;
  virtual ~CodeView() override;

  virtual std::optional<int>
  GetFileTokenCursorPosition(const RawEntityId &file_token_id) const override;

  virtual std::optional<int>
  GetTokenCursorPosition(const Token &token) const override;

  virtual std::optional<int> GetStartTokenRangeCursorPosition(
      const TokenRange &token_range) const override;

  virtual int GetCursorPosition() const override;
  virtual bool SetCursorPosition(int start,
                                 std::optional<int> opt_end) const override;

  virtual QString Text() const override;

  virtual bool
  ScrollToFileToken(const RawEntityId &file_token_id) const override;

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
  bool IsValidFileToken(const RawEntityId &file_token_id) const;

  std::optional<CodeModelIndex> ModelIndexFromMousePosition(QPoint pos);
  void OnTextEditViewportMouseMoveEvent(QMouseEvent *event);
  void OnTextEditViewportMouseButtonEvent(QMouseEvent *event,
                                          bool double_click);

 private slots:
  void OnModelReset();
  void OnTextEditViewportMouseButtonReleaseEvent(QMouseEvent *event);
  void OnTextEditViewportMouseButtonDblClick(QMouseEvent *event);
  void OnCursorPositionChange();
  void OnGutterPaintEvent(QPaintEvent *event);
  void OnTextEditUpdateRequest(const QRect &, int);

  friend class ICodeView;
};

}  // namespace mx::gui
