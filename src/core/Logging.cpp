#include "core/Logging.hpp"

#include <QDateTime>
#include <QThread>
#include <atomic>
#include <cstdio>

// --- Category definitions (quiet by default: QtInfoMsg threshold) ---
Q_LOGGING_CATEGORY(lcAA,     "oap.aa",     QtInfoMsg)
Q_LOGGING_CATEGORY(lcBT,     "oap.bt",     QtInfoMsg)
Q_LOGGING_CATEGORY(lcAudio,  "oap.audio",  QtInfoMsg)
Q_LOGGING_CATEGORY(lcPlugin, "oap.plugin", QtInfoMsg)
Q_LOGGING_CATEGORY(lcUI,     "oap.ui",     QtInfoMsg)
Q_LOGGING_CATEGORY(lcCore,   "oap.core",   QtInfoMsg)

namespace {

static std::atomic<bool> g_verbose{false};
static bool g_logToFile{false};
static FILE* g_logFile{nullptr};

// Known bracket tags from open-androidauto library
static const char* const g_libraryTags[] = {
    "[TCPTransport]", "[ControlChannel]", "[Messenger]",
    "[Session]", "[MediaChannel]", "[InputChannel]",
    "[SensorChannel]", "[AudioChannel]", "[VideoChannel]",
    "[BluetoothChannel]", "[WiFiChannel]", "[AVInputChannel]",
    "[FrameParser]", "[CryptoLayer]",
    nullptr
};

// Lifecycle keywords that pass through quiet-mode library filtering
static const char* const g_lifecycleKeywords[] = {
    "opened", "closed", "connected", "disconnected", "session",
    "started", "stopped", "shutdown", "initialized",
    nullptr
};

const char* shortTag(const char* category)
{
    if (!category) return "App";

    // oap.* categories
    if (qstrcmp(category, "oap.aa") == 0)     return "AA";
    if (qstrcmp(category, "oap.bt") == 0)     return "BT";
    if (qstrcmp(category, "oap.audio") == 0)  return "Audio";
    if (qstrcmp(category, "oap.plugin") == 0) return "Plugin";
    if (qstrcmp(category, "oap.ui") == 0)     return "UI";
    if (qstrcmp(category, "oap.core") == 0)   return "Core";

    // Library categories
    if (qstrncmp(category, "oaa.", 4) == 0) return "Lib";

    return "App";
}

const char* levelTag(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "D";
    case QtInfoMsg:     return "I";
    case QtWarningMsg:  return "W";
    case QtCriticalMsg: return "C";
    case QtFatalMsg:    return "F";
    }
    return "?";
}

bool containsLifecycleKeyword(const QString& message)
{
    for (int i = 0; g_lifecycleKeywords[i]; ++i) {
        if (message.contains(QLatin1String(g_lifecycleKeywords[i]), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void logMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    // Null-check category (Pitfall 5 from research)
    const char* cat = ctx.category ? ctx.category : "default";

    bool isLib = oap::isLibraryMessage(cat, ctx.file, msg);
    bool verbose = g_verbose.load(std::memory_order_relaxed);

    // In quiet mode, suppress library debug/info unless it's a lifecycle event
    if (!verbose && isLib) {
        if (type == QtDebugMsg || type == QtInfoMsg) {
            if (!containsLifecycleKeyword(msg))
                return;
        }
    }

    // Format timestamp
    QByteArray timestamp = QDateTime::currentDateTime()
                               .toString(QStringLiteral("HH:mm:ss.zzz"))
                               .toUtf8();

    const char* tag = shortTag(cat);
    const char* level = levelTag(type);
    QByteArray msgUtf8 = msg.toUtf8();

    if (verbose) {
        // Verbose: include thread name/ID
        QString threadName = QThread::currentThread()->objectName();
        if (threadName.isEmpty())
            threadName = QStringLiteral("0x%1").arg(
                reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
        QByteArray threadUtf8 = threadName.toUtf8();

        fprintf(stderr, "[%s] [%s] [%s] [%s] %s\n",
                timestamp.constData(), level, tag,
                threadUtf8.constData(), msgUtf8.constData());

        if (g_logToFile && g_logFile) {
            fprintf(g_logFile, "[%s] [%s] [%s] [%s] %s\n",
                    timestamp.constData(), level, tag,
                    threadUtf8.constData(), msgUtf8.constData());
            fflush(g_logFile);
        }
    } else {
        fprintf(stderr, "[%s] [%s] [%s] %s\n",
                timestamp.constData(), level, tag, msgUtf8.constData());

        if (g_logToFile && g_logFile) {
            fprintf(g_logFile, "[%s] [%s] [%s] %s\n",
                    timestamp.constData(), level, tag, msgUtf8.constData());
            fflush(g_logFile);
        }
    }
}

} // anonymous namespace

namespace oap {

void installLogHandler()
{
    qInstallMessageHandler(logMessageHandler);
}

void setVerbose(bool verbose)
{
    g_verbose.store(verbose, std::memory_order_relaxed);

    if (verbose) {
        // Enable debug for all oap.* and oaa.* categories
        QLoggingCategory::setFilterRules(QStringLiteral(
            "oap.*.debug=true\n"
            "oaa.*.debug=true\n"
        ));
    } else {
        // Restore quiet defaults: suppress debug for all oap.* and oaa.*
        QLoggingCategory::setFilterRules(QStringLiteral(
            "oap.*.debug=false\n"
            "oaa.*.debug=false\n"
        ));
    }
}

bool isVerbose()
{
    return g_verbose.load(std::memory_order_relaxed);
}

void setDebugCategories(const QStringList& categories)
{
    g_verbose.store(false, std::memory_order_relaxed);

    // Start with everything suppressed, then enable specific ones
    QString rules = QStringLiteral("oap.*.debug=false\noaa.*.debug=false\n");

    for (const QString& cat : categories) {
        rules += QStringLiteral("oap.%1.debug=true\n").arg(cat);
        // If "aa" is requested, also enable oaa.* (library)
        if (cat == QLatin1String("aa")) {
            rules += QStringLiteral("oaa.*.debug=true\n");
        }
    }

    QLoggingCategory::setFilterRules(rules);
}

void setLogFile(const QString& path)
{
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }

    if (path.isEmpty()) {
        g_logToFile = false;
        return;
    }

    g_logFile = fopen(path.toUtf8().constData(), "a");
    g_logToFile = (g_logFile != nullptr);
}

bool isLibraryMessage(const char* category, const char* file, const QString& message)
{
    // Check category prefix
    if (category && qstrncmp(category, "oaa.", 4) == 0)
        return true;

    // Check file path for open-androidauto
    if (file && strstr(file, "open-androidauto") != nullptr)
        return true;

    // Check for known bracket tags in message
    for (int i = 0; g_libraryTags[i]; ++i) {
        if (message.contains(QLatin1String(g_libraryTags[i])))
            return true;
    }

    return false;
}

} // namespace oap
