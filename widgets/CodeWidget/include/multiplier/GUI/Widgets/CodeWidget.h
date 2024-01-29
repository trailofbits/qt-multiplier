/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QWidget>

#include <memory>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/Index.h>
#include <optional>
#include <utility>

QT_BEGIN_NAMESPACE
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QKeySequence;
class QMenu;
class QModelIndex;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;
QT_END_NAMESPACE

Q_DECLARE_METATYPE(mx::VariantEntity)

namespace mx {
class TokenTree;
}  // namespace mx
namespace mx::gui {

class ConfigManager;
class IWindowManager;
class MediaManager;
class ThemeManager;

// Renders code.
class CodeWidget Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  //! Options that configure the initial state of the scene.
  struct SceneOptions {
    QSet<RawEntityId> macros_to_expand;
    QMap<RawEntityId, QString> new_entity_names;
  };

  // Opaque representing for a Y-axis position in a document.
  struct OpaquePosition {
    qreal scale{0};
    int relative{-1};
    int physical{-1};
  };

  // Opaquely represents the current location in the code. This captures the
  // scroll configuration and the cursor location.
  //
  // TODO(pag): Try to implement the internal code that maintains scroll
  //            position in terms of this opaque location thing, that way it's
  //            more reliable and in the critical path.
  struct OpaqueLocation {
    OpaquePosition scroll_y;
    OpaquePosition current_y;
    OpaquePosition cursor_y;

    qreal scroll_x_scale{0};
    qreal cursor_x_scale{0};
    int cursor_index{-1};
  };

  virtual ~CodeWidget(void);

  enum : int {
    SelectedTextRole = IModel::MultiplierUserRole
  };

  // Create a code widget with the given configuration manager (used for theme
  // access), and `model_id`, which is used to identify the model used by the
  // signals emitted by this widget.
  CodeWidget(const ConfigManager &config_manager,
             const QString &model_id,
             bool browse_mode = false,
             QWidget *parent = nullptr);

  //! Change the underlying data / model being rendered by this code widget.
  void ChangeScene(const TokenTree &token_tree, const SceneOptions &options);

  //! Called when we want to act on the context menu.
  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index);

  // Return the last location from this widget.
  OpaqueLocation LastLocation(void) const;

  // Try to go to an opaque location.
  void TryGoToLocation(const OpaqueLocation &location, bool take_focus);

 protected:
  bool eventFilter(QObject *object, QEvent *event) Q_DECL_FINAL;
  void focusInEvent(QFocusEvent *event) Q_DECL_FINAL;
  void focusOutEvent(QFocusEvent *event) Q_DECL_FINAL;
  void paintEvent(QPaintEvent *event) Q_DECL_FINAL;
  void mousePressEvent(QMouseEvent *event) Q_DECL_FINAL;
  void mouseMoveEvent(QMouseEvent *event) Q_DECL_FINAL;
  void mouseReleaseEvent(QMouseEvent *event) Q_DECL_FINAL;
  void wheelEvent(QWheelEvent *event) Q_DECL_FINAL;
  void keyPressEvent(QKeyEvent *event) Q_DECL_FINAL;

 private slots:
  void OnIndexChanged(const ConfigManager &);
  void OnThemeChanged(const ThemeManager &);
  void OnToggleBrowseMode(const QVariant &toggled);
  void OnVerticalScroll(int change);
  void OnHorizontalScroll(int change);
  void OnGoToLineNumber(unsigned line);
  void OnSearchParametersChange(void);
  void OnShowSearchResult(size_t result_index);

 public slots:

  // Invoked when the set of macros to be expanded changes.
  void OnExpandMacros(const QSet<RawEntityId> &macros_to_expand);

  // Invoked when the set of entities to be renamed changes.
  void OnRenameEntities(const QMap<RawEntityId, QString> &new_entity_names);

  // Invoked when we want to scroll to a specific entity.
  void OnGoToEntity(const VariantEntity &entity, bool take_focus);
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::gui::CodeWidget::OpaquePosition)
Q_DECLARE_METATYPE(mx::gui::CodeWidget::OpaqueLocation)
