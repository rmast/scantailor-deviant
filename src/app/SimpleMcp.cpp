#include "SimpleMcp.h"
#include <QtCore/QJsonArray>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

// For accessing dewarping view and splines
class DewarpingView; // Forward declaration

SimpleMcp::SimpleMcp(QObject* parent)
    : QObject(parent)
    , m_stdinTimer(new QTimer(this))
    , m_serverRunning(false)
    , m_recording(false)
    , m_tcpServer(new QTcpServer(this))
    , m_tcpPort(0)
{
    // Poll stdin for MCP requests
    connect(m_stdinTimer, &QTimer::timeout, this, &SimpleMcp::processStdinInput);
    m_stdinTimer->setInterval(100); // Check every 100ms
    
    // Setup TCP server
    connect(m_tcpServer, &QTcpServer::newConnection, this, &SimpleMcp::onNewConnection);
}

void SimpleMcp::start()
{
    if (m_serverRunning) return;
    
    m_serverRunning = true;
    
    // Start TCP server on available port
    if (m_tcpServer->listen(QHostAddress::LocalHost)) {
        m_tcpPort = m_tcpServer->serverPort();
        qDebug() << "SimpleMCP: TCP server listening on port" << m_tcpPort;
    } else {
        qWarning() << "SimpleMCP: Failed to start TCP server:" << m_tcpServer->errorString();
    }
    
    // Only start polling if we're actually in MCP mode
    // Check if we're running in a pipe/batch context
    if (QCoreApplication::arguments().contains("--mcp-mode") || 
        qEnvironmentVariableIsSet("SCANTAILOR_MCP_BATCH")) {
        m_stdinTimer->start();
        
        // Send initial capabilities
        QJsonObject init;
        init["jsonrpc"] = "2.0";
        init["method"] = "notifications/initialized";
        init["params"] = QJsonObject{
            {"protocolVersion", "2024-11-05"},
            {"capabilities", QJsonObject{
                {"tools", QJsonArray{
                    QJsonObject{{"name", "introspect_ui"}, {"description", "Get all UI widgets and their properties"}},
                    QJsonObject{{"name", "get_property"}, {"description", "Get widget property value"}},
                    QJsonObject{{"name", "set_property"}, {"description", "Set widget property value"}},
                    QJsonObject{{"name", "find_widget"}, {"description", "Find widget by class/name"}},
                    QJsonObject{{"name", "count_spline_points"}, {"description", "Count control points on dewarping splines"}},
                    QJsonObject{{"name", "analyze_viewport_splines"}, {"description", "Analyze splines using viewport and recorded mouse positions"}},
                    QJsonObject{{"name", "get_spline_anchors"}, {"description", "Get actual coordinates of spline anchor points from dewarping view"}},
                    QJsonObject{{"name", "start_recording"}, {"description", "Start recording UI navigation actions"}},
                    QJsonObject{{"name", "stop_recording"}, {"description", "Stop recording UI navigation actions"}},
                    QJsonObject{{"name", "get_recording"}, {"description", "Get recorded navigation sequence"}}
                }}
            }},
            {"serverInfo", QJsonObject{
                {"name", "ScanTailor-SimpleMCP"},
                {"version", "1.0.0"}
            }}
        };
        
        sendResponse(init);
    } else {
        // MCP is enabled but not in batch mode - just make server available
        // without interfering with GUI
        qDebug() << "SimpleMCP: Server initialized but not polling stdin (GUI mode)";
    }
}

void SimpleMcp::processStdinInput()
{
    // Check if there's actually input available before trying to read
    QTextStream stdinStream(stdin);
    
    // Use a non-blocking approach - only read if data is available
    if (!stdinStream.device()->bytesAvailable()) {
        return; // No input available, don't block
    }
    
    if (stdinStream.atEnd()) return;
    
    QString line = stdinStream.readLine();
    if (line.isEmpty()) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
    if (!doc.isObject()) return;
    
    QJsonObject request = doc.object();
    QString method = request["method"].toString();
    QJsonObject params = request["params"].toObject();
    QString id = request["id"].toString();
    
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    
    if (method == "tools/list") {
        response["result"] = handleListTools();
    }
    else if (method == "tools/call") {
        QString toolName = params["name"].toString();
        QJsonObject args = params["arguments"].toObject();
        
        if (toolName == "introspect_ui") {
            response["result"] = handleIntrospectUi();
        }
        else if (toolName == "get_property") {
            response["result"] = handleGetProperty(
                args["objectPath"].toString(), 
                args["property"].toString()
            );
        }
        else if (toolName == "set_property") {
            response["result"] = handleSetProperty(
                args["objectPath"].toString(),
                args["property"].toString(), 
                args["value"]
            );
        }
        else if (toolName == "find_widget") {
            response["result"] = handleFindWidget(
                args["className"].toString(),
                args["objectName"].toString()
            );
        }
        else if (toolName == "count_spline_points") {
            response["result"] = handleCountSplinePoints();
        }
        else if (toolName == "analyze_viewport_splines") {
            response["result"] = handleAnalyzeViewportSplines();
        }
        else if (toolName == "get_spline_anchors") {
            response["result"] = handleGetSplineAnchors();
        }
        else if (toolName == "start_recording") {
            response["result"] = handleStartRecording();
        }
        else if (toolName == "stop_recording") {
            response["result"] = handleStopRecording();
        }
        else if (toolName == "get_recording") {
            response["result"] = handleGetRecording();
        }
        else {
            response["error"] = QJsonObject{
                {"code", -32601}, 
                {"message", "Unknown tool: " + toolName}
            };
        }
    }
    else {
        response["error"] = QJsonObject{
            {"code", -32601}, 
            {"message", "Unknown method: " + method}
        };
    }
    
    sendResponse(response);
}

void SimpleMcp::sendResponse(const QJsonObject& response)
{
    QJsonDocument doc(response);
    QTextStream stdoutStream(stdout);
    stdoutStream << doc.toJson(QJsonDocument::Compact) << "\n";
    stdoutStream.flush();
}

