#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // The rest of the Omarchy palette, and what the shell derives from it, so
    // the window reads like the bar and its panels.
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString urgent READ urgent NOTIFY changed)
    Q_PROPERTY(QString dim READ dim NOTIFY changed)
    Q_PROPERTY(QString faint READ faint NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString hover READ hover NOTIFY changed)
    Q_PROPERTY(QString selected READ selected NOTIFY changed)
    Q_PROPERTY(QString pressed READ pressed NOTIFY changed)
    Q_PROPERTY(QString line READ line NOTIFY changed)
    Q_PROPERTY(QString overlay READ overlay NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }
    QString background() const { return m_background; }
    QString foreground() const { return m_foreground; }
    QString urgent() const { return m_urgent; }
    QString dim() const;
    QString faint() const { return mix(0.45); }
    QString surface() const { return mix(0.04); }
    QString hover() const { return mix(0.06); }
    QString selected() const { return mix(0.10); }
    QString pressed() const { return mix(0.16); }
    QString line() const { return mix(0.18); }
    QString overlay() const;

    static QString readAccent(const QString &path);
    // One `key = "#rrggbb"` from colors.toml, or the fallback.
    static QString readColor(const QString &path, const QString &key, const QString &fallback);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QString m_background = "#121212";
    QString m_foreground = "#e6e6e6";
    QString m_urgent = "#e5534b";
    // The foreground laid over the background at this strength.
    QString mix(double amount) const;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
