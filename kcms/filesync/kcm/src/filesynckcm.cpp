// Settings module for the two-way folder sync.
//
// A thin front end: every action shells out to ~/.local/bin/filesync, the same
// script the systemd units call. One engine means the panel and the automatic
// runs can never disagree about what a sync does.
#include <KPluginFactory>
#include <KLocalizedString>
#include <KQuickConfigModule>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QVariantList>

static QString confPath() { return QDir::homePath() + QStringLiteral("/.config/filesync/filesync.conf"); }
static QString bin(const QString &n) { return QDir::homePath() + QStringLiteral("/.local/bin/") + n; }

class FileSyncKCM : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(bool syncEnabled READ syncEnabled WRITE setSyncEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int debounce READ debounce WRITE setDebounce NOTIFY settingsChanged)
    Q_PROPERTY(int periodicMinutes READ periodicMinutes WRITE setPeriodicMinutes NOTIFY settingsChanged)
    Q_PROPERTY(QString conflictPolicy READ conflictPolicy WRITE setConflictPolicy NOTIFY settingsChanged)
    Q_PROPERTY(QString ignorePatterns READ ignorePatterns WRITE setIgnorePatterns NOTIFY settingsChanged)
    Q_PROPERTY(bool notifyOnSync READ notifyOnSync WRITE setNotifyOnSync NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList pairs READ pairs NOTIFY settingsChanged)
    Q_PROPERTY(bool nasMounted READ nasMounted NOTIFY settingsChanged)
    Q_PROPERTY(bool running READ running NOTIFY settingsChanged)
    Q_PROPERTY(bool unitsActive READ unitsActive NOTIFY settingsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY settingsChanged)
    Q_PROPERTY(QString lastRun READ lastRun NOTIFY settingsChanged)

