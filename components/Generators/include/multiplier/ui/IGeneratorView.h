/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemModel>
#include <QWidget>
#include <QStyledItemDelegate>

namespace mx::gui {

//! Interface class for a configurable generator view
class IGeneratorView : public QWidget {
  Q_OBJECT

 public:
  //! Configuration settings for an IGeneratorView view
  struct Configuration final {
    //! How data is displayed on screen
    enum class ViewType {
      List,
      Table,
      Tree,
    };

    //! A list of actions that will populate either the menu or the OSD
    struct ActionList final {
      //! A list of active, connected actions to populate the view width
      std::vector<QAction *> action_list;

      //! Action update callback. The action data contains the model index
      std::function<void(QAction *)> update_action_callback;
    };

    //! The view type. Defaults to list
    ViewType view_type{ViewType::List};

    //! The optional item delegate
    QStyledItemDelegate *item_delegate{nullptr};

    //! Enables or disables sort and filtering. Defaults to enabled
    bool enable_sort_and_filtering{true};

    //! Context menu actions
    ActionList menu_actions;

    //! OSD buttons
    ActionList osd_actions;
  };

  //! Factory method
  static IGeneratorView *Create(QAbstractItemModel *model,
                                const Configuration &config,
                                QWidget *parent = nullptr);

  //! Constructor
  IGeneratorView(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IGeneratorView() override = default;

  //! Sets the currently selected item
  virtual void SetSelection(const QModelIndex &index) = 0;

  //! Disabled copy constructor
  IGeneratorView(const IGeneratorView &) = delete;

  //! Disabled copy assignment operator
  IGeneratorView &operator=(const IGeneratorView &) = delete;

 signals:
  //! Emitted when an item is selected
  //! \todo Change this to just click notfication
  void SelectedItemChanged(const QModelIndex &index);

  //! Emitted when a key is pressed on a selected item
  void KeyPressedOnItem(const QModelIndex &index, const Qt::Key &key);
};

}  // namespace mx::gui
