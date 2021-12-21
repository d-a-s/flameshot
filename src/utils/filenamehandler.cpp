// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "filenamehandler.h"
#include "abstractlogger.h"
#include "src/utils/confighandler.h"
#include "src/utils/strfparse.h"
#include <QDir>
#include <ctime>
#include <exception>
#include <locale>

FileNameHandler::FileNameHandler(QObject* parent)
  : QObject(parent)
{
    auto err = AbstractLogger::error(AbstractLogger::Stderr);
    try {
        std::locale::global(std::locale(""));
    } catch (std::exception& e) {
        err << "Locales on your system are not properly configured. Falling "
               "back to defaults";

        std::locale::global(std::locale("en_US.UTF-8"));
    }
}

QString FileNameHandler::parsedPattern()
{
    return parseFilename(ConfigHandler().filenamePattern());
}

QString FileNameHandler::parseFilename(const QString& name)
{
    QString res = name;
    if (name.isEmpty()) {
        res = ConfigHandler().filenamePatternDefault();
    }

    // remove trailing characters '%' in the pattern
    while (res.endsWith('%')) {
        res.chop(1);
    }

    res =
      QString::fromStdString(strfparse::format_time_string(name.toStdString()));

    // add the parsed pattern in a correct format for the filesystem
    res = res.replace(QLatin1String("/"), QStringLiteral("⁄"))
            .replace(QLatin1String(":"), QLatin1String("-"));
    return res;
}

/**
 * @brief Generate a valid destination path from the possibly incomplete `path`.
 * The input `path` can be one of:
 * - empty string
 * - an existing directory
 * - a file in an existing directory
 * In each case, the output path will be an absolute path to a file with a
 * suffix matching the specified `format`.
 * @note
 * - If `path` points to a directory, the file name will be generated from the
 *   formatted file name from the user configuration
 * - If `path` points to a file, its suffix will be changed to match `format`
 * - If `format` is not given, the suffix will remain untouched, unless `path`
 *   has no suffix, in which case it will be given the "png" suffix
 * - If the path generated by the previous steps points to an existing file,
 *   "_NUM" will be appended to its base name, where NUM is the first
 * available number that produces a non-existent path (starting from 1).
 * @param path Possibly incomplete file name to transform
 * @param format Desired output file suffix (excluding an initial '.' character)
 */
QString FileNameHandler::properScreenshotPath(QString path,
                                              const QString& format)
{
    QFileInfo info(path);
    QString suffix = info.suffix();

    if (info.isDir()) {
        // path is a directory => generate filename from configured pattern
        path = QDir(QDir(path).absolutePath() + "/" + parsedPattern()).path();
    } else {
        // path points to a file => strip it of its suffix for now
        path = QDir(info.dir().absolutePath() + "/" + info.completeBaseName())
                 .path();
    }

    if (!format.isEmpty()) {
        // Override suffix to match format
        path += "." + format;
    } else if (!suffix.isEmpty()) {
        // Leave the suffix as it was
        path += "." + suffix;
    } else {
        path += ".png";
    }

    if (!QFileInfo::exists(path)) {
        return path;
    } else {
        return autoNumerateDuplicate(path);
    }
}

QString FileNameHandler::autoNumerateDuplicate(QString path)
{
    // add numeration in case of repeated filename in the directory
    // find unused name adding _n where n is a number
    QFileInfo checkFile(path);
    QString directory = checkFile.dir().absolutePath(),
            filename = checkFile.completeBaseName(),
            suffix = checkFile.suffix();
    if (!suffix.isEmpty()) {
        suffix = QStringLiteral(".") + suffix;
    }
    if (checkFile.exists()) {
        filename += QLatin1String("_");
        int i = 1;
        while (true) {
            checkFile.setFile(directory + "/" + filename + QString::number(i) +
                              suffix);
            if (!checkFile.exists()) {
                filename += QString::number(i);
                break;
            }
            ++i;
        }
    }
    return checkFile.filePath();
}
