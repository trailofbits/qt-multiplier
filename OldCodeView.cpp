// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "OldCodeView.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPlainTextDocumentLayout>
#include <QString>
#include <QStringRef>
#include <QTextBlock>
#include <QTextDocument>
#include <QThreadPool>
#include <QDebug>

#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include <multiplier/AST.h>
#include <multiplier/Index.h>
#include <multiplier/Util.h>
#include <multiplier/CodeTheme.h>
//#include <multiplier/DownloadCodeThread.h>

#include "Code.h"


namespace mx::gui {
namespace {

enum class CodeViewState {
  kInitialized,
  kDownloading,
  kRendering,
  kRendered,
  kFailed
};

}  // namespace

struct OldCodeView::PrivateData {

  // Current rendering state for the code view.
  CodeViewState state{CodeViewState::kInitialized};

  // Used to track re-entrancy issues, e.g. if in the process of rendering one
  // code, we've started to try to render another code.
  std::atomic<uint64_t> counter;

  // Thread-safe cache for figuring out line/column numbers.
  std::unique_ptr<Code> code;

  const CodeTheme &theme;
  const FileLocationCache locs;

  // The entity id of a file token that we'll target for scrolling.
  RawEntityId scroll_target_eid{kInvalidEntityId};

  // Keep track of the last clicked block number. If we click to go somewhere
  // in our own file, and where we're going is on the same line or a nearby
  // line, then we don't want to have to scroll, as that can be jarring.
  int last_block{-1};

  CodeViewLineNumberArea *line_area{nullptr};

  const Index index;

