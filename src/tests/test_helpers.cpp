#include "test_helpers.h"
#include "worker.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <random>

QByteArray TestHelpers::createTestFile(const QString &path, qint64 sizeMB)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
  {
    qCritical() << "Failed to create test file:" << path;
    return QByteArray();
  }

  qint64 totalBytes = sizeMB * 1024 * 1024;
  QByteArray data = generateRandomData(totalBytes);

  const qint64 WRITE_CHUNK = 4 * 1024 * 1024;
  qint64 written = 0;
  while (written < totalBytes)
  {
    qint64 toWrite = qMin(WRITE_CHUNK, totalBytes - written);
    qint64 result = file.write(data.constData() + written, toWrite);
    if (result != toWrite)
    {
      file.close();
      return QByteArray();
    }
    written += result;
  }

  file.close();
  return data;
}

QList<QByteArray> TestHelpers::createTestFiles(const QString &dir, int count, qint64 sizeMB)
{
  QList<QByteArray> dataList;
  for (int i = 0; i < count; ++i)
  {
    QString path = dir + QString("/file_%1.bin").arg(i);
    QByteArray data = createTestFile(path, sizeMB);
    if (!data.isEmpty())
    {
      dataList.append(data);
    }
  }
  return dataList;
}

QByteArray TestHelpers::generateRandomData(qint64 sizeBytes)
{
  QByteArray data(sizeBytes, Qt::Uninitialized);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, 255);

  for (qint64 i = 0; i < sizeBytes; ++i)
  {
    data[i] = static_cast<char>(distrib(gen));
  }

  return data;
}

void TestHelpers::setupWorker(Worker *worker,
                              const QString &inputDir,
                              const QString &outputDir,
                              const QString &fileMask,
                              const QByteArray &xorKey,
                              bool deleteInput,
                              ConflictMode conflictMode)
{
  worker->setConfig(
      inputDir,
      outputDir,
      fileMask,
      "XOR",
      xorKey,
      deleteInput,
      conflictMode,
      4);
}

bool TestHelpers::runAndWait(Worker *worker, int timeoutMs)
{
  QSignalSpy finishedSpy(worker, &Worker::finished);
  worker->processFiles();

  if (!finishedSpy.wait(timeoutMs))
  {
    return false;
  }

  // Обрабатываем queued-вызовы из QMetaObject::invokeMethod
  QCoreApplication::processEvents();

  return true;
}

bool TestHelpers::verifyFilesProcessed(const QString &outputDir, int expectedCount)
{
  QDir dir(outputDir);
  QStringList files = dir.entryList(QStringList() << "*.xor", QDir::Files);

  if (files.size() != expectedCount)
  {
    qDebug() << "Expected" << expectedCount << "files, found" << files.size();
    return false;
  }

  return true;
}

bool TestHelpers::verifyFileContent(const QString &filePath, const QByteArray &expectedContent)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
  {
    qDebug() << "Failed to open file:" << filePath;
    return false;
  }

  QByteArray content = file.readAll();
  file.close();

  if (content != expectedContent)
  {
    qDebug() << "Content mismatch for:" << filePath;
    return false;
  }

  return true;
}

bool TestHelpers::verifyFileNotModified(const QString &filePath, const QByteArray &originalContent)
{
  return verifyFileContent(filePath, originalContent);
}

bool TestHelpers::verifyFileExists(const QString &path)
{
  return QFile::exists(path);
}

bool TestHelpers::verifyFileNotExists(const QString &path)
{
  return !QFile::exists(path);
}

bool TestHelpers::createExistingFile(const QString &path, const QByteArray &content)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
  {
    return false;
  }
  file.write(content);
  file.close();
  return true;
}