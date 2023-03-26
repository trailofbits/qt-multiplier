/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <QWidget>

#include <multiplier/Types.h>

namespace mx {
class Index;
class FileLocationCache;
namespace gui {

struct EntityInformation;

//! Provide general information about a given entity. What gets shown for each
//! entity is specific to the entity's category (e.g. function, variable, etc.).
class InformationExplorer final : public QWidget {
  Q_OBJECT

 public:
  enum class ModelState {
    UpdateInProgress,
    UpdateFailed,
    UpdateCancelled,
    Ready,
  };

  //! Destructor
  virtual ~InformationExplorer(void);

  //! Constructor
  InformationExplorer(const Index &index,
                     const FileLocationCache &file_location_cache,
                     QWidget *parent);

  //! Tell the information browser that the focus has changed to a different
  //! entity. This might not trigger changes in the information browser, as it
  //! may be pinned to a specific entity.
  bool AddEntityId(RawEntityId entity_id);

 signals:
  void BeginResetModel(void);
  void EndResetModel(const ModelState &model_state);

 private slots:
  void FutureResultStateChanged(void);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Cancel any running requests for information about an entity.
  void CancelRunningRequest(void);
};

}  // namespace gui
}  // namespace mx
