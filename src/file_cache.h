#ifndef FILE_CACHE_H
#define FILE_CACHE_H

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QMutex>

class FileCache
{
public:
  explicit FileCache(const QString &outputDir);

  // Проверяет, обработан ли файл с данным хешем и ключом операции
  bool isProcessed(const QByteArray &fileHash, const QByteArray &operationKey, QString &outputPath) const;

  // Добавляет запись в кеш
  void markProcessed(const QByteArray &fileHash, const QByteArray &operationKey, const QString &outputPath);

  // Вычисляет SHA-256 хеш файла
  static QByteArray computeFileHash(const QString &filePath);

  QString cacheFilePath() const { return m_cacheFilePath; }

private:
  void load();
  void save();
  QString makeCacheKey(const QByteArray &fileHash, const QByteArray &operationKey) const;

  QString m_outputDir;
  QString m_cacheFilePath;
  QJsonObject m_cacheData;
  mutable QMutex m_mutex;
};

#endif // FILE_CACHE_H