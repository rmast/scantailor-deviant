#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QApplication>
#include <QtCore/QMetaProperty>
#include <iostream>

/**
 * Simple MCP (Model Context Protocol) server for exposing Qt UI elements
 * to external tools like GitHub Copilot for code analysis and testing.
 * 
 * Much lighter than qtmcp - focuses only on what we need:
 * - UI element discovery and introspection  
 * - Property reading for state analysis
 * - Simple manipulation for testing
 */
class SimpleMcp : public QObject
{
    Q_OBJECT

public:
    explicit SimpleMcp(QObject* parent = nullptr);
    
    // Start the MCP server (stdio protocol)
    void start();
    
private slots:
    void processStdinInput();
    void sendResponse(const QJsonObject& response);
    
private:
    // Core MCP capabilities
    QJsonObject handleListTools();
    QJsonObject handleIntrospectUi();
    QJsonObject handleGetProperty(const QString& objectPath, const QString& property);
    QJsonObject handleSetProperty(const QString& objectPath, const QString& property, const QJsonValue& value);
    QJsonObject handleFindWidget(const QString& className, const QString& objectName = QString());
    
    // Utility functions
    QWidget* findWidgetByPath(const QString& path);
    QString getWidgetPath(QWidget* widget);
    QJsonObject widgetToJson(QWidget* widget);
    QJsonArray getAllWidgets();
    
    QTimer* m_stdinTimer;
    bool m_serverRunning;
};
