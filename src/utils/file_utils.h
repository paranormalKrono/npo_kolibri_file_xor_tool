#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <QString>

class FileUtils
{
public:
  static QString getUniquePath(const QString &basePath);
};

#endif // FILE_UTILS_H