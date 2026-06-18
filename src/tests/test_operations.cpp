#include <QtTest/QtTest>
#include "operations/ibinary_operation.h"
#include "operations/binary_operations.h"
#include "operations/operation_factory.h"

class TestOperations : public QObject
{
  Q_OBJECT

private slots:
  // XOR тесты
  void test_xorBasic();
  void test_xorEmptyKey();
  void test_xorSingleByteKey();
  void test_xorLongKey();
  void test_xorReversibility();
  void test_xorWithOffset();

  // NOT тесты
  void test_notBasic();
  void test_notReversibility();
  void test_notAllBytes();

  // ROT тесты
  void test_rotBasic();
  void test_rotZeroShift();
  void test_rotFullCycle();
  void test_rotNegativeShift();
  void test_rotReversibility();

  // Фабрика
  void test_factoryCreateXor();
  void test_factoryCreateNot();
  void test_factoryCreateRot();
  void test_factoryCreateByName();
  void test_factoryInvalidName();
  void test_factoryAvailableOperations();

private:
  QByteArray createTestData(int size);
};

QByteArray TestOperations::createTestData(int size)
{
  QByteArray data(size, Qt::Uninitialized);
  for (int i = 0; i < size; ++i)
  {
    data[i] = static_cast<char>(i % 256);
  }
  return data;
}

// ==========================================
// XOR тесты
// ==========================================

void TestOperations::test_xorBasic()
{
  QByteArray key = QByteArray::fromHex("FF00FF00");
  XorOperation op(key);

  QByteArray data = QByteArray::fromHex("00FF00FF");
  // 00^FF=FF, FF^00=FF, 00^FF=FF, FF^00=FF
  QByteArray expected = QByteArray::fromHex("FFFFFFFF");

  op.apply(data, 0);

  QCOMPARE(data, expected);
}

void TestOperations::test_xorEmptyKey()
{
  QByteArray key;
  XorOperation op(key);

  QByteArray data = "test data";
  QByteArray original = data;

  op.apply(data, 0);

  // С пустым ключом данные не должны измениться
  QCOMPARE(data, original);
}

void TestOperations::test_xorSingleByteKey()
{
  QByteArray key = QByteArray::fromHex("AA");
  XorOperation op(key);

  QByteArray data = QByteArray::fromHex("00 55 FF");
  QByteArray expected = QByteArray::fromHex("AA FF 55");

  op.apply(data, 0);

  QCOMPARE(data, expected);
}

void TestOperations::test_xorLongKey()
{
  QByteArray key = QByteArray::fromHex("0102030405060708");
  XorOperation op(key);

  QByteArray data = QByteArray::fromHex("0000000000000000");
  QByteArray expected = QByteArray::fromHex("0102030405060708");

  op.apply(data, 0);

  QCOMPARE(data, expected);
}

void TestOperations::test_xorReversibility()
{
  QByteArray key = QByteArray::fromHex("1234567890ABCDEF");
  XorOperation op(key);

  QByteArray original = createTestData(1000);
  QByteArray data = original;

  // Применяем XOR дважды
  op.apply(data, 0);
  QVERIFY(data != original); // Данные изменились

  op.apply(data, 0);
  QCOMPARE(data, original); // Вернулись к оригиналу
}

void TestOperations::test_xorWithOffset()
{
  QByteArray key = QByteArray::fromHex("FF00");
  XorOperation op(key);

  // Данные: 00 00 00 00
  QByteArray data = QByteArray::fromHex("00000000");

  // Применяем с offset = 1
  // Ключ начинается со второй позиции: 00 FF 00 FF
  op.apply(data, 1);

  // Ожидаем: 00 FF 00 FF
  QByteArray expected = QByteArray::fromHex("00FF00FF");
  QCOMPARE(data, expected);
}

// ==========================================
// NOT тесты
// ==========================================

void TestOperations::test_notBasic()
{
  NotOperation op;

  QByteArray data = QByteArray::fromHex("00 FF 55 AA");
  QByteArray expected = QByteArray::fromHex("FF 00 AA 55");

  op.apply(data, 0);

  QCOMPARE(data, expected);
}

void TestOperations::test_notReversibility()
{
  NotOperation op;

  QByteArray original = createTestData(1000);
  QByteArray data = original;

  // Применяем NOT дважды
  op.apply(data, 0);
  QVERIFY(data != original);

  op.apply(data, 0);
  QCOMPARE(data, original);
}

