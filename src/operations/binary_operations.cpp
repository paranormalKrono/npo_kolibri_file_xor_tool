#include "binary_operations.h"

// ==========================================
// XorOperation
// ==========================================

XorOperation::XorOperation(const QByteArray &key)
    : m_key(key)
{
}

void XorOperation::apply(QByteArray &data, qint64 offset)
{
  if (m_key.isEmpty())
    return;

  for (int i = 0; i < data.size(); ++i)
  {
    qint64 keyIndex = (offset + i) % m_key.size();
    data[i] = data[i] ^ m_key[keyIndex];
  }
}

QByteArray XorOperation::key() const
{
  return m_key;
}

QString XorOperation::name() const
{
  return "XOR";
}

// ==========================================
// NotOperation
// ==========================================

void NotOperation::apply(QByteArray &data, qint64 offset)
{
  Q_UNUSED(offset);
  for (int i = 0; i < data.size(); ++i)
  {
    data[i] = ~data[i];
  }
}

QByteArray NotOperation::key() const
{
  return QByteArray("NOT");
}

QString NotOperation::name() const
{
  return "NOT (инверсия)";
}

// ==========================================
// RotOperation
// ==========================================

RotOperation::RotOperation(int shift)
    : m_shift(shift)
{
}

void RotOperation::apply(QByteArray &data, qint64 offset)
{
  Q_UNUSED(offset);
  for (int i = 0; i < data.size(); ++i)
  {
    data[i] = static_cast<char>((static_cast<unsigned char>(data[i]) + m_shift) & 0xFF);
  }
}

QByteArray RotOperation::key() const
{
  return QByteArray("ROT_") + QByteArray::number(m_shift);
}

QString RotOperation::name() const
{
  return QString("ROT (сдвиг на %1)").arg(m_shift);
}