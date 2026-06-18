#ifndef OPERATION_FACTORY_H
#define OPERATION_FACTORY_H

#include <memory>
#include <QString>
#include <QByteArray>
#include "ibinary_operation.h"

enum class OperationType
{
  XOR,
  NOT,
  ROT
};

class OperationFactory
{
public:
  // Создаёт операцию по типу и параметрам
  static std::unique_ptr<IBinaryOperation> create(OperationType type, const QByteArray &params = QByteArray());

  // Создаёт операцию по названию (для UI)
  static std::unique_ptr<IBinaryOperation> createByName(const QString &name, const QByteArray &params = QByteArray());

  // Возвращает список доступных операций
  static QStringList availableOperations();
};

#endif // OPERATION_FACTORY_H