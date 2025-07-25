// SimpleMcp_Qt5_C++11.h - Dezelfde functionaliteit met Qt5/C++11
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QWidget>
#include <QApplication>
#include <QMetaProperty>
#include <iostream>

/**
 * SimpleMcp - zou EXACT hetzelfde werken met Qt5 + C++11
 * Gebruikt alleen basis Qt features die al jaren bestaan
 */
class SimpleMcp : public QObject
{
    Q_OBJECT

public:
    explicit SimpleMcp(QObject* parent = nullptr);  // C++11 nullptr
    void start();
    
private slots:
    void processStdinInput();
    void sendResponse(const QJsonObject& response);
    
private:
    // Alle gebruikte Qt classes bestaan al sinds Qt4/Qt5:
    // - QJsonObject/QJsonDocument (Qt5.0+)
    // - QTimer, QWidget, QApplication (Qt4+)
    // - QMetaProperty (Qt4+)
    // - findChildren<>() (Qt4+)
    
    QTimer* m_stdinTimer;
    bool m_serverRunning;
    
    // Alle member functions zouden identiek zijn
    QJsonObject handleListTools();
    QJsonObject handleIntrospectUi();
    // ... etc
};
