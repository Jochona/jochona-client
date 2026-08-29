#pragma once

#include <QObject>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QUrl>

class SupportBundleManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString previewText READ previewText NOTIFY previewChanged)
    Q_PROPERTY(bool includeAddresses READ includeAddresses WRITE setIncludeAddresses
               NOTIFY includeAddressesChanged)
    Q_PROPERTY(int historyRetentionDays READ historyRetentionDays
               WRITE setHistoryRetentionDays NOTIFY historyRetentionDaysChanged)
    Q_PROPERTY(bool telemetryEnabled READ telemetryEnabled CONSTANT)
    Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStatusChanged)

public:
    static SupportBundleManager* get();

    QString previewText() const { return m_PreviewText; }
    bool includeAddresses() const { return m_IncludeAddresses; }
    void setIncludeAddresses(bool include);
    int historyRetentionDays() const { return m_HistoryRetentionDays; }
    void setHistoryRetentionDays(int days);
    bool telemetryEnabled() const { return false; }
    QString exportStatus() const { return m_ExportStatus; }

    Q_INVOKABLE void refreshPreview();
    Q_INVOKABLE bool exportBundle(const QUrl& destination);
    Q_INVOKABLE bool clearHistory();
    Q_INVOKABLE bool exportDefaultBundle();

signals:
    void previewChanged();
    void includeAddressesChanged();
    void historyRetentionDaysChanged();
    void exportStatusChanged();

private:
    explicit SupportBundleManager(QObject* parent = nullptr);
    ~SupportBundleManager() override;

    bool ensureConnection();
    QJsonObject buildBundle();
    static QString pseudonym(const QString& value, const QString& prefix);
    static QString redactLog(QString text, bool includeAddresses);
    void setExportStatus(const QString& status);

    static SupportBundleManager* s_Instance;
    QString m_ConnectionName;
    QSqlDatabase m_Db;
    QString m_PreviewText;
    QJsonObject m_PreviewBundle;
    bool m_IncludeAddresses = false;
    int m_HistoryRetentionDays = 90;
    QString m_ExportStatus;
};
