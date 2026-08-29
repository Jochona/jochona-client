#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

class AutoUpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString channel READ channel WRITE setChannel
               NOTIFY channelChanged)
    Q_PROPERTY(bool checkEnabled READ checkEnabled WRITE setCheckEnabled
               NOTIFY checkEnabledChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString availableVersion READ availableVersion
               NOTIFY updateStateChanged)
    Q_PROPERTY(QUrl availableUrl READ availableUrl
               NOTIFY updateStateChanged)

public:
    explicit AutoUpdateChecker(QObject* parent = nullptr);

    QString channel() const { return m_Channel; }
    void setChannel(const QString& channel);
    bool checkEnabled() const { return m_CheckEnabled; }
    void setCheckEnabled(bool enabled);
    QString status() const { return m_Status; }
    QString availableVersion() const { return m_AvailableVersion; }
    QUrl availableUrl() const { return m_AvailableUrl; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void checkNow();

signals:
    void updateAvailable(QString newVersion, QUrl url);
    void channelChanged();
    void checkEnabledChanged();
    void statusChanged();
    void updateStateChanged();

private slots:
    void handleUpdateCheckRequestFinished(QNetworkReply* reply);

private:
    static QVector<int> parseVersion(const QString& string);
    static int compareVersion(const QVector<int>& first,
                              const QVector<int>& second);
    void setStatus(const QString& status);

    QVector<int> m_CurrentVersion;
    QNetworkAccessManager m_Nam;
    QString m_Channel;
    bool m_CheckEnabled;
    bool m_Started = false;
    QString m_Status = QStringLiteral("idle");
    QString m_AvailableVersion;
    QUrl m_AvailableUrl;
};
