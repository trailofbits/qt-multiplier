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

#include "OldCodeView.h"
#include "Configuration.h"
#include "Multiplier.h"

namespace mx::gui {

struct FileView::PrivateData {
  FileConfiguration &config;
  QVBoxLayout *layout{nullptr};
  OldCodeView *content{nullptr};

  inline PrivateData(FileConfiguration &config_)
      : config(config_) {}
};

FileView::~FileView(void) {}

FileView::FileView(Multiplier &multiplier, std::filesystem::path file_path,
                   RawEntityId file_id, QWidget *parent)
    : QWidget(parent),
      d(std::make_unique<PrivateData>(multiplier.Configuration().file)) {

  setWindowTitle(file_path.c_str());

  d->layout = new QVBoxLayout;
  d->layout->setContentsMargins(0, 0, 0, 0);
  setLayout(d->layout);

  d->content = new OldCodeView(multiplier.CodeTheme(),
                            multiplier.FileLocationCache(),
                            multiplier.Index());
  d->layout->addWidget(d->content);
  d->content->SetFile(multiplier.Index(), file_id);
  d->content->viewport()->installEventFilter(&multiplier);

  /////////////////////////
  auto code_model = ICodeModel::Create(multiplier.FileLocationCache(), multiplier.Index());
  auto code_view2 = ICodeView::Create(code_model, this);
  code_model->SetFile(file_id);
  d->layout->addWidget(code_view2);
  /////////////////////////

  connect(d->content, &OldCodeView::TokenPressEvent,
          this, &FileView::ActOnTokenPressEvent);

  connect(this, &FileView::TokenPressEvent,
          &multiplier, &Multiplier::ActOnTokenPressEvent);

  connect(d->content, &OldCodeView::SetSingleEntityGlobal,
          &multiplier, &Multiplier::SetSingleEntityGlobal);

  connect(d->content, &OldCodeView::SetMultipleEntitiesGlobal,
          &multiplier, &Multiplier::SetMultipleEntitiesGlobal);
}

void FileView::ScrollToToken(RawEntityId file_tok_id) const {
  d->content->ScrollToFileToken(file_tok_id);
}

void FileView::ActOnTokenPressEvent(EventLocations locs) {

  // NOTE(pag): This is ugly. Often, we want a click in the code view to
  //            go to the referenced declaration, not just go back to
  //            itself. Therefore, we need to "mute" some of the components
  //            that we actually have data for.

  for (EventLocation loc : locs) {
    emit TokenPressEvent(EventSource::kCodeBrowserClickSource, loc);
    if (loc.UnpackDeclarationId()) {
      loc.SetParsedTokenId(kInvalidEntityId);
      loc.SetFileTokenId(kInvalidEntityId);
      emit TokenPressEvent(EventSource::kCodeBrowserClickDest, loc);
    }
  }
}

}  // namespace mx::gui
