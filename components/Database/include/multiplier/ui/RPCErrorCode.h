/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

namespace mx::gui {

//! Error codes used by the IDatabase interface
enum class RPCErrorCode {
  Interrupted,
  NoDataReceived,
  InvalidEntityID,
  InvalidInformationRequestType,
  InvalidDownloadRequestType,
  IndexMismatch,
  FragmentMismatch,
  InvalidFragmentOffsetRange,
  InvalidTokenRangeRequest,
  FileMismatch,
  InvalidFileOffsetRange,
  InvalidFileTokenSorting,
};

}  // namespace mx::gui
