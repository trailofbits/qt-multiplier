// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QWidget>

#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/IThemeManager.h>

#include <multiplier/Index.h>

namespace mx::gui {

//! A top-most code view used for hover events
class QuickCodeView final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  QuickCodeView(const Index &index,
                const FileLocationCache &file_location_cache,
                RawEntityId entity_id, QWidget *parent = nullptr);

  //! Destructor
  virtual ~QuickCodeView() override;

  //! Disabled copy constructor
  QuickCodeView(const QuickCodeView &) = delete;

  //! Disabled copy assignment operator
  QuickCodeView &operator=(const QuickCodeView &) = delete;

 protected:
  //! Closes the widget when the escape key is pressed
  virtual void keyPressEvent(QKeyEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void showEvent(QShowEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void closeEvent(QCloseEvent *event) override;

  //! Used to handle window movements
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(const Index &index,
                         const FileLocationCache &file_location_cache,
                         RawEntityId entity_id);

  //! Updates the widget icons to match the active theme
  void UpdateIcons();

  //! Used to start window dragging
  void OnTitleFrameMousePress(QMouseEvent *event);

  //! Used to move the window by moving the title frame
  void OnTitleFrameMouseMove(QMouseEvent *event);

  //! Used to stop window dragging
  void OnTitleFrameMouseRelease(QMouseEvent *event);

 private slots:
  //! Restores the widget visibility when the application gains focus
  void OnApplicationStateChange(Qt::ApplicationState state);

  //! Tells us when we probably have the entity available.
  void OnEntityRequestFutureStatusChanged();

  //! Updates the window name with the entity name request output
  void EntityNameFutureStatusChanged();

  //! Forwards a subset of ICodeView::TokenTriggered events
  void OnTokenTriggered(const ICodeView::TokenAction &token_action,
                        const QModelIndex &index);

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

 signals:
  //! \brief This signal will only fire for TokenAction::Type::Keyboard events
  //! The reason it is limited to a single event type is that the popup
  //! needs to be closed automatically, and handling other interactions
  //! becomes trickier to make available without a design first.
  void TokenTriggered(const ICodeView::TokenAction &token_action,
                      const QModelIndex &index);
};

}  // namespace mx::gui
