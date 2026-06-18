#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QComboBox>
#include "utils/hex_parser.h"

#include "mainwindow.h"
#include "worker.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_workerThread(new QThread(this)), m_worker(new Worker()), m_timer(new QTimer(this)), m_watcher(new QFileSystemWatcher(this))
{
  createUI();
  setupConnections();

  m_worker->moveToThread(m_workerThread);

  connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(this, &MainWindow::destroyed, m_workerThread, &QThread::quit);
}

MainWindow::~MainWindow()
{
  if (m_watcherActive)
  {
    m_watcher->removePaths(m_watcher->directories());
    m_watcher->removePaths(m_watcher->files());
  }

  m_timer->stop();

  // Сигнализируем задачам о завершении
  m_worker->stop();

  // Останавливаем поток Worker
  m_workerThread->quit();

  // Ждём завершения
  m_workerThread->wait();

  // Worker уничтожается автоматически (unique_ptr или deleteLater)
}

void MainWindow::createUI()
{
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setSpacing(8);

  // Описание программы
  QLabel *descriptionLabel = new QLabel(
      "<b>FileXorTool</b> — утилита для пакетной обработки файлов с помощью операции XOR.<br>"
      "Программа читает файлы чанками по 4 МБ, что позволяет обрабатывать файлы любого размера "
      "без загрузки их целиком в память. Результаты сохраняются с расширением .xor.");
  descriptionLabel->setWordWrap(true);
  descriptionLabel->setStyleSheet("QLabel { padding: 8px; background-color: #f0f0f0; border-radius: 4px; }");
  mainLayout->addWidget(descriptionLabel);

  // Группа: Пути
  QGroupBox *pathsGroup = new QGroupBox("Пути обработки");
  QVBoxLayout *pathsLayout = new QVBoxLayout(pathsGroup);

  QHBoxLayout *inputLayout = new QHBoxLayout();
  inputLayout->addWidget(new QLabel("Входная директория:"));
  m_inputDirEdit = new QLineEdit();
  m_inputDirEdit->setPlaceholderText("Папка с файлами для обработки");
  m_browseInputBtn = new QPushButton("Обзор...");
  m_browseInputBtn->setFixedWidth(80);
  inputLayout->addWidget(m_inputDirEdit);
  inputLayout->addWidget(m_browseInputBtn);
  pathsLayout->addLayout(inputLayout);

  QHBoxLayout *outputLayout = new QHBoxLayout();
  outputLayout->addWidget(new QLabel("Выходная директория:"));
  m_outputDirEdit = new QLineEdit();
  m_outputDirEdit->setPlaceholderText("Папка для сохранения результатов (файлы .xor)");
  m_browseOutputBtn = new QPushButton("Обзор...");
  m_browseOutputBtn->setFixedWidth(80);
  outputLayout->addWidget(m_outputDirEdit);
  outputLayout->addWidget(m_browseOutputBtn);
  pathsLayout->addLayout(outputLayout);

  mainLayout->addWidget(pathsGroup);

  // Группа: Параметры
  QGroupBox *paramsGroup = new QGroupBox("Параметры обработки");
  QVBoxLayout *paramsLayout = new QVBoxLayout(paramsGroup);

  // Маска и ключ
  QHBoxLayout *paramsRow1 = new QHBoxLayout();

  QHBoxLayout *maskLayout = new QHBoxLayout();
  QLabel *maskLabel = new QLabel("Маска файлов:");
  maskLabel->setToolTip("Шаблон имени файлов для обработки. Например: *.bin, data_*.txt, test?.dat");
  maskLayout->addWidget(maskLabel);
  m_fileMaskEdit = new QLineEdit("*.bin");
  m_fileMaskEdit->setFixedWidth(100);
  m_fileMaskEdit->setToolTip("Поддерживаются символы * (любая последовательность) и ? (один символ)");
  maskLayout->addWidget(m_fileMaskEdit);
  paramsRow1->addLayout(maskLayout);

  paramsRow1->addSpacing(15);

  QHBoxLayout *keyLayout = new QHBoxLayout();
  QLabel *keyLabel = new QLabel("XOR ключ (hex):");
  keyLabel->setToolTip("Ключ для операции XOR. Должен содержать ровно 16 hex-символов (8 байт)");
  keyLayout->addWidget(keyLabel);
  m_xorKeyEdit = new QLineEdit();
  m_xorKeyEdit->setPlaceholderText("1234567890ABCDEF");
  m_xorKeyEdit->setMaxLength(16);
  m_xorKeyEdit->setFixedWidth(150);
  m_xorKeyEdit->setToolTip("Введите ровно 16 hex-символов (0-9, A-F). Ключ циклически применяется к каждому байту файла");
  keyLayout->addWidget(m_xorKeyEdit);
  m_xorKeyStatusLabel = new QLabel("0/16");
  m_xorKeyStatusLabel->setStyleSheet("QLabel { color: #999; }");
  keyLayout->addWidget(m_xorKeyStatusLabel);
  paramsRow1->addLayout(keyLayout);

  paramsRow1->addStretch();
  paramsLayout->addLayout(paramsRow1);

  // Опции с пояснениями
  QHBoxLayout *optionsLayout = new QHBoxLayout();
  m_deleteInputCheck = new QCheckBox("Удалять исходные файлы");
  m_deleteInputCheck->setToolTip("После успешной обработки исходный файл будет удалён. Используйте с осторожностью!");
  optionsLayout->addWidget(m_deleteInputCheck);
  optionsLayout->addSpacing(15);

  QLabel *conflictLabel = new QLabel("Если выходной файл уже существует:");
  optionsLayout->addWidget(conflictLabel);

  m_overwriteRadio = new QRadioButton("Перезаписать");
  m_overwriteRadio->setToolTip(
      "Перезаписать существующий выходной файл, НО только если входной файл изменился.\n"
      "Проверяется SHA-256 хеш: если файл с таким хешем уже обработан — пропускаем.\n"
      "Если хеш изменился — перезаписываем.");
  optionsLayout->addWidget(m_overwriteRadio);

  m_skipRadio = new QRadioButton("Пропустить");
  m_skipRadio->setChecked(true);
  m_skipRadio->setToolTip(
      "Пропустить файл, если выходной файл с таким именем уже существует.\n"
      "Хеш НЕ проверяется — смотрим только на имя файла.\n"
      "Быстрее, но менее надёжно.");
  optionsLayout->addWidget(m_skipRadio);

  m_numberingRadio = new QRadioButton("Добавить счётчик");
  m_numberingRadio->setToolTip(
      "Создать новый файл с уникальным именем: file_1.bin.xor, file_2.bin.xor и т.д.\n"
      "Всегда обрабатываем, хеш НЕ проверяем, в кеш НЕ добавляем.\n"
      "Создаёт копии, занимает больше места.");
  optionsLayout->addWidget(m_numberingRadio);
  optionsLayout->addStretch();
  paramsLayout->addLayout(optionsLayout);

  mainLayout->addWidget(paramsGroup);

  // Группа: Режим запуска
  QGroupBox *modeGroup = new QGroupBox("Режим запуска");
  QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

  m_manualRadio = new QRadioButton("Ручной запуск");
  m_manualRadio->setToolTip("Обработка начнётся только после нажатия кнопки Старт");
  m_manualRadio->setChecked(true);
  modeLayout->addWidget(m_manualRadio);

  // Таймер
  QHBoxLayout *timerLayout = new QHBoxLayout();
  m_timerRadio = new QRadioButton("По таймеру: каждые");
  m_timerRadio->setToolTip("Программа будет автоматически запускать обработку каждые N секунд/миллисекунд");
  timerLayout->addWidget(m_timerRadio);

  m_timerIntervalSpin = new QSpinBox();
  m_timerIntervalSpin->setRange(1, 3600000); // до 1 часа
  m_timerIntervalSpin->setValue(60000);      // 60 секунд по умолчанию
  m_timerIntervalSpin->setSuffix(" мс");
  m_timerIntervalSpin->setToolTip("Интервал в миллисекундах (1000 мс = 1 секунда)");
  timerLayout->addWidget(m_timerIntervalSpin);

  // Добавим быстрые пресеты
  QComboBox *timerPreset = new QComboBox();
  timerPreset->addItem("1 сек", 1000);
  timerPreset->addItem("5 сек", 5000);
  timerPreset->addItem("30 сек", 30000);
  timerPreset->addItem("1 мин", 60000);
  timerPreset->addItem("5 мин", 300000);
  timerPreset->setToolTip("Быстрый выбор интервала");
  connect(timerPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, timerPreset](int index)
          { m_timerIntervalSpin->setValue(timerPreset->itemData(index).toInt()); });
  timerLayout->addWidget(timerPreset);

  timerLayout->addStretch();
  modeLayout->addLayout(timerLayout);

  m_watcherRadio = new QRadioButton("Отслеживание изменений (реальное время)");
  m_watcherRadio->setToolTip("Программа автоматически обрабатывает файлы сразу после их появления или изменения в входной директории");
  modeLayout->addWidget(m_watcherRadio);

  mainLayout->addWidget(modeGroup);

  // Группа: Управление
  QGroupBox *controlGroup = new QGroupBox("Управление");
  QVBoxLayout *controlLayout = new QVBoxLayout(controlGroup);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->setSpacing(10);

  m_startButton = new QPushButton("Старт");
  m_startButton->setMinimumHeight(35);
  m_startButton->setStyleSheet("QPushButton { font-weight: bold; }");

  m_pauseButton = new QPushButton("Пауза");
  m_pauseButton->setMinimumHeight(35);
  m_pauseButton->setEnabled(false);

  m_stopButton = new QPushButton("Стоп");
  m_stopButton->setMinimumHeight(35);
  m_stopButton->setEnabled(false);
  m_stopButton->setStyleSheet("QPushButton { color: #cc0000; }");

  buttonLayout->addWidget(m_startButton);
  buttonLayout->addWidget(m_pauseButton);
  buttonLayout->addWidget(m_stopButton);
  controlLayout->addLayout(buttonLayout);

  m_progressBar = new QProgressBar();
  m_progressBar->setTextVisible(true);
  m_progressBar->setFormat("%p%");
  controlLayout->addWidget(m_progressBar);

  m_fileStatusLabel = new QLabel("");
  m_fileStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; padding: 2px; }");
  controlLayout->addWidget(m_fileStatusLabel);

  mainLayout->addWidget(controlGroup);

  // Группа: Статистика
  QGroupBox *statsGroup = new QGroupBox("Статистика");
  QVBoxLayout *statsLayout = new QVBoxLayout(statsGroup);

  m_statsLabel = new QLabel("Обработано: 0 | Пропущено: 0 | Осталось: 0");
  m_statsLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12px; }");
  statsLayout->addWidget(m_statsLabel);

  // Кеш
  QHBoxLayout *cacheLayout = new QHBoxLayout();
  QLabel *cacheLabel = new QLabel("Кеш обработанных файлов:");
  cacheLayout->addWidget(cacheLabel);

  m_checkCacheBtn = new QPushButton("Проверить кеш");
  m_checkCacheBtn->setToolTip("Просканировать выходную директорию и показать информацию о ранее обработанных файлах");
  cacheLayout->addWidget(m_checkCacheBtn);

  m_clearCacheBtn = new QPushButton("Очистить кеш");
  m_clearCacheBtn->setToolTip("Удалить информацию о ранее обработанных файлах. После очистки все файлы будут обработаны заново");
  cacheLayout->addWidget(m_clearCacheBtn);
  cacheLayout->addStretch();
  statsLayout->addLayout(cacheLayout);

  m_cacheInfoLabel = new QLabel("Кеш не проверен. Нажмите 'Проверить кеш' для получения информации.");
  m_cacheInfoLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
  statsLayout->addWidget(m_cacheInfoLabel);

  mainLayout->addWidget(statsGroup);

  // Группа: Список файлов
  QGroupBox *filesGroup = new QGroupBox("Список файлов");
  QVBoxLayout *filesLayout = new QVBoxLayout(filesGroup);

  m_fileTable = new QTableWidget();
  m_fileTable->setColumnCount(3);
  m_fileTable->setHorizontalHeaderLabels({"Имя файла", "Статус", "Прогресс"});
  m_fileTable->horizontalHeader()->setStretchLastSection(true);
  m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_fileTable->verticalHeader()->setVisible(false);
  filesLayout->addWidget(m_fileTable);

  mainLayout->addWidget(filesGroup);

  // Статус
  m_statusLabel = new QLabel("Готов к работе");
  m_statusLabel->setStyleSheet("QLabel { font-weight: bold; padding: 5px; background-color: #e8f5e9; border-radius: 3px; }");
  mainLayout->addWidget(m_statusLabel);

  // qDebug() << "=== UI Initialization Check ===";
  // qDebug() << "m_inputDirEdit:" << (!m_inputDirEdit.isNull() ? "OK" : "NULL");
  // qDebug() << "m_outputDirEdit:" << (!m_outputDirEdit.isNull() ? "OK" : "NULL");
  // qDebug() << "m_fileMaskEdit:" << (!m_fileMaskEdit.isNull() ? "OK" : "NULL");
  // qDebug() << "m_xorKeyEdit:" << (!m_xorKeyEdit.isNull() ? "OK" : "NULL");
  // qDebug() << "m_xorKeyStatusLabel:" << (!m_xorKeyStatusLabel.isNull() ? "OK" : "NULL");
  // qDebug() << "m_deleteInputCheck:" << (!m_deleteInputCheck.isNull() ? "OK" : "NULL");
  // qDebug() << "m_overwriteRadio:" << (!m_overwriteRadio.isNull() ? "OK" : "NULL");
  // qDebug() << "m_skipRadio:" << (!m_skipRadio.isNull() ? "OK" : "NULL");
  // qDebug() << "m_numberingRadio:" << (!m_numberingRadio.isNull() ? "OK" : "NULL");
  // qDebug() << "m_manualRadio:" << (!m_manualRadio.isNull() ? "OK" : "NULL");
  // qDebug() << "m_timerRadio:" << (!m_timerRadio.isNull() ? "OK" : "NULL");
  // qDebug() << "m_watcherRadio:" << (!m_watcherRadio.isNull() ? "OK" : "NULL");
  // qDebug() << "m_timerIntervalSpin:" << (!m_timerIntervalSpin.isNull() ? "OK" : "NULL");
  // qDebug() << "m_browseInputBtn:" << (!m_browseInputBtn.isNull() ? "OK" : "NULL");
  // qDebug() << "m_browseOutputBtn:" << (!m_browseOutputBtn.isNull() ? "OK" : "NULL");
  // qDebug() << "m_startButton:" << (!m_startButton.isNull() ? "OK" : "NULL");
  // qDebug() << "m_pauseButton:" << (!m_pauseButton.isNull() ? "OK" : "NULL");
  // qDebug() << "m_stopButton:" << (!m_stopButton.isNull() ? "OK" : "NULL");
  // qDebug() << "m_clearCacheBtn:" << (!m_clearCacheBtn.isNull() ? "OK" : "NULL");
  // qDebug() << "m_checkCacheBtn:" << (!m_checkCacheBtn.isNull() ? "OK" : "NULL");
  // qDebug() << "m_progressBar:" << (!m_progressBar.isNull() ? "OK" : "NULL");
  // qDebug() << "m_fileStatusLabel:" << (!m_fileStatusLabel.isNull() ? "OK" : "NULL");
  // qDebug() << "m_statusLabel:" << (!m_statusLabel.isNull() ? "OK" : "NULL");
  // qDebug() << "m_statsLabel:" << (!m_statsLabel.isNull() ? "OK" : "NULL");
  // qDebug() << "m_cacheInfoLabel:" << (!m_cacheInfoLabel.isNull() ? "OK" : "NULL");
  // qDebug() << "m_fileTable:" << (!m_fileTable.isNull() ? "OK" : "NULL");
  // qDebug() << "==============================";

  setWindowTitle("FileXorTool");
  resize(800, 900);
}

