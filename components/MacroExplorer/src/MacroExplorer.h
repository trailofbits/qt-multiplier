/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IMacroExplorer.h>

namespace mx {
class DefineMacroDirective;
class FileLocationCache;
class Index;
class MacroExpansion;
class MacroSubstitution;
class TokenTreeVisitor;
}  // namespace mx
namespace mx::gui {

class MacroExplorerItem;
class ICodeModel;

class MacroExplorer final : public IMacroExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~MacroExplorer(void);

  //! \copybrief IMacroExplorer::CreateCodeModel
  virtual ICodeModel *
  CreateCodeModel(const FileLocationCache &file_location_cache,
                  const Index &index, const bool &remap_related_entity_id_role,
                  QObject *parent) override;

 public slots:
  //! \copybrief IMacroExplorer::AddMacro
  virtual void AddMacro(RawEntityId macro_id, RawEntityId token_id) override;

 private slots:
  void RemoveMacro(RawEntityId macro_id);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  MacroExplorer(const Index &index,
                const FileLocationCache &file_location_cache, QWidget *parent);

  void AlwaysExpandMacro(const DefineMacroDirective &);
  void ExpandSpecificMacro(const DefineMacroDirective &,
                           const MacroExpansion &);
  void ExpandSpecificSubstitution(const Token &tok, const MacroSubstitution &);

  void UpdateList(void);

 signals:
  //! Signals all registered `ICodeView`s that they should re-run their token
  //! token serialization.
  void ExpandMacros(const TokenTreeVisitor *visitor);

  friend class IMacroExplorer;
  friend class MacroExplorerItem;
};

}  // namespace mx::gui
