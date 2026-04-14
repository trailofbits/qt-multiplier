// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace mx::gui {

struct AgentMessage {
  int64_t message_id{-1};
  int64_t session_id{-1};
  QString role;  // system, user, assistant, tool_call, tool_result
  QString content;
  QString tool_name;
  QString tool_call_id;
  QJsonObject tool_args;
  QJsonObject tool_result;
  QDateTime timestamp;
  int token_count{0};
};

// Structured result from the finish tool.
struct SessionResult {
  QString summary;
  QStringList next_actions;
  QString status;  // "completed", "blocked", "needs_input"
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::gui::AgentMessage)
