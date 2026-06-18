#ifndef BINARY_OPERATIONS_H
#define BINARY_OPERATIONS_H

#include "ibinary_operation.h"

// XOR операция
class XorOperation : public IBinaryOperation
{
public:
  explicit XorOperation(const QByteArray &key);
  void apply(QByteArray &data, qint64 offset) override;
  QByteArray key() const override;
  QString name() const override;

private:
  QByteArray m_key;
};

// NOT операция (инверсия)
class NotOperation : public IBinaryOperation
{
public:
  void apply(QByteArray &data, qint64 offset) override;
  QByteArray key() const override;
  QString name() const override;
};

// ROT операция (циклический сдвиг)
class RotOperation : public IBinaryOperation
{
public:
  explicit RotOperation(int shift);
  void apply(QByteArray &data, qint64 offset) override;
  QByteArray key() const override;
  QString name() const override;

private:
  int m_shift;
};

#endif // BINARY_OPERATIONS_H