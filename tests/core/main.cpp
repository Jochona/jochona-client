#include <QCoreApplication>
#include <QtTest>

#include "tst_credentialstore.h"
#include "tst_settingsdatabase.h"

// A single test binary drives every core/ test class so `core.pro` produces
// one executable to build and run, following the QTEST_MAIN-per-class
// convention used by the vendored qmdnsengine tests but aggregated here
// since CredentialStore and SettingsDatabase share no state.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    int status = 0;

    TestSettingsDatabase settingsDatabaseTest;
    status |= QTest::qExec(&settingsDatabaseTest, argc, argv);

    TestCredentialStore credentialStoreTest;
    status |= QTest::qExec(&credentialStoreTest, argc, argv);

    return status;
}
