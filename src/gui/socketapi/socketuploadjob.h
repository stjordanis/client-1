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

#pragma once
#include <QObject>
#include <QTemporaryFile>

#include "socketapi.h"
#include "account.h"

namespace OCC {

class SyncJournalDb;
class SyncEngine;

class SocketUploadJob : public QObject
{
    Q_OBJECT
public:
    SocketUploadJob(OCC::SocketListener *listener, AccountPtr acc, const QString &localDir, const QString &pattern, const QString &remoteDir, QObject *parent = nullptr);

    void start();

    void finish(const QString &error);

    SocketListener *_listener;
    QString _localPath;
    QString _remotePath;
    QString _pattern;
    QTemporaryFile tmp;
    SyncJournalDb *db;
    SyncEngine *engine;
    QStringList syncedFiles;

    bool _finished = false;
};
}
