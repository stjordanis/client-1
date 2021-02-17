/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "socketuploadjob.h"
#include "socketapi_p.h"

#include "common/syncjournaldb.h"
#include "syncengine.h"

#include <QRegularExpression>

using namespace OCC;

SocketUploadJob::SocketUploadJob(OCC::SocketListener *listener, OCC::AccountPtr acc, const QString &localDir, const QString &pattern, const QString &remoteDir, QObject *parent)
    : QObject(parent)
    , _listener(listener)
    , _localPath(localDir)
    , _remotePath(remoteDir)
    , _pattern(pattern)
{
    OC_ENFORCE(tmp.open());

    db = new SyncJournalDb(tmp.fileName(), this);
    engine = new SyncEngine(acc, localDir, remoteDir, db);
    engine->setParent(db);

    connect(engine, &OCC::SyncEngine::itemCompleted, this, [this](const OCC::SyncFileItemPtr item) {
        syncedFiles.append(item->_file);
    });

    connect(engine, &OCC::SyncEngine::finished, this, [this](bool ok) {
        if (ok) {
            finish({});
        }
    });
    connect(engine, &OCC::SyncEngine::syncError, this, &SocketUploadJob::finish);
}

void SocketUploadJob::start()
{
    auto opt = engine->syncOptions();
    opt.setFilePattern(_pattern);
    if (!opt.fileRegex()->isValid()) {
        finish(opt.fileRegex()->errorString());
        return;
    }
    engine->setSyncOptions(opt);

    // create the dir, don't fail if it already exists
    auto mkdir = new OCC::MkColJob(engine->account(), _remotePath);
    connect(mkdir, &OCC::MkColJob::finishedWithoutError, engine, &OCC::SyncEngine::startSync);
    connect(mkdir, &OCC::MkColJob::finishedWithError, this, [this](QNetworkReply *reply) {
        if (reply->error() == 202) {
            // folder does exist, we could also assert here if we want to enforce a backup over a sync
            engine->startSync();
        } else {
            finish(reply->errorString());
        }
    });
    mkdir->start();
}

void SocketUploadJob::finish(const QString &error)
{
    if (!_finished) {
        _finished = true;
        _listener->sendMessage(QStringLiteral("UPLOAD_FILE_RESULT:") + _localPath + SoketApiHelper::RecordSeparator() + (error.isEmpty() ? QStringLiteral("ok") : error.toUtf8().toPercentEncoding()) + SoketApiHelper::RecordSeparator() + syncedFiles.join(QLatin1Char(';')));
        deleteLater();
    }
}