void MainWindow::setupConnections()
{
  connect(m_browseInputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseInput);
  connect(m_browseOutputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
  connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
  connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);
  connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
  connect(m_clearCacheBtn, &QPushButton::clicked, this, &MainWindow::onClearCache);
  connect(m_checkCacheBtn, &QPushButton::clicked, this, &MainWindow::onCheckCache);
  connect(m_xorKeyEdit, &QLineEdit::textChanged, this, &MainWindow::onXorKeyChanged);
  connect(m_timerRadio, &QRadioButton::toggled, this, &MainWindow::onTimerToggled);
  connect(m_watcherRadio, &QRadioButton::toggled, this, &MainWindow::onWatcherToggled);

  connect(m_worker, &Worker::progressChanged, this, &MainWindow::onWorkerProgressChanged);
  connect(m_worker, &Worker::fileProgressChanged, this, &MainWindow::onWorkerFileProgressChanged);
  connect(m_worker, &Worker::statusMessage, this, &MainWindow::onWorkerStatusMessage);
  connect(m_worker, &Worker::finished, this, &MainWindow::onWorkerFinished);
  connect(m_worker, &Worker::fileStarted, this, &MainWindow::onWorkerFileStarted);
  connect(m_worker, &Worker::fileFinished, this, &MainWindow::onWorkerFileFinished);
  connect(m_worker, &Worker::fileSkipped, this, &MainWindow::onWorkerFileSkipped);

  connect(m_worker, &Worker::totalFilesCount, this, [this](int count)
          {
        if (!this || m_statsLabel.isNull() || m_fileTable.isNull()) return;
        m_totalFiles = count;
        m_processedCount = 0;
        m_skippedCount = 0;
        m_statsLabel->setText(QString("Обработано: 0 | Пропущено: 0 | Осталось: %1").arg(count));
        m_fileTable->setRowCount(0);
        m_fileRowMap.clear(); }, Qt::QueuedConnection);

  connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onDirectoryChanged);
  connect(m_timer, &QTimer::timeout, this, &MainWindow::onStart);

  connect(m_worker, &Worker::finished, this, &MainWindow::onWorkerFinished);
  connect(m_worker, &Worker::progressChanged, this, [this](qint64 processed, qint64 total)
          {
    if (total > 0) {
        int percent = static_cast<int>((processed * 100) / total);
        m_progressBar->setValue(percent);
        m_progressBar->setFormat(QString("%1% (%2/%3 MB)").arg(percent)
            .arg(processed / (1024 * 1024))
            .arg(total / (1024 * 1024)));
    } });
}

