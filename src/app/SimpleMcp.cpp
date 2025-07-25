#include "SimpleMcp.h"
#include <QtCore/QJsonArray>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

SimpleMcp::SimpleMcp(QObject* parent)
    : QObject(parent)
    , m_stdinTimer(new QTimer(this))
    , m_serverRunning(false)
{
    // Poll stdin for MCP requests
    connect(m_stdinTimer, &QTimer::timeout, this, &SimpleMcp::processStdinInput);
    m_stdinTimer->setInterval(100); // Check every 100ms
}

void SimpleMcp::start()
{
    if (m_serverRunning) return;
    
    m_serverRunning = true;
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
                QJsonObject{{"name", "find_widget"}, {"description", "Find widget by class/name"}}
            }}
        }},
        {"serverInfo", QJsonObject{
            {"name", "ScanTailor-SimpleMCP"},
            {"version", "1.0.0"}
        }}
    };
    
    sendResponse(init);
}

void SimpleMcp::processStdinInput()
{
    QTextStream stdinStream(stdin);
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
