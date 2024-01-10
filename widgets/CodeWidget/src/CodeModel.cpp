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

class Range {
  virtual ~Range(void) = default;

  // Updated 
  int begin{-1};
  int end{-1};

  virtual void Update()


  RawEntityId id;
  RawEntityId parent_id;
};

// A choice between multiple fragments.
struct ChoiceRange Q_DECL_FINAL : public Range {

};

struct FragmentRange {

};

// A choice between expansions.
struct MacroRange Q_DECL_FINAL : public Range {
  Range *before{nullptr};
  Range *after{nullptr};
};

// Range of text of a token.
struct TextRange Q_DECL_FINAL : public Range {
  Token token;
  QString text;
};

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

// Import a choice node.
void CodeModel::PrivateData::ImportChoiceNode(ChoiceTokenTreeNode node) {

  QTextCursor cursor(document);
  cursor.movePosition(QTextCursor::End);

  auto &range = choices.emplace_back();

  auto i = 0u;
  for (auto &item : range.node.children()) {
    auto fragment_id = item.first.id().Pack();
    range.fragments.emplace_back(fragment_id);
    range.choices.emplace_back(static_cast<int>(data.size()));
    fragments.try_emplace(fragment_id, &range, i++);
    bool block_added = false;
    ImportNode(block_added, std::move(item.second));
  }

  if (!i) {

  }
  
  // Always add a last one so that we can know the bounds on the last fragment.
  cursor.movePosition(QTextCursor::End);
  range.choices.emplace_back(cursor.position());
}

// Import a substitution node.
void CodeModel::PrivateData::ImportSubstitutionNode(
    SubstitutionTokenTreeNode node) {


  

  auto after_pos = static_cast<int>(data.size());

  MacroRange *range = nullptr;

  // Figure out the macro ID.
  std::optional<Macro> macro;
  auto sub = node.macro();
  if (std::holds_alternative<MacroSubstitution>(sub)) {
    macro = std::get<MacroSubstitution>(sub);
  } else {
    macro = std::get<MacroVAOpt>(sub);
  }

  auto &range = substitutions.emplace(
      macro->id().Pack(), std::move(macro.value())).first->second;

  range.before = static_cast<int>(data.size());
  ImportNode(range.node.before());

  position_to_range.emplace_back(range.before, &range);

  range.between = cursor.position();
  ImportNode(range.node.after());

  position_to_range.emplace_back(range.between, &range);

  range.after = cursor.position();
}

// Import a sequence of nodes.
void CodeModel::PrivateData::ImportSequenceNode(
    bool &block_added, SequenceTokenTreeNode node) {
  for (auto child_node : node.children()) {
    ImportNode(block_added, std::move(child_node));
  }
}

// Import a node containing a token.
void CodeModel::PrivateData::ImportTokenNode(
    bool &block_added, TokenTokenTreeNode node) {

  // Get the data of this token in Qt's native format.
  auto token = node.token();
  auto utf8_data = token.data();
  if (utf8_data.empty()) {
    return;
  }

  auto utf16_data = QString::fromUtf8(
      utf8_data.data(), static_cast<qsizetype>(utf8_data.size()));

  QTextCursor cursor(document);
  cursor.movePosition(QTextCursor::End);

  auto &range = tokens.emplace_back();
  range.token = std::move(token);
  range.before = cursor.position();

  QString data;

  auto add_and_clear_data = [&] (void) {
    if (!data.isEmpty()) {
      if (!block_added) {
        cursor.insertBlock();
        cursor.movePosition(QTextCursor::End);
        block_added = true;
      }
      cursor.insertText(data);
      cursor.movePosition(QTextCursor::End);
      data.clear();
    }
  };

  for (QChar ch : utf16_data) {
    switch (ch.unicode()) {
      case QChar::Tabulation:
        data.append(QChar::Tabulation);
        break;

      case QChar::Space:
      case QChar::Nbsp:
        data.append(QChar::Space);
        break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator: {

        data.append(QChar::LineSeparator);  // What `QPainter::paintText` likes.

        add_and_clear_data();        
        cursor.insertBlock();
        cursor.movePosition(QTextCursor::End);
        block_added = true;  // Force true if `data` was empty.
        break;
      }

      case QChar::CarriageReturn:
        continue;

      // TODO(pag): Consult with QFontMetrics or something else to determine
      //            if this character is visible?
      default:
        data.append(ch);
        break;
    }
  }

  add_and_clear_data();

  range.after = cursor.position();

  // E.g. a unitary newline.
  if (range.before == range.after) {
    tokens.pop_back();
    return;
  }

  position_to_range.emplace_back(range.before, &range);
}

// Import a generic node, dispatching to the relevant node.
void CodeModel::PrivateData::ImportNode(bool &block_added, TokenTreeNode node) {
  switch (node.kind()) {
    case TokenTreeNodeKind::EMPTY:
      break; 
    case TokenTreeNodeKind::TOKEN:
      ImportTokenNode(
          block_added,
          std::move(reinterpret_cast<TokenTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::CHOICE:
      ImportChoiceNode(
          std::move(reinterpret_cast<ChoiceTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SUBSTITUTION:
      ImportSubstitutionNode(
          std::move(reinterpret_cast<SubstitutionTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SEQUENCE:
      ImportSequenceNode(
          block_added,
          std::move(reinterpret_cast<SequenceTokenTreeNode &&>(node)));
      break;
  }
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