void MainWindow::onXorKeyChanged(const QString &text)
{
  updateXorKeyValidation();
}

void MainWindow::updateXorKeyValidation()
{
  QString key = m_xorKeyEdit->text();
  int len = key.length();

  m_xorKeyStatusLabel->setText(QString("%1/16").arg(len));

  if (len == 16)
  {
    m_xorKeyStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
  }
  else
  {
    m_xorKeyStatusLabel->setStyleSheet("QLabel { color: #999; }");
  }
}

void MainWindow::onClearCache()
{
  QString outputDir = m_outputDirEdit->text();
  if (outputDir.isEmpty())
  {
    QMessageBox::warning(this, "Ошибка", "Укажите выходную директорию");
    return;
  }

  QString cacheFile = outputDir + "/.xor_tool_cache.json";
  qDebug() << "Clearing cache file:" << cacheFile;

  if (QFile::exists(cacheFile))
  {
    bool removed = QFile::remove(cacheFile);
    qDebug() << "Cache file removed:" << removed;

    if (removed)
    {
      QMessageBox::information(this, "Кеш очищен", "Информация о обработанных файлах удалена");
      m_cacheInfoLabel->setText("Кеш очищен");
    }
    else
    {
      QMessageBox::warning(this, "Ошибка", "Не удалось удалить файл кеша");
    }
  }
  else
  {
    QMessageBox::information(this, "Кеш пуст", "Кеш уже пуст");
    m_cacheInfoLabel->setText("Кеш пуст");
  }
}

