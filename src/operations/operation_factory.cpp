#include <QStringList>

#include "operation_factory.h"
#include "binary_operations.h"

std::unique_ptr<IBinaryOperation> OperationFactory::create(OperationType type, const QByteArray &params)
{
  switch (type)
  {
  case OperationType::XOR:
    return std::make_unique<XorOperation>(params);

  case OperationType::NOT:
    return std::make_unique<NotOperation>();

  case OperationType::ROT:
    int shift = params.isEmpty() ? 1 : params.toInt();
    return std::make_unique<RotOperation>(shift);
  }

  return nullptr;
}

std::unique_ptr<IBinaryOperation> OperationFactory::createByName(const QString &name, const QByteArray &params)
{
  if (name == "XOR")
  {
    return create(OperationType::XOR, params);
  }
  else if (name == "NOT")
  {
    return create(OperationType::NOT, params);
  }
  else if (name.startsWith("ROT"))
  {
    return create(OperationType::ROT, params);
  }

  return nullptr;
}

QStringList OperationFactory::availableOperations()
{
  return {"XOR", "NOT", "ROT"};
}