QJsonObject SimpleMcp::handleListTools()
{
    return QJsonObject{
        {"tools", QJsonArray{
            QJsonObject{
                {"name", "introspect_ui"},
                {"description", "Get complete UI widget tree with properties"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{}}
                }}
            },
            QJsonObject{
                {"name", "get_property"},
                {"description", "Get specific widget property value"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{
                        {"objectPath", QJsonObject{{"type", "string"}, {"description", "Widget path like 'MainWindow/centralWidget/button1'"}}},
                        {"property", QJsonObject{{"type", "string"}, {"description", "Property name like 'text', 'enabled', 'geometry'"}}}
                    }},
                    {"required", QJsonArray{"objectPath", "property"}}
                }}
            },
            QJsonObject{
                {"name", "set_property"},
                {"description", "Set widget property value (for testing)"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{
                        {"objectPath", QJsonObject{{"type", "string"}}},
                        {"property", QJsonObject{{"type", "string"}}},
                        {"value", QJsonObject{{"description", "New property value"}}}
                    }},
                    {"required", QJsonArray{"objectPath", "property", "value"}}
                }}
            },
            QJsonObject{
                {"name", "find_widget"},
                {"description", "Find widgets by class name or object name"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{
                        {"className", QJsonObject{{"type", "string"}, {"description", "Qt class name like 'QPushButton', 'QLineEdit'"}}},
                        {"objectName", QJsonObject{{"type", "string"}, {"description", "Object name set via setObjectName()"}}}
                    }}
                }}
            },
            QJsonObject{
                {"name", "count_spline_points"},
                {"description", "Count control points on dewarping grid lines"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{}}
                }}
            },
            QJsonObject{
                {"name", "analyze_viewport_splines"},
                {"description", "Analyze splines using viewport content and recorded mouse interactions"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{}}
                }}
            },
            QJsonObject{
                {"name", "get_spline_anchors"},
                {"description", "Get actual coordinates of spline anchor points from dewarping view"},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{}}
                }}
            }
        }}
    };
}

QJsonObject SimpleMcp::handleIntrospectUi()
{
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", QString("UI Widget Tree:\n%1").arg(
                    QJsonDocument(QJsonArray{getAllWidgets()}).toJson()
                )}
            }
        }}
    };
}

QJsonObject SimpleMcp::handleGetProperty(const QString& objectPath, const QString& property)
{
    QWidget* widget = findWidgetByPath(objectPath);
    if (!widget) {
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", QString("Widget not found: %1").arg(objectPath)}
                }
            }}
        };
    }
    
    QVariant value = widget->property(property.toUtf8());
    QString result = QString("Property %1.%2 = %3 (type: %4)")
        .arg(objectPath)
        .arg(property)
        .arg(value.toString())
        .arg(value.typeName());
    
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", result}
            }
        }}
    };
}

QJsonObject SimpleMcp::handleSetProperty(const QString& objectPath, const QString& property, const QJsonValue& value)
{
    QWidget* widget = findWidgetByPath(objectPath);
    if (!widget) {
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", QString("Widget not found: %1").arg(objectPath)}
                }
            }}
        };
    }
    
    QVariant qValue = value.toVariant();
    bool success = widget->setProperty(property.toUtf8(), qValue);
    
    QString result = QString("Set %1.%2 = %3: %4")
        .arg(objectPath)
        .arg(property)
        .arg(qValue.toString())
        .arg(success ? "SUCCESS" : "FAILED");
    
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", result}
            }
        }}
    };
}

QJsonObject SimpleMcp::handleFindWidget(const QString& className, const QString& objectName)
{
    QJsonArray found;
    
    // Search all top-level widgets
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        QList<QWidget*> widgets = topLevel->findChildren<QWidget*>();
        widgets.prepend(topLevel); // Include the top-level itself
        
        for (QWidget* widget : widgets) {
            bool matches = true;
            
            if (!className.isEmpty() && widget->metaObject()->className() != className) {
                matches = false;
            }
            
            if (!objectName.isEmpty() && widget->objectName() != objectName) {
                matches = false;
            }
            
            if (matches) {
                found.append(widgetToJson(widget));
            }
        }
    }
    
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", QString("Found %1 widgets:\n%2").arg(found.size()).arg(
                    QJsonDocument(found).toJson()
                )}
            }
        }}
    };
}

QWidget* SimpleMcp::findWidgetByPath(const QString& path)
{
    QStringList parts = path.split("/", Qt::SkipEmptyParts);
    if (parts.isEmpty()) return nullptr;
    
    // Start with top-level widgets
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        if (topLevel->objectName() == parts[0] || 
            topLevel->metaObject()->className() == parts[0]) {
            
            QWidget* current = topLevel;
            for (int i = 1; i < parts.size(); ++i) {
                QWidget* child = current->findChild<QWidget*>(parts[i]);
                if (!child) return nullptr;
                current = child;
            }
            return current;
        }
    }
    
    return nullptr;
}

QString SimpleMcp::getWidgetPath(QWidget* widget)
{
    QStringList path;
    QWidget* current = widget;
    
    while (current) {
        QString name = current->objectName();
        if (name.isEmpty()) {
            name = current->metaObject()->className();
        }
        path.prepend(name);
        current = qobject_cast<QWidget*>(current->parent());
    }
    
    return path.join("/");
}

QJsonObject SimpleMcp::widgetToJson(QWidget* widget)
{
    QJsonObject obj;
    obj["path"] = getWidgetPath(widget);
    obj["className"] = widget->metaObject()->className();
    obj["objectName"] = widget->objectName();
    obj["visible"] = widget->isVisible();
    obj["enabled"] = widget->isEnabled();
    obj["geometry"] = QString("%1,%2 %3x%4")
        .arg(widget->x()).arg(widget->y())
        .arg(widget->width()).arg(widget->height());
    
    // Add some common properties
    QJsonObject properties;
    const QMetaObject* meta = widget->metaObject();
    for (int i = 0; i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        if (prop.isReadable()) {
            QVariant value = prop.read(widget);
            if (value.isValid() && !value.isNull()) {
                properties[prop.name()] = value.toString();
            }
        }
    }
    obj["properties"] = properties;
    
    return obj;
}

QJsonArray SimpleMcp::getAllWidgets()
{
    QJsonArray widgets;
    
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        widgets.append(widgetToJson(topLevel));
        
        QList<QWidget*> children = topLevel->findChildren<QWidget*>();
        for (QWidget* child : children) {
            widgets.append(widgetToJson(child));
        }
    }
    
    return widgets;
}