void MainWindow::onCheckCache()
{
  QString outputDir = m_outputDirEdit->text();
  if (outputDir.isEmpty())
  {
    QMessageBox::warning(this, "Ошибка", "Укажите выходную директорию");
    return;
  }

  QDir dir(outputDir);
  if (!dir.exists())
  {
    m_cacheInfoLabel->setText("Выходная директория не существует");
    m_cacheInfoLabel->setStyleSheet("QLabel { color: #cc0000; }");
    return;
  }

  QString cacheFile = outputDir + "/.xor_tool_cache.json";
  if (!QFile::exists(cacheFile))
  {
    m_cacheInfoLabel->setText("Кеш пуст. Файлы ещё не обрабатывались.");
    m_cacheInfoLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    return;
  }

  // Загружаем кеш и считаем записи
  QFile file(cacheFile);
  if (!file.open(QIODevice::ReadOnly))
  {
    m_cacheInfoLabel->setText("Ошибка чтения кеша");
    m_cacheInfoLabel->setStyleSheet("QLabel { color: #cc0000; }");
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isObject())
  {
    m_cacheInfoLabel->setText("Кеш повреждён");
    m_cacheInfoLabel->setStyleSheet("QLabel { color: #cc0000; }");
    return;
  }

  QJsonObject cacheData = doc.object();
  int totalEntries = cacheData.size();
  int validEntries = 0;

  // Проверяем, какие файлы из кеша ещё существуют
  for (auto it = cacheData.begin(); it != cacheData.end(); ++it)
  {
    QJsonObject entry = it.value().toObject();
    QString outputPath = entry["output_path"].toString();
    if (QFile::exists(outputPath))
    {
      validEntries++;
    }
  }

  m_cacheInfoLabel->setText(QString("В кеше: %1 записей | Актуальных файлов: %2")
                                .arg(totalEntries)
                                .arg(validEntries));
  m_cacheInfoLabel->setStyleSheet("QLabel { color: #2e7d32; font-weight: bold; }");
}

