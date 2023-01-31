/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeView.h>

#include <QWidget>

namespace mx::gui {

class InternalSearchWidget final : public QWidget {
  Q_OBJECT

 public:
  InternalSearchWidget(ICodeView *code_view, QWidget *parent = nullptr);

  virtual ~InternalSearchWidget() override;

  InternalSearchWidget(const InternalSearchWidget &) = delete;
  InternalSearchWidget &operator=(const InternalSearchWidget &) = delete;

 signals:
  void OnSearchForText(const QString &text, bool case_sensitive,
                       bool whole_word, bool regex);

 public slots:
  void Activate();
  void Deactivate();

  void OnShowPrevResult();
  void OnShowNextResult();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void LoadIcons();
  void InitializeWidgets();

  void SetDisplayMessage(bool error, const QString &message);
  void ClearDisplayMessage();

  void EnableNavigation(bool enable);
  void NavigateToResult(std::size_t result_index);

  std::size_t GetNextResultIndex(bool forward_direction);

 private slots:
  void OnSearchInputTextChanged(const QString &text);
  void OnCaseSensitiveSearchOptionToggled(bool checked);
  void OnWholeWordSearchOptionToggled(bool checked);
  void OnRegexSearchOptionToggled(bool checked);
  void OnTextSearch();
};

}  // namespace mx::gui
