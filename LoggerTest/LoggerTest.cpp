#include "Logger.h"

int main() {
    Logger logger;

    // Укажем путь к файлу логов и создадим папку, если нужно
    logger.init(LogLevel::DEBUG, "logs/mylog.txt", true);

    logger.log(LogLevel::TRACE, "This is a trace message", __FILE__, __LINE__);
    logger.log(LogLevel::DEBUG, "This is a debug message", __FILE__, __LINE__);
    logger.log(LogLevel::ERROR, "This is an error message", __FILE__, __LINE__);

    return 0;
}