void MainWindow::updateFileTable(const QString &fileName, const QString &status, int progress)
{
  if (!m_fileRowMap.contains(fileName))
  {
    // Файл не в таблице — добавляем (для watcher режима)
    int row = m_fileTable->rowCount();
    m_fileTable->insertRow(row);
    m_fileTable->setItem(row, 0, new QTableWidgetItem(fileName));
    m_fileTable->setItem(row, 1, new QTableWidgetItem(status));
    m_fileTable->setItem(row, 2, new QTableWidgetItem(""));
    m_fileRowMap[fileName] = row;
  }
  else
  {
    int row = m_fileRowMap[fileName];
    m_fileTable->item(row, 1)->setText(status);
  }

  if (progress >= 0)
  {
    int row = m_fileRowMap[fileName];
    m_fileTable->item(row, 2)->setText(QString("%1%").arg(progress));
  }
}

void MainWindow::onWorkerFileStarted(const QString &fileName, qint64 fileSize)
{
  updateFileTable(fileName, "Обработка...", 0);
}

void MainWindow::onWorkerFileFinished(const QString &fileName, bool success)
{
  qDebug() << "MainWindow::onWorkerFileFinished() called for" << fileName;

  m_processedCount++;
  int remaining = m_totalFiles - m_processedCount - m_skippedCount;

  if (remaining < 0)
    remaining = 0;

  m_statsLabel->setText(QString("Обработано: %1 | Пропущено: %2 | Осталось: %3")
                            .arg(m_processedCount)
                            .arg(m_skippedCount)
                            .arg(remaining));

  if (success)
  {
    updateFileTable(fileName, "Готово", 100);
  }
  else
  {
    updateFileTable(fileName, "Ошибка", -1);
  }
}

