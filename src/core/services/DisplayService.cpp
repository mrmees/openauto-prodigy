#include "DisplayService.hpp"
#include "core/Logging.hpp"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

namespace oap {

DisplayService::DisplayService(QObject* parent)
    : QObject(parent)
    , backend_(detectBackend())
{
    qCInfo(lcCore) << "DisplayService: backend =" << backendName();
}

DisplayService::Backend DisplayService::detectBackend()
{
    // 1. Check sysfs backlight
    QDir backlightDir(QStringLiteral("/sys/class/backlight"));
    if (backlightDir.exists()) {
        const auto entries = backlightDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& entry : entries) {
            QString path = backlightDir.absoluteFilePath(entry);
            QFile brightnessFile(path + QStringLiteral("/brightness"));
            if (brightnessFile.exists()) {
                sysfsPath_ = path;
                // Read max_brightness
                QFile maxFile(path + QStringLiteral("/max_brightness"));
                if (maxFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    bool ok = false;
                    int maxVal = QTextStream(&maxFile).readAll().trimmed().toInt(&ok);
                    if (ok && maxVal > 0) {
                        sysfsMax_ = maxVal;
                    }
                }
                qCInfo(lcCore) << "DisplayService: found sysfs backlight at"
                               << path << "max_brightness =" << sysfsMax_;
                return Backend::SysfsBacklight;
            }
        }
    }

    // 2. Check ddcutil
    QString ddcutil = QStandardPaths::findExecutable(QStringLiteral("ddcutil"));
    if (!ddcutil.isEmpty()) {
        QProcess probe;
        probe.start(ddcutil, {QStringLiteral("detect")});
        if (probe.waitForFinished(2000)) {
            QString output = QString::fromUtf8(probe.readAllStandardOutput());
            if (output.contains(QStringLiteral("Display"), Qt::CaseInsensitive)) {
                qCInfo(lcCore) << "DisplayService: ddcutil detected a display";
                return Backend::DdcUtil;
            }
        }
    }

    // 3. Software overlay fallback
    qCInfo(lcCore) << "DisplayService: no hardware backlight found, using software dimming overlay";
    return Backend::SoftwareOverlay;
}

QSize DisplayService::screenSize() const
{
    return QSize(1024, 600);
}

int DisplayService::brightness() const
{
    return brightness_;
}

void DisplayService::setBrightness(int value)
{
    value = std::clamp(value, 5, 100);
    if (value == brightness_) {
        return;
    }
    brightness_ = value;

    switch (backend_) {
    case Backend::SysfsBacklight:
        applySysfs(value);
        break;
    case Backend::DdcUtil:
        applyDdcutil(value);
        break;
    case Backend::SoftwareOverlay:
        // No hardware action — QML reads dimOverlayOpacity via binding
        break;
    }

    emit brightnessChanged();
}

QString DisplayService::orientation() const
{
    return QStringLiteral("landscape");
}

qreal DisplayService::dimOverlayOpacity() const
{
    return (100.0 - brightness_) / 100.0 * 0.9;
}

QString DisplayService::backendName() const
{
    switch (backend_) {
    case Backend::SysfsBacklight: return QStringLiteral("sysfs");
    case Backend::DdcUtil:        return QStringLiteral("ddcutil");
    case Backend::SoftwareOverlay: return QStringLiteral("software-overlay");
    }
    return QStringLiteral("unknown");
}

void DisplayService::applySysfs(int value)
{
    int scaled = value * sysfsMax_ / 100;
    QFile file(sysfsPath_ + QStringLiteral("/brightness"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(lcCore) << "DisplayService: cannot write sysfs brightness:"
                          << file.errorString();
        return;
    }
    QTextStream(&file) << scaled;
}

void DisplayService::applyDdcutil(int value)
{
    QString ddcutil = QStandardPaths::findExecutable(QStringLiteral("ddcutil"));
    if (ddcutil.isEmpty()) {
        return;
    }
    auto* proc = new QProcess(this);
    proc->start(ddcutil, {QStringLiteral("setvcp"), QStringLiteral("0x10"),
                          QString::number(value)});
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            proc, &QProcess::deleteLater);
    // Fire-and-forget with 2s timeout
    QTimer::singleShot(2000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning) {
            proc->kill();
        }
    });
}

} // namespace oap
