#pragma once

#include <QObject>

class TestBeaconSpake2 : public QObject
{
    Q_OBJECT

private slots:
    void hashesCompleteSubjectPublicKeyInfoDer();
    void matchesLockedProtocolVector();
    void rejectsWrongBeaconConfirmation();
};
