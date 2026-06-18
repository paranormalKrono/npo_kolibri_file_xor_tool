#include <QtTest/QtTest>
#include "test_helpers.h"
#include "worker.h"

class TestWorker : public QObject
{
  Q_OBJECT

private slots:
  // Базовые тесты
  void test_processSmallFile();
  void test_xorReversibility();
  void test_multipleFilesParallel();
  void test_largeFiles();
  void test_stopDuringProcessing();

  // Data-driven тесты для режимов коллизий
  void test_conflictModes_data();
  void test_conflictModes();
};

void TestWorker::test_processSmallFile()
{
  TestEnvironment env;
  QVERIFY(env.isValid());

  QByteArray originalData = TestHelpers::createTestFile(env.inputPath() + "/input.bin", 1);
  QVERIFY(!originalData.isEmpty());

  Worker worker;
  TestHelpers::setupWorker(&worker, env.inputPath(), env.outputPath(), "input.bin");

  QSignalSpy finishedSpy(&worker, &Worker::finished);
  worker.processFiles();
  QVERIFY(finishedSpy.wait(30000));

  QString outputPath = env.outputPath() + "/input.bin.xor";
  QVERIFY(TestHelpers::verifyFileExists(outputPath));

  QFile outputFile(outputPath);
  QVERIFY(outputFile.open(QIODevice::ReadOnly));
  QByteArray processedData = outputFile.readAll();
  outputFile.close();

  QVERIFY(processedData != originalData);
  QCOMPARE(processedData.size(), originalData.size());
}

void TestWorker::test_xorReversibility()
{
  QTemporaryDir dir1, dir2, dir3;
  QVERIFY(dir1.isValid() && dir2.isValid() && dir3.isValid());

  QByteArray originalData = TestHelpers::createTestFile(dir1.path() + "/test.bin", 1);
  QVERIFY(!originalData.isEmpty());

  QByteArray xorKey = QByteArray::fromHex("1234567890ABCDEF");

  // Первый проход
  Worker worker1;
  TestHelpers::setupWorker(&worker1, dir1.path(), dir2.path(), "test.bin", xorKey);
  QVERIFY(TestHelpers::runAndWait(&worker1));

  // Второй проход
  Worker worker2;
  TestHelpers::setupWorker(&worker2, dir2.path(), dir3.path(), "*.xor", xorKey);
  QVERIFY(TestHelpers::runAndWait(&worker2));

  QString finalPath = dir3.path() + "/test.bin.xor.xor";
  QVERIFY(TestHelpers::verifyFileContent(finalPath, originalData));
}

void TestWorker::test_multipleFilesParallel()
{
  TestEnvironment env;
  QVERIFY(env.isValid());

  const int fileCount = 10;
  TestHelpers::createTestFiles(env.inputPath(), fileCount, 1);

  Worker worker;
  TestHelpers::setupWorker(&worker, env.inputPath(), env.outputPath());

  auto startTime = QTime::currentTime();
  QVERIFY(TestHelpers::runAndWait(&worker));
  auto duration = startTime.msecsTo(QTime::currentTime());

  qDebug() << "Processing" << fileCount << "files took:" << duration << "ms";
  QVERIFY(TestHelpers::verifyFilesProcessed(env.outputPath(), fileCount));
}

void TestWorker::test_largeFiles()
{
  TestEnvironment env;
  QVERIFY(env.isValid());

  const int fileCount = 3;
  const int fileSizeMB = 100;

  qDebug() << "Creating" << fileCount << "files of" << fileSizeMB << "MB each...";
  TestHelpers::createTestFiles(env.inputPath(), fileCount, fileSizeMB);

  Worker worker;
  TestHelpers::setupWorker(&worker, env.inputPath(), env.outputPath(), "*.bin",
                           QByteArray::fromHex("FEDCBA0987654321"));

  auto startTime = QTime::currentTime();
  QVERIFY(TestHelpers::runAndWait(&worker, 60000));
  auto duration = startTime.msecsTo(QTime::currentTime());

  qDebug() << "Processing" << fileCount << "files of" << fileSizeMB << "MB took:" << duration << "ms";
  QVERIFY(TestHelpers::verifyFilesProcessed(env.outputPath(), fileCount));
}

void TestWorker::test_stopDuringProcessing()
{
  TestEnvironment env;
  QVERIFY(env.isValid());

  TestHelpers::createTestFiles(env.inputPath(), 5, 50);

  Worker worker;
  TestHelpers::setupWorker(&worker, env.inputPath(), env.outputPath());

  QSignalSpy finishedSpy(&worker, &Worker::finished);
  worker.processFiles();

  QTest::qWait(500);
  worker.stop();

  QVERIFY(finishedSpy.count() > 0 || finishedSpy.wait(5000));

  QDir outDir(env.outputPath());
  QStringList files = outDir.entryList(QStringList() << "*.xor", QDir::Files);

  qDebug() << "Files processed after stop:" << files.size() << "out of 5";
  QVERIFY(files.size() < 5);
}

void TestWorker::test_conflictModes_data()
{
  QTest::addColumn<int>("mode");
  QTest::addColumn<QString>("expectedFileName");
  QTest::addColumn<bool>("shouldOverwrite");

  QTest::newRow("Skip") << 1 << "test.bin.xor" << false;
  QTest::newRow("Numbering") << 2 << "test_1.bin.xor" << false;
  QTest::newRow("Overwrite") << 0 << "test.bin.xor" << true;
}

void TestWorker::test_conflictModes()
{
  QFETCH(int, mode);
  QFETCH(QString, expectedFileName);
  QFETCH(bool, shouldOverwrite);

  TestEnvironment env;
  QVERIFY(env.isValid());

  QByteArray originalData = TestHelpers::createTestFile(env.inputPath() + "/test.bin", 1);
  QVERIFY(!originalData.isEmpty());

  QByteArray existingContent = "EXISTING_CONTENT";
  QString existingPath = env.outputPath() + "/test.bin.xor";
  QVERIFY(TestHelpers::createExistingFile(existingPath, existingContent));

  Worker worker;
  TestHelpers::setupWorker(&worker, env.inputPath(), env.outputPath(), "test.bin",
                           QByteArray::fromHex("1234567890ABCDEF"), false, static_cast<ConflictMode>(mode));

  QVERIFY(TestHelpers::runAndWait(&worker));

  QString expectedPath = env.outputPath() + "/" + expectedFileName;
  QVERIFY2(TestHelpers::verifyFileExists(expectedPath),
           qPrintable("Expected file not found: " + expectedPath));

  if (shouldOverwrite)
  {
    QFile file(expectedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();

    QVERIFY2(content != existingContent, "File was not overwritten");
    QCOMPARE(content.size(), originalData.size());
  }
  else
  {
    QVERIFY(TestHelpers::verifyFileNotModified(existingPath, existingContent));
  }
}

QTEST_MAIN(TestWorker)
#include "test_worker.moc"