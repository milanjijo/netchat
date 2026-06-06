#pragma once

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>

class ClientController;

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

   protected:
    void closeEvent(QCloseEvent* event) override;

   private slots:
    void onSendButtonClicked();
    void onReturnPressed();
    void onMessageReceived(const QString& message);
    void onConnectionStatusChanged(bool connected);
    void onErrorOccurred(const QString& error);
    void onConnectAction();
    void onDisconnectAction();

   private:
    void setupUI();
    void createMenus();
    void appendMessage(const QString& message);
    void setConnected(bool connected);

    // UI Components
    QTextEdit* chatDisplay;
    QLineEdit* inputBox;
    QPushButton* sendButton;
    QLabel* statusLabel;

    // Connection dialog fields
    QString currentUsername;
    QString currentIp;
    int currentPort;

    // Controller
    ClientController* controller;

    // State
    bool isConnected;
};
