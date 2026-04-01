cmake_minimum_required(VERSION 3.16)
project(OPCDATgBot VERSION 1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas")

add_definitions(-D_WIN32_WINNT=0x0A00)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
add_definitions(-DBOOST_ASIO_ENABLE_OLD_SERVICES)
add_definitions(-DHAVE_CURL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpcUa)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
set(OPC_INCLUDE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../OPC_Foundation/Include")

set(TgBot_DIR "${THIRD_PARTY_DIR}/tgbot/lib/cmake/TgBot")
find_package(TgBot REQUIRED)

set(OPENSSL_ROOT_DIR       "${THIRD_PARTY_DIR}/openssl")
set(OPENSSL_INCLUDE_DIR    "${THIRD_PARTY_DIR}/openssl/include")
set(OPENSSL_SSL_LIBRARY    "${THIRD_PARTY_DIR}/openssl/lib/libssl.dll.a")
set(OPENSSL_CRYPTO_LIBRARY "${THIRD_PARTY_DIR}/openssl/lib/libcrypto.dll.a")
find_package(OpenSSL REQUIRED)

set(ZLIB_INCLUDE_DIR "${THIRD_PARTY_DIR}/zlib/include")
set(ZLIB_LIBRARY     "${THIRD_PARTY_DIR}/zlib/lib/libz.a")
find_package(ZLIB REQUIRED)

set(CURL_INCLUDE_DIR "${THIRD_PARTY_DIR}/curl/include")
set(CURL_LIBRARY     "${THIRD_PARTY_DIR}/curl/lib/libcurl.a")
find_package(CURL 7.58.0 REQUIRED)

set(BOOST_ROOT "C:/dev/boost_1_90_0")
find_package(Boost REQUIRED)

find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPC_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    "${THIRD_PARTY_DIR}/tgbot/include"
    ${BOOST_ROOT}
)

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
    copcuaclient.h copcuaclient.cpp
)

target_link_libraries(OPCDATgBot PRIVATE
    TgBot::TgBot
    Qt6::Widgets
    Qt6::OpcUa
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    ${CMAKE_THREAD_LIBS_INIT}
    ws2_32
    wldap32
    crypt32
    normaliz
    ole32
    oleaut32
    uuid
)


set_target_properties(OPCDATgBot PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    add_custom_command(TARGET OPCDATgBot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_SOURCE_DIR}/bin/OPCDATgBot.exe"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:OPCDATgBot>" "${CMAKE_SOURCE_DIR}/bin/"
    )
endif()
