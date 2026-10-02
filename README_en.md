# IndustryBridgeBot

A configurable Telegram bot (Windows) that reads and writes industrial PLC tags over **OPC DA 2.0** and **OPC UA**, and exposes convenient access to them through Telegram.

🇷🇺 [Русская версия](README.md)

![](https://github.com/jkapter/IndustryBridgeBot/blob/main/img/OPC_DA_Telegram_bot_opc_main.png)

## Features

**Data sources**
- **OPC DA 2.0** — classic COM/DCOM connection to servers on the local machine or over the network.
- **OPC UA** — `opc.tcp://` connections, server tree browsing, periodic subscription-based reading, value writes; recognizes both built-in OPC UA types and IEC 61131-3 elementary types (BYTE/WORD/DWORD/TIME, etc.).
- Both sources run simultaneously; tags live in a single registry and are equally available for reading, writing, and use in bot messages.
- Tag value post-processing: a scaling factor and text-value substitution (e.g. `0` → "Off").

**Configurable bot**
- **Messages** — free-form Markdown text with current tag values substituted directly into the text (`#TAG:id#`).
- **Messages waiting for a reply** — the bot asks a question; the user's reply is validated and written into the selected PLC tag.
- **Inline buttons** — an arbitrary set of messages and/or a fixed tag write triggered on press.
- **Commands** (`/command_name`) — send a set of messages and/or write tags; can be added to the bot's main menu.
- **Events** — triggered by a tag value change (greater/less/equal/changed, with hysteresis) or on a schedule (every minute/hour/day/week/month/year).

**Users and access control**
- Roles: unregistered / registered / admin / banned.
- When a new user first contacts the bot, admins get a notification with **"Register"** and **"Ban"** buttons.
- All commands and actions require registration by default; individual commands/events/buttons can be restricted to a higher access level.
- An in-Telegram admin panel (`/admin`): user list, OPC connection status, restart polling, restart the application.

**Application**
- Runs from the system tray; OPC polling and the bot both auto-start with the application.
- Configuration (tags, messages, commands, buttons, events, users) is persisted to JSON, including autosave on exit.

![](https://github.com/jkapter/IndustryBridgeBot/blob/main/img/OPC_DA_Telegram_bot_messages_example.png)
![](https://github.com/jkapter/IndustryBridgeBot/blob/main/img/Screenshot_20260117-142327_Telegram.jpg)

## How it's built

- **`src/sourcedrivers/`** — data source drivers: `OPCDADriver`/`COPCClient` (COM/DCOM, classic OPC DA) and `OPCUADriver`/`COPCUAClient` (on top of `Qt6::OpcUa`), unified behind a common `DriverInterface`.
- **`src/datatag*`, `src/datatagregistry*`** — a unified tag model (`DataTag` and its specializations `DataTagOpcDA`/`DataTagOpcUA`) and the application's tag registry.
- **`src/tgobjects/`** — the Telegram bot's domain logic: users and permissions (`TGParent`, `USER_TYPE`), scripted objects (`TGMessage`, `TGMessageWaitAnswer`, `TGTriggerUserCommand`, `TGTriggerTagValue`, `TGScheduledEvent`, `TGButtonWCallback`), and their manager `TgBotManager`.
- **`third_party/tgbot/`** — a prebuilt copy of the [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp) library (shared `TgBot.dll`) for the Telegram Bot API.
- **`src/*widget*`, `src/*configurationwidget*`** — the Qt Widgets configuration UI: tag browsing/viewing, message/command/button/event setup, bot settings.
- **`src/logger.*`** — size-rotated file logging with optional minimum-level filtering (see "Configuration" below).

## Build dependencies

- Qt 6 (`Widgets`, `OpcUa` modules) — built and tested against Qt 6.10.
- [OPC Core Components](https://opcfoundation.org/developer-tools/samples-and-tools-classic/core-components/) — classic OPC DA headers (`OPC_Foundation/Include`).
- OpenSSL, libcurl, zlib — used by `tgbot-cpp` for the Telegram Bot API (expected under `third_party/openssl`, `third_party/curl`, `third_party/zlib`).
- Boost — header-only dependency of `tgbot-cpp` (asio).
- A C++20-capable compiler (built against the MinGW toolchain bundled with Qt).
- Windows

## Building

```powershell
cmake -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"
cmake --build build
```

Before building, make sure the OPC Foundation Core Components and the contents of `third_party/` (`tgbot`, `openssl`, `curl`, `zlib`) are in place — `src/CMakeLists.txt` references them with paths relative to the repository root.

## Configuration

All configuration — data sources, tags, messages, commands, buttons, events, and user permissions — is done through the application's GUI.

**Telegram bot token** is set once via a command-line argument and stored encrypted in `bin/token.dat`:
```
IndustryBridgeBot.exe -token="<token>"
IndustryBridgeBot.exe -show-token
```
`industrybridgebot_set_token.bat` and `industrybridgebot_show_token.bat` next to the executable do this for you.

**`bin/settings.json`** (optional) — extra startup options:
```json
{
  "start_application_on_tray": true,
  "log_level": "Warning"
}
```
- `start_application_on_tray` — start minimized to the system tray.
- `log_level` — minimum log level (`None`/`Debug`/`Info`/`Warning`/`Critical`/`Fatal`).
