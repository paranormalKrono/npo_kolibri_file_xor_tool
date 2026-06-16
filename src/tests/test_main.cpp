#include <QtTest/QtTest>
#include <random>

#include "utils/hex_parser.h"
#include "utils/file_utils.h"
#include "worker.h"

#include <QFile>
#include <QDir>
#include <QTemporaryDir>

class TestFileXorTool : public QObject
{
  Q_OBJECT

private slots:
  // Тесты HexParser
  void test_parseHexKey_validInput();
  void test_parseHexKey_invalidLength();
  void test_parseHexKey_invalidChars();
  void test_parseHexKey_mixedCase();
  void test_parseHexKey_emptyString();
  void test_parseHexKey_withSpaces();

  // Тесты FileUtils
  void test_getUniquePath_noCollision();
  void test_getUniquePath_withCollision();
  void test_getUniquePath_multipleCollisions();

  // Тесты Worker (интеграционные)
  void test_worker_processSmallFile();
  void test_worker_xorReversibility();

private:
  // Возвращаем QByteArray. При ошибке вернем пустой массив.
  QByteArray createTestFile(const QString &path, qint64 sizeMB);
};

// ==========================================
// Тесты HexParser
// ==========================================

void TestFileXorTool::test_parseHexKey_validInput()
{
  QByteArray result = HexParser::parseHexKey("1234567890ABCDEF");
  QCOMPARE(result.size(), 8);
  QCOMPARE(result[0], '\x12');
  QCOMPARE(result[1], '\x34');
  QCOMPARE(result[7], '\xEF');
}

void TestFileXorTool::test_parseHexKey_invalidLength()
{
  QVERIFY(HexParser::parseHexKey("1234").isEmpty());
  QVERIFY(HexParser::parseHexKey("1234567890ABCDEF00").isEmpty());
}

void TestFileXorTool::test_parseHexKey_invalidChars()
{
  QVERIFY(HexParser::parseHexKey("ZZZZZZZZZZZZZZZZ").isEmpty());
}

void TestFileXorTool::test_parseHexKey_mixedCase()
{
  QCOMPARE(HexParser::parseHexKey("1234567890ABCDEF"),
           HexParser::parseHexKey("1234567890abcdef"));
}

void TestFileXorTool::test_parseHexKey_emptyString()
{
  QVERIFY(HexParser::parseHexKey("").isEmpty());
}

void TestFileXorTool::test_parseHexKey_withSpaces()
{
  QByteArray result = HexParser::parseHexKey("12 34 56 78 90 AB CD EF");
  QCOMPARE(result.size(), 8);
  QCOMPARE(result[0], '\x12');
}

// ==========================================
// Тесты FileUtils
// ==========================================

void TestFileXorTool::test_getUniquePath_noCollision()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString basePath = tempDir.path() + "/test.bin";
  QCOMPARE(FileUtils::getUniquePath(basePath), basePath);
}

void TestFileXorTool::test_getUniquePath_withCollision()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString basePath = tempDir.path() + "/test.bin";

  QFile f(basePath);
  QVERIFY(f.open(QIODevice::WriteOnly)); // Здесь OK, так как тестовый метод возвращает void
  f.close();

  QCOMPARE(FileUtils::getUniquePath(basePath), tempDir.path() + "/test_1.bin");
}

void TestFileXorTool::test_getUniquePath_multipleCollisions()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString basePath = tempDir.path() + "/test.bin";

  QFile f1(basePath);
  f1.open(QIODevice::WriteOnly);
  f1.close();
  QFile f2(tempDir.path() + "/test_1.bin");
  f2.open(QIODevice::WriteOnly);
  f2.close();

  QCOMPARE(FileUtils::getUniquePath(basePath), tempDir.path() + "/test_2.bin");
}

// ==========================================
// Вспомогательный метод (БЕЗ QVERIFY/QFAIL внутри!)
// ==========================================

