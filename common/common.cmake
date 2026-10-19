# Shared helpers for every app in casioPrizmApps.
# Include from an app's CMakeLists.txt, then call casio_app(<target> ...).

set(CASIO_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR})

# Read the shared version from version.h ("v1.0") and turn it into the g3a
# header format "MM.mm.pppp" so the add-in's version shows on the calculator.
file(READ ${CASIO_COMMON_DIR}/include/version.h _version_h)
string(REGEX MATCH "APPS_VERSION[ \t]+\"v([0-9]+)\\.([0-9]+)\"" _m "${_version_h}")
set(_vmaj ${CMAKE_MATCH_1})
set(_vmin ${CMAKE_MATCH_2})
if(_vmaj LESS 10)
  set(_vmaj "0${_vmaj}")
endif()
if(_vmin LESS 10)
  set(_vmin "0${_vmin}")
endif()
set(APPS_VERSION_G3A "${_vmaj}.${_vmin}.0000")

# Shared code, built as a static library: the linker only takes the modules
# an app actually uses, so adding a module here costs the other apps nothing
set(COMMON_SOURCES
  ${CASIO_COMMON_DIR}/src/utils.c
  ${CASIO_COMMON_DIR}/src/info.c
  ${CASIO_COMMON_DIR}/src/i18n.c
  ${CASIO_COMMON_DIR}/src/keys.c
  ${CASIO_COMMON_DIR}/src/fmt.c
  ${CASIO_COMMON_DIR}/src/numfield.c
  ${CASIO_COMMON_DIR}/src/form.c
  ${CASIO_COMMON_DIR}/src/menu.c
  ${CASIO_COMMON_DIR}/src/units.c
  ${CASIO_COMMON_DIR}/src/ui.c
)

# Shared assets: big font and the repository QR code for the info screen
set(COMMON_ASSETS_cg
  ${CASIO_COMMON_DIR}/assets-cg/font_big.png
  ${CASIO_COMMON_DIR}/assets-cg/qr_repo.png
)

# casio_app(<NAME> [NAME_PT <name>] SOURCES ... [ASSETS ...] [ASSETS_fx ...]
#           [ASSETS_cg ...])
# Builds <NAME>.g3a (fx-CG) or <NAME>.g1a (fx-9860G) with the common helpers.
# NAME_PT is the menu name shown when the calculator is set to Portuguese.
function(casio_app NAME)
  cmake_parse_arguments(APP "" "NAME_PT" "SOURCES;ASSETS;ASSETS_fx;ASSETS_cg"
    ${ARGN})

  fxconv_declare_assets(${APP_ASSETS} ${APP_ASSETS_fx} ${APP_ASSETS_cg}
    ${COMMON_ASSETS_cg} WITH_METADATA)

  add_library(casio_common STATIC ${COMMON_SOURCES})
  target_include_directories(casio_common PUBLIC ${CASIO_COMMON_DIR}/include)
  target_compile_options(casio_common PRIVATE -Wall -Wextra -Os)
  target_link_libraries(casio_common PUBLIC Gint::Gint)

  # The assets stay in the executable: info.c (in the library) uses them
  add_executable(${NAME} ${APP_SOURCES}
    ${APP_ASSETS} ${APP_ASSETS_${FXSDK_PLATFORM}}
    ${COMMON_ASSETS_${FXSDK_PLATFORM}})
  target_compile_options(${NAME} PRIVATE -Wall -Wextra -Os)
  target_link_libraries(${NAME} casio_common)

  if("${FXSDK_PLATFORM_LONG}" STREQUAL fx9860G)
    generate_g1a(TARGET ${NAME} OUTPUT "${NAME}.g1a"
      NAME "${NAME}" ICON assets-fx/icon.png)
  elseif("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
    generate_g3a(TARGET ${NAME} OUTPUT "${NAME}.g3a"
      NAME "${NAME}" VERSION "${APPS_VERSION_G3A}"
      ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
    if(DEFINED APP_NAME_PT)
      # Runs after generate_g3a's own POST_BUILD step
      set(_g3a "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.g3a")
      add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND python3 ${CASIO_COMMON_DIR}/../scripts/g3a-name.py
          ${_g3a} pt "${APP_NAME_PT}"
        COMMAND fxgxa -r ${_g3a}
        VERBATIM)
    endif()
  endif()
endfunction()
