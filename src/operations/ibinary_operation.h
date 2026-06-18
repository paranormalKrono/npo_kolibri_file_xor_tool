#ifndef IBINARY_OPERATION_H
#define IBINARY_OPERATION_H

#include <QByteArray>
#include <QString>

class IBinaryOperation
{
public:
  virtual ~IBinaryOperation() = default;

  // Применяет операцию к блоку данных
  // offset — позиция блока в файле (для операций, зависящих от позиции)
  virtual void apply(QByteArray &data, qint64 offset) = 0;

  // Возвращает ключ операции (для кеша)
  virtual QByteArray key() const = 0;

  // Возвращает название операции (для UI)
  virtual QString name() const = 0;
};

#endif // IBINARY_OPERATION_H