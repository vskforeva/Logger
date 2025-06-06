#include <windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <thread>
#include <chrono>
#include "Logger.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    setlocale(LC_ALL, "");

    std::wcout << L"Куда выводить лог? (1 - консоль, 2 - файл, 3 - оба, 4 - пользовательские шаблоны): ";
    int choice = 0;
    std::wcin >> choice;

    switch (choice) {
    case 1:
        LoggerInstance.setOutputTarget(OutputTarget::Console);
        break;
    case 2:
        LoggerInstance.setOutputTarget(OutputTarget::File);
        break;
    case 3:
        LoggerInstance.setOutputTarget(OutputTarget::Both);
        break;
    case 4:
        LoggerInstance.setOutputTarget(OutputTarget::Console);
        {
            std::wcout << L"Выберите шаблон лога:\n";
            std::wcout << L"1: {t} | {L} | {f}:{l} -> {m}\n";
            std::wcout << L"2: [{L}] {m}\n";
            std::wcout << L"3: {t} - {m}\n";
            std::wcout << L"4: {m} ({f}:{l})\n";
            std::wcout << L"Введите номер шаблона (1-4): ";
            int fmtChoice = 1;
            std::wcin >> fmtChoice;

            switch (fmtChoice) {
            case 1:
                LoggerInstance.setFormatTemplate("{t} | {L} | {f}:{l} -> {m}");
                break;
            case 2:
                LoggerInstance.setFormatTemplate("[{L}] {m}");
                break;
            case 3:
                LoggerInstance.setFormatTemplate("{t} - {m}");
                break;
            case 4:
                LoggerInstance.setFormatTemplate("{m} ({f}:{l})");
                break;
            default:
                LoggerInstance.setFormatTemplate("{t} | {L} | {f}:{l} -> {m}");
                break;
            }
        }
        break;
    default:
        std::wcout << L"Неверный выбор. Используется вывод в консоль.\n";
        LoggerInstance.setOutputTarget(OutputTarget::Console);
        break;
    }

    LoggerInstance.init(LogLevel::TRACE, "app_log.log", true, true);
    LoggerInstance.setLogLevel(LogLevel::TRACE);

    LOGT("Trace message: start app");
    LOGD("Debug message: value x = ", 123);
    LOGI("Info message: user login");
    LOGW("Warning message: low data");
    LOGE("Error message: error - cant open file ", "config.txt");
    LOGC("Critical message: system error!");

    LoggerInstance.init(LogLevel::DEBUG, "fixed_name_log.log", true, false);
    LoggerInstance.setLogLevel(LogLevel::DEBUG);

    LOGD("Debug message posle smeni loga");
    LOGI("Info message s neskolkimi parametrami: ", 3.14, ", string ", "primer");

    std::string user = "Alice";
    int errorCode = -404;
    LOGE("User error ", user, " with code ", errorCode);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::wcout << L"Завершение программы." << std::endl;
    return 0;
}
