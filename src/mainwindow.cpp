#include "mainwindow.h"
#include <QCloseEvent>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_isProcessing(false)
{
  setWindowTitle("FileXorTool");
  resize(600, 500);
  createUI();

  m_workerThread = new QThread(this);
  m_worker = new Worker();
  m_worker->moveToThread(m_workerThread);

  m_timer = new QTimer(this);
  m_timer->setSingleShot(false);

  // Связи сигналов worker
  connect(m_worker, &Worker::progressChanged,
          this, &MainWindow::onWorkerProgressChanged);
  connect(m_worker, &Worker::fileProgressChanged,
          this, &MainWindow::onWorkerFileProgressChanged);
  connect(m_worker, &Worker::statusMessage,
          this, &MainWindow::onWorkerStatusMessage);
  connect(m_worker, &Worker::finished,
          this, &MainWindow::onWorkerFinished);

  // Таймер
  connect(m_timer, &QTimer::timeout, this, [this]()
          { startProcessing(); });
}

MainWindow::~MainWindow()
{
  if (m_workerThread->isRunning())
  {
    m_worker->stop();
    m_workerThread->wait(3000);
  }
}

void MainWindow::createUI()
{
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  // Группа настроек файлов
  QGroupBox *fileGroup = new QGroupBox("Настройки файлов");
  QFormLayout *fileLayout = new QFormLayout(fileGroup);

  m_inputDirEdit = new QLineEdit();
  m_inputDirEdit->setPlaceholderText("Путь для поиска файлов");
  QPushButton *browseInputBtn = new QPushButton("Обзор...");
  connect(browseInputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseInput);
  QHBoxLayout *inputLayout = new QHBoxLayout();
  inputLayout->addWidget(m_inputDirEdit);
  inputLayout->addWidget(browseInputBtn);
  fileLayout->addRow("Входная директория:", inputLayout);

  m_outputDirEdit = new QLineEdit();
  m_outputDirEdit->setPlaceholderText("Путь для сохранения");
  QPushButton *browseOutputBtn = new QPushButton("Обзор...");
  connect(browseOutputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
  QHBoxLayout *outputLayout = new QHBoxLayout();
  outputLayout->addWidget(m_outputDirEdit);
  outputLayout->addWidget(browseOutputBtn);
  fileLayout->addRow("Выходная директория:", outputLayout);

  m_fileMaskEdit = new QLineEdit("*.bin");
  fileLayout->addRow("Маска файлов:", m_fileMaskEdit);

  m_deleteInputCheck = new QCheckBox("Удалять входные файлы");
  fileLayout->addRow(m_deleteInputCheck);

  mainLayout->addWidget(fileGroup);

  // Группа настроек обработки
  QGroupBox *processGroup = new QGroupBox("Настройки обработки");
  QFormLayout *processLayout = new QFormLayout(processGroup);

  m_xorKeyEdit = new QLineEdit("1234567890ABCDEF");
  m_xorKeyEdit->setPlaceholderText("8 байт в hex (16 символов)");
  processLayout->addRow("XOR ключ (hex):", m_xorKeyEdit);

  QGroupBox *collisionGroup = new QGroupBox("При повторении имени:");
  QVBoxLayout *collisionLayout = new QVBoxLayout(collisionGroup);
  m_overwriteRadio = new QRadioButton("Перезапись");
  m_counterRadio = new QRadioButton("Добавить счетчик");
  m_overwriteRadio->setChecked(true);
  collisionLayout->addWidget(m_overwriteRadio);
  collisionLayout->addWidget(m_counterRadio);
  processLayout->addRow(collisionGroup);

  mainLayout->addWidget(processGroup);

  // Группа режима работы
  QGroupBox *modeGroup = new QGroupBox("Режим работы");
  QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

  m_onceRadio = new QRadioButton("Разовый запуск");
  m_timerRadio = new QRadioButton("По таймеру");
  m_onceRadio->setChecked(true);
  modeLayout->addWidget(m_onceRadio);
  modeLayout->addWidget(m_timerRadio);

  QHBoxLayout *timerLayout = new QHBoxLayout();
  m_timerIntervalSpin = new QSpinBox();
  m_timerIntervalSpin->setRange(1, 3600);
  m_timerIntervalSpin->setValue(10);
  m_timerIntervalSpin->setSuffix(" сек");
  m_timerIntervalSpin->setEnabled(false);
  timerLayout->addWidget(new QLabel("Периодичность:"));
  timerLayout->addWidget(m_timerIntervalSpin);
  modeLayout->addLayout(timerLayout);

  connect(m_timerRadio, &QRadioButton::toggled, this, &MainWindow::onTimerToggled);

  mainLayout->addWidget(modeGroup);

  // Кнопки управления
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  m_startButton = new QPushButton("Старт");
  m_pauseButton = new QPushButton("Пауза");
  m_pauseButton->setEnabled(false);
  buttonLayout->addWidget(m_startButton);
  buttonLayout->addWidget(m_pauseButton);
  mainLayout->addLayout(buttonLayout);

  connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartStop);
  connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);

  // Прогресс и статус
  m_progressBar = new QProgressBar();
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  mainLayout->addWidget(m_progressBar);

  m_statusLabel = new QLabel("Готов к работе");
  mainLayout->addWidget(m_statusLabel);

  m_fileStatusLabel = new QLabel("");
  mainLayout->addWidget(m_fileStatusLabel);

  mainLayout->addStretch();
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

