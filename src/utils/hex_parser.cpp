#include "hex_parser.h"
#include <QRegularExpression>

QByteArray HexParser::parseHexKey(const QString &hex)
{
  QByteArray key;

  // 1. Создаем копию строки, так как hex является const
  QString cleanHex = hex;

  // 2. Удаляем все не-hex символы из копии
  cleanHex.remove(QRegularExpression("[^0-9A-Fa-f]"));

  // 3. Проверяем длину (8 байт = 16 hex-символов)
  if (cleanHex.length() != 16)
  {
    return QByteArray();
  }

  // 4. Конвертируем каждые 2 символа в байт
  for (int i = 0; i < 16; i += 2)
  {
    bool ok;
    quint8 byte = cleanHex.mid(i, 2).toUInt(&ok, 16);
    if (ok)
    {
      key.append(static_cast<char>(byte));
    }
    else
    {
      return QByteArray();
    }
  }

  return key;
}