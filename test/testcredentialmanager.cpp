/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#include "account.h"
#include "libsync/creds/credentialmanager.h"

#include "syncenginetestutils.h"

#include <QTest>

namespace OCC {

class TestCredentialManager : public QObject
{
    Q_OBJECT

    bool _finished = false;

private Q_SLOTS:
    void init()
    {
        _finished = false;
    }

    void testSetGet_data()
    {
        QTest::addColumn<QVariant>("data");

        QTest::newRow("bool") << QVariant::fromValue(true);
        QTest::newRow("int") << QVariant::fromValue(1);
        QTest::newRow("map") << QVariant::fromValue(QVariantMap { { "foo", QColor(Qt::red) }, { "bar", "42" } });
    }

    void testSetGet()
    {
        QFETCH(QVariant, data);
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        auto creds = fakeFolder.account()->credentialManager();

        const QString key = QStringLiteral("test");
        auto job = creds->set(key, data);

        connect(job, &QKeychain::Job::finished, this, [creds, data, key, this] {
            auto job = creds->get(key);
            connect(job, &CredentialJob::finished, this, [job, data, creds, this] {
                QCOMPARE(job->data(), data);
                auto jobs = creds->clear();
                connect(jobs[0], &QKeychain::Job::finished, this, [creds, data, this] {
                    QVERIFY(creds->knownKeys().isEmpty());
                    _finished = true;
                });
            });
        });
        QTest::qWaitFor([this] { return _finished; });
    }
};
}

QTEST_GUILESS_MAIN(OCC::TestCredentialManager)
#include "testcredentialmanager.moc"