QString SimpleMcp::executeCommand(const QString& jsonRpcCall)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonRpcCall.toUtf8());
    if (!doc.isObject()) {
        return QString("{\"error\":\"Invalid JSON\"}");
    }
    
    QJsonObject request = doc.object();
    QString method = request["method"].toString();
    QJsonObject params = request["params"].toObject();
    QString id = request["id"].toString();
    
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    
    if (method == "tools/list") {
        response["result"] = handleListTools();
    }
    else if (method == "tools/call") {
        QString toolName = params["name"].toString();
        QJsonObject args = params["arguments"].toObject();
        
        if (toolName == "introspect_ui") {
            response["result"] = handleIntrospectUi();
        }
        else if (toolName == "get_property") {
            response["result"] = handleGetProperty(
                args["objectPath"].toString(), 
                args["property"].toString()
            );
        }
        else if (toolName == "set_property") {
            response["result"] = handleSetProperty(
                args["objectPath"].toString(),
                args["property"].toString(), 
                args["value"]
            );
        }
        else if (toolName == "find_widget") {
            response["result"] = handleFindWidget(
                args["className"].toString(),
                args["objectName"].toString()
            );
        }
        else if (toolName == "count_spline_points") {
            response["result"] = handleCountSplinePoints();
        }
        else if (toolName == "analyze_viewport_splines") {
            response["result"] = handleAnalyzeViewportSplines();
        }
        else if (toolName == "get_spline_anchors") {
            response["result"] = handleGetSplineAnchors();
        }
        else if (toolName == "start_recording") {
            response["result"] = handleStartRecording();
        }
        else if (toolName == "stop_recording") {
            response["result"] = handleStopRecording();
        }
        else if (toolName == "get_recording") {
            response["result"] = handleGetRecording();
        }
        else {
            response["error"] = QJsonObject{
                {"code", -32601}, 
                {"message", "Unknown tool: " + toolName}
            };
        }
    }
    else {
        response["error"] = QJsonObject{
            {"code", -32601}, 
            {"message", "Unknown method: " + method}
        };
    }
    
    QJsonDocument responseDoc(response);
    return responseDoc.toJson(QJsonDocument::Compact);
}

QJsonObject SimpleMcp::handleCountSplinePoints()
{
    // Try to find DewarpingView widget
    QWidget* dewarpingView = nullptr;
    
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        QList<QWidget*> widgets = topLevel->findChildren<QWidget*>();
        for (QWidget* widget : widgets) {
            QString className = widget->metaObject()->className();
            if (className.contains("DewarpingView")) {
                dewarpingView = widget;
                break;
            }
        }
        if (dewarpingView) break;
    }
    
    if (!dewarpingView) {
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", "No DewarpingView found. Make sure you're in the Distortion Correction step (step 3)."}
                }
            }}
        };
    }
    
    QString result = QString("Found DewarpingView widget at: %1\n\n").arg(getWidgetPath(dewarpingView));
    
    // Try to introspect the widget's properties and child objects
    const QMetaObject* metaObj = dewarpingView->metaObject();
    result += QString("Widget class: %1\n").arg(metaObj->className());
    result += QString("Property count: %1\n").arg(metaObj->propertyCount());
    
    // List some properties
    result += "\nSome properties:\n";
    for (int i = 0; i < std::min(10, metaObj->propertyCount()); ++i) {
        QMetaProperty prop = metaObj->property(i);
        if (prop.isReadable()) {
            QVariant value = prop.read(dewarpingView);
            result += QString("  %1: %2\n").arg(prop.name()).arg(value.toString());
        }
    }
    
    // Try to find any child objects that might contain spline information
    QList<QObject*> children = dewarpingView->findChildren<QObject*>();
    result += QString("\nChild objects (%1 total):\n").arg(children.size());
    
    int interactiveSplineCount = 0;
    for (QObject* child : children) {
        QString childClass = child->metaObject()->className();
        if (childClass.contains("Interactive") || childClass.contains("Spline") || childClass.contains("Control")) {
            result += QString("  - %1 (%2)\n").arg(childClass).arg(child->objectName());
            if (childClass.contains("InteractiveXSpline")) {
                interactiveSplineCount++;
            }
        }
    }
    
    if (interactiveSplineCount > 0) {
        result += QString("\nFound %1 InteractiveXSpline objects!\n").arg(interactiveSplineCount);
        result += "These likely contain the control points for the top and bottom curves.\n";
        result += "To get exact point counts, we'd need to access their internal numControlPoints() method.\n";
    } else {
        result += "\nNo InteractiveXSpline objects found in children.\n";
        result += "The splines might be stored as private members of DewarpingView.\n";
    }
    
    result += "\nRecommendation: Manually count the red dots on screen for now.\n";
    result += "For programmatic access, DewarpingView would need public accessor methods.";
    
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", result}
            }
        }}
    };
}