void MainWindow::onWorkerFileSkipped(const QString &fileName)
{
  qDebug() << "MainWindow::onWorkerFileSkipped() called for" << fileName;

  m_skippedCount++;
  int remaining = m_totalFiles - m_processedCount - m_skippedCount;

  if (remaining < 0)
    remaining = 0;

  m_statsLabel->setText(QString("Обработано: %1 | Пропущено: %2 | Осталось: %3")
                            .arg(m_processedCount)
                            .arg(m_skippedCount)
                            .arg(remaining));

  updateFileTable(fileName, "Пропущен (кеш)", -1);
}

void MainWindow::onWorkerFileProgressChanged(const QString &fileName, qint64 processed, qint64 total)
{
  if (total > 0)
  {
    int percent = static_cast<int>((processed * 100) / total);
    updateFileTable(fileName, "Обработка...", percent);
  }
}

void MainWindow::onBrowseInput()
{
  QString dir = QFileDialog::getExistingDirectory(this, "Выберите входную директорию");
  if (!dir.isEmpty())
  {
    m_inputDirEdit->setText(dir);
  }
}

void MainWindow::onBrowseOutput()
{
  QString dir = QFileDialog::getExistingDirectory(this, "Выберите выходную директорию");
  if (!dir.isEmpty())
  {
    m_outputDirEdit->setText(dir);
  }
}

