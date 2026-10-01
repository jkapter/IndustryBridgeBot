#ifndef LOGGER_H
#define LOGGER_H

#include <QtMessageHandler>
#include <QString>
#include <QMutex>

#include <unordered_map>
#include <optional>

class QFile;
class QMessageLogContext;

class Logger
{
public:
    explicit Logger(const QString& dir, const QString& name);
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static void Init(const QString& dir, const QString& name);
    static void Clear();
    static void SetMaxSize(unsigned int new_size);
    static void LogMessage(QtMsgType type, const QMessageLogContext & context, const QString & message);
    static void SetMinLevel(std::optional<QtMsgType> level);
    static std::optional<QtMsgType> MinLevelFromString(const QString& str);

    ~Logger();


private:
    static QFile* logfile_;
    static QtMessageHandler original_handler_;
    static const std::unordered_map<QtMsgType, QString> type_names_;

    static unsigned int max_size_;

    static QString file_name_;
    static QString app_directory_;

    static std::optional<QtMsgType> min_level_;

    static QMutex logger_mtx_;

    static void check_and_rename_();
    static bool should_log_(QtMsgType type, const QMessageLogContext& context);
    static int severity_rank_(QtMsgType type);

};

#endif // LOGGER_H
