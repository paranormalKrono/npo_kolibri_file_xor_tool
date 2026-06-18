#include "file_cache.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDateTime>

FileCache::FileCache(const QString &outputDir)
    : m_outputDir(outputDir), m_cacheFilePath(outputDir + "/.xor_tool_cache.json")
{
  load();
}

bool FileCache::isProcessed(const QByteArray &fileHash, const QByteArray &operationKey, QString &outputPath) const
{
  QMutexLocker locker(&m_mutex);
  QString key = makeCacheKey(fileHash, operationKey);

  if (m_cacheData.contains(key))
  {
    QJsonObject entry = m_cacheData[key].toObject();
    outputPath = entry["output_path"].toString();

    // Проверяем, что выходной файл ещё существует
    if (QFile::exists(outputPath))
    {
      return true;
    }
  }

  return false;
}

void FileCache::markProcessed(const QByteArray &fileHash, const QByteArray &operationKey, const QString &outputPath)
{
  QMutexLocker locker(&m_mutex);
  QString key = makeCacheKey(fileHash, operationKey);

  QJsonObject entry;
  entry["output_path"] = outputPath;
  entry["processed_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  m_cacheData[key] = entry;
  save();
}

QByteArray FileCache::computeFileHash(const QString &filePath)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
  {
    return QByteArray();
  }

  QCryptographicHash hash(QCryptographicHash::Sha256);

  const qint64 CHUNK_SIZE = 4 * 1024 * 1024;
  QByteArray buffer(CHUNK_SIZE, Qt::Uninitialized);

  while (!file.atEnd())
  {
    qint64 bytesRead = file.read(buffer.data(), CHUNK_SIZE);
    if (bytesRead > 0)
    {
      hash.addData(QByteArrayView(buffer.constData(), bytesRead));
    }
  }

  file.close();
  return hash.result();
}

void FileCache::load()
{
  QFile file(m_cacheFilePath);
  if (file.open(QIODevice::ReadOnly))
  {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject())
    {
      m_cacheData = doc.object();
    }
    file.close();
  }
}

void FileCache::save()
{
  QFile file(m_cacheFilePath);
  if (!file.open(QIODevice::WriteOnly))
  {
    qWarning() << "Failed to save cache:" << m_cacheFilePath;
    return;
  }

  QJsonDocument doc(m_cacheData);
  file.write(doc.toJson());
  file.close();
}

QString FileCache::makeCacheKey(const QByteArray &fileHash, const QByteArray &operationKey) const
{
  return QString::fromUtf8(fileHash.toHex()) + "_" + QString::fromUtf8(operationKey.toHex());
}