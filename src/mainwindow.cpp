#include "mainwindow.h"
#include "utils/hdr.h"
#include "utils/utils.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  // Ui config
  ui->setupUi(this);
  QMenuBar *mBar = menuBar();
  setMenuBar(mBar);
  setWindowTitle(tr("Wirecat"));

  QTableView *table = ui->tableView;
  QTextBrowser *text = ui->textBrowser;
  QTreeView *tree = ui->treeView;
  view = new View(table, text, tree);

  // variables
  sniffer = new Sniffer();
  sniffer->getView(view);

  // filter
  filter = new Filter();
  connect(this->ui->filter_rule, SIGNAL(textChanged(const QString &)), this,
          SLOT(on_filter_textChanged(const QString &)));
  connect(this->ui->filterButton, SIGNAL(clicked()), this,
          SLOT(on_filter_Pressed()));

  // Device choice
  DevWindow *devwindow = new DevWindow(sniffer, this);
  devwindow->show();
  connect(devwindow, SIGNAL(subWndClosed()), this, SLOT(showMainWnd()));

  // catch thread
  cthread = new QThread;
  sniffer->moveToThread(cthread);
  cthread->start();
  connect(this, SIGNAL(sig()), sniffer, SLOT(sniff()));
}

MainWindow::~MainWindow() {
  delete ui;
  delete filter;
}

// SLOT function
void MainWindow::showMainWnd() {
  LOG(sniffer->dev);
  char errbuf[PCAP_ERRBUF_SIZE];
  sniffer->handle = pcap_open_live(sniffer->dev, BUFSIZ, -1, 1000, errbuf);
  if (sniffer->handle == NULL) {
    ERROR_INFO(errbuf);
    exit(1);
  }

  emit sig();
  this->show();
}

void MainWindow::start_catch() {
  LOG("Start");
  sniffer->status = Start;
}

void MainWindow::stop_catch() {
  LOG("Stop");
  sniffer->status = Stop;
  // on_filter_Pressed();
}

void MainWindow::clear_catch() {
  LOG("Clear");
  view->clearView();
}

// Menu
void MainWindow::setMenuBar(QMenuBar *mBar) {
  // QMenu *pFile = mBar->addMenu("Files");
  // QAction *pOpen = pFile->addAction("Open");
  // connect(pOpen, &QAction::triggered, [=]() { qDebug() << "Open"; });
  // pFile->addSeparator();
  // QAction *pSave = pFile->addAction("Save");
  // connect(pSave, &QAction::triggered, this, &MainWindow::save_file);

  // QMenu *pRun = mBar->addMenu("Run");
  QAction *pStart = mBar->addAction("Start");
  connect(pStart, &QAction::triggered, this, &MainWindow::start_catch);
  QAction *pStop = mBar->addAction("Stop");
  connect(pStop, &QAction::triggered, this, &MainWindow::stop_catch);
  QAction *pRestart = mBar->addAction("Clear");
  connect(pRestart, &QAction::triggered, this, &MainWindow::clear_catch);

  QAction *pSave = mBar->addAction("Save");
  connect(pSave, &QAction::triggered, this, &MainWindow::save_file);

  QMenu *pRe = mBar->addMenu("Reassemble");
  QAction *pIPre = pRe->addAction("IP Reassemble");
  connect(pIPre, &QAction::triggered, this, &MainWindow::ip_reassemble);
  // TODO:make IPassemble a QCheckBox, change sniffer.is_IPreassmble_ticked to
  // true if ticked.
  pRe->addSeparator();
  QAction *pFre = pRe->addAction("File Reassemble");
  connect(pFre, &QAction::triggered, [=]() { qDebug() << "File Reassemble"; });
}

/*
 * filter control functions
 * when text changes, check the syntax.
 * when Filter button is pressed.
 */
void MainWindow::on_filter_textChanged(const QString &command) {
  QLineEdit *le = ui->filter_rule;
  if (filter->checkCommand(command)) {
    le->setStyleSheet("QLineEdit {background-color: #AFE1AF;}");
  } else {
    le->setStyleSheet("QLineEdit {background-color: #FAA0A0;}");
  }
}

void MainWindow::on_filter_Pressed() {
  if (ui->filter_rule->text() == tr("-h")) {
    QMessageBox::about(this, tr("The Usage of filter"),
                       tr("<-options>\t<filter rule>\n"
                          "-h\thelp\n-p\tprotocol\n-s\tsource IP "
                          "address\n-d\tdestination IP address\n"
                          "-sport\tsource port\n-dport\tdestination "
                          "port\n-c\tpacket content"));
    return;
  }
  filter->loadCommand(ui->filter_rule->text());
  filter->launchFilter(view);
}

void MainWindow::save_file() {
  LOG("Save file");
  QDateTime time = QDateTime::currentDateTime();
  QString dateTime = time.toString("MM-dd_hh-mm-ss");
  QString timeName = QString("%1.log").arg(dateTime);

  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Network Packet"), "../test/log/" + timeName,
      tr("Log File (*.log);;All Files (*)"));
  if (fileName.isEmpty())
    return;
  else { // TODO
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
      QMessageBox::information(this, tr("Unable to open file"),
                               file.errorString());
      return;
    }
    // QDataStream out(&file);
    // out.setVersion(QDataStream::Qt_4_5);
    QTextStream out(&file);
    out.setCodec("UTF-8");

    sniffer->status = Stop; // TODO
    for (auto &pkt : view->pkt) {
      out << QString::fromStdString("Time: ").toUtf8()
          << QString::fromStdString(pkt->time).toUtf8()
          << QString::fromStdString("\n").toUtf8()
          << QString::fromStdString("NO: ").toUtf8()
          << QString::number(pkt->no).toUtf8()
          << QString::fromStdString("\n").toUtf8()
          << QString::fromStdString(
                 store_payload((u_char *)pkt->eth_hdr, pkt->len))
                 .toUtf8()
          << QString::fromStdString("\n").toUtf8();
    }
    sniffer->status = Start;
  }
}

void MainWindow::ip_reassemble() {
  QItemSelectionModel *select = ui->tableView->selectionModel();
  if (select->selectedRows().empty()) {
    QMessageBox::critical(this, tr("Warning"), tr("Please select a packet"));
    return;
  } else {
    int row = select->selectedIndexes().at(0).row();
    const packet_struct *packet = view->pkt[row];
    if (packet->net_type != IPv4) {
      QMessageBox::critical(this, tr("Warning"), tr("Not a IP packet"));
    } else if ((ntohs(packet->net_hdr.ipv4_hdr->ip_off) & IP_DF) == 1) {
      QMessageBox::critical(this, tr("Warning"), tr("Not a Fragment packet"));
    } else {
      print_payload((u_char *)packet, packet->len); // TODO: delete this
      auto res = sniffer->ipv4Reassmble(packet);
      ui->textBrowser->clear();
      ui->textBrowser->insertPlainText(
          QString::fromStdString(store_payload((u_char *)res, res->len)));
    }
    delete packet;
    return;
  }
}