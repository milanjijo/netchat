#include "gui/MainWindow.h"
#include "gui/ClientController.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), isConnected(false), currentPort(12345) {
    
    setupUI();
    createMenus();
    
    // Create controller
    controller = new ClientController(this);
    
    // Connect controller signals to UI slots
    connect(controller, &ClientController::messageReceived,
            this, &MainWindow::onMessageReceived);
    connect(controller, &ClientController::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(controller, &ClientController::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
    setWindowTitle("Chat Client");
    resize(800, 600);
}

MainWindow::~MainWindow() {
    // Controller will be deleted by Qt parent-child relationship
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (isConnected) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Disconnect", 
            "You are still connected. Disconnect and exit?",
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes) {
            controller->disconnectFromServer();
            
            // Wait briefly for graceful disconnect
            QEventLoop loop;
            QTimer::singleShot(300, &loop, &QEventLoop::quit);
            loop.exec();
            
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void MainWindow::setupUI() {
    // Create central widget and main layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Chat display area (read-only)
    chatDisplay = new QTextEdit(this);
    chatDisplay->setReadOnly(true);
    chatDisplay->setStyleSheet(
        "QTextEdit {"
        "   background-color: #2b2b2b;"
        "   color: #ffffff;"
        "   font-family: 'Courier New', monospace;"
        "   font-size: 12pt;"
        "   border: 1px solid #555;"
        "}"
    );
    mainLayout->addWidget(chatDisplay);
    
    // Input area layout
    QHBoxLayout* inputLayout = new QHBoxLayout();
    
    // Input box
    inputBox = new QLineEdit(this);
    inputBox->setPlaceholderText("Type your message here...");
    inputBox->setEnabled(false);  // Disabled until connected
    inputBox->setStyleSheet(
        "QLineEdit {"
        "   padding: 8px;"
        "   font-size: 12pt;"
        "   border: 1px solid #555;"
        "}"
    );
    
    // Connect return key press
    connect(inputBox, &QLineEdit::returnPressed,
            this, &MainWindow::onReturnPressed);
    
    inputLayout->addWidget(inputBox);
    
    // Send button
    sendButton = new QPushButton("Send", this);
    sendButton->setEnabled(false);  // Disabled until connected
    sendButton->setStyleSheet(
        "QPushButton {"
        "   padding: 8px 16px;"
        "   font-size: 12pt;"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45a049;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #cccccc;"
        "   color: #666666;"
        "}"
    );
    
    connect(sendButton, &QPushButton::clicked,
            this, &MainWindow::onSendButtonClicked);
    
    inputLayout->addWidget(sendButton);
    mainLayout->addLayout(inputLayout);
    
    // Status bar
    statusLabel = new QLabel("Not connected", this);
    statusBar()->addWidget(statusLabel);
    
    setCentralWidget(centralWidget);
}

void MainWindow::createMenus() {
    QMenuBar* menuBar = this->menuBar();
    
    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    
    QAction* connectAction = new QAction("&Connect", this);
    connectAction->setShortcut(QKeySequence("Ctrl+N"));
    connect(connectAction, &QAction::triggered,
            this, &MainWindow::onConnectAction);
    fileMenu->addAction(connectAction);
    
    QAction* disconnectAction = new QAction("&Disconnect", this);
    disconnectAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(disconnectAction, &QAction::triggered,
            this, &MainWindow::onDisconnectAction);
    fileMenu->addAction(disconnectAction);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered,
            this, &QMainWindow::close);
    fileMenu->addAction(exitAction);
}

void MainWindow::onSendButtonClicked() {
    QString message = inputBox->text().trimmed();
    if (message.isEmpty()) {
        return;
    }
    
    if (!isConnected) {
        QMessageBox::warning(this, "Not Connected",
                           "You must connect to a server first.");
        return;
    }
    
    // Send message through controller
    controller->sendMessage(message);
    
    // Clear input box
    inputBox->clear();
    inputBox->setFocus();
}

void MainWindow::onReturnPressed() {
    onSendButtonClicked();
}

void MainWindow::onMessageReceived(const QString& message) {
    appendMessage(message);
}

void MainWindow::onConnectionStatusChanged(bool connected) {
    setConnected(connected);
    
    if (connected) {
        statusLabel->setText("Connected to " + currentIp + ":" + QString::number(currentPort));
        appendMessage("<span style='color: #4CAF50;'>[SYSTEM] Connected successfully!</span>");
    } else {
        statusLabel->setText("Disconnected");
        appendMessage("<span style='color: #f44336;'>[SYSTEM] Disconnected from server</span>");
    }
}

void MainWindow::onErrorOccurred(const QString& error) {
    QMessageBox::critical(this, "Error", error);
    appendMessage("<span style='color: #f44336;'>[ERROR] " + error + "</span>");
}

void MainWindow::onConnectAction() {
    if (isConnected) {
        QMessageBox::information(this, "Already Connected",
                               "You are already connected. Disconnect first.");
        return;
    }
    
    // Get username
    bool ok;
    QString username = QInputDialog::getText(this, "Username",
                                            "Enter your username:",
                                            QLineEdit::Normal,
                                            currentUsername, &ok);
    if (!ok || username.isEmpty()) {
        return;
    }
    currentUsername = username;
    
    // Get IP address
    QString ip = QInputDialog::getText(this, "Server IP",
                                      "Enter server IP address:",
                                      QLineEdit::Normal,
                                      currentIp.isEmpty() ? "127.0.0.1" : currentIp,
                                      &ok);
    if (!ok || ip.isEmpty()) {
        return;
    }
    currentIp = ip;
    
    // Get port
    int port = QInputDialog::getInt(this, "Server Port",
                                   "Enter server port:",
                                   currentPort, 1, 65535, 1, &ok);
    if (!ok) {
        return;
    }
    currentPort = port;
    
    // Attempt connection
    appendMessage("<span style='color: #2196F3;'>[SYSTEM] Connecting to " + 
                 currentIp + ":" + QString::number(currentPort) + "...</span>");
    controller->connectToServer(currentIp, currentPort, currentUsername);
}

void MainWindow::onDisconnectAction() {
    if (!isConnected) {
        QMessageBox::information(this, "Not Connected",
                               "You are not connected to any server.");
        return;
    }
    
    controller->disconnectFromServer();
}

void MainWindow::appendMessage(const QString& message) {
    chatDisplay->append(message);
    
    // Auto-scroll to bottom
    QTextCursor cursor = chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    chatDisplay->setTextCursor(cursor);
}

void MainWindow::setConnected(bool connected) {
    isConnected = connected;
    inputBox->setEnabled(connected);
    sendButton->setEnabled(connected);
    
    if (connected) {
        inputBox->setFocus();
    }
}
