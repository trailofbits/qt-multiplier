/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ISearchWidget.h>
#include <multiplier/ui/IGeneratorView.h>

#include <QPoint>

namespace mx::gui {

//! A concrete implementation for the IGeneratorView interface
class GeneratorView : public IGeneratorView {
 public:
  //! Constructor
  GeneratorView(QAbstractItemModel *model, const Configuration &config,
                QWidget *parent);

  //! Destructor
  virtual ~GeneratorView() override;

  //! \copybrief IGeneratorView::SetSelection
  virtual void SetSelection(const QModelIndex &index) override;

 protected:
  //! Used to display and update OSD buttons
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

  //! Used to update the OSD buttons
  virtual void resizeEvent(QResizeEvent *event) override;

 private:
  Q_OBJECT

  struct PrivateData;

  //! Private instance data
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets();

  //! Updates and displays the OSD buttons
  void UpdateOSDButtons();

 private slots:
  //! Updates and displays the menu actions
  void OnOpenItemContextMenu(const QPoint &point);

  //! Used to auto-expand inserted rows
  void OnRowsInserted(const QModelIndex &parent, int first, int last);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Used to reset the hover
  void OnModelReset();
};

}  // namespace mx::gui
