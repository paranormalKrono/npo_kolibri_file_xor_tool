#include <QtTest/QtTest>
#include "file_utils.h"
#include <QFile>
#include <QTemporaryDir>

class TestFileUtils : public QObject
{
  Q_OBJECT

private slots:
  void test_noCollision();
  void test_withCollision();
  void test_multipleCollisions();
  void test_getProcessedPath();
  void test_getProcessedPath_complexName();
};

void TestFileUtils::test_noCollision()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString basePath = tempDir.path() + "/test.bin";
  QCOMPARE(FileUtils::getUniquePath(basePath), basePath);
}

void TestFileUtils::test_withCollision()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString basePath = tempDir.path() + "/test.bin";

  QFile f(basePath);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.close();

  QCOMPARE(FileUtils::getUniquePath(basePath), tempDir.path() + "/test_1.bin");
}

void TestFileUtils::test_multipleCollisions()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString basePath = tempDir.path() + "/test.bin";

  QFile f1(basePath);
  QVERIFY(f1.open(QIODevice::WriteOnly));
  f1.close();

  QFile f2(tempDir.path() + "/test_1.bin");
  QVERIFY(f2.open(QIODevice::WriteOnly));
  f2.close();

  QCOMPARE(FileUtils::getUniquePath(basePath), tempDir.path() + "/test_2.bin");
}

void TestFileUtils::test_getProcessedPath()
{
  QString result = FileUtils::getProcessedPath("/output", "test.bin");
  QCOMPARE(result, "/output/test.bin.xor");
}

void TestFileUtils::test_getProcessedPath_complexName()
{
  QString result = FileUtils::getProcessedPath("/output", "my.data.file.tar");
  QCOMPARE(result, "/output/my.data.file.tar.xor");
}

QTEST_MAIN(TestFileUtils)
#include "test_file_utils.moc"