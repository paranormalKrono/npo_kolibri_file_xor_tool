#ifndef HEX_PARSER_H
#define HEX_PARSER_H

#include <QByteArray>
#include <QString>

class HexParser
{
public:
  static QByteArray parseHexKey(const QString &hex);
};

#endif // HEX_PARSER_H