# Shared helpers for every app in casioPrizmApps.
# Include from an app's CMakeLists.txt, then call casio_app(<target> ...).

set(CASIO_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR})

set(COMMON_SOURCES
  ${CASIO_COMMON_DIR}/src/utils.c
  ${CASIO_COMMON_DIR}/src/info.c
  ${CASIO_COMMON_DIR}/src/i18n.c
)

# Shared assets: big font and the repository QR code for the info screen
set(COMMON_ASSETS_cg
  ${CASIO_COMMON_DIR}/assets-cg/font_big.png
  ${CASIO_COMMON_DIR}/assets-cg/qr_repo.png
)

# casio_app(<NAME> SOURCES ... [ASSETS ...] [ASSETS_fx ...] [ASSETS_cg ...])
# Builds <NAME>.g3a (fx-CG) or <NAME>.g1a (fx-9860G) with the common helpers.
function(casio_app NAME)
  cmake_parse_arguments(APP "" "" "SOURCES;ASSETS;ASSETS_fx;ASSETS_cg" ${ARGN})

  fxconv_declare_assets(${APP_ASSETS} ${APP_ASSETS_fx} ${APP_ASSETS_cg}
    ${COMMON_ASSETS_cg} WITH_METADATA)

  add_executable(${NAME} ${APP_SOURCES} ${COMMON_SOURCES}
    ${APP_ASSETS} ${APP_ASSETS_${FXSDK_PLATFORM}}
    ${COMMON_ASSETS_${FXSDK_PLATFORM}})
  target_include_directories(${NAME} PRIVATE ${CASIO_COMMON_DIR}/include)
  target_compile_options(${NAME} PRIVATE -Wall -Wextra -Os)
  target_link_libraries(${NAME} Gint::Gint)

  if("${FXSDK_PLATFORM_LONG}" STREQUAL fx9860G)
    generate_g1a(TARGET ${NAME} OUTPUT "${NAME}.g1a"
      NAME "${NAME}" ICON assets-fx/icon.png)
  elseif("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
    generate_g3a(TARGET ${NAME} OUTPUT "${NAME}.g3a"
      NAME "${NAME}" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
  endif()
endfunction()
