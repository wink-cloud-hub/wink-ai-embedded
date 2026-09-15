# SPDX-License-Identifier: LGPL-3.0-only
# mcs51_families.cmake — resolve MCS-51 chip families from the manifest SSOT
# (Stage6 S6-2 / review S6-H1, PLAN-20260911-MCS51-S6).
#
# The root build (wink-micro-os/CMakeLists.txt) must not hardcode family names
# (cms8s/at89/...): a new chip that lands `chips/<family>/` + its
# `tools/manifests/chips/<family>.yaml` must select and link with no root
# CMake edit. CMake has no YAML parser, so this helper deliberately supports
# ONLY the restricted manifest subset the schema allows:
#
#     family: <scalar>
#     aliases:
#       - "name"          # or inline: aliases: ["name", ...]
#
# Anything else is ignored (validation is the Python loader's job).

set(_WINK_MCS51_MANIFEST_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../frameworks/mcs51/tools/manifests/chips")

# Collect family + alias names for every manifest into caller-scope lists.
function(_wink_mcs51_collect_families out_families out_alias_pairs)
    set(_families)
    set(_pairs)  # "alias=family" flat list
    file(GLOB _manifests
        "${_WINK_MCS51_MANIFEST_DIR}/*.yaml"
        "${_WINK_MCS51_MANIFEST_DIR}/*.yml")
    foreach(_mf IN LISTS _manifests)
        file(READ "${_mf}" _text)
        string(REGEX MATCH "family:[ \t]*([A-Za-z0-9_]+)" _m "${_text}")
        set(_fam "${CMAKE_MATCH_1}")
        if(_fam STREQUAL "")
            continue()
        endif()
        list(APPEND _families "${_fam}")

        string(REGEX MATCH "aliases:[ \t]*\\[([^]]*)\\]" _inline "${_text}")
        if(NOT _inline STREQUAL "")
            string(REPLACE "," ";" _items "${CMAKE_MATCH_1}")
            foreach(_it IN LISTS _items)
                string(STRIP "${_it}" _it)
                string(REPLACE "\"" "" _it "${_it}")
                string(REPLACE "'" "" _it "${_it}")
                string(TOLOWER "${_it}" _it)
                if(NOT _it STREQUAL "")
                    list(APPEND _pairs "${_it}=${_fam}")
                endif()
            endforeach()
        else()
            # Block list: '- name' lines between 'aliases:' and the next key.
            string(REPLACE "\r\n" "\n" _text "${_text}")
            string(REPLACE "\n" ";" _lines "${_text}")
            set(_in_aliases FALSE)
            foreach(_line IN LISTS _lines)
                if(_line MATCHES "^aliases:")
                    set(_in_aliases TRUE)
                    continue()
                endif()
                if(_in_aliases)
                    if(_line MATCHES "^[ \t]*-[ \t]*\"?([A-Za-z0-9_]+)\"?")
                        string(TOLOWER "${CMAKE_MATCH_1}" _alias)
                        list(APPEND _pairs "${_alias}=${_fam}")
                    elseif(_line MATCHES "^[A-Za-z_][A-Za-z0-9_]*:")
                        set(_in_aliases FALSE)
                    endif()
                endif()
            endforeach()
        endif()
    endforeach()
    set(${out_families} ${_families} PARENT_SCOPE)
    set(${out_alias_pairs} ${_pairs} PARENT_SCOPE)
endfunction()

# wink_mcs51_family_for_mcu(<mcu> <out_family> <out_known>)
# Exact match against family keys and aliases (case-insensitive).
function(wink_mcs51_family_for_mcu mcu out_family out_known)
    set(_family "")
    set(_known FALSE)
    string(TOLOWER "${mcu}" _mcu_lc)
    if(NOT _mcu_lc STREQUAL "")
        _wink_mcs51_collect_families(_families _pairs)
        foreach(_fam IN LISTS _families)
            if("${_mcu_lc}" STREQUAL "${_fam}")
                set(_family "${_fam}")
                set(_known TRUE)
                break()
            endif()
        endforeach()
        if(NOT _known)
            foreach(_pair IN LISTS _pairs)
                string(FIND "${_pair}" "=" _eq)
                string(SUBSTRING "${_pair}" 0 ${_eq} _alias)
                math(EXPR _fam_off "${_eq} + 1")
                string(SUBSTRING "${_pair}" ${_fam_off} -1 _fam)
                if("${_mcu_lc}" STREQUAL "${_alias}")
                    set(_family "${_fam}")
                    set(_known TRUE)
                    break()
                endif()
            endforeach()
        endif()
    endif()
    set(${out_family} "${_family}" PARENT_SCOPE)
    set(${out_known} ${_known} PARENT_SCOPE)
endfunction()

# wink_mcs51_family_for_board(<board-name> <out_family> <out_known>)
# Substring match: board names embed the family/alias (e.g.
# stc89c52_devboard -> at89c52 alias, cms8s78xx_devboard -> cms8s78xx).
function(wink_mcs51_family_for_board board out_family out_known)
    set(_family "")
    set(_known FALSE)
    string(TOLOWER "${board}" _board_lc)
    if(NOT _board_lc STREQUAL "")
        _wink_mcs51_collect_families(_families _pairs)
        foreach(_fam IN LISTS _families)
            string(FIND "${_board_lc}" "${_fam}" _pos)
            if(NOT _pos EQUAL -1)
                set(_family "${_fam}")
                set(_known TRUE)
                break()
            endif()
        endforeach()
        if(NOT _known)
            foreach(_pair IN LISTS _pairs)
                string(FIND "${_pair}" "=" _eq)
                string(SUBSTRING "${_pair}" 0 ${_eq} _alias)
                math(EXPR _fam_off "${_eq} + 1")
                string(SUBSTRING "${_pair}" ${_fam_off} -1 _fam)
                string(FIND "${_board_lc}" "${_alias}" _pos)
                if(NOT _pos EQUAL -1)
                    set(_family "${_fam}")
                    set(_known TRUE)
                    break()
                endif()
            endforeach()
        endif()
    endif()
    set(${out_family} "${_family}" PARENT_SCOPE)
    set(${out_known} ${_known} PARENT_SCOPE)
endfunction()
