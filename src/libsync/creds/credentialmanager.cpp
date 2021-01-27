#include "credentialmanager.h"

#include "account.h"
#include "theme.h"

#include "common/asserts.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSettings>
#include <QTimer>

using namespace OCC;

Q_LOGGING_CATEGORY(lcCredentaislManager, "sync.credentials.manager", QtDebugMsg)

namespace {
QString credentialKeyC()
{
    return QStringLiteral("%1_credentials").arg(Theme::instance()->appName());
}

QString accoutnKey(const Account *acc)
{
    OC_ASSERT(!acc->url().isEmpty());
    return QStringLiteral("%1:%2:%3").arg(credentialKeyC(), acc->url().host(), acc->uuid().toString(QUuid::WithoutBraces));
}


QString scope(const CredentialManager *const manager)
{
    return manager->account() ? accoutnKey(manager->account()) : credentialKeyC();
}

QString scopedKey(const CredentialManager *const manager, const QString &key)
{
    return scope(manager) + QLatin1Char(':') + key;
}
}

CredentialManager::CredentialManager(Account *acc)
    : QObject(acc)
    , _account(acc)
{
}

CredentialManager::CredentialManager(QObject *parent)
    : QObject(parent)
{
}


CredentialJob *CredentialManager::get(const QString &key)
{
    qCInfo(lcCredentaislManager) << "get" << scopedKey(this, key);
    auto out = new CredentialJob(this, key);
    QTimer::singleShot(0, out, &CredentialJob::start);
    return out;
}

QKeychain::Job *CredentialManager::set(const QString &key, const QVariant &data)
{
    qCInfo(lcCredentaislManager) << "set" << scopedKey(this, key);
    auto writeJob = new QKeychain::WritePasswordJob(Theme::instance()->appName());
    writeJob->setKey(scopedKey(this, key));
    connect(writeJob, &QKeychain::WritePasswordJob::finished, this, [writeJob, key, this] {
        if (writeJob->error() == QKeychain::NoError) {
            qCInfo(lcCredentaislManager) << "added" << scopedKey(this, key);
            QSettings settings(scope(this));
            settings.setValue(key, true);
        } else {
            qCWarning(lcCredentaislManager) << "Failed to set:" << scopedKey(this, key) << writeJob->errorString();
        }
    });
    QJsonObject obj;
    if (data.canConvert(QVariant::Map)) {
        obj = QJsonObject::fromVariantMap(data.toMap());
    } else {
        obj.insert(QStringLiteral("d"), QJsonValue::fromVariant(data));
    }
    //    qCDebug(lcCredentaislManager) << "wrote" << QJsonDocument(obj).toJson();
    writeJob->setBinaryData(QJsonDocument(obj).toBinaryData());
    // start is delayed so we can directly call it
    writeJob->start();
    return writeJob;
}

QKeychain::Job *CredentialManager::remove(const QString &key)
{
    OC_ASSERT(contains(key));
    qCInfo(lcCredentaislManager) << "del" << scopedKey(this, key);
    auto keychainJob = new QKeychain::DeletePasswordJob(Theme::instance()->appName());
    keychainJob->setKey(scopedKey(this, key));
    connect(keychainJob, &QKeychain::DeletePasswordJob::finished, this, [keychainJob, key, this] {
        OC_ASSERT(keychainJob->error() != QKeychain::EntryNotFound);
        if (keychainJob->error() == QKeychain::NoError) {
            qCInfo(lcCredentaislManager) << "removed" << scopedKey(this, key);
            QSettings settings(scope(this));
            settings.remove(key);
        } else {
            qCWarning(lcCredentaislManager) << "Failed to remove:" << scopedKey(this, key) << keychainJob->errorString();
        }
    });
    // start is delayed so we can directly call it
    keychainJob->start();
    return keychainJob;
}

QVector<QPointer<QKeychain::Job>> CredentialManager::clear()
{
    OC_ENFORCE(_account);
    const auto keys = knownKeys();
    QVector<QPointer<QKeychain::Job>> out;
    out.reserve(keys.size());
    for (const auto &key : keys) {
        out << remove(key);
    }
    return out;
}

const Account *CredentialManager::account() const
{
    return _account;
}

bool CredentialManager::contains(const QString &key) const
{
    QSettings settings(scope(this));
    return settings.contains(key);
}

QStringList CredentialManager::knownKeys() const
{
    return QSettings(scope(this)).allKeys();
}

CredentialJob::CredentialJob(CredentialManager *parent, const QString &key)
    : QObject(parent)
    , _key(key)
    , _parent(parent)
{
    connect(this, &CredentialJob::finished, this, &CredentialJob::deleteLater);
}

QString CredentialJob::errorString() const
{
    return _errorString;
}

const QVariant &CredentialJob::data() const
{
    return _data;
}

QKeychain::Error CredentialJob::error() const
{
    return _error;
}

void CredentialJob::start()
{
    if (!_parent->contains(_key)) {
        _error = QKeychain::EntryNotFound;
        Q_EMIT finished();
        return;
    }

    auto keychainJob = new QKeychain::ReadPasswordJob(Theme::instance()->appName());
    keychainJob->setKey(scopedKey(_parent, _key));
    connect(keychainJob, &QKeychain::ReadPasswordJob::finished, this, [this, keychainJob] {
        OC_ASSERT(keychainJob->error() != QKeychain::EntryNotFound);
        if (keychainJob->error() == QKeychain::NoError) {
            const auto doc = QJsonDocument::fromBinaryData(keychainJob->binaryData());
            if (doc.isNull()) {
                _error = QKeychain::OtherError;
                _errorString = tr("Failed to parse credentials");
                return;
            }
            const auto obj = doc.object();
            //            qCDebug(lcCredentaislManager) << "read" << keychainJob->key() << QJsonDocument(obj).toJson();
            if (obj.count() == 1) {
                _data = obj.value(QLatin1String("d"));
            } else {
                _data = obj.toVariantMap();
            }
            OC_ASSERT(_data.isValid());
        } else {
            qCWarning(lcCredentaislManager) << "Failed to read client id" << keychainJob->errorString();
            _error = keychainJob->error();
            _errorString = keychainJob->errorString();
        }
        Q_EMIT finished();
    });
    keychainJob->start();
}

QString CredentialJob::key() const
{
    return _key;
}
