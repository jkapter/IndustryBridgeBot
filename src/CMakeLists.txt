set(HOME FALSE)
set(GARDEN FALSE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wunknown-pragmas")
set(Boost_USE_MULTITHREADED ON)
set(OPENSSL_ROOT_DIR "C:/Qt/Tools/mingw1310_64/opt")
set(OLELIB "-lole32")

if(HOME)
    set(Boost_ROOT "C:/dev/boost_1_87_0")
    set(CURL_LIBRARY "C:/dev/curl-8.12.1_1-win64-mingw")
    set(opc_system_path "C:/Program Files (x86)/Common Files/OPC Foundation/Include")
elseif(GARDEN)
    set(OPENSSL_ROOT_DIR "C:/Qt/Tools/mingw1310_64/opt")
    set(CURL_LIBRARY "C:/dev/curl-8.11.0")
    set(CURL_INCLUDE_DIR "C:/dev/curl-8.11.0/include")
    set(opc_system_path "C:/Program Files (x86)/Common Files/OPC Foundation/Include")
else()
    set(Boost_ROOT "C:/dev/boost_1_90_0")
    set(CURL_LIBRARY "C:/dev/curl-8.11.0")
    set(CURL_INCLUDE_DIR "C:/dev/curl-8.11.0/include")
    set(app_icon_resource_windows "../img/opcdabot.rc")
    set(opc_system_path "../OPC_Foundation/Include")
endif()


find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost CONFIG)
find_package(Boost REQUIRED)
find_package(CURL)

add_compile_options(-Wno-unknown-pragmas)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${OPENSSL_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${opc_system_path})

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

qt_add_executable(OPCDATgBot ${PROJECT_SOURCES}
    # opctag.h opctag.cpp
    logger.h logger.cpp
    # opcdatamanager.h opcdatamanager.cpp
    opcclientworker.h opcclientworker.cpp
    copcclient.h copcclient.cpp
    tgobject.h tgobject.cpp
    tgbotmanager.h tgbotmanager.cpp
    tgbotthreadwrapper.h tgbotthreadwrapper.cpp
    res.qrc
    ${app_icon_resource_windows}
    ../styles/styles.qss
    opcbrowsewidget.h opcbrowsewidget.cpp opcbrowsewidget.ui
    opcvaluesviewer.h opcvaluesviewer.cpp opcvaluesviewer.ui
    tgbotsettingswidget.h tgbotsettingswidget.cpp tgbotsettingswidget.ui
    plaintextconsole.cpp plaintextconsole.h
    hintinputdialog.cpp hintinputdialog.h
    tgbotconfigurationwidget.h tgbotconfigurationwidget.cpp tgbotconfigurationwidget.ui
    selectitemstablewigget.h selectitemstablewigget.cpp
    opctagpostprocessingwidget.h opctagpostprocessingwidget.cpp opctagpostprocessingwidget.ui
    tgmessageconfigurationwidget.h tgmessageconfigurationwidget.cpp tgmessageconfigurationwidget.ui
    tgcommandconfigurationwidget.h tgcommandconfigurationwidget.cpp tgcommandconfigurationwidget.ui
    tginlinebuttonsconfigurationwidget.h tginlinebuttonsconfigurationwidget.cpp tginlinebuttonsconfigurationwidget.ui
    tgeventsconfigurationwidget.h tgeventsconfigurationwidget.cpp tgeventsconfigurationwidget.ui
    datatag.h datatag.cpp
    datatagregistry.h datatagregistry.cpp
    sourcedrivermanager.h sourcedrivermanager.cpp
    sourcedrivers.h sourcedrivers.cpp
)

target_link_libraries(OPCDATgBot PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(OPCDATgBot PUBLIC TgBot ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})

set_target_properties(OPCDATgBot PROPERTIES
    ${BUNDLE_ID_OPTION}
    WIN64_EXECUTABLE TRUE
    WIN32_EXECUTABLE TRUE
)

set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lws2_32 -luuid ${OLELIB} ${CMAKE_CSS_STANDARD_LIBRARIES}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-L${CMAKE_SOURCE_DIR}/lib/ -Wl,-Bstatic,--whole-archive -lwinpthread-rc -Wl,--no-whole-archive")

if((NOT HOME) AND (NOT GARDEN) AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
add_custom_command(TARGET OPCDATgBot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_SOURCE_DIR}/bin/OPCDATgBot.exe"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:OPCDATgBot>" "${CMAKE_SOURCE_DIR}/bin/"
)
endif()