  inline PrivateData(const CodeTheme &theme_, const FileLocationCache &locs_, Index index_)
      : theme(theme_),
        locs(locs_),
        index(index_) {}
};

OldCodeView::~OldCodeView(void) {}

OldCodeView::OldCodeView(const CodeTheme &theme_, const FileLocationCache &locs_, Index index_,
                   QWidget *parent)
    : QPlainTextEdit(parent),
      d(std::make_unique<PrivateData>(theme_, locs_, index_)) {
  InitializeWidgets();
}

void OldCodeView::ScrollToFileToken(const TokenRange &range) {
  if (range) {
    ScrollToFileToken(range[0].id());
  } else {
    ScrollToFileToken(kInvalidEntityId);
  }
}

void OldCodeView::ScrollToFileToken(const Token &tok) {
  if (tok) {
    ScrollToFileToken(tok.id());
  } else {
    ScrollToFileToken(kInvalidEntityId);
  }
}

void OldCodeView::ScrollToFileToken(RawEntityId file_tok_id) {
  if (d->state != CodeViewState::kRendered) {
    d->scroll_target_eid = file_tok_id;
    return;
  }

  if (file_tok_id == kInvalidEntityId) {
    moveCursor(QTextCursor::MoveOperation::Start);
    ensureCursorVisible();
    centerCursor();
    return;
  }

  VariantId vid = EntityId(file_tok_id).Unpack();
  if (!std::holds_alternative<FileTokenId>(vid)) {
    assert(false);
    return;
  }

  auto begin_it = d->code->file_token_ids.begin();
  auto end_it = d->code->file_token_ids.end();
  auto it = std::lower_bound(begin_it, end_it, file_tok_id);
  if (it == end_it || *it != file_tok_id) {
    return;
  }

  auto tok_index = static_cast<unsigned>((it - begin_it));
  if (tok_index >= d->code->start_of_token.size()) {
    assert(false);  // Strange.
    return;
  }

  QPoint bottom_right(viewport()->width() - 1, viewport()->height() - 1);
  int desired_position = d->code->start_of_token[tok_index];
  int start_pos = cursorForPosition(QPoint(0, 0)).position();
  int end_pos = cursorForPosition(bottom_right).position();

  // Move the cursor to the desired location.
  QTextCursor loc = textCursor();
  loc.setPosition(desired_position,
                  QTextCursor::MoveMode::MoveAnchor);
  setTextCursor(loc);

  // Highlight the line containing the cursor.
  OnHighlightLine();

  // Figure out if we can avoid scrolling due to the text being (probably)
  // visible. If we have a `last_block`, then we clicked on something in here,
  // so it must have been visible.
  if (d->last_block != -1) {
    d->last_block = -1;
    if (start_pos < desired_position && desired_position < end_pos) {
      return;
    }
  }

  // We want to change the scroll in the viewport, so move us to the end of the
  // document (trick from StackOverflow), then back to the text cursor, then
  // center on the cursor.
  moveCursor(QTextCursor::MoveOperation::End);
  setTextCursor(loc);
  ensureCursorVisible();
  centerCursor();
}

void OldCodeView::SetFile(const File &file) {
  SetFile(Index::containing(file), file.id());
}

void OldCodeView::SetFile(const Index &index, RawEntityId file_id) {
  /*d->state = CodeViewState::kDownloading;
  d->scroll_target_eid = kInvalidEntityId;
  auto prev_counter = d->counter.fetch_add(1u);  // Go to the next version.

  auto downloader = DownloadCodeThread::CreateFileDownloader(
      index, d->locs, file_id);

  connect(downloader, &DownloadCodeThread::DownloadFailed,
          this, &OldCodeView::OnDownloadFailed);

  connect(downloader, &DownloadCodeThread::RenderCode,
          this, &OldCodeView::OnRenderCode);

  QThreadPool::globalInstance()->start(downloader);
  update();*/
}

void OldCodeView::SetFragment(const Fragment &fragment) {
  //SetFragment(Index::containing(fragment), fragment.id());
}

void OldCodeView::SetFragment(const Index &index, RawEntityId fragment_id) {
  /*d->state = CodeViewState::kDownloading;
  d->scroll_target_eid = kInvalidEntityId;
  auto prev_counter = d->counter.fetch_add(1u);  // Go to the next version.

  auto downloader = DownloadCodeThread::CreateFragmentDownloader(
      index, d->locs, fragment_id);

  connect(downloader, &DownloadCodeThread::DownloadFailed,
          this, &OldCodeView::OnDownloadFailed);

  connect(downloader, &DownloadCodeThread::RenderCode,
          this, &OldCodeView::OnRenderCode);

  QThreadPool::globalInstance()->start(downloader);
  update();*/
}

void OldCodeView::SetTokenRange(const Index &index, RawEntityId begin_tok_id,
                             RawEntityId end_tok_id) {
  /*d->state = CodeViewState::kDownloading;
  d->scroll_target_eid = kInvalidEntityId;
  auto prev_counter = d->counter.fetch_add(1u);  // Go to the next version.

  auto downloader = DownloadCodeThread::CreateTokenRangeDownloader(
      index, d->locs, begin_tok_id, end_tok_id);

  connect(downloader, &DownloadCodeThread::DownloadFailed,
          this, &OldCodeView::OnDownloadFailed);

  connect(downloader, &DownloadCodeThread::RenderCode,
          this, &OldCodeView::OnRenderCode);

  QThreadPool::globalInstance()->start(downloader);
  update();*/
}

void OldCodeView::Clear(void) {
  d->counter.fetch_add(1u);
  d->state = CodeViewState::kInitialized;
  d->code.reset();
  d->last_block = -1;
  d->scroll_target_eid = kInvalidEntityId;
  this->QPlainTextEdit::clear();
}

void OldCodeView::InitializeWidgets(void) {

  setReadOnly(true);
  setOverwriteMode(false);
  setTextInteractionFlags(Qt::TextSelectableByMouse);
  setContextMenuPolicy(Qt::CustomContextMenu);
  viewport()->setCursor(Qt::ArrowCursor);
  setFont(d->theme.Font());

  d->line_area = new CodeViewLineNumberArea(this);
  connect(this, &OldCodeView::updateRequest,
          this, &OldCodeView::UpdateLineNumberArea);

  connect(this, &OldCodeView::DataChanged,
          this, &OldCodeView::UpdateLineNumberAreaWidth);

  QFontMetrics fm(font());
  setLineWrapMode(d->theme.LineWrap() ?
                  QPlainTextEdit::LineWrapMode::WidgetWidth :
                  QPlainTextEdit::LineWrapMode::NoWrap);
  setTabStopDistance(d->theme.NumSpacesInTab() *
                     fm.horizontalAdvance(QChar::Space));

  auto p = palette();
  p.setColor(QPalette::All, QPalette::Base, d->theme.BackgroundColor());
  setPalette(p);
  setBackgroundVisible(false);

  connect(this, &OldCodeView::cursorPositionChanged,
          this, &OldCodeView::OnHighlightLine);
  
  connect(this, &OldCodeView::customContextMenuRequested,
          this, &OldCodeView::ShowContextMenu);

  update();
}

void OldCodeView::UpdateLineNumberAreaWidth(void) {
  setViewportMargins(LineNumberAreaWidth(), 0, 0, 0);
}

void OldCodeView::UpdateLineNumberArea(const QRect &rect, int dy) {
  if (dy) {
    d->line_area->scroll(0, dy);
  } else {
    d->line_area->update(0, rect.y(), d->line_area->width(), rect.height());
  }
  if (rect.contains(viewport()->rect())) {
    UpdateLineNumberAreaWidth();
  }
}

void OldCodeView::OnDownloadFailed(void) {
  d->state = CodeViewState::kFailed;
  update();
}

void OldCodeView::OnHighlightLine(void) {
  if (d->state != CodeViewState::kRendered) {
    return;
  }

  QList<QTextEdit::ExtraSelection> extra_selections;
  QTextEdit::ExtraSelection selection;

  selection.format.setBackground(d->theme.SelectedLineBackgroundColor());
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = textCursor();
  selection.cursor.clearSelection();
  extra_selections.append(selection);
  setExtraSelections(extra_selections);
}

void OldCodeView::OnRenderCode(void *code_, uint64_t counter) {
  std::unique_ptr<Code> code(reinterpret_cast<Code *>(code_));
  if (d->counter.load() != counter) {
    return;
  }

  d->state = CodeViewState::kRendering;
  d->last_block = ~0u;

  // An event may have asked for a new rendering; check re-entrancy.
  update();
  if (d->counter.load() != counter) {
    return;
  }

  d->code.swap(code);

  // Break the text into lines, so that each line is represented by a
  // `QTextBlock` behind the scenes.
  {
    int last_line = 0;
    int i = 0;
    auto first = true;
    auto append = [&, this] (void) {
      QStringRef line(&(d->code->data), last_line, i - last_line);
      if (first) {
        setPlainText(line.toString());
        first = false;
      } else {
        appendPlainText(line.toString());
      }
    };

    for (QChar ch : d->code->data) {
      if (QChar::LineSeparator == ch) {
        append();
        last_line = i + 1;
      }
      ++i;
    }
    if (last_line < i) {
      append();
    }
  }

  QTextCharFormat format;

  auto num_tokens = d->code->foreground.size();
  const auto &start_of_token = d->code->start_of_token;
  const auto &foreground_color = d->code->foreground;
  const auto &background_color = d->code->background;
  const auto &is_bold = d->code->bold;
  const auto &is_italic = d->code->italic;
  const auto &is_underlined = d->code->underline;

  QTextCursor cursor = textCursor();

  cursor.beginEditBlock();
  for (auto i = 0u; i < num_tokens; ++i) {
    cursor.setPosition(start_of_token[i], QTextCursor::MoveMode::MoveAnchor);
    cursor.setPosition(start_of_token[i + 1], QTextCursor::MoveMode::KeepAnchor);
    format.setForeground(*(foreground_color[i]));
    format.setBackground(*(background_color[i]));
    format.setFontItalic(is_italic[i]);
    format.setFontWeight(is_bold[i] ? QFont::DemiBold : QFont::Normal);
    format.setFontUnderline(is_underlined[i]);
    cursor.setCharFormat(format);
  }
  cursor.endEditBlock();

  d->state = CodeViewState::kRendered;
  ScrollToFileToken(d->scroll_target_eid);

  assert(d->counter.load() == counter);

  emit DataChanged();
  update();
}

std::optional<std::pair<unsigned, int>> OldCodeView::TokenIndexForPosition(
    const QPoint &pos) const {

  if (d->state != CodeViewState::kRendered) {
    return std::nullopt;
  }

  QTextCursor cursor = cursorForPosition(pos);
  if (cursor.isNull()) {
    return std::nullopt;
  }

  const auto position = cursor.position();
  if (0 > position || position >= d->code->start_of_token.back()) {
    return std::nullopt;
  }

  auto begin_it = d->code->start_of_token.begin();
  auto end_it = d->code->start_of_token.end();
  auto it = std::lower_bound(begin_it, end_it, position);
  if (it == begin_it || it == end_it) {
    return std::nullopt;
  }

  return std::make_pair<unsigned, int>(
      static_cast<unsigned>((it - begin_it) - 1), cursor.blockNumber());
}

void OldCodeView::EmitEventsForIndex(unsigned index) {

  assert((index + 1u) < d->code->tok_decl_ids_begin.size());
  auto locs_begin_index = d->code->tok_decl_ids_begin[index];
  auto locs_end_index = d->code->tok_decl_ids_begin[index + 1u];
  EventLocation loc;
  RawEntityId file_tok_id = d->code->file_token_ids[index];
  assert(file_tok_id != kInvalidEntityId);
  assert(std::holds_alternative<FileTokenId>(EntityId(file_tok_id).Unpack()));
  loc.SetFileTokenId(file_tok_id);

  if (auto num_locs = locs_end_index - locs_begin_index) {
    if (num_locs == 1u) {
      auto [frag_tok_id, decl_id] = d->code->tok_decl_ids[locs_begin_index];
      assert(frag_tok_id != kInvalidEntityId);
      loc.SetParsedTokenId(frag_tok_id);
      loc.SetReferencedDeclarationId(decl_id);

      emit TokenPressEvent(loc);

    } else {
      std::vector<EventLocation> locs(num_locs);
      for (auto i = locs_begin_index; i < locs_end_index; ++i) {
        auto [frag_tok_id, decl_id] = d->code->tok_decl_ids[i];
        assert(frag_tok_id != kInvalidEntityId);
        loc.SetParsedTokenId(frag_tok_id);
        loc.SetReferencedDeclarationId(decl_id);
        locs[i - locs_begin_index] = loc;
      }

      emit TokenPressEvent(locs);
    }

  // No fragments / declarations associated with this token.
  } else {
    emit TokenPressEvent(loc);
  }
}

void OldCodeView::mousePressEvent(QMouseEvent *event) {
  if (auto pos = TokenIndexForPosition(event->pos())) {
    auto [index, block] = pos.value();
    d->last_block = block;
    EmitEventsForIndex(index);
  } else {
    d->last_block = -1;
  }
  this->QPlainTextEdit::mousePressEvent(event);
}

void OldCodeView::ShowContextMenu(const QPoint& point) {
  if (auto pos = TokenIndexForPosition(point)) {
    auto [index, block] = pos.value();

    assert((index + 1u) < d->code->tok_decl_ids_begin.size());
    auto locs_begin_index = d->code->tok_decl_ids_begin[index];
    auto locs_end_index = d->code->tok_decl_ids_begin[index + 1u];
    auto file_tok_id = d->code->file_token_ids[index];
    assert(file_tok_id != kInvalidEntityId);
    assert(std::holds_alternative<FileTokenId>(EntityId(file_tok_id).Unpack()));

    auto contextMenu = this->createStandardContextMenu();
    contextMenu->addSeparator();

    auto useFileToken = new QAction("Use file token in console", this);
    connect(useFileToken, &QAction::triggered, [&](bool checked) {
      QString name = "file_token_" + QString::number(file_tok_id, 16);
      emit SetSingleEntityGlobal(name, file_tok_id);
    });
    contextMenu->addAction(useFileToken);

    if (auto num_locs = locs_end_index - locs_begin_index) {
      if (num_locs == 1u) {
        auto [frag_tok_id_, decl_id_] = d->code->tok_decl_ids[locs_begin_index];
        assert(frag_tok_id_ != kInvalidEntityId);

        auto frag_tok_id = frag_tok_id_;
        auto decl_id = decl_id_;

        auto useFragToken = new QAction("Use fragment token in console", this);
        connect(useFragToken, &QAction::triggered, [&](bool checked) {
          QString name = "frag_token_" + QString::number(frag_tok_id, 16);
          emit SetSingleEntityGlobal(name, frag_tok_id);
        });
        contextMenu->addAction(useFragToken);

        {
          auto useDecl = new QAction("Use declaration in console", this);
          useDecl->setEnabled(decl_id != kInvalidEntityId);
          connect(useDecl, &QAction::triggered, [&](bool checked) {
            QString name = "decl_" + QString::number(decl_id, 16);
            emit SetSingleEntityGlobal(name, decl_id);
          });
          contextMenu->addAction(useDecl);
        }
        
        auto frag_tok = std::get<Token>(d->index.entity(frag_tok_id));
        {
          std::vector<RawEntityId> type_ids;
          for(auto type : Type::containing(frag_tok)) {
            type_ids.push_back(type.id().Pack());
          }

          auto useTypes = new QAction("Use types in console", this);
          useTypes->setEnabled(!type_ids.empty());
          connect(useTypes, &QAction::triggered, [&, type_ids](bool checked) {
            QString name = "types";
            emit SetMultipleEntitiesGlobal(name, type_ids);
          });
          contextMenu->addAction(useTypes);
        }
        
        {
          std::vector<RawEntityId> stmt_ids;
          for(auto stmt : Stmt::containing(frag_tok)) {
            stmt_ids.push_back(stmt.id().Pack());
          }

          auto useStmts = new QAction("Use statements in console", this);
          useStmts->setEnabled(!stmt_ids.empty());
          connect(useStmts, &QAction::triggered, [&, stmt_ids](bool checked) {
            QString name = "stmts";
            emit SetMultipleEntitiesGlobal(name, stmt_ids);
          });
          contextMenu->addAction(useStmts);
        }
  
      } else {
        std::vector<mx::RawEntityId> frag_ids(num_locs);
        std::vector<mx::RawEntityId> decls;
        for (auto i = locs_begin_index; i < locs_end_index; ++i) {
          auto [frag_tok_id, decl_id] = d->code->tok_decl_ids[i];
          assert(frag_tok_id != kInvalidEntityId);
          frag_ids[i - locs_begin_index] = frag_tok_id;
          decls.push_back(decl_id);
        }

        auto useFragTokens = new QAction("Use fragment tokens in console", this);
        connect(useFragTokens, &QAction::triggered, [&, frag_ids](bool checked) {
          QString name = "frag_tokens";
          emit SetMultipleEntitiesGlobal(name, frag_ids);
        });
        contextMenu->addAction(useFragTokens);

        {
          auto useDecls = new QAction("Use declarations in console", this);
          useDecls->setEnabled(!decls.empty());
          connect(useDecls, &QAction::triggered, [&, decls](bool checked) {
            QString name = "decls";
            emit SetMultipleEntitiesGlobal(name, decls);
          });
          contextMenu->addAction(useDecls);
        }

        std::vector<RawEntityId> type_ids;
        std::vector<RawEntityId> stmt_ids;
        for(auto frag_tok_id : frag_ids) {
          auto frag_tok = std::get<Token>(d->index.entity(frag_tok_id));
          for(auto type : Type::containing(frag_tok)) {
            type_ids.push_back(type.id().Pack());
          }
          for(auto stmt : Stmt::containing(frag_tok)) {
            stmt_ids.push_back(stmt.id().Pack());
          }
        }

        {
          auto useTypes = new QAction("Use types in console", this);
          useTypes->setEnabled(!type_ids.empty());
          connect(useTypes, &QAction::triggered, [&, type_ids](bool checked) {
            QString name = "types";
            emit SetMultipleEntitiesGlobal(name, type_ids);
          });
          contextMenu->addAction(useTypes);
        }

        {
          auto useStmts = new QAction("Use statements in console", this);
          useStmts->setEnabled(!stmt_ids.empty());
          connect(useStmts, &QAction::triggered, [&, stmt_ids](bool checked) {
            QString name = "stmts";
            emit SetMultipleEntitiesGlobal(name, stmt_ids);
          });
          contextMenu->addAction(useStmts);
        }
      }
    }

    contextMenu->exec(mapToGlobal(point));
  }
}

void OldCodeView::resizeEvent(QResizeEvent *event) {
  this->QPlainTextEdit::resizeEvent(event);

  QRect cr = contentsRect();
  d->line_area->setGeometry(QRect(cr.left(), cr.top(),
                                  LineNumberAreaWidth(), cr.height()));
}

void OldCodeView::scrollContentsBy(int dx, int dy) {
  d->last_block = -1;
  this->QPlainTextEdit::scrollContentsBy(dx, dy);
}

void OldCodeView::paintEvent(QPaintEvent *event) {
  QString message;
  switch (d->state) {
    case CodeViewState::kInitialized:
      message = tr("Preparing to download...");
      break;
    case CodeViewState::kDownloading:
      message = tr("Downloading...");
      break;
    case CodeViewState::kRendering:
      message = tr("Rendering...");
      break;
    case CodeViewState::kRendered:
      this->QPlainTextEdit::paintEvent(event);
      return;
    case CodeViewState::kFailed:
      message = tr("Failed");
      break;
  }

  static const auto kTextFlags = Qt::AlignCenter | Qt::TextSingleLine;

  QFont message_font = font();
  message_font.setPointSizeF(message_font.pointSizeF() * 2.0);
  message_font.setBold(true);

  QFontMetrics font_metrics(message_font);
  auto message_rect = font_metrics.boundingRect(QRect(0, 0, 0xFFFF, 0xFFFF),
                                                kTextFlags, message);

  const auto &event_rec = event->rect();
  auto message_x_pos = (event_rec.width() / 2) - (message_rect.width() / 2);
  auto message_y_pos = (event_rec.height() / 2) - (message_rect.height() / 2);

  message_rect.moveTo(message_x_pos, message_y_pos);

  QPainter painter(viewport());
  painter.fillRect(event_rec, d->theme.BackgroundColor());

  painter.setFont(message_font);
  painter.setPen(QPen(palette().color(QPalette::WindowText)));
  painter.drawText(message_rect, kTextFlags, message);
  painter.end();

  event->accept();
}

int OldCodeView::LineNumberAreaWidth(void) {
  if (d->state != CodeViewState::kRendered) {
    return 0;
  }

  if (!d->code->first_line || !d->code->last_line) {
    return 0;
  }
  assert(d->code->first_line <= d->code->last_line);

  auto num_digits = 0;
  for (auto l = d->code->last_line; l; l /= 10) {
    ++num_digits;
  }

  QFontMetricsF metrics(font());
  return static_cast<int>(ceil(
      3 + (metrics.horizontalAdvance(QLatin1Char('9')) * num_digits)));
}

void OldCodeView::LineNumberAreaPaintEvent(QPaintEvent *event) {
  QPainter painter(d->line_area);
  painter.fillRect(event->rect(), d->theme.LineNumberBackgroundColor());

  QTextBlock block = firstVisibleBlock();
  unsigned line_number = static_cast<unsigned>(block.blockNumber()) +
                         d->code->first_line;
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  const QBrush &color = d->theme.LineNumberForegroundColor();
  auto area_width = d->line_area->width();
  auto font_height = fontMetrics().height();

  for (; block.isValid() && top <= event->rect().bottom(); ++line_number) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      QString number = QString::number(line_number);
      painter.setPen(color.color());
      painter.drawText(0, top, area_width, font_height,
                       Qt::AlignRight, number);
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
  }
}

CodeViewLineNumberArea::CodeViewLineNumberArea(OldCodeView *code_view_)
    : QWidget(code_view_),
      code_view(code_view_) {}

}  // namespace mx::gui