void MainWindow::onTimerToggled(bool checked)
{
  m_timerIntervalSpin->setEnabled(checked);
}

QByteArray MainWindow::parseHexKey(const QString &hex)
{
  return HexParser::parseHexKey(hex);
}

void MainWindow::startProcessing()
{
  if (m_isProcessing)
  {
    return;
  }

  // Валидация
  if (m_inputDirEdit->text().isEmpty() || m_outputDirEdit->text().isEmpty())
  {
    QMessageBox::warning(this, "Ошибка", "Укажите входную и выходную директории");
    return;
  }

  QByteArray xorKey = parseHexKey(m_xorKeyEdit->text());
  if (xorKey.isEmpty())
  {
    QMessageBox::warning(this, "Ошибка", "Некорректный XOR ключ. Нужно 8 байт в hex (16 символов)");
    return;
  }

  // Настройка worker
  m_worker->setConfig(
      m_inputDirEdit->text(),
      m_outputDirEdit->text(),
      m_fileMaskEdit->text(),
      xorKey,
      m_deleteInputCheck->isChecked(),
      m_overwriteRadio->isChecked());

  // UI обновления
  m_isProcessing = true;
  m_startButton->setText("Стоп");
  m_pauseButton->setEnabled(true);
  m_progressBar->setValue(0);
  m_fileStatusLabel->setText("");

  // Запуск потока
  connect(m_workerThread, &QThread::started, m_worker, &Worker::processFiles);
  m_workerThread->start();
}

void MainWindow::stopProcessing()
{
  m_worker->stop();
  m_workerThread->wait(3000);

  m_isProcessing = false;
  m_startButton->setText("Старт");
  m_pauseButton->setEnabled(false);
  m_pauseButton->setText("Пауза");
}

void MainWindow::onStartStop()
{
  if (m_isProcessing)
  {
    stopProcessing();
  }
  else
  {
    if (m_timerRadio->isChecked())
    {
      int interval = m_timerIntervalSpin->value() * 1000;
      m_timer->setInterval(interval);
      m_timer->start();
      m_statusLabel->setText("Таймер запущен (интервал: " +
                             QString::number(m_timerIntervalSpin->value()) + " сек)");
    }
    startProcessing();
  }
}

void MainWindow::onPauseResume()
{
  if (!m_isProcessing)
  {
    return;
  }

  // Проверяем текущее состояние через статус
  if (m_pauseButton->text() == "Пауза")
  {
    m_worker->pause();
    m_pauseButton->setText("Возобновить");
    m_statusLabel->setText("Пауза");
  }
  else
  {
    m_worker->resume();
    m_pauseButton->setText("Пауза");
    m_statusLabel->setText("Возобновление...");
  }
}

void MainWindow::onWorkerProgressChanged(qint64 processed, qint64 total)
{
  if (total > 0)
  {
    int percent = static_cast<int>((processed * 100) / total);
    m_progressBar->setValue(percent);
  }
}

void MainWindow::onWorkerFileProgressChanged(const QString &fileName, qint64 processed, qint64 total)
{
  QString processedMB = QString::number(processed / (1024 * 1024));
  QString totalMB = QString::number(total / (1024 * 1024));
  m_fileStatusLabel->setText("Файл: " + fileName + " (" + processedMB + " / " + totalMB + " MB)");
}

void MainWindow::onWorkerStatusMessage(const QString &msg)
{
  m_statusLabel->setText(msg);
}

void MainWindow::onWorkerFinished()
{
  m_isProcessing = false;
  m_startButton->setText("Старт");
  m_pauseButton->setEnabled(false);
  m_pauseButton->setText("Пауза");

  // Отключаем сигнал started, чтобы не запускался повторно
  disconnect(m_workerThread, &QThread::started, m_worker, &Worker::processFiles);

  if (!m_timerRadio->isChecked() || !m_timer->isActive())
  {
    m_statusLabel->setText("Обработка завершена");
  }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (m_isProcessing)
  {
    m_worker->stop();
    m_timer->stop();
    m_workerThread->wait(3000);
  }
  event->accept();
}