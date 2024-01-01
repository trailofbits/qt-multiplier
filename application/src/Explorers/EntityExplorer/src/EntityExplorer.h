/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/IEntityExplorer.h>
#include <multiplier/GUI/ISearchWidget.h>
#include <multiplier/GUI/ThemeManager.h>

#include <QEvent>
#include <QWidget>

namespace mx::gui {

//! The entity explorer widget
class EntityExplorer final : public IEntityExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~EntityExplorer() override;

  //! Returns the active model
  virtual IEntityExplorerModel *Model() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  EntityExplorer(IEntityExplorerModel *model, QWidget *parent,
                 IGlobalHighlighter *global_highlighter);

  //! Initializes the internal widgets
  void InitializeWidgets();

  //! Installs the specified model, taking ownership of it
  void InstallModel(IEntityExplorerModel *model,
                    IGlobalHighlighter *global_highlighter);

  //! Installs the item delegate that paints the tokens
  void InstallItemDelegate(const CodeViewTheme &code_view_theme);

 private slots:
  //! Try to open the token related to a specific model index.
  void SelectionChanged(const QModelIndex &index, const QModelIndex &previous);

  //! Called automatically whenever the model is reset
  void OnModelReset();

  //! Called by the ISearchWidget component whenever filter options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Called whenever the query parameters are changed
  void QueryParametersChanged();

  //! Called when the token category filter changes
  void OnCategoryChange(const std::optional<TokenCategory> &opt_token_category);

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  friend class IEntityExplorer;
};

}  // namespace mx::gui
