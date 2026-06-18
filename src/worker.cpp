#include "worker.h"
#include "file_processor.h"
#include "file_cache.h"
#include "operations/ibinary_operation.h"
#include "operations/operation_factory.h"
#include <QDir>
#include <QFileInfo>

Worker::Worker(QObject *parent)
    : QObject(parent), m_threadPool(new QThreadPool(this))
{
}

Worker::~Worker()
{
  // Сигнализируем всем задачам о завершении
  m_isStopped = true;

  // Убираем задачи из очереди (которые ещё не начались)
  m_threadPool->clear();

  // Ждём завершения текущих задач
  // Они проверят m_isStopped и быстро завершатся
  m_threadPool->waitForDone();
}

void Worker::setConfig(const QString &inputDir,
                       const QString &outputDir,
                       const QString &fileMask,
                       const QString &operationName,
                       const QByteArray &operationParams,
                       bool deleteInput,
                       ConflictMode conflictMode,
                       int threadCount)
{
  m_inputDir = inputDir;
  m_outputDir = outputDir;
  m_fileMask = fileMask;

  m_operation = OperationFactory::createByName(operationName, operationParams);
  if (!m_operation)
  {
    qCritical() << "Failed to create operation:" << operationName;
  }

  m_deleteInput = deleteInput;
  m_conflictMode = conflictMode;
  m_threadCount = threadCount;
  m_threadPool->setMaxThreadCount(threadCount);
}

void Worker::setFilesToProcess(const QStringList &files)
{
  m_filesToProcess = files;
}

void Worker::processFiles()
{
  qDebug() << "=== Worker::processFiles() started ===";

  if (!m_operation)
  {
    emit statusMessage("Ошибка: операция не создана");
    emit finished();
    return;
  }

  m_state = WorkerState::Running;
  m_isPaused = false;
  m_isStopped = false;
  m_totalProcessed.store(0);

  {
    QMutexLocker locker(&m_progressMutex);
    m_fileProgress.clear();
    m_finishedEmitted = false;
  }

  QFileInfoList fileList;
  if (!m_filesToProcess.isEmpty())
  {
    for (const QString &path : m_filesToProcess)
    {
      QFileInfo fi(path);
      if (m_cache && fi.absoluteFilePath() == m_cache->cacheFilePath())
      {
        continue;
      }
      fileList << fi;
    }
    m_filesToProcess.clear();
  }
  else
  {
    QDir dir(m_inputDir);
    if (!dir.exists())
    {
      emit statusMessage("Директория не найдена: " + m_inputDir);
      emit finished();
      return;
    }

    QStringList filters;
    filters << m_fileMask;
    QFileInfoList allFiles = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : allFiles)
    {
      if (m_cache && fi.absoluteFilePath() == m_cache->cacheFilePath())
      {
        continue;
      }
      fileList << fi;
    }
  }

  if (fileList.isEmpty())
  {
    emit statusMessage("Файлы не найдены");
    emit finished();
    return;
  }

  qDebug() << "Found" << fileList.size() << "files to process";

  m_totalSize = 0;
  for (const QFileInfo &fi : fileList)
  {
    m_totalSize += fi.size();
  }

  m_filesRemaining.store(fileList.size());
  emit totalFilesCount(fileList.size());

  m_cache = std::make_unique<FileCache>(m_outputDir);

  for (const QFileInfo &fileInfo : fileList)
  {
    if (m_isStopped)
      break;

    auto *processor = new FileProcessor(
        fileInfo.absoluteFilePath(),
        m_outputDir,
        m_operation.get(),
        m_conflictMode,
        m_isPaused,
        m_isStopped,
        m_cache.get(),
        this);

    emit fileStarted(fileInfo.fileName(), fileInfo.size());
    m_threadPool->start(processor);
  }

  qDebug() << "=== Worker::processFiles() completed ===";
}

void Worker::pause()
{
  m_isPaused = true;
  m_state = WorkerState::Paused;
  emit statusMessage("Пауза");
}

void Worker::resume()
{
  m_isPaused = false;
  m_state = WorkerState::Running;
  emit statusMessage("Возобновление");
}

void Worker::stop()
{
  m_isStopped = true;
  m_isPaused = false;
  m_state = WorkerState::Stopping;

  m_threadPool->clear();
  m_threadPool->waitForDone(5000);

  m_filesRemaining.store(0);
  emit statusMessage("Обработка остановлена");

  QMutexLocker locker(&m_counterMutex);
  if (!m_finishedEmitted)
  {
    m_finishedEmitted = true;
    emit finished();
  }
}

void Worker::onFileResult(const FileResult &result)
{
  qDebug() << "Worker::onFileResult():" << result.fileName
           << "status:" << static_cast<int>(result.status);

  switch (result.status)
  {
  case FileResult::Status::Success:
    emit statusMessage("Обработан: " + result.fileName);
    emit fileFinished(result.fileName, true);
    break;

  case FileResult::Status::Skipped:
    emit statusMessage("Пропущен: " + result.fileName);
    emit fileSkipped(result.fileName);
    break;

  case FileResult::Status::Error:
    emit statusMessage("Ошибка: " + result.fileName + " — " + result.errorMessage);
    emit fileFinished(result.fileName, false);
    break;
  }

  int remaining = m_filesRemaining.fetch_sub(1) - 1;

  qDebug() << "Remaining:" << remaining;

  if (remaining == 0)
  {
    QMutexLocker locker(&m_counterMutex);
    if (!m_finishedEmitted)
    {
      m_finishedEmitted = true;
      qDebug() << "Emitting finished()";
      emit statusMessage("Все файлы обработаны");
      emit finished();
    }
  }
}

void Worker::onFileProgress(const QString &fileName, qint64 processed, qint64 total)
{
  qint64 totalProcessed = 0;

  {
    QMutexLocker locker(&m_progressMutex);
    m_fileProgress[fileName] = processed;

    for (qint64 p : m_fileProgress.values())
    {
      totalProcessed += p;
    }
  }

  emit fileProgressChanged(fileName, processed, total);

  if (m_totalSize > 0)
  {
    emit progressChanged(totalProcessed, m_totalSize);
  }
}