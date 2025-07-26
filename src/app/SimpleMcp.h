#pragma once

#include <QtCore/QObject>
#include <QtCore/QIODevice>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtGui/QMouseEvent>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
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
    
    // Execute a single MCP command and return result (for on-demand use)
    QString executeCommand(const QString& jsonRpcCall);
    
    // Get the port number for TCP connections
    quint16 getPort() const { return m_tcpPort; }

protected:
    // Event filter to capture UI interactions during recording
    bool eventFilter(QObject* watched, QEvent* event) override;
    
private slots:
    void processStdinInput();
    void sendResponse(const QJsonObject& response);
    void onNewConnection();
    void onSocketDataReady();
    
private:
    // Core MCP capabilities
    QJsonObject handleListTools();
    QJsonObject handleIntrospectUi();
    QJsonObject handleGetProperty(const QString& objectPath, const QString& property);
    QJsonObject handleSetProperty(const QString& objectPath, const QString& property, const QJsonValue& value);
    QJsonObject handleFindWidget(const QString& className, const QString& objectName = QString());
    QJsonObject handleCountSplinePoints(); // New function to count dewarping spline points
    QJsonObject handleAnalyzeViewportSplines(); // Analyze splines using viewport and mouse positions
    QJsonObject handleGetSplineAnchors(); // Get actual coordinates of spline anchor points
    
    // Recording functionality
    QJsonObject handleStartRecording();
    QJsonObject handleStopRecording();
    QJsonObject handleGetRecording();
    
    // Utility functions
    QWidget* findWidgetByPath(const QString& path);
    QString getWidgetPath(QWidget* widget);
    QJsonObject widgetToJson(QWidget* widget);
    QJsonArray getAllWidgets();
    
    QTimer* m_stdinTimer;
    bool m_serverRunning;
    
    // TCP Server for network connections
    QTcpServer* m_tcpServer;
    quint16 m_tcpPort;
    QList<QTcpSocket*> m_clients;
    
    // Recording state
    bool m_recording;
    QJsonArray m_recordedActions;
    void recordAction(const QString& action, const QJsonObject& details);
};
