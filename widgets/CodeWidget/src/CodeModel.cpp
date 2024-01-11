/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <QPlainTextDocumentLayout>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextFrameFormat>

#include <deque>
#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/Frontend/TokenCategory.h>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mx::gui {
namespace {

struct ChoiceRange;
struct MacroRange;
struct TextRange;

// class Range {
//   virtual ~Range(void) = default;

//   // Updated 
//   int begin{-1};
//   int end{-1};

//   virtual void Update()


//   RawEntityId id;
//   RawEntityId parent_id;
// };

// class EmptyRange Q_DECL_FINAL : public Range {};

// // A choice between multiple fragments.
// struct ChoiceRange Q_DECL_FINAL : public Range {

// };

// struct FragmentRange {

// };

// // A choice between expansions.
// struct MacroRange Q_DECL_FINAL : public Range {
//   Range *before{nullptr};
//   Range *after{nullptr};
// };

// // Range of text of a token.
// struct TextRange Q_DECL_FINAL : public Range {
//   Token token;
//   QString text;
// };

}  // namespace

struct CodeModel::PrivateData {
  QTextDocument *document{nullptr};
  QPlainTextDocumentLayout *layout{nullptr};

  const QTextFrameFormat default_format;

  // All data in all versions/variants of the code. 
  QString data;

  // Allocation management for choice nodes.
  std::deque<ChoiceRange> choices;
  std::deque<MacroRange> macros;
  std::deque<TextRange> tokens;

  // Maps IDs to token text.
  std::unordered_multimap<RawEntityId, TextRange *> id_to_token;

  // Maps macro IDs to macro ranges.
  std::unordered_map<RawEntityId, MacroRange *> id_to_macro;

  // Maps fragment IDs to the specific choice within a choice range.
  std::unordered_map<RawEntityId, std::pair<ChoiceRange *, unsigned>>
      id_to_choice;

  // Sorted vector of positions to the ranges starting at that position.
  std::vector<std::pair<int, Range *>> position_to_range;

  void ImportChoiceNode(int depth, ChoiceTokenTreeNode node);

  void ImportSubstitutionNode(int depth, SubstitutionTokenTreeNode node);

  void ImportSequenceNode(int depth, SequenceTokenTreeNode node);

  void ImportTokenNode(int depth, TokenTokenTreeNode node);

  void ImportNode(int depth, TokenTreeNode node);
};

CodeModel::~CodeModel(void) {}

CodeModel::CodeModel(QObject *parent)
    : QObject(parent),
      d(new PrivateData) {

  d->document = new QTextDocument(this);
  d->layout = new QPlainTextDocumentLayout(d->document);
  d->document->setDocumentLayout(d->layout);
}

QTextDocument *CodeModel::Set(TokenTree tokens, const ITheme *theme) {
  bool block_added = false;
  auto doc = Reset();
  d->ImportNode(block_added, tokens.root());
  ChangeTheme(theme);
  return doc;
}

QTextDocument *CodeModel::Reset(void) {
  d->data.clear();
  d->choices.clear();
  d->macros.clear();
  d->tokens.clear();
  d->id_to_text.clear();
  d->id_to_macro.clear();
  d->id_to_choice.clear();
  d->position_to_range.clear();
  return d->document;
}

void CodeModel::ChangeTheme(const ITheme *theme) {
  // d->default_font_point_size = font.pointSizeF();
  auto font = theme->Font();

  d->document->setDefaultFont(font);

  QTextCharFormat format;
  for (const TextRange &text_range : d->tokens) {
    auto cs = theme->TokenColorAndStyle(text_range.token);

    format.setBackground(cs.background_color);
    format.setForeground(cs.foreground_color);
    format.setFontItalic(cs.italic);
    format.setFontWeight(cs.bold ? QFont::DemiBold : QFont::Normal);
    format.setFontUnderline(cs.underline);
    format.setFontStrikeOut(cs.strikeout);


    QTextCursor cursor(d->document);

    // Create a selection covering the data of the token.
    cursor.setPosition(text_range.before);
    cursor.setPosition(text_range.after, QTextCursor::KeepAnchor);

    // Format the selection.
    cursor.setCharFormat(format);

    qDebug() << text_range.before
             << (text_range.after - text_range.before)
             << text_range.token.data().size()
             << EnumeratorName(text_range.token.kind())
             << EnumeratorName(text_range.token.category());
  }
}

}  // namespace mx::gui
