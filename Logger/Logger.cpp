#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <windows.h>

/**
 * @brief Глобальный объект логгера.
 */
Logger LoggerInstance;

/**
 * @brief Конвертирует строку UTF-8 в широкую строку (wstring) для вывода в консоль Windows.
 * @param utf8Str Входная строка в UTF-8.
 * @return Широкая строка (wstring).
 */
std::wstring utf8_to_wstring(const std::string& utf8Str) {
    if (utf8Str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), (int)utf8Str.size(), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);

    MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), (int)utf8Str.size(), &wstrTo[0], size_needed);

    return wstrTo;
}

/**
 * @brief Конструктор Logger.
 *
 * Инициализирует время запуска, запускает поток обработки сообщений.
 */
Logger::Logger() {
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);
    std::tm timeInfo;
    localtime_s(&timeInfo, &t_c);

    std::ostringstream oss;
    oss << std::put_time(&timeInfo, "%Y-%m-%d_%H-%M-%S");
    startupTime = oss.str();

    workerThread = std::thread(&Logger::workerFunc, this);
}

/**
 * @brief Деструктор Logger.
 *
 * Завершает поток обработки, закрывает файл лога.
 */
Logger::~Logger() {
    exitFlag = true;
    cv.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }

    if (logFileStream.is_open()) {
        logFileStream.close();
    }
}

/**
 * @brief Инициализация логгера.
 * @param level Минимальный уровень логирования.
 * @param filePath Имя файла лога.
 * @param append Добавлять в конец файла или перезаписывать.
 * @param addTimestampSuffix Добавлять временной суффикс к имени файла.
 */
void Logger::init(LogLevel level, const std::string& filePath, bool append, bool addTimestampSuffix) {
    std::lock_guard<std::mutex> lock(queueMutex);
    currentLevel = level;

    std::filesystem::path path(filePath);
    std::filesystem::path dir = path.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    if (addTimestampSuffix) {
        std::string fullName;
        auto dot = filePath.rfind('.');
        if (dot != std::string::npos) {
            fullName = filePath.substr(0, dot) + "_" + startupTime + filePath.substr(dot);
        }
        else {
            fullName = filePath + "_" + startupTime;
        }
        logFilePath = fullName;
    }
    else {
        logFilePath = filePath;
    }

    std::ios_base::openmode mode = std::ios::out;
    if (append) mode |= std::ios::app;

    logFileStream.open(logFilePath, mode);
    if (logFileStream.is_open() && logFileStream.tellp() == 0) {
        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        logFileStream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    }
}

/**
 * @brief Устанавливает минимальный уровень логирования.
 * @param level Новый уровень.
 */
void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(queueMutex);
    currentLevel = level;
}

/**
 * @brief Устанавливает цель вывода логов (консоль, файл, оба).
 * @param target Цель вывода.
 */
void Logger::setOutputTarget(OutputTarget target) {
    std::lock_guard<std::mutex> lock(queueMutex);
    outputTarget = target;
}

/**
 * @brief Устанавливает шаблон форматирования сообщений.
 * @param formatTemplate Строка шаблона с плейсхолдерами.
 */
void Logger::setFormatTemplate(const std::string& formatTemplate) {
    std::lock_guard<std::mutex> lock(queueMutex);
    this->formatTemplate = formatTemplate;
}

/**
 * @brief Получить текущую временную метку.
 * @return Строка с датой и временем в формате "YYYY-MM-DD HH:MM:SS".
 */
std::string Logger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);
    std::tm timeInfo;
    localtime_s(&timeInfo, &t_c);

    std::ostringstream oss;
    oss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Преобразует уровень логирования в строку.
 * @param level Уровень логирования.
 * @return Строковое представление уровня.
 */
std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
    case LogLevel::TRACE: return "TRACE";
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::ERROR_: return "ERROR";
    case LogLevel::CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
    }
}

/**
 * @brief Форматирует сообщение согласно шаблону.
 * @param msg Сообщение для форматирования.
 * @return Отформатированная строка.
 */
std::string Logger::formatLogMessage(const LogMessage& msg) const {
    std::string formatted = formatTemplate;

    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        };

    replaceAll(formatted, "{t}", msg.timestamp);
    replaceAll(formatted, "{L}", levelToString(msg.level));
    replaceAll(formatted, "{f}", msg.file);
    replaceAll(formatted, "{l}", std::to_string(msg.line));
    replaceAll(formatted, "{m}", msg.message);

    return formatted;
}

/**
 * @brief Логирует сообщение, если уровень не ниже текущего.
 * @param level Уровень сообщения.
 * @param message Текст сообщения.
 * @param file Имя файла вызова.
 * @param line Номер строки.
 */
void Logger::log(LogLevel level, const std::string& message,
    const std::string& file, int line) {
    if (level < currentLevel) return;

    LogMessage msg;
    msg.level = level;
    msg.message = message;
    msg.file = file;
    msg.line = line;
    msg.timestamp = getTimestamp();

    enqueueLog(msg);
}

/**
 * @brief Добавляет сообщение в очередь для асинхронной обработки.
 * @param msg Сообщение для добавления.
 */
void Logger::enqueueLog(const LogMessage& msg) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(msg);
    }
    cv.notify_one();
}

/**
 * @brief Записывает сообщение в выбранные места вывода.
 * @param msg Сообщение для записи.
 */
void Logger::writeLog(const LogMessage& msg) {
    std::string output = formatLogMessage(msg);
    int target = static_cast<int>(outputTarget);

    if ((target & static_cast<int>(OutputTarget::Console)) != 0) {
        std::wcout << L"[Console] " << utf8_to_wstring(output) << std::endl;
    }

    if ((target & static_cast<int>(OutputTarget::File)) != 0) {
        if (logFileStream.is_open()) {
            std::wcout << L"[File] Запись в файл: " << utf8_to_wstring(logFilePath) << std::endl;
            std::wcout << L"[File] Записано байт: " << output.size() << std::endl;
            logFileStream << output << std::endl;
            logFileStream.flush();
        }
        else {
            std::wcout << L"[File] Файл не открыт!" << std::endl;
        }
    }
}

/**
 * @brief Функция потока, обрабатывающего очередь сообщений.
 *
 * Ожидает появления сообщений или сигнала выхода,
 * затем последовательно записывает их.
 */
void Logger::workerFunc() {
    while (!exitFlag) {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [this]() { return !messageQueue.empty() || exitFlag; });

        while (!messageQueue.empty()) {
            LogMessage msg = messageQueue.front();
            messageQueue.pop();
            lock.unlock();

            writeLog(msg);

            lock.lock();
        }
    }

    // Обработка оставшихся сообщений при завершении
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!messageQueue.empty()) {
        writeLog(messageQueue.front());
        messageQueue.pop();
    }
}