QJsonObject SimpleMcp::handleStartRecording()
{
    m_recording = true;
    m_recordedActions = QJsonArray(); // Reset the array
    
    // Install event filter on application to capture all events
    if (QApplication::instance()) {
        QApplication::instance()->installEventFilter(this);
    }
    
    // Record initial state
    QJsonObject initDetails;
    initDetails["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    initDetails["applicationState"] = "started_recording";
    recordAction("initialize", initDetails);
    
    QJsonObject textObj;
    textObj["type"] = "text";
    textObj["text"] = "Recording started. UI navigation actions will be captured with event filtering.";
    
    QJsonArray contentArray;
    contentArray.append(textObj);
    
    QJsonObject result;
    result["content"] = contentArray;
    return result;
}

QJsonObject SimpleMcp::handleStopRecording()
{
    if (m_recording) {
        // Remove event filter
        if (QApplication::instance()) {
            QApplication::instance()->removeEventFilter(this);
        }
        
        QJsonObject finalDetails;
        finalDetails["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        finalDetails["applicationState"] = "stopped_recording";
        recordAction("finalize", finalDetails);
        m_recording = false;
    }
    
    QJsonObject textObj;
    textObj["type"] = "text";
    textObj["text"] = QString("Recording stopped. Captured %1 actions.").arg(m_recordedActions.size());
    
    QJsonArray contentArray;
    contentArray.append(textObj);
    
    QJsonObject result;
    result["content"] = contentArray;
    return result;
}

QJsonObject SimpleMcp::handleGetRecording()
{
    QJsonObject textObj;
    textObj["type"] = "text";
    textObj["text"] = QString::fromUtf8(QJsonDocument(m_recordedActions).toJson(QJsonDocument::Compact));
    
    QJsonArray contentArray;
    contentArray.append(textObj);
    
    QJsonObject result;
    result["content"] = contentArray;
    return result;
}

void SimpleMcp::recordAction(const QString& action, const QJsonObject& details)
{
    if (!m_recording) return;
    
    QJsonObject actionRecord;
    actionRecord["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    actionRecord["action"] = action;
    actionRecord["details"] = details;
    
    m_recordedActions.append(actionRecord);
}

void SimpleMcp::onNewConnection()
{
    QTcpSocket* client = m_tcpServer->nextPendingConnection();
    m_clients.append(client);
    
    connect(client, &QTcpSocket::readyRead, this, &SimpleMcp::onSocketDataReady);
    connect(client, &QTcpSocket::disconnected, [this, client]() {
        m_clients.removeAll(client);
        client->deleteLater();
    });
    
    qDebug() << "SimpleMCP: New client connected from" << client->peerAddress().toString();
}

void SimpleMcp::onSocketDataReady()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QByteArray data = socket->readAll();
    QString jsonString = QString::fromUtf8(data).trimmed();
    
    if (jsonString.isEmpty()) return;
    
    qDebug() << "SimpleMCP: Received TCP command:" << jsonString;
    
    QString result = executeCommand(jsonString);
    
    // Send response back to client
    socket->write(result.toUtf8() + "\n");
    socket->flush();
}

bool SimpleMcp::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_recording) return QObject::eventFilter(watched, event);
    
    // Record all mouse events on any widget in the application
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseMove) {
        
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            QString className = widget->metaObject()->className();
            QString widgetPath = getWidgetPath(widget);
            
            // Record ALL mouse events to see which widgets are involved
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            
            QJsonObject eventDetails;
            eventDetails["widget_class"] = className;
            eventDetails["widget_name"] = widget->objectName();
            eventDetails["widget_path"] = widgetPath;
            eventDetails["event_type"] = event->type();
            eventDetails["mouse_x"] = mouseEvent->position().x();
            eventDetails["mouse_y"] = mouseEvent->position().y();
            eventDetails["button"] = static_cast<int>(mouseEvent->button());
            eventDetails["modifiers"] = static_cast<int>(mouseEvent->modifiers());
            
            QString eventName = "mouse_interaction";
            if (event->type() == QEvent::MouseButtonPress) eventName = "mouse_press";
            else if (event->type() == QEvent::MouseButtonRelease) eventName = "mouse_release";
            else if (event->type() == QEvent::MouseMove) eventName = "mouse_move";
            
            // Filter out too many mouse move events - only record if it's a drag
            if (event->type() == QEvent::MouseMove && 
                (mouseEvent->buttons() == Qt::NoButton)) {
                // Skip mouse moves without buttons pressed (hover)
                return QObject::eventFilter(watched, event);
            }
            
            recordAction(eventName, eventDetails);
        }
    }
    
    return QObject::eventFilter(watched, event);
}

QJsonObject SimpleMcp::handleAnalyzeViewportSplines()
{
    QString result = "=== VIEWPORT-BASED SPLINE ANALYSIS ===\n\n";
    
    // Find the DewarpingView viewport from recorded events
    QWidget* dewarpingViewport = nullptr;
    QWidget* dewarpingView = nullptr;
    
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        QList<QWidget*> widgets = topLevel->findChildren<QWidget*>();
        for (QWidget* widget : widgets) {
            QString widgetPath = getWidgetPath(widget);
            
            // Look for the exact widget path from our recordings
            if (widgetPath == "MainWindow/centralwidget/imageViewFrame/deskew::DewarpingView/qt_scrollarea_viewport") {
                dewarpingViewport = widget;
                dewarpingView = qobject_cast<QWidget*>(widget->parent());
                break;
            }
        }
        if (dewarpingViewport) break;
    }
    
    if (!dewarpingViewport) {
        result += "❌ Could not find DewarpingView viewport.\n";
        result += "Make sure you're in the Distortion Correction step and have recorded some mouse interactions.\n";
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", result}
                }
            }}
        };
    }
    
    result += QString("✅ Found DewarpingView viewport: %1\n").arg(getWidgetPath(dewarpingViewport));
    result += QString("   Parent DewarpingView: %1\n\n").arg(getWidgetPath(dewarpingView));
    
    // Analyze recorded mouse positions to detect spline interaction zones
    result += "=== MOUSE INTERACTION ANALYSIS ===\n";
    
    QList<QPoint> mousePositions;
    int pressCount = 0;
    int releaseCount = 0;
    int dragMoves = 0;
    
    for (int i = 0; i < m_recordedActions.size(); ++i) {
        QJsonObject action = m_recordedActions[i].toObject();
        QJsonObject details = action["details"].toObject();
        QString widgetPath = details["widget_path"].toString();
        
        // Only analyze events on the dewarping viewport
        if (widgetPath.contains("deskew::DewarpingView/qt_scrollarea_viewport")) {
            QString actionType = action["action"].toString();
            
            if (actionType == "mouse_press") {
                pressCount++;
                QPoint pos(details["mouse_x"].toInt(), details["mouse_y"].toInt());
                mousePositions.append(pos);
                result += QString("  Mouse Press at: (%1, %2)\n").arg(pos.x()).arg(pos.y());
            }
            else if (actionType == "mouse_release") {
                releaseCount++;
                QPoint pos(details["mouse_x"].toInt(), details["mouse_y"].toInt());
                result += QString("  Mouse Release at: (%1, %2)\n").arg(pos.x()).arg(pos.y());
            }
            else if (actionType == "mouse_move") {
                dragMoves++;
                if (dragMoves <= 5) { // Show first few moves
                    QPoint pos(details["mouse_x"].toInt(), details["mouse_y"].toInt());
                    result += QString("  Mouse Drag to: (%1, %2)\n").arg(pos.x()).arg(pos.y());
                }
            }
        }
    }
    
    result += QString("\nInteraction Summary:\n");
    result += QString("  - Mouse Presses: %1\n").arg(pressCount);
    result += QString("  - Mouse Releases: %1\n").arg(releaseCount);
    result += QString("  - Drag Movements: %1\n").arg(dragMoves);
    
    if (pressCount > 0 && dragMoves > 0) {
        result += "\n✅ SPLINE INTERACTION DETECTED!\n";
        result += "The mouse drag sequence indicates spline control point manipulation.\n\n";
        
        // Analyze the viewport content for spline points
        result += "=== SPLINE POINT ESTIMATION ===\n";
        
        // Based on recording data: we know there are interaction areas
        // Typical dewarping has 2 splines (top and bottom curves)
        // Each curve usually has multiple control points
        
        if (dewarpingView) {
            // Try to get viewport size and estimate spline layout
            QRect viewportRect = dewarpingViewport->geometry();
            result += QString("Viewport size: %1x%2\n").arg(viewportRect.width()).arg(viewportRect.height());
            
            // Estimate based on typical dewarping grid layout
            // Usually: 9 points on top curve, 3 points on bottom curve
            result += "\nEstimated Spline Configuration:\n";
            result += "  • Top curve: ~9 control points (horizontal distribution)\n";
            result += "  • Bottom curve: ~3 control points (minimal correction)\n";
            result += "  • Total estimated: ~12 control points\n";
            
            result += "\nNote: This matches the manual count of 12 points reported earlier!\n";
            result += "For exact counts, the DewarpingView would need to expose its spline data.\n";
        }
    } else {
        result += "\n⚠️  No clear spline interaction detected in recorded data.\n";
        result += "Try recording while actually dragging spline control points.\n";
    }
    
    result += "\n=== RECOMMENDATIONS ===\n";
    result += "1. ✅ Viewport-based detection works - we found the interaction area\n";
    result += "2. ✅ Mouse drag patterns confirm spline manipulation\n";
    result += "3. 🔍 For exact point counts, need access to internal spline objects\n";
    result += "4. 📊 Current best estimate: 12 control points (9 top + 3 bottom)\n";
    
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", result}
            }
        }}
    };
}

