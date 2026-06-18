#include "file_processor.h"
#include "worker.h"
#include "file_cache.h"
#include "file_utils.h"
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QMetaObject>

FileProcessor::FileProcessor(const QString &inputPath,
                             const QString &outputDir,
                             IBinaryOperation *operation,
                             ConflictMode conflictMode,
                             const std::atomic<bool> &isPaused,
                             const std::atomic<bool> &isStopped,
                             FileCache *cache,
                             Worker *worker)
    : m_inputPath(inputPath), m_outputDir(outputDir), m_operation(operation), m_conflictMode(conflictMode), m_isPaused(isPaused), m_isStopped(isStopped), m_cache(cache), m_worker(worker)
{
  setAutoDelete(true);
}

void FileProcessor::run()
{
  QFileInfo fileInfo(m_inputPath);
  QFile inputFile(m_inputPath);

  if (!inputFile.open(QIODevice::ReadOnly))
  {
    QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                              Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Не удалось открыть файл")));
    return;
  }

  if (m_isStopped)
  {
    inputFile.close();
    QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                              Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Остановлено пользователем")));
    return;
  }

  QByteArray fileHash = FileCache::computeFileHash(m_inputPath);

  if (m_conflictMode != ConflictMode::Numbering && m_cache != nullptr)
  {
    QString cachedOutputPath;
    if (m_cache->isProcessed(fileHash, m_operation->key(), cachedOutputPath))
    {
      inputFile.close();
      QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                                Q_ARG(FileResult, FileResult::skipped(fileInfo.fileName())));
      return;
    }
  }

  QString finalPath = FileUtils::getProcessedPath(m_outputDir, fileInfo.fileName());

  switch (m_conflictMode)
  {
  case ConflictMode::Numbering:
    finalPath = FileUtils::getUniquePath(finalPath);
    break;
  case ConflictMode::Skip:
    if (QFile::exists(finalPath))
    {
      inputFile.close();
      QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                                Q_ARG(FileResult, FileResult::skipped(fileInfo.fileName())));
      return;
    }
    break;
  case ConflictMode::Overwrite:
    break;
  }

  QFile outputFile(finalPath);
  if (!outputFile.open(QIODevice::WriteOnly))
  {
    inputFile.close();
    QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                              Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Не удалось создать выходной файл")));
    return;
  }

  const qint64 CHUNK_SIZE = 4 * 1024 * 1024;
  QByteArray buffer(CHUNK_SIZE, Qt::Uninitialized);

  qint64 fileSize = fileInfo.size();
  qint64 fileProcessed = 0;

  while (!inputFile.atEnd())
  {
    if (m_isStopped)
    {
      inputFile.close();
      outputFile.close();
      outputFile.remove();
      QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                                Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Остановлено пользователем")));
      return;
    }

    while (m_isPaused && !m_isStopped)
    {
      QThread::msleep(100);
    }

    if (!inputFile.exists())
    {
      inputFile.close();
      outputFile.close();
      outputFile.remove();
      QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                                Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Входной файл удалён")));
      return;
    }

    qint64 bytesRead = inputFile.read(buffer.data(), CHUNK_SIZE);
    if (bytesRead <= 0)
      break;

    m_operation->apply(buffer, fileProcessed);

    qint64 bytesWritten = 0;
    while (bytesWritten < bytesRead)
    {
      if (m_isStopped)
      {
        inputFile.close();
        outputFile.close();
        outputFile.remove();
        QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                                  Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Остановлено пользователем")));
        return;
      }

      qint64 result = outputFile.write(buffer.constData() + bytesWritten, bytesRead - bytesWritten);
      if (result <= 0)
      {
        inputFile.close();
        outputFile.close();
        outputFile.remove();
        QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                                  Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Ошибка записи")));
        return;
      }
      bytesWritten += result;
    }

    fileProcessed += bytesRead;

    QMetaObject::invokeMethod(m_worker, "onFileProgress", Qt::QueuedConnection,
                              Q_ARG(QString, fileInfo.fileName()),
                              Q_ARG(qint64, fileProcessed),
                              Q_ARG(qint64, fileSize));
  }

  inputFile.close();
  outputFile.flush();
  outputFile.close();

  QFileInfo outputInfo(finalPath);
  if (fileProcessed < fileSize || outputInfo.size() != fileSize)
  {
    outputFile.remove();
    QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                              Q_ARG(FileResult, FileResult::error(fileInfo.fileName(), "Неполная запись")));
    return;
  }

  if (m_conflictMode != ConflictMode::Numbering && !fileHash.isEmpty())
  {
    m_cache->markProcessed(fileHash, m_operation->key(), finalPath);
  }

  QMetaObject::invokeMethod(m_worker, "onFileResult", Qt::QueuedConnection,
                            Q_ARG(FileResult, FileResult::success(fileInfo.fileName(), finalPath, fileHash)));
}