void MainWindow::onStart()
{
  qDebug() << "=== MainWindow::onStart() called ===";

  if (m_isProcessing)
  {
    qDebug() << "Already processing, ignoring";
    return;
  }

  if (m_fileStatusLabel.isNull() || m_statusLabel.isNull() ||
      m_progressBar.isNull() || m_fileTable.isNull() || m_statsLabel.isNull())
  {
    qCritical() << "UI elements not initialized!";
    return;
  }

  if (m_inputDirEdit->text().isEmpty() || m_outputDirEdit->text().isEmpty())
  {
    QMessageBox::warning(this, "Ошибка", "Укажите входную и выходную директории");
    return;
  }

  QByteArray xorKey = HexParser::parseHexKey(m_xorKeyEdit->text());
  if (xorKey.isEmpty())
  {
    QMessageBox::warning(this, "Ошибка", "Некорректный XOR ключ (16 hex-символов)");
    return;
  }

  ConflictMode conflictMode = m_overwriteRadio->isChecked() ? ConflictMode::Overwrite : m_skipRadio->isChecked() ? ConflictMode::Skip
                                                                                                                 : ConflictMode::Numbering;

  // MainWindow только передаёт параметры, Worker создаёт операцию
  m_worker->setConfig(
      m_inputDirEdit->text(),
      m_outputDirEdit->text(),
      m_fileMaskEdit->text(),
      "XOR",  // Название операции
      xorKey, // Параметры (ключ для XOR)
      m_deleteInputCheck->isChecked(),
      conflictMode,
      8);

  // Собираем файлы
  QStringList filesToProcess;
  if (m_watcherRadio->isChecked() && !m_fileQueue.isEmpty())
  {
    QMutexLocker locker(&m_queueMutex);
    filesToProcess = m_fileQueue;
    m_fileQueue.clear();
  }
  else
  {
    QDir dir(m_inputDirEdit->text());
    QStringList filters;
    filters << m_fileMaskEdit->text();
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fi : fileList)
    {
      filesToProcess << fi.absoluteFilePath();
    }
  }

  if (filesToProcess.isEmpty())
  {
    m_statusLabel->setText("Нет файлов для обработки");
    return;
  }

  m_worker->setFilesToProcess(filesToProcess);

  // Сброс счётчиков
  m_processedCount = 0;
  m_skippedCount = 0;
  m_totalFiles = filesToProcess.size();

  // ВАЖНО: Заполняем таблицу ВСЕМИ файлами СРАЗУ
  qDebug() << "Filling table with" << filesToProcess.size() << "files";

  // Заполняем таблицу
  m_fileTable->setRowCount(0);
  m_fileRowMap.clear();

  for (const QString &filePath : filesToProcess)
  {
    QFileInfo fileInfo(filePath);
    int row = m_fileTable->rowCount();
    m_fileTable->insertRow(row);
    m_fileTable->setItem(row, 0, new QTableWidgetItem(fileInfo.fileName()));
    m_fileTable->setItem(row, 1, new QTableWidgetItem("Ожидание"));
    m_fileTable->setItem(row, 2, new QTableWidgetItem("0%"));
    m_fileRowMap[fileInfo.fileName()] = row;
  }

  qDebug() << "Table filled successfully";

  m_statsLabel->setText(QString("Обработано: 0 | Пропущено: 0 | Осталось: %1").arg(m_totalFiles));

  m_isProcessing = true;
  m_startButton->setEnabled(false);
  m_pauseButton->setEnabled(true);
  m_stopButton->setEnabled(true);
  m_progressBar->setValue(0);
  m_fileStatusLabel->clear();
  m_statusLabel->setText("Обработка запущена...");

  // Запускаем таймер если нужно
  if (m_timerRadio->isChecked() && !m_timer->isActive())
  {
    int interval = m_timerIntervalSpin->value();
    m_timer->setInterval(interval);
    m_timer->start();
    qDebug() << "Timer started with interval:" << interval << "ms";
  }

  // Запускаем поток
  if (!m_workerThread->isRunning())
  {
    m_workerThread->start();
  }

  QMetaObject::invokeMethod(m_worker, "processFiles", Qt::QueuedConnection);

  qDebug() << "=== MainWindow::onStart() completed ===";
}

void MainWindow::onPauseResume()
{
  if (!m_isProcessing)
  {
    return;
  }

  if (m_pauseButton->text() == "Пауза")
  {
    m_worker->pause();
    m_pauseButton->setText("Продолжить");
    m_statusLabel->setText("Пауза");
  }
  else
  {
    m_worker->resume();
    m_pauseButton->setText("Пауза");
    m_statusLabel->setText("Возобновление...");
  }
}

void MainWindow::onStop()
{
  qDebug() << "MainWindow::onStop() called";

  // Останавливаем обработку, если идёт
  if (m_isProcessing)
  {
    m_worker->stop();
  }

  // Останавливаем таймер
  if (m_timer->isActive())
  {
    m_timer->stop();
    qDebug() << "Timer stopped";
  }

  // Отключаем watcher
  if (m_watcherActive)
  {
    m_watcher->removePaths(m_watcher->directories());
    m_watcher->removePaths(m_watcher->files());
    m_watcherActive = false;
    qDebug() << "Watcher stopped";
  }

  // Сбрасываем UI
  m_isProcessing = false;
  m_startButton->setEnabled(true);
  m_pauseButton->setEnabled(false);
  m_pauseButton->setText("Пауза");
  m_stopButton->setEnabled(false);

  m_statusLabel->setText("Остановлено");

  qDebug() << "MainWindow::onStop() completed";
}