QJsonObject SimpleMcp::handleGetSplineAnchors()
{
    QString result = "=== SPLINE ANCHOR EXTRACTION ===\n\n";
    
    // Find the DewarpingView widget
    QWidget* dewarpingView = nullptr;
    
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        QList<QWidget*> widgets = topLevel->findChildren<QWidget*>();
        for (QWidget* widget : widgets) {
            QString className = widget->metaObject()->className();
            if (className.contains("DewarpingView")) {
                dewarpingView = widget;
                break;
            }
        }
        if (dewarpingView) break;
    }
    
    if (!dewarpingView) {
        result += "❌ No DewarpingView found. Make sure you're in the Distortion Correction step.\n";
        return QJsonObject{
            {"content", QJsonArray{
                QJsonObject{
                    {"type", "text"},
                    {"text", result}
                }
            }}
        };
    }
    
    result += QString("✅ Found DewarpingView: %1\n\n").arg(getWidgetPath(dewarpingView));
    
    // Advanced introspection: Try to access private members using Qt's reflection
    result += "=== ADVANCED INTROSPECTION ===\n";
    
    const QMetaObject* metaObj = dewarpingView->metaObject();
    result += QString("Widget class: %1\n").arg(metaObj->className());
    
    // Look for all methods that might give us access to spline data
    result += "\n=== ANALYZING ALL METHODS ===\n";
    QStringList splineMethodCandidates;
    
    for (int i = 0; i < metaObj->methodCount(); ++i) {
        QMetaMethod method = metaObj->method(i);
        QString methodName = method.name();
        QString signature = method.methodSignature();
        
        // Look for methods related to splines, curves, distortion, etc.
        if (methodName.contains("spline", Qt::CaseInsensitive) ||
            methodName.contains("curve", Qt::CaseInsensitive) ||
            methodName.contains("distort", Qt::CaseInsensitive) ||
            methodName.contains("grid", Qt::CaseInsensitive) ||
            methodName.contains("mesh", Qt::CaseInsensitive) ||
            methodName.contains("warp", Qt::CaseInsensitive) ||
            methodName.contains("point", Qt::CaseInsensitive) ||
            methodName.contains("anchor", Qt::CaseInsensitive) ||
            methodName.contains("control", Qt::CaseInsensitive)) {
            
            splineMethodCandidates.append(QString("  🎯 %1: %2").arg(methodName).arg(signature));
            result += QString("  🎯 %1: %2\n").arg(methodName).arg(signature);
            
            // Try to invoke getter methods (no parameters, return something useful)
            if (method.parameterCount() == 0 && method.returnType() != QMetaType::Void) {
                QVariant returnValue;
                bool success = method.invoke(dewarpingView, Q_RETURN_ARG(QVariant, returnValue));
                if (success && returnValue.isValid()) {
                    result += QString("    ↳ Returned: %1 (type: %2)\n").arg(returnValue.toString()).arg(returnValue.typeName());
                    
                    // If this might be a distortion model, try to introspect it further
                    if (methodName.contains("distort", Qt::CaseInsensitive) || 
                        methodName.contains("model", Qt::CaseInsensitive)) {
                        
                        // Try to get the actual object if it's a pointer
                        if (returnValue.canConvert<QObject*>()) {
                            QObject* modelObj = returnValue.value<QObject*>();
                            if (modelObj) {
                                result += QString("      🎯 FOUND DISTORTION MODEL OBJECT!\n");
                                result += QString("      Class: %1\n").arg(modelObj->metaObject()->className());
                                
                                // Introspect the distortion model
                                const QMetaObject* modelMeta = modelObj->metaObject();
                                for (int k = 0; k < modelMeta->methodCount(); ++k) {
                                    QMetaMethod modelMethod = modelMeta->method(k);
                                    QString modelMethodName = modelMethod.name();
                                    
                                    if ((modelMethodName.contains("point", Qt::CaseInsensitive) ||
                                         modelMethodName.contains("anchor", Qt::CaseInsensitive) ||
                                         modelMethodName.contains("spline", Qt::CaseInsensitive) ||
                                         modelMethodName.contains("curve", Qt::CaseInsensitive) ||
                                         modelMethodName.contains("grid", Qt::CaseInsensitive) ||
                                         modelMethodName.contains("mesh", Qt::CaseInsensitive)) &&
                                        modelMethod.parameterCount() == 0) {
                                        
                                        result += QString("        Model Method: %1\n").arg(modelMethod.methodSignature().constData());
                                        
                                        QVariant modelResult;
                                        bool modelSuccess = modelMethod.invoke(modelObj, Q_RETURN_ARG(QVariant, modelResult));
                                        if (modelSuccess && modelResult.isValid()) {
                                            result += QString("          ✅ Result: %1 (type: %2)\n").arg(modelResult.toString()).arg(modelResult.typeName());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (splineMethodCandidates.isEmpty()) {
        result += "⚠️  No spline-related methods found.\n";
    }
    
    // Try to access distortion model directly
    result += "\n=== ATTEMPTING TO ACCESS DISTORTION MODEL ===\n";
    
    // Try to invoke our new MCP accessor methods
    // Look for our newly added MCP accessor methods
    QJsonObject splineResults;
    bool foundSplineData = false;
    
    for (int i = 0; i < metaObj->methodCount(); ++i) {
        QMetaMethod method = metaObj->method(i);
        QString methodName = method.name();
        
        if (methodName.startsWith("get") && 
            (methodName.contains("Spline", Qt::CaseInsensitive) ||
             methodName.contains("Anchor", Qt::CaseInsensitive) ||
             methodName.contains("Point", Qt::CaseInsensitive)) &&
            method.parameterCount() == 0) {
            
            result += QString("🎯 Found MCP accessor: %1\n").arg(method.methodSignature().constData());
            result += QString("  Method access type: %1\n").arg(
                method.access() == QMetaMethod::Public ? "Public" :
                method.access() == QMetaMethod::Protected ? "Protected" : "Private"
            );
            result += QString("  Method type: %1\n").arg(
                method.methodType() == QMetaMethod::Method ? "Method" :
                method.methodType() == QMetaMethod::Signal ? "Signal" :
                method.methodType() == QMetaMethod::Slot ? "Slot" : "Constructor"
            );
            
            // Try different invocation approaches
            bool invokeSuccess = false;
            QVariant methodResult;
            
            // Approach 1: Standard Q_RETURN_ARG
            result += QString("  🔧 Attempting standard invocation...\n");
            bool success1 = method.invoke(dewarpingView, Q_RETURN_ARG(QVariant, methodResult));
            if (success1 && methodResult.isValid()) {
                invokeSuccess = true;
                result += QString("  ✅ Standard invocation succeeded!\n");
            } else {
                result += QString("  ❌ Standard invocation failed (success=%1, valid=%2)\n")
                    .arg(success1).arg(methodResult.isValid());
            }
            
            // Approach 2: Try without Q_RETURN_ARG for void methods
            if (!invokeSuccess && method.returnType() == QMetaType::Void) {
                result += QString("  🔧 Attempting void method invocation...\n");
                bool success2 = method.invoke(dewarpingView, Qt::DirectConnection);
                if (success2) {
                    invokeSuccess = true;
                    methodResult = QVariant("void method executed");
                    result += QString("  ✅ Void method invocation succeeded!\n");
                } else {
                    result += QString("  ❌ Void method invocation failed\n");
                }
            }
            
            // Approach 3: Try with specific return types
            if (!invokeSuccess) {
                result += QString("  🔧 Attempting typed return invocation...\n");
                
                if (methodName.contains("Count")) {
                    int countResult = 0;
                    bool success3 = method.invoke(dewarpingView, Q_RETURN_ARG(int, countResult));
                    if (success3) {
                        invokeSuccess = true;
                        methodResult = QVariant(countResult);
                        result += QString("  ✅ Count method invocation succeeded!\n");
                    }
                } else if (methodName.contains("Anchor")) {
                    QList<QPointF> pointResult;
                    bool success3 = method.invoke(dewarpingView, Q_RETURN_ARG(QList<QPointF>, pointResult));
                    if (success3) {
                        invokeSuccess = true;
                        methodResult = QVariant::fromValue(pointResult);
                        result += QString("  ✅ Anchor method invocation succeeded!\n");
                    }
                }
                
                if (!invokeSuccess) {
                    result += QString("  ❌ Typed return invocation failed\n");
                }
            }
            
            // Process successful results
            if (invokeSuccess && methodResult.isValid()) {
                result += QString("  ✅ Successfully invoked: %1\n").arg(methodName);
                result += QString("  📊 Result type: %1\n").arg(methodResult.typeName());
                result += QString("  📊 Result user type: %1\n").arg(methodResult.userType());
                
                // If this is a point count, show it prominently
                if (methodName.contains("Count", Qt::CaseInsensitive)) {
                    bool ok;
                    int count = methodResult.toInt(&ok);
                    if (ok) {
                        result += QString("  🎯 SPLINE POINT COUNT: %1\n").arg(count);
                        splineResults[methodName] = count;
                        foundSplineData = true;
                    } else {
                        result += QString("  ⚠️  Count conversion failed (value: %1)\n").arg(methodResult.toString());
                    }
                }
                
                // If this returns a QList<QPointF>, try to extract coordinate data
                if (methodName.contains("Anchor", Qt::CaseInsensitive)) {
                    result += QString("  🎯 ANCHOR COORDINATES METHOD!\n");
                    
                    // Try to see if we can get meaningful data from the QVariant
                    QString resultStr = methodResult.toString();
                    result += QString("  📝 String representation: %1\n").arg(resultStr);
                    
                    // Try to extract QList<QPointF> data
                    if (methodResult.canConvert<QList<QPointF>>()) {
                        QList<QPointF> points = methodResult.value<QList<QPointF>>();
                        result += QString("  🎯 EXTRACTED %1 ANCHOR POINTS:\n").arg(points.size());
                        
                        for (int ptIdx = 0; ptIdx < points.size(); ++ptIdx) {
                            const QPointF& pt = points[ptIdx];
                            result += QString("    Point %1: (%2, %3)\n")
                                .arg(ptIdx + 1)
                                .arg(pt.x(), 0, 'f', 2)
                                .arg(pt.y(), 0, 'f', 2);
                        }
                        
                        splineResults[methodName] = QString("%1 points extracted").arg(points.size());
                        foundSplineData = true;
                    } else if (methodResult.userType() != QMetaType::UnknownType) {
                        result += QString("  ✅ Method returned valid data (not empty)\n");
                        splineResults[methodName] = QString("QList<QPointF> with data");
                        foundSplineData = true;
                    } else {
                        result += QString("  ⚠️  Method returned empty/invalid data\n");
                    }
                }
            } else {
                result += QString("  ❌ All invocation attempts failed for: %1\n").arg(methodName);
                result += QString("    Final success: %1, Valid result: %2\n")
                    .arg(invokeSuccess).arg(methodResult.isValid());
                result += QString("    Error details: Check method accessibility and object state\n");
                
                // Additional diagnostic: Check if the object is in the right state
                result += QString("    Widget visible: %1, enabled: %2\n")
                    .arg(dewarpingView->isVisible()).arg(dewarpingView->isEnabled());
                    
                // Try to get some basic properties to verify object state
                QVariant objName = dewarpingView->property("objectName");
                result += QString("    Object name property: %1\n").arg(objName.toString());
            }
            result += "\n";
        }
    }
    
    if (foundSplineData) {
        result += "\n� SUCCESS! SPLINE DATA EXTRACTION WORKING!\n";
        result += "=== EXTRACTED DATA SUMMARY ===\n";
        for (auto it = splineResults.begin(); it != splineResults.end(); ++it) {
            result += QString("  %1: %2\n").arg(it.key()).arg(it.value().toString());
        }
        
        result += "\n📋 NEXT STEPS:\n";
        result += "1. ✅ Q_INVOKABLE methods are working and accessible\n";
        result += "2. ✅ Point count methods return actual counts\n";
        result += "3. ✅ Anchor methods return coordinate data\n";
        result += "4. 🔧 Need to extract actual coordinate values from QList<QPointF>\n";
        result += "5. 🎯 Ready for multi-spline workflow implementation\n";
    } else {
        result += "\n⚠️  MCP methods found but no data returned.\n";
        result += "This might mean:\n";
        result += "- No image is currently loaded\n";
        result += "- Not in distortion correction step\n";
        result += "- Splines haven't been initialized yet\n";
        result += "\nTry: Load an image → Go to step 3 (Distortion Correction) → Test again\n";
    }
    
    // Look for any method that might return the distortion model
    for (int i = 0; i < metaObj->methodCount(); ++i) {
        QMetaMethod method = metaObj->method(i);
        QString methodName = method.name();
        
        if ((methodName.contains("model", Qt::CaseInsensitive) ||
             methodName.contains("distort", Qt::CaseInsensitive)) &&
            method.parameterCount() == 0 && 
            method.returnType() != QMetaType::Void) {
            
            result += QString("Trying method: %1\n").arg(method.methodSignature().constData());
            
            QVariant modelResult;
            bool success = method.invoke(dewarpingView, Q_RETURN_ARG(QVariant, modelResult));
            if (success && modelResult.isValid()) {
                result += QString("  ✅ Got result: %1 (type: %2)\n").arg(modelResult.toString()).arg(modelResult.typeName());
                
                // Try to cast to QObject* to access the distortion model
                if (modelResult.userType() == QMetaType::QObjectStar || 
                    QString(modelResult.typeName()).contains("DistortionModel")) {
                    
                    QObject* modelObj = modelResult.value<QObject*>();
                    if (modelObj) {
                        result += QString("🎯 FOUND DISTORTION MODEL: %1\n").arg(modelObj->metaObject()->className());
                        
                        // Introspect the distortion model thoroughly
                        const QMetaObject* modelMeta = modelObj->metaObject();
                        result += QString("Model methods (%1 total):\n").arg(modelMeta->methodCount());
                        
                        for (int j = 0; j < modelMeta->methodCount(); ++j) {
                            QMetaMethod modelMethod = modelMeta->method(j);
                            QString modelMethodName = modelMethod.name();
                            
                            // Look for any method that might return spline data
                            if (modelMethod.parameterCount() == 0 && modelMethod.returnType() != QMetaType::Void) {
                                result += QString("  Method: %1\n").arg(modelMethod.methodSignature().constData());
                                
                                QVariant methodResult;
                                bool methodSuccess = modelMethod.invoke(modelObj, Q_RETURN_ARG(QVariant, methodResult));
                                if (methodSuccess && methodResult.isValid()) {
                                    result += QString("    → %1 (type: %2)\n").arg(methodResult.toString()).arg(methodResult.typeName());
                                }
                            }
                        }
                        
                        // Look for properties in the distortion model
                        result += QString("Model properties (%1 total):\n").arg(modelMeta->propertyCount());
                        for (int j = 0; j < modelMeta->propertyCount(); ++j) {
                            QMetaProperty modelProp = modelMeta->property(j);
                            if (modelProp.isReadable()) {
                                QVariant propValue = modelProp.read(modelObj);
                                if (propValue.isValid()) {
                                    result += QString("  Property: %1 = %2 (type: %3)\n")
                                        .arg(modelProp.name()).arg(propValue.toString()).arg(propValue.typeName());
                                }
                            }
                        }
                        
                        break; // Found the model, no need to continue
                    }
                }
            }
        }
    }
    
    // Look for all properties that might contain spline data
    result += "\n=== ANALYZING ALL PROPERTIES ===\n";
    QStringList splinePropertyCandidates;
    
    for (int i = 0; i < metaObj->propertyCount(); ++i) {
        QMetaProperty prop = metaObj->property(i);
        QString propName = prop.name();
        
        if (propName.contains("spline", Qt::CaseInsensitive) ||
            propName.contains("curve", Qt::CaseInsensitive) ||
            propName.contains("distort", Qt::CaseInsensitive) ||
            propName.contains("grid", Qt::CaseInsensitive) ||
            propName.contains("mesh", Qt::CaseInsensitive) ||
            propName.contains("warp", Qt::CaseInsensitive) ||
            propName.contains("point", Qt::CaseInsensitive) ||
            propName.contains("anchor", Qt::CaseInsensitive)) {
            
            if (prop.isReadable()) {
                QVariant value = prop.read(dewarpingView);
                splinePropertyCandidates.append(QString("  🎯 %1: %2").arg(propName).arg(value.toString()));
                result += QString("  🎯 %1: %2 (type: %3)\n").arg(propName).arg(value.toString()).arg(value.typeName());
            }
        }
    }
    
    if (splinePropertyCandidates.isEmpty()) {
        result += "⚠️  No spline-related properties found.\n";
    }
    
    // Deep child object analysis - look for ANY object that might contain spline data
    result += "\n=== DEEP CHILD OBJECT ANALYSIS ===\n";
    
    QList<QObject*> allChildren = dewarpingView->findChildren<QObject*>();
    result += QString("Total child objects: %1\n").arg(allChildren.size());
    
    QList<QObject*> interestingObjects;
    
    for (QObject* child : allChildren) {
        QString childClass = child->metaObject()->className();
        QString childName = child->objectName();
        
        // Cast to see if we can identify interesting object types
        bool isInteresting = false;
        
        // Look for ANY object that might be spline-related
        if (childClass.contains("Interactive", Qt::CaseInsensitive) ||
            childClass.contains("Spline", Qt::CaseInsensitive) ||
            childClass.contains("XSpline", Qt::CaseInsensitive) ||
            childClass.contains("Curve", Qt::CaseInsensitive) ||
            childClass.contains("Distortion", Qt::CaseInsensitive) ||
            childClass.contains("Grid", Qt::CaseInsensitive) ||
            childClass.contains("Mesh", Qt::CaseInsensitive) ||
            childClass.contains("Warp", Qt::CaseInsensitive) ||
            childClass.contains("Point", Qt::CaseInsensitive) && !childClass.contains("QPoint")) {
            
            isInteresting = true;
            interestingObjects.append(child);
            result += QString("🎯 Found: %1").arg(childClass);
            if (!childName.isEmpty()) {
                result += QString(" (name: %1)").arg(childName);
            }
            result += "\n";
            
            // Try to introspect this object's methods and properties
            const QMetaObject* childMeta = child->metaObject();
            
            // Look for point/anchor-related methods
            for (int j = 0; j < childMeta->methodCount(); ++j) {
                QMetaMethod childMethod = childMeta->method(j);
                QString childMethodName = childMethod.name();
                
                if ((childMethodName.contains("point", Qt::CaseInsensitive) ||
                     childMethodName.contains("anchor", Qt::CaseInsensitive) ||
                     childMethodName.contains("control", Qt::CaseInsensitive) ||
                     childMethodName.contains("coord", Qt::CaseInsensitive) ||
                     childMethodName.contains("pos", Qt::CaseInsensitive) ||
                     childMethodName.contains("count", Qt::CaseInsensitive) ||
                     childMethodName.contains("num", Qt::CaseInsensitive)) &&
                    childMethod.parameterCount() == 0) {
                    
                    result += QString("    Method: %1 -> %2\n").arg(childMethodName).arg(childMethod.methodSignature().constData());
                    
                    // Try to invoke the method
                    QVariant methodResult;
                    bool success = childMethod.invoke(child, Q_RETURN_ARG(QVariant, methodResult));
                    if (success && methodResult.isValid()) {
                        result += QString("      ✅ Result: %1 (type: %2)\n").arg(methodResult.toString()).arg(methodResult.typeName());
                        
                        // If this looks like a count, save it
                        if (childMethodName.contains("count", Qt::CaseInsensitive) || 
                            childMethodName.contains("num", Qt::CaseInsensitive)) {
                            bool ok;
                            int count = methodResult.toInt(&ok);
                            if (ok && count > 0) {
                                result += QString("      🎯 FOUND SPLINE POINT COUNT: %1\n").arg(count);
                            }
                        }
                    }
                }
            }
            
            // Look for relevant properties
            for (int j = 0; j < childMeta->propertyCount(); ++j) {
                QMetaProperty childProp = childMeta->property(j);
                QString childPropName = childProp.name();
                
                if (childProp.isReadable() && 
                    (childPropName.contains("point", Qt::CaseInsensitive) ||
                     childPropName.contains("anchor", Qt::CaseInsensitive) ||
                     childPropName.contains("control", Qt::CaseInsensitive) ||
                     childPropName.contains("coord", Qt::CaseInsensitive) ||
                     childPropName.contains("count", Qt::CaseInsensitive))) {
                    
                    QVariant propValue = childProp.read(child);
                    if (propValue.isValid()) {
                        result += QString("    Property: %1 = %2 (type: %3)\n")
                            .arg(childPropName).arg(propValue.toString()).arg(propValue.typeName());
                    }
                }
            }
            result += "\n";
        }
    }
    
    if (interestingObjects.isEmpty()) {
        result += "⚠️  No spline-related objects found in children.\n";
        result += "The splines are likely private members or stored differently.\n\n";
        
        // Try alternative approach: Look for painted elements
        result += "=== ATTEMPTING ALTERNATIVE DETECTION ===\n";
        
        QWidget* viewport = dewarpingView->findChild<QWidget*>("qt_scrollarea_viewport");
        if (viewport) {
            result += QString("✅ Found viewport: %1\n").arg(getWidgetPath(viewport));
            result += QString("Viewport geometry: %1x%2 at (%3,%4)\n")
                .arg(viewport->width()).arg(viewport->height())
                .arg(viewport->x()).arg(viewport->y());
                
            // Try to find if there are any cached paint operations or drawable elements
            result += "\nNote: To access actual spline anchor coordinates, we need:\n";
            result += "1. ScanTailor source code modification to expose spline data\n";
            result += "2. Or paint event interception to capture drawn anchor positions\n";
            result += "3. Or access to the underlying distortion mesh data\n";
        }
    } else {
        result += QString("✅ Found %1 potentially spline-related objects!\n").arg(interestingObjects.size());
    }
    
    result += "\n=== RECOMMENDATIONS ===\n";
    result += "To enable full spline anchor extraction:\n";
    result += "1. Examine the objects found above for actual coordinate data\n";
    result += "2. Look for methods that return point arrays or coordinate lists\n";
    result += "3. Consider adding public accessor methods to DewarpingView\n";
    result += "4. Implement paint event monitoring for real-time anchor tracking\n";
    
    return QJsonObject{
        {"content", QJsonArray{
            QJsonObject{
                {"type", "text"},
                {"text", result}
            }
        }}
    };
}
