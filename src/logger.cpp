#include "logger.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cstdarg>
#include <memory>

// Static member definitions
LogLevel      Logger::s_logLevel       = LogLevel::NONE;
void*         Logger::s_consoleHandle  = nullptr;
bool          Logger::s_isInitialized  = false;
std::fstream* Logger::s_logFile        = nullptr;
std::mutex    Logger::s_logMutex;

static std::string
ConvertWideToUtf8(const wchar_t* wstr)
{
    if (!wstr) return {};
    std::wstring ws (wstr);
    return {ws.begin (), ws.end ()};
}

void
Logger::InitializeLogger(const LogLevel level, const bool logToFile, std::string logDir)
{
    if (s_isInitialized) {
        return;
    }
    s_isInitialized = true;

    s_logLevel = level;

    if (s_consoleHandle == nullptr) {
        s_consoleHandle = GetStdHandle (STD_OUTPUT_HANDLE);
    }

    if (logToFile) {
        try {
            const std::filesystem::path logsDir ("logs");
            if (!std::filesystem::exists (logsDir)) std::filesystem::create_directories (logsDir);
        } catch (const std::exception& e) {
            std::cerr << "[Logger] Failed to create logs directory: " << e.what () << std::endl;
        }

        // Construct a daily log filename
        const auto now   = std::chrono::system_clock::now ();
        const auto timeT = std::chrono::system_clock::to_time_t (now);
        std::tm localTm  = {};
        localtime_s (&localTm, &timeT);

        char dateFilename[256];
        // Example format: "TaikoArcadeLoader-YYYYMMDD.log"
        std::strftime (dateFilename, sizeof (dateFilename), "TaikoArcadeLoader-%Y%m%d.log", &localTm);

        if (!logDir.ends_with ("/")) logDir += "/";
        const std::string fullLogPath = logDir + dateFilename;
        s_logFile = new std::fstream (fullLogPath.c_str (), std::ios::out | std::ios::app);
        if (!s_logFile->is_open ()) {
            std::cerr << "[Logg er] Failed to open " << fullLogPath << " for writing.\n";
            s_logFile = nullptr;
        }
    }
}

void
Logger::LogMessageHandler (const char* function, const char* codeFile, int codeLine, LogLevel messageLevel, const char* format, ...) {
    // Return if logger uninitialized or messageLevel is more verbose than the set level
    if (!s_isInitialized || messageLevel > s_logLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock (s_logMutex);

    va_list args;
    va_start (args, format);
    int requiredSize = vsnprintf (nullptr, 0, format, args) + 1; // +1 for null terminator
    std::unique_ptr<char[]> buffer (new char[requiredSize]);
    vsnprintf (buffer.get (), requiredSize, format, args);
    std::string formattedMessage (buffer.get());
    va_end (args);

    std::string logType = Logger::GetLogLevelString(messageLevel);

    std::string short_function (function);
    std::regex re (R"(.*? (([\w<>]+::)*[\w]+( [()<>+-]+)?)\(\w+.*?\))");
    short_function = std::regex_replace (short_function, re, "$1");

    constexpr std::string_view build_dir = XSTRING (SOURCE_ROOT);
    std::string_view filename = codeFile;
    filename.remove_prefix (build_dir.size ());

    SYSTEMTIME systemTime;
    GetSystemTime (&systemTime);
    time_t rawTime = time (nullptr);
    tm* timeInfo   = localtime (&rawTime);

    std::ostringstream timeStamp;
    timeStamp << std::put_time (timeInfo, "%Y-%m-%d %H:%M:%S") << "."
              << std::setw (3) << std::setfill ('0') << systemTime.wMilliseconds;

    // Construct the log message
    std::ostringstream descStream;
    descStream << short_function << " (" << filename << ":" << codeLine << "): ";
    std::string descMessage = descStream.str();

    // Print to console
    std::cout << "[" << timeStamp.str() << "] ";
    SetConsoleTextAttribute (s_consoleHandle, Logger::GetLogLevelColor(messageLevel));
    std::cout << logType;
    SetConsoleTextAttribute (s_consoleHandle, FOREGROUND_INTENSITY);
    std::cout << descMessage;
    // Reset console color
    SetConsoleTextAttribute (s_consoleHandle, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cout << formattedMessage << std::endl;
    std::cout.flush ();

    if (s_logFile != nullptr) {
        *s_logFile << "[" << timeStamp.str () << "] " << logType << descMessage << formattedMessage << std::endl;
        s_logFile->flush ();
    }
}

void
Logger::LogMessageHandler (const char* function, const char* codeFile, int codeLine, LogLevel messageLevel, const wchar_t* format, ...) {
	const std::string utf8Message = ConvertWideToUtf8 (format);

    va_list args;
    va_start (args, format);
    Logger::LogMessageHandler (function, codeFile, codeLine, messageLevel, utf8Message.c_str(), args);
    va_end (args);
}

void
Logger::CleanupLogger () {
    if (s_isInitialized) {
        if (s_logFile) {
            s_logFile->close ();
            s_logFile = nullptr;
        }
        s_isInitialized = false;
    }
}

LogLevel
Logger::GetLogLevel (const std::string& logLevelStr) {
    if (logLevelStr == "DEBUG") return LogLevel::DEBUG;
	if (logLevelStr == "INFO")  return LogLevel::INFO;
	if (logLevelStr == "WARN")  return LogLevel::WARN;
	if (logLevelStr == "ERROR") return LogLevel::ERROR;
	if (logLevelStr == "HOOKS") return LogLevel::HOOKS;
	return LogLevel::NONE;
}

std::string
Logger::GetLogLevelString (const LogLevel messageLevel) {
    switch (messageLevel) {
        case LogLevel::DEBUG: return "DEBUG: ";
        case LogLevel::INFO:  return "INFO:  ";
        case LogLevel::WARN:  return "WARN:  ";
        case LogLevel::ERROR: return "ERROR: ";
        case LogLevel::HOOKS: return "HOOKS: ";
        default:              return "NONE:  ";
    }
}

int
Logger::GetLogLevelColor(const LogLevel messageLevel)
{
    // Colors: https://i.sstatic.net/ZG625.png
    switch (messageLevel) {
        case LogLevel::DEBUG:
            return FOREGROUND_BLUE | FOREGROUND_INTENSITY;                  // Pale Blue
        case LogLevel::INFO:
            return FOREGROUND_GREEN | FOREGROUND_INTENSITY;                 // Pale Green
        case LogLevel::WARN:
            return FOREGROUND_RED | FOREGROUND_GREEN;                       // Bright Yellow
        case LogLevel::ERROR:
            return FOREGROUND_RED;                                          // Bright RED
        case LogLevel::HOOKS:
            return FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY; // Pale Purple
        default:
            return FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    }
}
