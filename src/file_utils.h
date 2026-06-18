#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <QString>

class FileUtils
{
public:
  // Получить уникальный путь (с счётчиком при коллизии)
  static QString getUniquePath(const QString &basePath);

  // Получить путь для обработанного файла (добавляет .xor)
  static QString getProcessedPath(const QString &outputDir, const QString &fileName);
};

#endif // FILE_UTILS_H