// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "FileView.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QString>
#include <QVBoxLayout>

#include <multiplier/Index.h>
#include <multiplier/ui/ICodeView.h>

#include "Configuration.h"
#include "Multiplier.h"

namespace mx::gui {

struct FileView::PrivateData {
  FileConfiguration &config;

  ICodeModel *code_model{nullptr};
  ICodeView *code_view{nullptr};

  inline PrivateData(FileConfiguration &config_)
      : config(config_) {}
};

FileView::~FileView(void) {}

FileView::FileView(Multiplier &multiplier, std::filesystem::path file_path,
                   RawEntityId file_id, QWidget *parent)
    : QWidget(parent),
      d(std::make_unique<PrivateData>(multiplier.Configuration().file)) {

  setWindowTitle(file_path.c_str());

  /*d->content = new OldCodeView(multiplier.CodeTheme(),
                            multiplier.FileLocationCache(),
                            multiplier.Index());
  d->layout->addWidget(d->content);
  d->content->SetFile(multiplier.Index(), file_id);
  d->content->viewport()->installEventFilter(&multiplier);*/

  auto code_model = ICodeModel::Create(multiplier.FileLocationCache(), multiplier.Index());
  auto code_view = ICodeView::Create(code_model, this);
  code_model->SetFile(file_id);

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(code_view);
  setLayout(layout);

  /*connect(d->content, &OldCodeView::TokenPressEvent,
          this, &FileView::ActOnTokenPressEvent);

  connect(this, &FileView::TokenPressEvent,
          &multiplier, &Multiplier::ActOnTokenPressEvent);

  connect(d->content, &OldCodeView::SetSingleEntityGlobal,
          &multiplier, &Multiplier::SetSingleEntityGlobal);

  connect(d->content, &OldCodeView::SetMultipleEntitiesGlobal,
          &multiplier, &Multiplier::SetMultipleEntitiesGlobal);*/
}

void FileView::ScrollToToken(RawEntityId file_tok_id) const {
  //d->content->ScrollToFileToken(file_tok_id);
}

void FileView::ActOnTokenPressEvent(EventLocations locs) {

  // NOTE(pag): This is ugly. Often, we want a click in the code view to
  //            go to the referenced declaration, not just go back to
  //            itself. Therefore, we need to "mute" some of the components
  //            that we actually have data for.

  /*for (EventLocation loc : locs) {
    emit TokenPressEvent(EventSource::kCodeBrowserClickSource, loc);
    if (loc.UnpackDeclarationId()) {
      loc.SetParsedTokenId(kInvalidEntityId);
      loc.SetFileTokenId(kInvalidEntityId);
      emit TokenPressEvent(EventSource::kCodeBrowserClickDest, loc);
    }
  }*/
}

}  // namespace mx::gui