void MainWindow::onTimerToggled(bool checked)
{
  if (checked)
  {
    // Отключаем watcher
    if (m_watcherActive)
    {
      m_watcher->removePaths(m_watcher->directories());
      m_watcher->removePaths(m_watcher->files());
      m_watcherActive = false;
    }
  }

  // Обновляем состояние кнопки Стоп
  bool shouldBeEnabled = m_isProcessing || m_timer->isActive() || m_watcherActive;
  m_stopButton->setEnabled(shouldBeEnabled);
}

void MainWindow::onWatcherToggled(bool checked)
{
  if (checked)
  {
    m_timer->stop();

    if (!m_inputDirEdit->text().isEmpty())
    {
      m_watcher->addPath(m_inputDirEdit->text());
      m_watcherActive = true;
      m_statusLabel->setText("Отслеживание изменений активно");
    }
  }
  else
  {
    if (m_watcherActive)
    {
      m_watcher->removePaths(m_watcher->directories());
      m_watcher->removePaths(m_watcher->files());
      m_watcherActive = false;
    }
  }

  // Обновляем состояние кнопки Стоп
  bool shouldBeEnabled = m_isProcessing || m_timer->isActive() || m_watcherActive;
  m_stopButton->setEnabled(shouldBeEnabled);
}

void MainWindow::onFileChanged(const QString &path)
{
  QFileInfo fileInfo(path);
  QString mask = m_fileMaskEdit->text();
  QRegularExpression regex(mask.replace("*", ".*").replace("?", "."));

  if (regex.match(fileInfo.fileName()).hasMatch())
  {
    QMutexLocker locker(&m_queueMutex);
    if (!m_fileQueue.contains(path))
    {
      m_fileQueue.enqueue(path);
      m_statusLabel->setText("Обнаружено изменение: " + fileInfo.fileName());

      if (!m_isProcessing)
      {
        QTimer::singleShot(500, this, &MainWindow::onStart);
      }
    }
  }

  if (QFile::exists(path))
  {
    m_watcher->addPath(path);
  }
}

void MainWindow::onDirectoryChanged(const QString &path)
{
  QDir dir(path);
  QStringList filters;
  filters << m_fileMaskEdit->text();
  QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

  QMutexLocker locker(&m_queueMutex);
  for (const QFileInfo &fi : fileList)
  {
    if (!m_fileQueue.contains(fi.absoluteFilePath()))
    {
      m_fileQueue.enqueue(fi.absoluteFilePath());
    }
  }

  if (!m_fileQueue.isEmpty() && !m_isProcessing)
  {
    QTimer::singleShot(500, this, &MainWindow::onStart);
  }
}

void MainWindow::onWorkerProgressChanged(qint64 processed, qint64 total)
{
  if (total > 0)
  {
    m_progressBar->setValue(static_cast<int>((processed * 100) / total));
  }
}

void MainWindow::onWorkerStatusMessage(const QString &msg)
{
  m_statusLabel->setText(msg);
}

void MainWindow::onWorkerFinished()
{
  qDebug() << "MainWindow::onWorkerFinished() called";

  m_isProcessing = false;
  m_startButton->setEnabled(true);
  m_pauseButton->setEnabled(false);
  m_pauseButton->setText("Пауза");

  // Стоп активен, только если таймер/watcher тоже остановлены
  bool timerOrWatcherActive = (m_timerRadio->isChecked() && m_timer->isActive()) ||
                              (m_watcherRadio->isChecked() && m_watcherActive);
  m_stopButton->setEnabled(timerOrWatcherActive);

  disconnect(m_workerThread, &QThread::started, m_worker, &Worker::processFiles);

  if (m_timerRadio->isChecked() && m_timer->isActive())
  {
    m_statusLabel->setText(QString("Таймер активен. Следующий запуск через %1 сек")
                               .arg(m_timerIntervalSpin->value()));
  }
  else if (m_watcherRadio->isChecked() && m_watcherActive)
  {
    m_statusLabel->setText("Отслеживание изменений активно");
  }
  else
  {
    m_statusLabel->setText("Обработка завершена");
  }

  m_progressBar->setValue(100);
  m_progressBar->setFormat("%p%");

  qDebug() << "MainWindow::onWorkerFinished() completed";
  emit workerFinished();
}