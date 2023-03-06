// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QString>

#include <multiplier/Index.h>

#include <optional>

namespace mx::gui {

//! This component is used to asynchronously resolve entity names
class IEntityNameResolver : public QObject {
  Q_OBJECT

 public:
  //! Creates a new instance of the entity name resolver
  static IEntityNameResolver *Create(Index index, const RawEntityId &entity_id);

  //! Constructor
  IEntityNameResolver() = default;

  //! Destructor
  virtual ~IEntityNameResolver() override = default;

  //! Disabled copy constructor
  IEntityNameResolver(const IEntityNameResolver &) = delete;

  //! Disabled copy assignment operator
  IEntityNameResolver &operator=(const IEntityNameResolver &) = delete;

 public slots:
  //! Starts the name resolution process
  virtual void Start() = 0;

 signals:
  //! This signal is emitted when the name resolution has finished
  void Finished(const std::optional<QString> &opt_entity_name);
};

}  // namespace mx::gui
