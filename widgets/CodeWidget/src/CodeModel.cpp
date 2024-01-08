/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextFrameFormat>

#include <deque>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mx::gui {
namespace {

// Keep track of the before, between, and after of the macro. This way we can
// selectively show or hide stuff in the `[before, between)` or
// `[between, after)` ranges.
struct MacroRange {
  SubstitutionTokenTreeNode node;
  RawEntityId macro_id;
  QTextCursor before;
  QTextCursor between;
  QTextCursor after;

  inline MacroRange(SubstitutionTokenTreeNode node_)
      : node(std::move(node_)) {}
};

struct ChoiceRange {
  ChoiceTokenTreeNode node;
  std::vector<QTextCursor> choices;

  inline ChoiceRange(ChoiceTokenTreeNode node_)
      : node(std::move(node_)) {}
};

using Range = std::variant<MacroRange *, ChoiceRange *>;

}  // namespace

struct CodeModel::PrivateData {
  QTextDocument *document;

  const QTextFrameFormat default_format;

  // Allocation management for choice nodes.
  std::deque<ChoiceRange> choices;

  // Maps tokens to their fragments.
  std::unordered_multimap<RawEntityId, QTextFragment *> tokens;

  // Maps token related entity IDs to text fragments.
  std::unordered_multimap<RawEntityId, QTextFragment *> entities;

  // Maps macro substitution IDs to the ranges of the macros.
  std::unordered_map<RawEntityId, MacroRange> substitutions;

  // Maps fragment IDs to the specific choice within a choice range.
  std::unordered_map<RawEntityId, std::pair<ChoiceRange *, unsigned>> fragments;

  // Maps text frames to entity (e.g. macros, fragments, etc.) IDs.
  std::unordered_map<QTextFrame *, Range> frame_to_range;

  void ImportChoiceNode(
      QTextFrame *parent, ChoiceTokenTreeNode node);

  void ImportSubstitutionNode(
      QTextFrame *parent, SubstitutionTokenTreeNode node);

  void ImportSequenceNode(
      QTextFrame *parent, bool &block_added, SequenceTokenTreeNode node);

  void ImportTokenNode(
      QTextFrame *parent, bool &block_added, TokenTokenTreeNode node);

  void ImportNode(QTextFrame *parent, bool &block_added, TokenTreeNode node);
};

CodeModel::~CodeModel(void) {}

CodeModel::CodeModel(QObject *parent)
    : QObject(parent),
      d(new PrivateData) {

  d->document = new QTextDocument(this);
}

QTextDocument *CodeModel::Import(TokenTree tokens) {
  bool block_added = false;
  d->document->clear();
  d->ImportNode(d->document->rootFrame(), block_added, tokens.root());
  return d->document;
}

// Import a choice node.
void CodeModel::PrivateData::ImportChoiceNode(
    QTextFrame *parent, ChoiceTokenTreeNode node) {

  auto &range = choices.emplace_back(std::move(node));

  // Keep track of this range's containing frame.
  auto frame = parent->lastCursorPosition().insertFrame(default_format);
  frame_to_range.emplace(frame, &range);

  auto i = 0u;
  for (auto &item : range.node.children()) {
    range.choices.emplace_back(frame->lastCursorPosition());
    fragments.try_emplace(item.first.id().Pack(), &range, i++);
    bool block_added = false;
    ImportNode(frame, block_added, std::move(item.second));
  }
  
  // Always add a last one so that we can know the bounds on the last fragment.
  range.choices.emplace_back(frame->lastCursorPosition());
}

// Import a substitution node.
void CodeModel::PrivateData::ImportSubstitutionNode(
    QTextFrame *parent, SubstitutionTokenTreeNode node) {
  
  // Figure out the macro ID.
  auto macro_id = kInvalidEntityId;
  auto macro = node.macro();
  if (std::holds_alternative<MacroSubstitution>(macro)) {
    macro_id = std::get<MacroSubstitution>(macro).id().Pack();
  } else {
    macro_id = std::get<MacroVAOpt>(macro).id().Pack();
  }

  auto &range = substitutions.emplace(macro_id, std::move(node)).first->second;

  // Keep track of this range's containing frame.
  auto frame = parent->lastCursorPosition().insertFrame(default_format);
  frame_to_range.emplace(frame, &range);

  // Get the bounds between children.
  range.before = frame->lastCursorPosition();

  bool block_added = false;
  ImportNode(frame, block_added, range.node.before());
  range.between = frame->lastCursorPosition();

  block_added = false;
  ImportNode(frame, block_added, range.node.after());
  range.after = frame->lastCursorPosition();
}

// Import a sequence of nodes.
void CodeModel::PrivateData::ImportSequenceNode(
    QTextFrame *parent, bool &block_added, SequenceTokenTreeNode node) {
  for (auto child_node : node.children()) {
    ImportNode(parent, block_added, std::move(child_node));
  }
}

// Import a node containing a token.
void CodeModel::PrivateData::ImportTokenNode(
    QTextFrame *parent, bool &block_added, TokenTokenTreeNode node) {
  if (!block_added) {
    parent->lastCursorPosition().insertBlock();
    block_added = true;
  }

  auto token = node.token();
  auto data = token.data();

  auto cursor = parent->lastCursorPosition();
  cursor.insertText(QString::fromUtf8(
      data.data(), static_cast<qsizetype>(data.size())));
}

// Import a generic node, dispatching to the relevant node.
void CodeModel::PrivateData::ImportNode(
    QTextFrame *parent, bool &block_added, TokenTreeNode node) {
  switch (node.kind()) {
    case TokenTreeNodeKind::EMPTY:
      break; 
    case TokenTreeNodeKind::TOKEN:
      ImportTokenNode(
          parent, block_added,
          std::move(reinterpret_cast<TokenTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::CHOICE:
      ImportChoiceNode(
          parent,
          std::move(reinterpret_cast<ChoiceTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SUBSTITUTION:
      ImportSubstitutionNode(
          parent,
          std::move(reinterpret_cast<SubstitutionTokenTreeNode &&>(node)));
      break;
    case TokenTreeNodeKind::SEQUENCE:
      ImportSequenceNode(
          parent, block_added,
          std::move(reinterpret_cast<SequenceTokenTreeNode &&>(node)));
      break;
  }
}

}  // namespace mx::gui