public:
    FileSyncKCM(QObject *parent, const KPluginMetaData &data) : KQuickConfigModule(parent, data) {}

    // QSettings/IniFormat maps top-level keys onto the [General] section, so
    // these are read WITHOUT a "General/" prefix. Using the prefix silently
    // creates a separate [%General] section and every read comes back empty -
    // which is how the units once got enabled while the switch said off.
    bool syncEnabled() const { return str(QStringLiteral("enabled")) == QLatin1String("true"); }
    void setSyncEnabled(bool v) { put(QStringLiteral("enabled"), v ? QStringLiteral("true") : QStringLiteral("false")); }

    int debounce() const { return num(QStringLiteral("debounce"), 5); }
    void setDebounce(int v) { put(QStringLiteral("debounce"), QString::number(qBound(1, v, 3600))); }

    int periodicMinutes() const { return num(QStringLiteral("periodicMinutes"), 15); }
    void setPeriodicMinutes(int v) { put(QStringLiteral("periodicMinutes"), QString::number(qBound(1, v, 1440))); }

    QString conflictPolicy() const { const auto v = str(QStringLiteral("conflictPolicy")); return v.isEmpty() ? QStringLiteral("newer") : v; }
    void setConflictPolicy(const QString &v) { put(QStringLiteral("conflictPolicy"), v); }

    QString ignorePatterns() const { return str(QStringLiteral("ignorePatterns")); }
    void setIgnorePatterns(const QString &v) { put(QStringLiteral("ignorePatterns"), v); }

    bool notifyOnSync() const { return str(QStringLiteral("notify")) != QLatin1String("false"); }
    void setNotifyOnSync(bool v) { put(QStringLiteral("notify"), v ? QStringLiteral("true") : QStringLiteral("false")); }

    // "Available", not "mounted right now". The share sits behind an automount
    // that releases it after ten idle minutes; touching the path brings it
    // straight back. Reporting the raw mount state made the page cry wolf and
    // grey out the sync buttons on a perfectly healthy share.
    bool nasMounted() const
    {
        // findmnt prints BOTH stacked filesystems here - "autofs" for the
        // trigger and "cifs" for the real mount - so this is a contains check,
        // not equality. Comparing the whole output never matched.
        if (sh(QStringLiteral("findmnt"), {QStringLiteral("-no"), QStringLiteral("FSTYPE"), QStringLiteral("/mnt/nas")}).contains(QLatin1String("cifs")))
            return true;
        return sh(QStringLiteral("systemctl"), {QStringLiteral("is-active"), QStringLiteral("mnt-nas.automount")}).trimmed() == QLatin1String("active");
    }
    bool running() const { return !sh(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("unison")}).trimmed().isEmpty(); }

    // The status line used to describe the switch position instead of reality:
    // it read syncEnabled() straight from the config file. When systemctl
    // failed the config still said true, so the panel claimed "Watching for
    // changes" while nothing watched and the user had no way to tell. Ask
    // systemd what is actually running.
    bool unitsActive() const
    {
        return sh(QStringLiteral("systemctl"),
                  {QStringLiteral("--user"), QStringLiteral("is-active"),
                   QStringLiteral("filesync-watch.service")}).trimmed() == QLatin1String("active");
    }
    QString lastError() const { return m_lastError; }

    QString lastRun() const
    {
        QFile f(QDir::homePath() + QStringLiteral("/.cache/filesync/filesync.log"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QStringLiteral("never");
        QString last;
        while (!f.atEnd()) {
            const QString l = QString::fromUtf8(f.readLine()).trimmed();
            if (!l.isEmpty())
                last = l;
        }
        return last.isEmpty() ? QStringLiteral("never") : last;
    }

    QVariantList pairs() const
    {
        QVariantList out;
        QSettings st(confPath(), QSettings::IniFormat);
        for (const QString &g : st.childGroups()) {
            if (!g.startsWith(QLatin1String("Pair_")))
                continue;
            QVariantMap m;
            m[QStringLiteral("key")] = g;
            m[QStringLiteral("name")] = g.mid(5);
            m[QStringLiteral("local")] = st.value(g + QStringLiteral("/local")).toString();
            m[QStringLiteral("remote")] = st.value(g + QStringLiteral("/remote")).toString();
            m[QStringLiteral("enabled")] = st.value(g + QStringLiteral("/enabled")).toString() == QLatin1String("true");
            const QString d = st.value(g + QStringLiteral("/direction")).toString();
            m[QStringLiteral("direction")] = d.isEmpty() ? QStringLiteral("twoway") : d;
            out << m;
        }
        return out;
    }

    Q_INVOKABLE void setPairValue(const QString &key, const QString &field, const QString &value)
    {
        QSettings st(confPath(), QSettings::IniFormat);
        st.setValue(key + QLatin1Char('/') + field, value);
        st.sync();
        Q_EMIT settingsChanged();
    }
    Q_INVOKABLE void addPair(const QString &name, const QString &local, const QString &remote)
    {
        if (name.trimmed().isEmpty())
            return;
        QString safe = name;
        safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
        QSettings st(confPath(), QSettings::IniFormat);
        const QString g = QStringLiteral("Pair_") + safe;
        st.setValue(g + QStringLiteral("/local"), local);
        st.setValue(g + QStringLiteral("/remote"), remote);
        st.setValue(g + QStringLiteral("/enabled"), QStringLiteral("false"));
        st.setValue(g + QStringLiteral("/direction"), QStringLiteral("twoway"));
        st.sync();
        Q_EMIT settingsChanged();
    }
    Q_INVOKABLE void removePair(const QString &key)
    {
        QSettings st(confPath(), QSettings::IniFormat);
        st.remove(key);
        st.sync();
        Q_EMIT settingsChanged();
    }

    // Only ever called from an explicit button press. It used to run on load,
    // which enabled the units while the master switch still said off.
    Q_INVOKABLE void apply()
    {
        const bool on = syncEnabled();
        m_lastError.clear();

        // Run these in order and wait. They used to be fired detached, which
        // threw away both the exit status and stderr: a systemctl that refused
        // to enable anything looked exactly like one that succeeded, and the
        // switch stayed on over units that were never started.
        if (!runOk({QStringLiteral("--user"), QStringLiteral("daemon-reload")}))
            return finish();

        const QString verb = on ? QStringLiteral("enable") : QStringLiteral("disable");
        for (const QString &unit : {QStringLiteral("filesync-watch.service"),
                                    QStringLiteral("filesync-periodic.timer")}) {
            if (!runOk({QStringLiteral("--user"), verb, QStringLiteral("--now"), unit}))
                return finish();
        }
        if (on)
            runOk({QStringLiteral("--user"), QStringLiteral("restart"), QStringLiteral("filesync-watch.service")});

        finish();
    }

    Q_INVOKABLE void refresh() { Q_EMIT settingsChanged(); }
    Q_INVOKABLE void syncNow() { term({bin(QStringLiteral("filesync")), QStringLiteral("sync"), QStringLiteral("--force")}); }
    Q_INVOKABLE void previewAll() { term({bin(QStringLiteral("filesync")), QStringLiteral("dry")}); }
    Q_INVOKABLE void previewPair(const QString &key) { term({bin(QStringLiteral("filesync")), QStringLiteral("dry"), key}); }
    Q_INVOKABLE void syncPair(const QString &key) { term({bin(QStringLiteral("filesync")), QStringLiteral("sync"), QStringLiteral("--force"), key}); }
    Q_INVOKABLE void openLog()
    {
        term({QStringLiteral("tail"), QStringLiteral("-n"), QStringLiteral("300"), QStringLiteral("-f"),
              QDir::homePath() + QStringLiteral("/.cache/filesync/filesync.log")});
    }
    // Presets: move a whole configuration to another machine.
    Q_INVOKABLE QString exportPreset(const QString &url)
    {
        const QString dest = QUrl(url).isLocalFile() ? QUrl(url).toLocalFile() : url;
        QFile src(confPath());
        if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
            return QStringLiteral("Cannot read the current configuration.");
        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return QStringLiteral("Cannot write to %1").arg(dest);
        out.write("# filesync preset - import from the File Sync settings page\n");
        out.write(src.readAll());
        return QString();
    }

    Q_INVOKABLE QString importPreset(const QString &url)
    {
        const QString from = QUrl(url).isLocalFile() ? QUrl(url).toLocalFile() : url;
        QFile in(from);
        if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
            return QStringLiteral("Cannot read %1").arg(from);
        const QByteArray body = in.readAll();
        if (!body.contains("[Pair_"))
            return QStringLiteral("That file has no folder pairs in it - not a filesync preset.");

        QFile::remove(confPath() + QStringLiteral(".bak"));
        QFile::copy(confPath(), confPath() + QStringLiteral(".bak"));
        QFile out(confPath());
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return QStringLiteral("Cannot write the configuration.");
        out.write(body);
        out.close();

        // An imported preset always lands switched OFF. The paths came from
        // another machine and may not exist here; syncing before someone has
        // looked at them could mirror the wrong thing in the wrong direction.
        setSyncEnabled(false);
        Q_EMIT settingsChanged();
        return QString();
    }

    Q_INVOKABLE QString statusText() { return sh(bin(QStringLiteral("filesync")), {QStringLiteral("status")}); }

Q_SIGNALS:
    void settingsChanged();

private:
    QString str(const QString &k) const { return QSettings(confPath(), QSettings::IniFormat).value(k).toString(); }
    int num(const QString &k, int def) const { return QSettings(confPath(), QSettings::IniFormat).value(k, def).toInt(); }
    void put(const QString &k, const QString &v)
    {
        QSettings st(confPath(), QSettings::IniFormat);
        st.setValue(k, v);
        st.sync();
        Q_EMIT settingsChanged();
    }
    static QString sh(const QString &prog, const QStringList &args)
    {
        QProcess p;
        p.start(prog, args);
        p.waitForFinished(8000);
        return QString::fromUtf8(p.readAllStandardOutput());
    }
    static void detach(const QString &prog, const QStringList &args) { QProcess::startDetached(prog, args); }

    // systemctl, synchrone. Rend false et retient stderr en cas d'échec, pour
    // que le panneau puisse le dire au lieu de laisser croire que tout va bien.
    bool runOk(const QStringList &args)
    {
        QProcess p;
        p.start(QStringLiteral("systemctl"), args);
        if (!p.waitForFinished(15000)) {
            m_lastError = i18n("systemctl did not answer in time (%1)", args.join(QLatin1Char(' ')));
            return false;
        }
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
            if (err.isEmpty())
                err = i18n("exit code %1", p.exitCode());
            m_lastError = i18n("systemctl %1 failed: %2", args.join(QLatin1Char(' ')), err);
            return false;
        }
        return true;
    }
    void finish() { Q_EMIT settingsChanged(); }

    QString m_lastError;
    // Long text output belongs in a terminal, not a cramped label.
    static void term(const QStringList &cmd)
    {
        QStringList a{QStringLiteral("--hold"), QStringLiteral("-e")};
        a += cmd;
        QProcess::startDetached(QStringLiteral("konsole"), a);
    }
};

K_PLUGIN_CLASS_WITH_JSON(FileSyncKCM, "kcm_filesync.json")
#include "filesynckcm.moc"
