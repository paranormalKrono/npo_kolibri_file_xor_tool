#include <QtTest/QtTest>
#include <random>
#include "file_cache.h"
#include <QFile>
#include <QTemporaryDir>

class TestFileCache : public QObject
{
  Q_OBJECT

private slots:
  void test_computeFileHash_emptyFile();
  void test_computeFileHash_smallFile();
  void test_computeFileHash_largeFile();
  void test_computeFileHash_sameContent();
  void test_computeFileHash_differentContent();

  void test_cache_isProcessed_empty();
  void test_cache_isProcessed_afterMark();
  void test_cache_isProcessed_deletedFile();
  void test_cache_persistence();

private:
  QByteArray createTestFile(const QString &path, qint64 sizeMB);
};

QByteArray TestFileCache::createTestFile(const QString &path, qint64 sizeMB)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
  {
    return QByteArray();
  }

  qint64 totalBytes = sizeMB * 1024 * 1024;
  QByteArray data(totalBytes, Qt::Uninitialized);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, 255);

  for (qint64 i = 0; i < totalBytes; ++i)
  {
    data[i] = static_cast<char>(distrib(gen));
  }

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

// ==========================================
// Тесты хеширования
// ==========================================

void TestFileCache::test_computeFileHash_emptyFile()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString filePath = tempDir.path() + "/empty.txt";
  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();

  QByteArray hash = FileCache::computeFileHash(filePath);
  QVERIFY(!hash.isEmpty());
  QCOMPARE(hash.size(), 32); // SHA-256 = 32 байта
}

void TestFileCache::test_computeFileHash_smallFile()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString filePath = tempDir.path() + "/small.bin";
  QByteArray data = createTestFile(filePath, 1);
  QVERIFY(!data.isEmpty());

  QByteArray hash = FileCache::computeFileHash(filePath);
  QVERIFY(!hash.isEmpty());
  QCOMPARE(hash.size(), 32);
}

void TestFileCache::test_computeFileHash_largeFile()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString filePath = tempDir.path() + "/large.bin";
  QByteArray data = createTestFile(filePath, 50); // 50 МБ
  QVERIFY(!data.isEmpty());

  QByteArray hash = FileCache::computeFileHash(filePath);
  QVERIFY(!hash.isEmpty());
  QCOMPARE(hash.size(), 32);
}

void TestFileCache::test_computeFileHash_sameContent()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString file1 = tempDir.path() + "/file1.bin";
  QString file2 = tempDir.path() + "/file2.bin";

  QByteArray data = createTestFile(file1, 5);
  QVERIFY(!data.isEmpty());

  // Копируем файл
  QVERIFY(QFile::copy(file1, file2));

  QByteArray hash1 = FileCache::computeFileHash(file1);
  QByteArray hash2 = FileCache::computeFileHash(file2);

  QCOMPARE(hash1, hash2);
}

void TestFileCache::test_computeFileHash_differentContent()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString file1 = tempDir.path() + "/file1.bin";
  QString file2 = tempDir.path() + "/file2.bin";

  QByteArray data1 = createTestFile(file1, 5);
  QByteArray data2 = createTestFile(file2, 5);
  QVERIFY(!data1.isEmpty());
  QVERIFY(!data2.isEmpty());

  QByteArray hash1 = FileCache::computeFileHash(file1);
  QByteArray hash2 = FileCache::computeFileHash(file2);

  QVERIFY(hash1 != hash2);
}

// ==========================================
// Тесты кеша
// ==========================================

void TestFileCache::test_cache_isProcessed_empty()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  FileCache cache(tempDir.path());

  QByteArray fileHash = QByteArray(32, 'a');
  QByteArray xorKey = QByteArray::fromHex("1234567890ABCDEF");
  QString outputPath;

  QVERIFY(!cache.isProcessed(fileHash, xorKey, outputPath));
}

void TestFileCache::test_cache_isProcessed_afterMark()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  // Создаём фиктивный выходной файл
  QString outputPath = tempDir.path() + "/output.bin";
  QFile outFile(outputPath);
  QVERIFY(outFile.open(QIODevice::WriteOnly));
  outFile.write("test");
  outFile.close();

  FileCache cache(tempDir.path());

  QByteArray fileHash = QByteArray(32, 'a');
  QByteArray xorKey = QByteArray::fromHex("1234567890ABCDEF");
  QString cachedPath;

  // Помечаем файл как обработанный
  cache.markProcessed(fileHash, xorKey, outputPath);

  // Проверяем, что он в кеше
  QVERIFY(cache.isProcessed(fileHash, xorKey, cachedPath));
  QCOMPARE(cachedPath, outputPath);
}

void TestFileCache::test_cache_isProcessed_deletedFile()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  // Создаём и удаляем фиктивный выходной файл
  QString outputPath = tempDir.path() + "/output.bin";
  QFile outFile(outputPath);
  QVERIFY(outFile.open(QIODevice::WriteOnly));
  outFile.write("test");
  outFile.close();

  FileCache cache(tempDir.path());

  QByteArray fileHash = QByteArray(32, 'a');
  QByteArray xorKey = QByteArray::fromHex("1234567890ABCDEF");
  QString cachedPath;

  cache.markProcessed(fileHash, xorKey, outputPath);

  // Удаляем выходной файл
  QVERIFY(outFile.remove());

  // Проверяем, что кеш вернул false (файл удалён)
  QVERIFY(!cache.isProcessed(fileHash, xorKey, cachedPath));
}

void TestFileCache::test_cache_persistence()
{
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString outputPath = tempDir.path() + "/output.bin";
  QFile outFile(outputPath);
  QVERIFY(outFile.open(QIODevice::WriteOnly));
  outFile.write("test");
  outFile.close();

  QByteArray fileHash = QByteArray(32, 'a');
  QByteArray xorKey = QByteArray::fromHex("1234567890ABCDEF");

  // Создаём кеш и помечаем файл
  {
    FileCache cache(tempDir.path());
    cache.markProcessed(fileHash, xorKey, outputPath);
  } // Кеш уничтожается, но должен сохраниться на диск

  // Создаём новый кеш (загружает из файла)
  FileCache cache2(tempDir.path());
  QString cachedPath;

  QVERIFY(cache2.isProcessed(fileHash, xorKey, cachedPath));
  QCOMPARE(cachedPath, outputPath);
}

QTEST_MAIN(TestFileCache)
#include "test_file_cache.moc"