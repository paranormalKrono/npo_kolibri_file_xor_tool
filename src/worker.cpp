#include "worker.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>

Worker::Worker(QObject *parent)
    : QObject(parent)
{
}

void Worker::setConfig(const QString &inputDir,
                       const QString &outputDir,
                       const QString &fileMask,
                       const QByteArray &xorKey,
                       bool deleteInput,
                       bool overwriteMode)
{
  m_inputDir = inputDir;
  m_outputDir = outputDir;
  m_fileMask = fileMask;
  m_xorKey = xorKey;
  m_deleteInput = deleteInput;
  m_overwriteMode = overwriteMode;
}

QString Worker::getUniquePath(const QString &basePath)
{
  return FileUtils::getUniquePath(basePath);
}

void Worker::processFiles()
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
  QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

  if (fileList.isEmpty())
  {
    emit statusMessage("Файлы не найдены по маске: " + m_fileMask);
    emit finished();
    return;
  }

  emit statusMessage("Найдено файлов: " + QString::number(fileList.size()));

  const qint64 CHUNK_SIZE = 4 * 1024 * 1024; // 4 MB
  qint64 totalProcessed = 0;
  qint64 totalSize = 0;

  // Подсчет общего размера
  for (const QFileInfo &fi : fileList)
  {
    totalSize += fi.size();
  }

  for (const QFileInfo &fileInfo : fileList)
  {
    if (m_isStopped)
    {
      break;
    }

    QFile inputFile(fileInfo.absoluteFilePath());
    if (!inputFile.open(QIODevice::ReadOnly))
    {
      emit statusMessage("Ошибка открытия: " + fileInfo.fileName());
      continue;
    }

    QString outPath = m_outputDir + "/" + fileInfo.fileName();
    if (!m_overwriteMode)
    {
      outPath = getUniquePath(outPath);
    }

    QFile outputFile(outPath);
    if (!outputFile.open(QIODevice::WriteOnly))
    {
      emit statusMessage("Ошибка создания: " + outPath);
      inputFile.close();
      continue;
    }

    emit fileProgressChanged(fileInfo.fileName(), 0, fileInfo.size());

    qint64 fileSize = fileInfo.size();
    qint64 fileProcessed = 0;
    QByteArray buffer(CHUNK_SIZE, Qt::Uninitialized);

    while (!inputFile.atEnd() && !m_isStopped)
    {
      // Проверка паузы
      while (m_isPaused && !m_isStopped)
      {
        QThread::msleep(100);
      }
      if (m_isStopped)
      {
        break;
      }

      qint64 bytesRead = inputFile.read(buffer.data(), CHUNK_SIZE);
      if (bytesRead <= 0)
      {
        break;
      }

      // XOR операция
      for (qint64 i = 0; i < bytesRead; ++i)
      {
        buffer[i] = buffer[i] ^ m_xorKey[i % 8];
      }

      outputFile.write(buffer.data(), bytesRead);
      fileProcessed += bytesRead;
      totalProcessed += bytesRead;

      emit fileProgressChanged(fileInfo.fileName(), fileProcessed, fileSize);
      emit progressChanged(totalProcessed, totalSize);
    }

    inputFile.close();
    outputFile.close();

    if (m_deleteInput && !m_isStopped)
    {
      inputFile.remove();
    }

    emit statusMessage("Обработан: " + fileInfo.fileName());
  }

  if (m_isStopped)
  {
    emit statusMessage("Обработка остановлена");
  }
  else
  {
    emit statusMessage("Обработка завершена");
  }

  emit finished();
}

void Worker::pause()
{
  m_isPaused = true;
}

void Worker::resume()
{
  m_isPaused = false;
}

void Worker::stop()
{
  m_isStopped = true;
  m_isPaused = false;
}