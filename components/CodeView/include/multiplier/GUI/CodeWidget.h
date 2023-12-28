// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QWidget>

#include <multiplier/GUI/ICodeView.h>
#include <multiplier/GUI/IThemeManager.h>
#include <multiplier/GUI/IGlobalHighlighter.h>
#include <multiplier/GUI/IMacroExplorer.h>

#include <multiplier/Index.h>

namespace mx::gui {

//! A widget containing a code view and its model
class CodeWidget final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  CodeWidget(const Index &index, const FileLocationCache &file_location_cache,
             RawEntityId entity_id, IGlobalHighlighter &highlighter,
             IMacroExplorer &macro_explorer, QWidget *parent = nullptr);

  //! Destructor
  virtual ~CodeWidget() override;

  //! Disabled copy constructor
  CodeWidget(const CodeWidget &) = delete;

  //! Disabled copy assignment operator
  CodeWidget &operator=(const CodeWidget &) = delete;


 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(const Index &index,
                         const FileLocationCache &file_location_cache,
                         RawEntityId entity_id, IGlobalHighlighter &highlighter,
                         IMacroExplorer &macro_explorer);

 private slots:
  //! Tells us when we probably have the entity available.
  void OnEntityRequestFutureStatusChanged();

  //! Forwards a subset of ICodeView::TokenTriggered events
  void OnTokenTriggered(const ICodeView::TokenAction &token_action,
                        const QModelIndex &index);


 public slots:
  //! Enables or disables the browser mode of the inner code view
  void SetBrowserMode(const bool &enabled);

 signals:
  //! \brief This signal will only fire for TokenAction::Type::Keyboard events
  //! The reason it is limited to a single event type is that the popup
  //! needs to be closed automatically, and handling other interactions
  //! becomes trickier to make available without a design first.
  void TokenTriggered(const ICodeView::TokenAction &token_action,
                      const QModelIndex &index);
};

}  // namespace mx::gui
