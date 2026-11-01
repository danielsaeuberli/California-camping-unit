# LVGL user interface; texts are generated from ui/i18n/de.json at build time.
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
file(GLOB FONTS ${ROOT}/ui/fonts/*.c)
set(I18N_DIR ${CMAKE_CURRENT_BINARY_DIR}/i18n)
idf_component_register(SRCS "ui.c" ${FONTS} ${I18N_DIR}/i18n_de.c
                       INCLUDE_DIRS "include" ${I18N_DIR}
                       REQUIRES lvgl state_bus modules)
add_custom_command(OUTPUT ${I18N_DIR}/i18n_de.c ${I18N_DIR}/i18n.h
                   COMMAND python3 ${ROOT}/tools/i18n_gen.py ${ROOT}/ui/i18n/de.json ${I18N_DIR}
                   DEPENDS ${ROOT}/tools/i18n_gen.py ${ROOT}/ui/i18n/de.json)
add_custom_target(ui_i18n DEPENDS ${I18N_DIR}/i18n_de.c)
add_dependencies(${COMPONENT_LIB} ui_i18n)
