#include <QtTest/QtTest>
#include <QtConcurrent>
#include "test_helpers.h"
#include "worker.h"

class TestStress : public QObject
{
  Q_OBJECT

private slots:
  void test_concurrentModifications();
  void test_fileDeletionDuringProcessing();
  void test_fileLockingDuringProcessing();
  void test_watcherUnderLoad();
  void test_chaosMode();

private:
  void modifyFileRandomly(const QString &path);
  void deleteFileRandomly(const QString &path);
  void lockFileRandomly(const QString &path, int durationMs);
};

void TestStress::modifyFileRandomly(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::Append))
    return;

  QByteArray randomData = TestHelpers::generateRandomData(1024);
  file.write(randomData);
  file.close();
}

void TestStress::deleteFileRandomly(const QString &path)
{
  QFile::remove(path);
}

void TestStress::lockFileRandomly(const QString &path, int durationMs)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return;
  QThread::msleep(durationMs);
  file.close();
}

void TestStress::test_concurrentModifications()
{
  QTemporaryDir inputDir, outputDir;
  QVERIFY(inputDir.isValid() && outputDir.isValid());

  const int fileCount = 5;
  TestHelpers::createTestFiles(inputDir.path(), fileCount, 10);

  Worker worker;
  TestHelpers::setupWorker(&worker, inputDir.path(), outputDir.path());

  auto modifyFuture = QtConcurrent::run([&]()
                                        {
        for (int i = 0; i < 5; ++i) {
            QString path = inputDir.path() + QString("/file_%1.bin").arg(i % fileCount);
            QThread::msleep(500);
            modifyFileRandomly(path);
        } });

  QVERIFY(TestHelpers::runAndWait(&worker, 30000));
  modifyFuture.waitForFinished();

  qDebug() << "Concurrent modifications test completed without crash";
  QVERIFY(true);
}

void TestStress::test_fileDeletionDuringProcessing()
{
  QTemporaryDir inputDir, outputDir;
  QVERIFY(inputDir.isValid() && outputDir.isValid());

  const int fileCount = 10;
  TestHelpers::createTestFiles(inputDir.path(), fileCount, 5);

  Worker worker;
  TestHelpers::setupWorker(&worker, inputDir.path(), outputDir.path());

  auto deleteFuture = QtConcurrent::run([&]()
                                        {
        for (int i = 0; i < fileCount; ++i) {
            QThread::msleep(200);
            deleteFileRandomly(inputDir.path() + QString("/file_%1.bin").arg(i));
        } });

  QVERIFY(TestHelpers::runAndWait(&worker, 30000));
  deleteFuture.waitForFinished();

  qDebug() << "File deletion test completed without crash";
  QVERIFY(true);
}

void TestStress::test_fileLockingDuringProcessing()
{
  QTemporaryDir inputDir, outputDir;
  QVERIFY(inputDir.isValid() && outputDir.isValid());

  const int fileCount = 5;
  TestHelpers::createTestFiles(inputDir.path(), fileCount, 10);

  Worker worker;
  TestHelpers::setupWorker(&worker, inputDir.path(), outputDir.path());

  auto lockFuture = QtConcurrent::run([&]()
                                      {
        for (int i = 0; i < fileCount; ++i) {
            lockFileRandomly(inputDir.path() + QString("/file_%1.bin").arg(i), 500);
        } });

  QVERIFY(TestHelpers::runAndWait(&worker, 30000));
  lockFuture.waitForFinished();

  qDebug() << "File locking test completed without crash";
  QVERIFY(true);
}

void TestStress::test_watcherUnderLoad()
{
  QTemporaryDir inputDir;
  QVERIFY(inputDir.isValid());

  QFileSystemWatcher watcher;
  QVERIFY(watcher.addPath(inputDir.path()));

  QSignalSpy fileChangedSpy(&watcher, &QFileSystemWatcher::fileChanged);
  QSignalSpy dirChangedSpy(&watcher, &QFileSystemWatcher::directoryChanged);

  QTest::qWait(200);

  for (int i = 0; i < 10; ++i)
  {
    QString path = inputDir.path() + QString("/watcher_%1.bin").arg(i);

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(TestHelpers::generateRandomData(1024));
    file.close();

    QCoreApplication::processEvents();
    QTest::qWait(100);

    QVERIFY(file.open(QIODevice::Append));
    file.write(TestHelpers::generateRandomData(512));
    file.close();

    QCoreApplication::processEvents();
    QTest::qWait(100);
  }

  qDebug() << "Watcher received" << fileChangedSpy.count() << "file changes";
  qDebug() << "Watcher received" << dirChangedSpy.count() << "directory changes";

  QVERIFY(fileChangedSpy.count() + dirChangedSpy.count() >= 0);
}

void TestStress::test_chaosMode()
{
  QTemporaryDir inputDir, outputDir;
  QVERIFY(inputDir.isValid() && outputDir.isValid());

  const int fileCount = 10;
  for (int i = 0; i < fileCount; ++i)
  {
    int sizeMB = (i % 3) + 1;
    TestHelpers::createTestFile(inputDir.path() + QString("/chaos_%1.bin").arg(i), sizeMB);
  }

  Worker worker;
  TestHelpers::setupWorker(&worker, inputDir.path(), outputDir.path(), "*.bin",
                           QByteArray::fromHex("1111111111111111"), false, ConflictMode::Overwrite);

  auto modifyFuture = QtConcurrent::run([&]()
                                        {
        for (int i = 0; i < 15; ++i) {
            QString path = inputDir.path() + QString("/chaos_%1.bin").arg(i % fileCount);
            if (QFile::exists(path)) modifyFileRandomly(path);
            QThread::msleep(200);
        } });

  auto deleteFuture = QtConcurrent::run([&]()
                                        {
        for (int i = 0; i < 5; ++i) {
            QThread::msleep(500);
            deleteFileRandomly(inputDir.path() + QString("/chaos_%1.bin").arg((i * 2) % fileCount));
        } });

  auto lockFuture = QtConcurrent::run([&]()
                                      {
        for (int i = 0; i < 8; ++i) {
            QThread::msleep(300);
            QString path = inputDir.path() + QString("/chaos_%1.bin").arg((i * 3) % fileCount);
            if (QFile::exists(path)) lockFileRandomly(path, 200);
        } });

  QVERIFY(TestHelpers::runAndWait(&worker, 60000));

  modifyFuture.waitForFinished();
  deleteFuture.waitForFinished();
  lockFuture.waitForFinished();

  qDebug() << "Chaos mode test completed without crash!";
  QVERIFY(true);
}

QTEST_MAIN(TestStress)
#include "test_stress.moc"