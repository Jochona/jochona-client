#include "tst_beaconspake2.h"

#include "backend/beacon/spake2client.h"

#include <QtTest>

namespace
{
BeaconSpake2Client vectorClient()
{
    return BeaconSpake2Client(
        QUuid(QStringLiteral("0f9e1a2b-3c4d-4e5f-8a9b-0c1d2e3f4a5b")),
        QUuid(QStringLiteral("7d6c5b4a-3928-4170-9e1d-2c3b4a5f6e7d")),
        QByteArrayLiteral("12345678"),
        QByteArray::fromHex(
            "aa11bb22cc33dd44ee55ff660011223344556677889900aabbccddeeff001122"),
        QByteArray::fromHex(
            "112233445566778899aabbccddeeff00112233445566778899aabbccddeeff11"));
}

const QByteArray kX = QByteArray::fromHex(
    "3f4e2a1b6c7d8e9f0a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f6071");
const QByteArray kClientShare = QByteArray::fromBase64(
    "BOc2/2XFEtpLIE5dyhXACT5AlMH60h0UpfRHWh7JFhs/5dqCHGwwLQRADdOpKsi3Gvfz46mvwkvQ2yQEUZuumzg=");
const QByteArray kBeaconShare = QByteArray::fromBase64(
    "BJaiznnzPyov715ehyGpGKRkhQcpK1rnJYVGOSTCiP3QJlsm8aC1eBwg1ApTCF7F963YZOmvw/TI+elyzWxhfaQ=");
const QByteArray kBeaconConfirmation = QByteArray::fromBase64(
    "8otjQyWycp3XAmvNjy0uCfoHuHXJ82PNJJX6v9wDEkA=");
const QByteArray kClientConfirmation = QByteArray::fromBase64(
    "W8X+XoTxl8pa/zzlH8f/mAs2mNkrCOVN40W9iH6QC0Q=");
}

const QByteArray kCertificateDer = QByteArray::fromBase64(
    "MIIBkjCCATmgAwIBAgIUewofa0r/WsTSuwgXdWUX6DsvarMwCgYIKoZIzj0EAwIw"
    "HzEdMBsGA1UEAwwUam9jaG9uYS1zbW9rZS1jbGllbnQwHhcNMjYwODI4MjAzMjQw"
    "WhcNMjYwODI5MjAzMjQwWjAfMR0wGwYDVQQDDBRqb2Nob25hLXNtb2tlLWNsaWVu"
    "dDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABM/aBXKNJ1hOgx16glKqfPhrZWKP"
    "UO8XwfFbKd/o9xTadsz0MVjw/CKHTuHPNRvHC8uyuZXpdURnY00QA5zYyHSjUzBR"
    "MB0GA1UdDgQWBBS4yflUm23e+oLD3z1z6f1rUd0bSjAfBgNVHSMEGDAWgBS4yflU"
    "m23e+oLD3z1z6f1rUd0bSjAPBgNVHRMBAf8EBTADAQH/MAoGCCqGSM49BAMCA0cA"
    "MEQCIHW9oGRrTjMPhJfcRaugHlGSg16pNjNzWT/pvGeP4k8aAiBEG3Ynx0ylfQmZ"
    "57El/Ird+tn1jRlortMEwU6vtRp9tA==");
const QByteArray kCertificateSpkiSha256 = QByteArray::fromHex(
    "6a53c1fafaf4afc33c5917075c5d29b21cd427f2649cbc7ecfa0b75a63946be9");

void TestBeaconSpake2::hashesCompleteSubjectPublicKeyInfoDer()
{
    QCOMPARE(
        BeaconSpake2Client::certificateSpkiSha256(kCertificateDer),
        kCertificateSpkiSha256);
}

void TestBeaconSpake2::matchesLockedProtocolVector()
{
    BeaconSpake2Client client = vectorClient();
    QByteArray clientShare;
    QString error;
    QVERIFY2(client.begin(&clientShare, &error, kX), qPrintable(error));
    QCOMPARE(clientShare, kClientShare);

    QByteArray clientConfirmation;
    QVERIFY2(client.finish(kBeaconShare, kBeaconConfirmation,
                           &clientConfirmation, &error),
             qPrintable(error));
    QCOMPARE(clientConfirmation, kClientConfirmation);
}

void TestBeaconSpake2::rejectsWrongBeaconConfirmation()
{
    BeaconSpake2Client client = vectorClient();
    QByteArray clientShare;
    QString error;
    QVERIFY(client.begin(&clientShare, &error, kX));

    QByteArray wrong(32, '\0');
    QByteArray clientConfirmation;
    QVERIFY(!client.finish(kBeaconShare, wrong,
                           &clientConfirmation, &error));
    QVERIFY(!error.isEmpty());
}
