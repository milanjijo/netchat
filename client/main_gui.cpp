#include "gui/MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // Set application metadata
    QApplication::setApplicationName("Chat Client");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("ChatApp");
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    // Start Qt event loop
    return app.exec();
}