QByteArray TestFileXorTool::createTestFile(const QString &path, qint64 sizeMB)
{
  QFile file(path);
  // Стандартная проверка C++. При ошибке просто возвращаем пустой QByteArray
  if (!file.open(QIODevice::WriteOnly))
  {
    qCritical() << "Не удалось создать тестовый файл:" << path;
    return QByteArray();
  }

  qint64 totalBytes = sizeMB * 1024 * 1024;
  QByteArray data(totalBytes, Qt::Uninitialized);

  // Современный C++11/17 способ генерации случайных чисел
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, 255);

  for (qint64 i = 0; i < totalBytes; ++i)
  {
    data[i] = static_cast<char>(distrib(gen));
  }

  file.write(data);
  file.close();

  return data; // Успешный возврат данных
}

// ==========================================
// Тесты Worker
// ==========================================
void TestFileXorTool::test_worker_processSmallFile()
{
  QTemporaryDir inputDir;
  QTemporaryDir outputDir;
  QVERIFY(inputDir.isValid());
  QVERIFY(outputDir.isValid());

  QString inputPath = inputDir.path() + "/input.bin";
  QString outputPath = outputDir.path() + "/input.bin"; // То же имя, но ДРУГАЯ папка

  QByteArray originalData = createTestFile(inputPath, 1);
  QVERIFY(!originalData.isEmpty());

  Worker worker;
  worker.setConfig(
      inputDir.path(),
      outputDir.path(),
      "input.bin",
      HexParser::parseHexKey("1234567890ABCDEF"),
      false,
      true);

  QSignalSpy finishedSpy(&worker, &Worker::finished);
  worker.processFiles();
  QCOMPARE(finishedSpy.count(), 1);

  // Проверяем файл в выходной директории
  QFile outputFile(outputPath);
  QVERIFY(outputFile.exists());
  QVERIFY(outputFile.open(QIODevice::ReadOnly));

  QByteArray processedData = outputFile.readAll();
  outputFile.close();

  QVERIFY(processedData != originalData);
  QCOMPARE(processedData.size(), originalData.size());
}

void TestFileXorTool::test_worker_xorReversibility()
{
  QTemporaryDir dir1; // Исходник
  QTemporaryDir dir2; // После 1-го прохода (шифрование)
  QTemporaryDir dir3; // После 2-го прохода (дешифрование)

  QVERIFY(dir1.isValid());
  QVERIFY(dir2.isValid());
  QVERIFY(dir3.isValid());

  QString path1 = dir1.path() + "/test.bin";
  QString path2 = dir2.path() + "/test.bin";
  QString path3 = dir3.path() + "/test.bin";

  QByteArray originalData = createTestFile(path1, 1);
  QVERIFY(!originalData.isEmpty());

  QByteArray xorKey = HexParser::parseHexKey("1234567890ABCDEF");

  // Первый проход: читаем из dir1, пишем в dir2
  Worker worker1;
  worker1.setConfig(dir1.path(), dir2.path(), "test.bin", xorKey, false, true);
  QSignalSpy spy1(&worker1, &Worker::finished);
  worker1.processFiles();
  QCOMPARE(spy1.count(), 1);

  // Второй проход: читаем из dir2, пишем в dir3
  Worker worker2;
  worker2.setConfig(dir2.path(), dir3.path(), "test.bin", xorKey, false, true);
  QSignalSpy spy2(&worker2, &Worker::finished);
  worker2.processFiles();
  QCOMPARE(spy2.count(), 1);

  // Читаем итоговый файл из dir3
  QFile outputFile(path3);
  QVERIFY(outputFile.open(QIODevice::ReadOnly));
  QByteArray finalData = outputFile.readAll();
  outputFile.close();

  // Проверяем, что после двух применений XOR данные вернулись к исходным
  QCOMPARE(finalData, originalData);
}

QTEST_MAIN(TestFileXorTool)
#include "test_main.moc"