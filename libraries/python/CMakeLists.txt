#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(python_component_list
  "Development.Embed"
)

find_package("Python3" "${MX_PYTHON_VERSION}" MODULE
  REQUIRED
  COMPONENTS ${python_component_list}
  GLOBAL
)
