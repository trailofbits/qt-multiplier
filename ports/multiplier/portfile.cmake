vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL git@github.com:trailofbits/multiplier
    REF 4e0dd7aa4fd066f1a096a9c4d64d36989d47e1aa
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    CONFIG_PATH lib/cmake/multiplier
)

vcpkg_copy_tools(
    TOOL_NAMES
        mx-index
        mx-list-files
        mx-dump-files
        mx-print-file
        mx-highlight-entity
        mx-print-call-hierarchy
        mx-print-call-graph
        mx-print-include-graph
        mx-print-token-graph
        mx-print-token-tree
        mx-print-reference-graph
        mx-list-fragments
        mx-print-fragment
        mx-list-functions
        mx-list-variables
        mx-list-macros
        mx-list-redeclarations
        mx-list-structures
        mx-regex-query
        mx-find-flexible-user-copies
        mx-find-calls-in-macro-expansions
        mx-find-sketchy-casts
        mx-find-divergent-candidates
        mx-find-sketchy-strchr
        mx-find-symbol
        mx-taint-entity
        mx-harness
        mx-print-type-token-graph
        mx-find-linked-structures
        mx-count-sourceir
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")