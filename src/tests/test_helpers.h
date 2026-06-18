#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <QObject>
#include <QTemporaryDir>
#include <QByteArray>
#include <QString>
#include <QSignalSpy>

#include "file_processor.h"

class Worker;

struct TestEnvironment
{
  QTemporaryDir inputDir;
  QTemporaryDir outputDir;

  bool isValid() const
  {
    return inputDir.isValid() && outputDir.isValid();
  }

  QString inputPath() const { return inputDir.path(); }
  QString outputPath() const { return outputDir.path(); }
};

class TestHelpers
{
public:
  // Создание тестовых данных
  static QByteArray createTestFile(const QString &path, qint64 sizeMB);
  static QList<QByteArray> createTestFiles(const QString &dir, int count, qint64 sizeMB);
  static QByteArray generateRandomData(qint64 sizeBytes);

  // Работа с Worker
  static void setupWorker(Worker *worker,
                          const QString &inputDir,
                          const QString &outputDir,
                          const QString &fileMask = "*.bin",
                          const QByteArray &xorKey = QByteArray::fromHex("1234567890ABCDEF"),
                          bool deleteInput = false,
                          ConflictMode conflictMode = ConflictMode::Overwrite);

  static bool runAndWait(Worker *worker, int timeoutMs = 30000);

  // Проверка результатов
  static bool verifyFilesProcessed(const QString &outputDir, int expectedCount);
  static bool verifyFileContent(const QString &filePath, const QByteArray &expectedContent);
  static bool verifyFileNotModified(const QString &filePath, const QByteArray &originalContent);
  static bool verifyFileExists(const QString &path);
  static bool verifyFileNotExists(const QString &path);

  // Создание существующего файла для тестов коллизий
  static bool createExistingFile(const QString &path, const QByteArray &content);
};

#endif // TEST_HELPERS_H