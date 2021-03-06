/*******************************************************************
 This file is part of Synkron
 Copyright (C) 2005-2011 Matus Tomlein (matus.tomlein@gmail.com)

 Synkron is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public Licence
 as published by the Free Software Foundation; either version 2
 of the Licence, or (at your option) any later version.

 Synkron is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public Licence for more details.

 You should have received a copy of the GNU General Public Licence
 along with Synkron; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
********************************************************************/

#include "module.h"
#include "settings.h"
#include "syncpage.h"
#include "exceptions.h"
#include "backuphandler.h"

#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

Module::Module(const QString & path)
{
    settings_map = new QMap<QString, QVariant>;
    syncs = new QMap<int, AbstractSyncPage *>;
    exceptions = new Exceptions;
    backup_handler = new BackupHandler;

    setupSyncSettings(path);
    load();
}

Module::~Module()
{
    foreach (AbstractSyncPage * page, *syncs) {
        delete page;
    }

    delete backup_handler;
    delete exceptions;
    delete syncs;
    delete settings_map;
}

/**
  * initializes sync_settings
  * if a Synkron.ini file exists in the apps directory, it is going to be used
  * otherwise settings are stored in the users dir
  */
void Module::setupSyncSettings(const QString & path)
{
    QFileInfo file_info (path);
    QDir dir (file_info.dir());

#ifdef PORTABLE_APP
    dir.cdUp(); dir.cdUp();
    dir.cd("Data"); dir.cd("settings");
#endif

    if (dir.exists("Synkron 2.ini")) {
        sync_settings = new Settings(dir.absoluteFilePath("Synkron 2.ini"), QSettings::IniFormat);
    } else {
        sync_settings = new Settings(QSettings::IniFormat, QSettings::UserScope, "Matus Tomlein", "Synkron 2");
    }
}

/**
  * Returns value retrieved from the settings map for the given key
  */
QVariant Module::value(const QString & key)
{
    return settings_map->value(key);
}

/**
  * Inserts a new value into the settings map
  */
void Module::setValue(const QString & key, const QVariant & value)
{
    settings_map->insert(key, value);
}

/**
  * Finds the next id for a new SyncPage, returns the new SyncPage
  */
SyncPage * Module::addSync()
{
    bool higher = true;
    int n = 0;

    QMapIterator<int, AbstractSyncPage *> s(*syncs);

    while (higher) {
        n++;
        higher = false;
        while (s.hasNext()) {
            s.next();
            if (s.key() >= n) {
                n = s.key();
                higher = true;
                break;
            }
        }
        s.toFront();
    }

    return addSync(n);
}

/**
  * Creates a new SyncPage and inserts it into the QList of syncs
  */
SyncPage * Module::addSync(int sync_id)
{
    SyncPage * sync = new SyncPage(sync_id, exceptions, backup_handler);
    syncs->insert(sync_id, sync);

    QObject::connect(exceptions, SIGNAL(bundleAddedSig(ExceptionBundle*)), sync, SLOT(addExceptionBundle(ExceptionBundle*)));
    QObject::connect(exceptions, SIGNAL(bundleChangedSig(ExceptionBundle*)), sync, SIGNAL(exceptionBundleChanged(ExceptionBundle*)));
    QObject::connect(exceptions, SIGNAL(bundleRemovedSig(int)), sync, SLOT(removeExceptionBundle(int)));
    return sync;
}

/**
  * Deletes the AbstractSyncPage with the given id
  */
void Module::closeSync(int sync_id)
{
    delete syncs->take(sync_id);
}

/**
  * Adds a new folder to the AbstractSyncPage.
  */
Folder * Module::addSyncFolder(int sync_id, int folder_id)
{
    return syncs->value(sync_id)->addFolder(folder_id);
}

/**
  * Deletes the specific folder.
  */
void Module::closeSyncFolder(int sync_id, int folder_id)
{
    syncs->value(sync_id)->closeFolder(folder_id);
}

QMapIterator<int, AbstractSyncPage *> * Module::syncIterator()
{
    QMapIterator<int, AbstractSyncPage *> * s = new QMapIterator<int, AbstractSyncPage *>(*syncs);
    return s;
}

/**
  * Saves all settings to disk
  */
void Module::save()
{
    // Saving general settings
    sync_settings->beginGroup("general");
    QMapIterator<QString, QVariant> i(*settings_map);
    while (i.hasNext()) {
        i.next();
        sync_settings->setValue(i.key(), i.value());
    }
    sync_settings->endGroup();

    // Saving sync tabs
    sync_settings->beginGroup("syncs");
    sync_settings->remove("");
    QMapIterator<int, AbstractSyncPage *> s(*syncs);
    while (s.hasNext()) {
        s.next();
        s.value()->save(sync_settings);
    }
    sync_settings->endGroup();

    // Saving exceptions
    exceptions->save(sync_settings);

    // Saving restore settings
    backup_handler->save(sync_settings);
}

/**
  * Loads all settings - usually done on startup
  */
void Module::load()
{
    // Loading general settings
    sync_settings->beginGroup("general");
    QStringList general_keys = sync_settings->allKeys();
    for (int i = 0; i < general_keys.count(); ++i) {
        setValue(general_keys.at(i), sync_settings->value(general_keys.at(i)));
    }
    sync_settings->endGroup();

    // Loading exceptions
    exceptions->load(sync_settings);

    // Loading restore settings
    backup_handler->load(sync_settings);

    // Loading sync tabs
    sync_settings->beginGroup("syncs");
    QStringList sync_groups = sync_settings->childGroups();
    for (int i = 0; i < sync_groups.count(); ++i) {
        int s_id = sync_groups.at(i).toInt();

        SyncPage * page = addSync(s_id);
        page->load(sync_settings);
    }
    sync_settings->endGroup();
}
