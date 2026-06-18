#ifndef FILE_RESULT_H
#define FILE_RESULT_H

#include <QString>
#include <QByteArray>
#include <QMetaType>

enum class ConflictMode
{
  Overwrite,
  Skip,
  Numbering
};

struct FileResult
{
  enum class Status
  {
    Success,
    Skipped,
    Error
  };

  QString fileName;
  Status status;
  QString errorMessage;
  QString outputPath;
  QByteArray fileHash;

  static FileResult success(const QString &name, const QString &path, const QByteArray &hash)
  {
    return {name, Status::Success, "", path, hash};
  }

  static FileResult skipped(const QString &name)
  {
    return {name, Status::Skipped, "", "", QByteArray()};
  }

  static FileResult error(const QString &name, const QString &error)
  {
    return {name, Status::Error, error, "", QByteArray()};
  }
};

Q_DECLARE_METATYPE(FileResult)

#endif // FILE_RESULT_H