void TestOperations::test_notAllBytes()
{
  NotOperation op;

  // Проверяем все возможные значения байта
  QByteArray data(256, Qt::Uninitialized);
  for (int i = 0; i < 256; ++i)
  {
    data[i] = static_cast<char>(i);
  }

  op.apply(data, 0);

  // Проверяем, что каждый байт инвертирован
  for (int i = 0; i < 256; ++i)
  {
    unsigned char original = static_cast<unsigned char>(i);
    unsigned char inverted = static_cast<unsigned char>(data[i]);
    QCOMPARE(inverted, static_cast<unsigned char>(~original));
  }
}

// ==========================================
// ROT тесты
// ==========================================

void TestOperations::test_rotBasic()
{
  RotOperation op(1);

  QByteArray data = QByteArray::fromHex("00 01 FE FF");
  QByteArray expected = QByteArray::fromHex("01 02 FF 00");

  op.apply(data, 0);

  QCOMPARE(data, expected);
}

void TestOperations::test_rotZeroShift()
{
  RotOperation op(0);

  QByteArray original = createTestData(100);
  QByteArray data = original;

  op.apply(data, 0);

  // С нулевым сдвигом данные не должны измениться
  QCOMPARE(data, original);
}

void TestOperations::test_rotFullCycle()
{
  RotOperation op(256);

  QByteArray original = createTestData(100);
  QByteArray data = original;

  op.apply(data, 0);

  // Сдвиг на 256 = сдвиг на 0 (полный цикл)
  QCOMPARE(data, original);
}

void TestOperations::test_rotNegativeShift()
{
  RotOperation op(-1);

  QByteArray data = QByteArray::fromHex("01 02 FF 00");
  QByteArray expected = QByteArray::fromHex("00 01 FE FF");

  op.apply(data, 0);

  QCOMPARE(data, expected);
}

void TestOperations::test_rotReversibility()
{
  int shift = 42;
  RotOperation opForward(shift);
  RotOperation opBackward(-shift);

  QByteArray original = createTestData(1000);
  QByteArray data = original;

  // Применяем прямой сдвиг
  opForward.apply(data, 0);
  QVERIFY(data != original);

  // Применяем обратный сдвиг
  opBackward.apply(data, 0);
  QCOMPARE(data, original);
}

// ==========================================
// Фабрика
// ==========================================

void TestOperations::test_factoryCreateXor()
{
  QByteArray key = QByteArray::fromHex("1234567890ABCDEF");
  auto op = OperationFactory::create(OperationType::XOR, key);

  QVERIFY(op != nullptr);
  QCOMPARE(op->name(), QString("XOR"));
  QCOMPARE(op->key(), key);
}

void TestOperations::test_factoryCreateNot()
{
  auto op = OperationFactory::create(OperationType::NOT);

  QVERIFY(op != nullptr);
  QCOMPARE(op->name(), QString("NOT (инверсия)"));
  QCOMPARE(op->key(), QByteArray("NOT"));
}

void TestOperations::test_factoryCreateRot()
{
  QByteArray params = QByteArray::number(42);
  auto op = OperationFactory::create(OperationType::ROT, params);

  QVERIFY(op != nullptr);
  QCOMPARE(op->name(), QString("ROT (сдвиг на 42)"));
  QCOMPARE(op->key(), QByteArray("ROT_42"));
}

void TestOperations::test_factoryCreateByName()
{
  QByteArray key = QByteArray::fromHex("FF00FF00");

  auto xorOp = OperationFactory::createByName("XOR", key);
  QVERIFY(xorOp != nullptr);
  QCOMPARE(xorOp->name(), QString("XOR"));

  auto notOp = OperationFactory::createByName("NOT");
  QVERIFY(notOp != nullptr);
  QCOMPARE(notOp->name(), QString("NOT (инверсия)"));

  auto rotOp = OperationFactory::createByName("ROT", QByteArray::number(10));
  QVERIFY(rotOp != nullptr);
  QCOMPARE(rotOp->name(), QString("ROT (сдвиг на 10)"));
}

void TestOperations::test_factoryInvalidName()
{
  auto op = OperationFactory::createByName("INVALID");
  QVERIFY(op == nullptr);
}

void TestOperations::test_factoryAvailableOperations()
{
  QStringList ops = OperationFactory::availableOperations();

  QCOMPARE(ops.size(), 3);
  QVERIFY(ops.contains("XOR"));
  QVERIFY(ops.contains("NOT"));
  QVERIFY(ops.contains("ROT"));
}

QTEST_MAIN(TestOperations)
#include "test_operations.moc"