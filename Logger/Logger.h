#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

/**
 * @brief Конвертирует строку в кодировке UTF-8 в широкую строку (wstring).
 * @param utf8Str Строка в кодировке UTF-8.
 * @return Широкая строка (wstring).
 */
std::wstring utf8_to_wstring(const std::string& utf8Str);

/**
 * @enum LogLevel
 * @brief Уровни логирования.
 */
enum class LogLevel {
    TRACE,    /**< Трассировка */
    DEBUG,    /**< Отладка */
    INFO,     /**< Информация */
    WARNING,  /**< Предупреждение */
    ERROR_,   /**< Ошибка */
    CRITICAL  /**< Критическая ошибка */
};

/**
 * @enum OutputTarget
 * @brief Места вывода логов.
 */
enum class OutputTarget {
    Console = 1,    /**< Вывод в консоль */
    File = 2,       /**< Вывод в файл */
    Both = Console | File  /**< Вывод и в консоль, и в файл */
};

/**
 * @class Logger
 * @brief Класс для асинхронного многопоточного логирования с поддержкой пользовательских шаблонов.
 *
 * Позволяет логировать сообщения разных уровней с указанием файла и строки,
 * поддерживает вывод в консоль, файл или оба варианта,
 * а также форматирование сообщений по настраиваемому шаблону.
 */
class Logger {
public:
    /**
     * @brief Конструктор. Запускает поток обработки логов.
     */
    Logger();

    /**
     * @brief Деструктор. Завершает поток обработки и закрывает файл.
     */
    ~Logger();

    /**
     * @brief Инициализация логгера.
     * @param level Минимальный уровень логирования.
     * @param filePath Имя файла лога.
     * @param append Добавлять в конец файла (true) или перезаписывать (false).
     * @param addTimestampSuffix Добавлять ли временной суффикс к имени файла.
     */
    void init(LogLevel level, const std::string& filePath, bool append = true, bool addTimestampSuffix = true);

    /**
     * @brief Устанавливает минимальный уровень логирования.
     * @param level Новый уровень логирования.
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Устанавливает место вывода логов.
     * @param target Место вывода (консоль, файл, оба).
     */
    void setOutputTarget(OutputTarget target);

    /**
     * @brief Устанавливает пользовательский шаблон форматирования лог-сообщений.
     *
     * Поддерживаемые плейсхолдеры:
     * - {t} - timestamp (дата и время)
     * - {L} - уровень логирования
     * - {f} - имя файла
     * - {l} - номер строки
     * - {m} - текст сообщения
     *
     * @param formatTemplate Строка шаблона.
     */
    void setFormatTemplate(const std::string& formatTemplate);

    /**
     * @brief Логирует сообщение с указанным уровнем, файлом и строкой.
     * @param level Уровень логирования.
     * @param message Текст сообщения.
     * @param file Имя файла, откуда вызван лог.
     * @param line Номер строки в файле.
     */
    void log(LogLevel level, const std::string& message,
        const std::string& file, int line);

    /**
     * @brief Шаблонный метод для логирования с произвольным количеством параметров.
     * @tparam Args Типы параметров.
     * @param level Уровень логирования.
     * @param file Имя файла, откуда вызван лог.
     * @param line Номер строки.
     * @param args Параметры для формирования сообщения.
     */
    template<typename... Args>
    void log(LogLevel level, const std::string& file, int line, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        log(level, oss.str(), file, line);
    }

private:
    struct LogMessage {
        LogLevel level;         /**< Уровень логирования */
        std::string message;    /**< Текст сообщения */
        std::string file;       /**< Имя файла */
        int line;               /**< Номер строки */
        std::string timestamp;  /**< Временная метка */
    };

    LogLevel currentLevel = LogLevel::TRACE;   /**< Текущий уровень логирования */
    OutputTarget outputTarget = OutputTarget::Console;  /**< Текущий таргет вывода */

    std::ofstream logFileStream;    /**< Поток файла лога */
    std::string startupTime;        /**< Время запуска программы */
    std::string logFilePath;        /**< Путь к файлу лога */

    std::mutex queueMutex;          /**< Мьютекс для очереди сообщений */
    std::condition_variable cv;     /**< Условная переменная для уведомления */
    std::queue<LogMessage> messageQueue;  /**< Очередь сообщений */

    std::thread workerThread;       /**< Поток обработки логов */
    std::atomic<bool> exitFlag{ false };  /**< Флаг завершения */

    std::string formatTemplate = "{t} | {L} | {f}:{l} -> {m}";  /**< Шаблон форматирования */

    void workerFunc();              /**< Функция потока обработки сообщений */

    std::string getTimestamp() const;  /**< Получить текущую временную метку */
    std::string levelToString(LogLevel level) const;  /**< Преобразовать уровень в строку */

    std::string formatLogMessage(const LogMessage& msg) const;  /**< Форматировать сообщение по шаблону */

    void writeLog(const LogMessage& msg);  /**< Записать сообщение в вывод */
    void enqueueLog(const LogMessage& msg);  /**< Добавить сообщение в очередь */
};

/**
 * @brief Глобальный объект логгера.
 */
extern Logger LoggerInstance;

/**
 * @def LOGT(...)
 * @brief Макрос для логирования сообщений уровня TRACE.
 */
#define LOGT(...) LoggerInstance.log(LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)

 /**
  * @def LOGD(...)
  * @brief Макрос для логирования сообщений уровня DEBUG.
  */
#define LOGD(...) LoggerInstance.log(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)

  /**
   * @def LOGI(...)
   * @brief Макрос для логирования сообщений уровня INFO.
   */
#define LOGI(...) LoggerInstance.log(LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)

   /**
    * @def LOGW(...)
    * @brief Макрос для логирования сообщений уровня WARNING.
    */
#define LOGW(...) LoggerInstance.log(LogLevel::WARNING, __FILE__, __LINE__, __VA_ARGS__)

    /**
     * @def LOGE(...)
     * @brief Макрос для логирования сообщений уровня ERROR.
     */
#define LOGE(...) LoggerInstance.log(LogLevel::ERROR_, __FILE__, __LINE__, __VA_ARGS__)

     /**
      * @def LOGC(...)
      * @brief Макрос для логирования сообщений уровня CRITICAL.
      */
#define LOGC(...) LoggerInstance.log(LogLevel::CRITICAL, __FILE__, __LINE__, __VA_ARGS__)
