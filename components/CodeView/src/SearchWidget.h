/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>
#include <QPlainTextEdit>

namespace mx::gui {

class SearchWidget final : public QWidget {
  Q_OBJECT

 public:
  SearchWidget(QPlainTextEdit *text_edit, QWidget *parent = nullptr);

  virtual ~SearchWidget() override;

  SearchWidget(const SearchWidget &) = delete;
  SearchWidget &operator=(const SearchWidget &) = delete;

 signals:
  void OnSearchForText(const QString &text, bool case_sensitive,
                       bool whole_word, bool regex);

 public slots:
  void Activate();
  void Deactivate();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void LoadIcons();
  void InitializeWidgets();

  void SetDisplayMessage(bool error, const QString &message);
  void ClearDisplayMessage();

  void EnableNavigation(bool enable);

 private slots:
  void OnSearchInputTextChanged(const QString &text);
  void OnCaseSensitiveSearchOptionToggled(bool checked);
  void OnWholeWordSearchOptionToggled(bool checked);
  void OnRegexSearchOptionToggled(bool checked);
  void OnTextSearch();
  void OnShowPrevResult();
  void OnShowNextResult();
};

}  // namespace mx::gui
