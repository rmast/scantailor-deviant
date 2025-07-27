# SimpleMcp - Lightweight MCP Server for ScanTailor

## Overzicht

SimpleMcp is een eenvoudige Model Context Protocol (MCP) server die Qt UI elementen van ScanTailor blootlegt aan externe tools zoals GitHub Copilot. Dit is een veel lichter alternatief voor qtmcp die specifiek voor onze behoeften is ontworpen.

## Waarom SimpleMcp vs qtmcp?

### qtmcp problemen:
- ❌ Complexe build dependencies (interne Qt tools)
- ❌ Conflicten met externe CMake projecten  
- ❌ Overkill - ondersteunt alle mogelijke Qt features
- ❌ Moeilijk te integreren in bestaande projecten

### SimpleMcp voordelen:
- ✅ Lightweight - alleen wat we nodig hebben
- ✅ Eenvoudige integratie - gewoon twee bestanden
- ✅ Geen extra dependencies
- ✅ Werkt met externe CMake builds
- ✅ Aangepast voor ScanTailor specifieke behoeften

## Gebruik

### Activeren

Er zijn nu meerdere manieren om SimpleMcp te activeren:

**Optie 1: Environment variabelen (originele methode)**
```bash
export SCANTAILOR_MCP_ENABLE=1
./scantailor-deviant
```

**Optie 2: Command line flag (nieuwe methode)**
```bash
# Voor MCP batch mode (JSON-RPC via stdin)
SCANTAILOR_MCP_BATCH=1 ./scantailor-deviant --mcp-mode

# Voor GUI mode met TCP server
./scantailor-deviant --mcp-mode
```

**Voor Ubuntu 24.04 gebruikers:**
Nu volledig compatibel met Qt 6.4.2 uit apt:
```bash
sudo apt install qt6-base-dev
# Geen Qt 6.8.1 compilatie nodig!
```

### MCP Capabilities

#### 1. UI Introspection
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "introspect_ui",
    "arguments": {}
  }
}
```

Geeft een complete lijst van alle UI widgets met:
- Widget paths (zoals "MainWindow/centralWidget/button1")
- Class names (QPushButton, QLineEdit, etc.)
- Object names
- Visibility en enabled status
- Geometrie informatie
- Alle properties

#### 2. Property Reading
```json
{
  "jsonrpc": "2.0", 
  "method": "tools/call",
  "params": {
    "name": "get_property",
    "arguments": {
      "objectPath": "MainWindow/centralWidget/someButton",
      "property": "text"
    }
  }
}
```

#### 3. Property Setting (voor testing)
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call", 
  "params": {
    "name": "set_property",
    "arguments": {
      "objectPath": "MainWindow/centralWidget/someButton",
      "property": "text",
      "value": "New Button Text"
    }
  }
}
```

#### 4. Widget Search
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "find_widget",
    "arguments": {
      "className": "QPushButton",
      "objectName": "btnProcess"
    }
  }
}
```

#### 5. Spline Analysis & Validation (Nieuwe Features!)

**Count Spline Points**
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "count_spline_points",
    "arguments": {}
  }
}
```

**Get Actual Spline Coordinates**
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "get_spline_anchors",
    "arguments": {}
  }
}
```

**Validate Project vs Live Data**
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "validate_project_splines",
    "arguments": {
      "projectFile": "/path/to/project.ScanTailor"
    }
  }
}
```

**UI Navigation Recording**
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "start_recording",
    "arguments": {}
  }
}
```

## Real-World Voorbeeld

**TCP Server Gebruik:**
```bash
# Start ScanTailor met MCP support (from project root)
build-qt642/src/app/scantailor-deviant --mcp-mode

# In een andere terminal, test spline validatie:
echo '{"jsonrpc": "2.0", "method": "tools/call", "params": {"name": "validate_project_splines", "arguments": {"projectFile": "/path/to/project.ScanTailor"}}, "id": 1}' | nc localhost <PORT>
```

**Resultaat voorbeeld:**
```
=== LIVE APPLICATION DATA ===
Live Top Curve: 9 points
  Point 1: (920.97, 428.59)
  Point 2: (2673.03, 432.11)
  Point 3: (3243.98, 471.75)
  Point 4: (3984.63, 503.11)
  Point 5: (4295.31, 503.05)
  Point 6: (4796.18, 473.29)
  Point 7: (5132.62, 443.06)
  Point 8: (5376.66, 428.88)
  Point 9: (5655.37, 433.76)

Live Bottom Curve: 4 points
  Point 1: (911.06, 2294.95)
  Point 2: (4469.71, 2290.90)
  Point 3: (4970.86, 2274.55)
  Point 4: (5643.29, 2260.08)

✅ Live spline data extracted successfully.
✅ Project file parsing successful
⚠️  Data inconsistency detected (Project: 3 bottom points vs Live: 4 bottom points)
💾 Consider saving project to sync data
```

## Use Cases voor GitHub Copilot

### 1. Code Analysis
Copilot kan nu zien:
- Welke UI elementen beschikbaar zijn
- Hoe ze georganiseerd zijn in de widget hiërarchie
- Wat hun properties zijn
- Of elementen enabled/visible zijn

### 2. Automated Testing
```cpp
// Copilot kan tests genereren die:
SimpleMcp* mcp = findMcp();
mcp->setProperty("MainWindow/btnStart", "enabled", false);
// Verifieer dat start button disabled is
QVERIFY(!startButton->isEnabled());
```

### 3. UI Documentation
Copilot kan automatisch UI documentatie genereren op basis van de widget tree.

### 4. Accessibility Analysis
Door alle widget properties te kunnen inspecteren, kan Copilot:
- Controleren op ontbrekende tooltips
- Verifiëren van accessible names
- Analyseren van keyboard navigation

### 5. ScanTailor Spline Analysis (Nieuwe Capabilities!)
Copilot kan nu:
- **Spline gegevens extraheren** uit project bestanden
- **Live spline data vergelijken** met opgeslagen project data
- **UI navigatie opnemen** voor spline interactie analyse
- **Dewarping workflows automatiseren** met precieze coördinaten

## Implementatie Details

### Lichtgewicht Design
- **~1000+ lijnen code** met geavanceerde spline features
- **Geen externe dependencies** behalve Qt zelf
- **Dubbele interface**: JSON-RPC over stdio EN TCP server
- **Focus op ScanTailor workflows** en spline automatisering

### Qt Integration
Gebruikt standaard Qt introspection:
- `QObject::property()` / `setProperty()`
- `QWidget::findChildren<>()`
- `QMetaObject` reflection met `Q_INVOKABLE` methods ✅ **WORKING**
- `QApplication::eventFilter()` voor UI recording
- Bestaande `objectName()` en `metaObject()->className()`

**✅ Live Spline Data Extraction:** 
De Q_INVOKABLE methods in DewarpingView werken perfect:
- `getTopSplineControlPointCount()` / `getTopSplineControlPoints()`
- `getBottomSplineControlPointCount()` / `getBottomSplineControlPoints()`
- Real-time coördinaten extractie uit actieve dewarping views
- Qt 6.4.2 MOC compatibiliteit gevalideerd

### ScanTailor Specifieke Features
- **Project file parsing** (XML format)
- **Dewarping spline extraction** 
- **Live UI state monitoring**
- **Qt 6.4.2 compatibiliteit** voor Ubuntu 24.04

### Performance
- **Lazy evaluation** - alleen scannen wanneer gevraagd
- **Caching mogelijk** voor frequently accessed data
- **Minimal overhead** - alleen actief met environment variable

## Toekomst Uitbreidingen

Eenvoudig uit te breiden met:
- ✅ **Event recording** - geïmplementeerd!
- **Screenshot capture** voor visual verification
- **Widget state serialization** voor test fixtures
- ✅ **Custom ScanTailor specific tools** - spline analysis geïmplementeerd!

## Vergelijking

| Feature | qtmcp | SimpleMcp |
|---------|-------|-----------|
| Build complexity | Hoog | Laag |
| Dependencies | Interne Qt tools | Alleen Qt |
| Code size | ~10,000+ lijnen | ~1000+ lijnen |
| ScanTailor integration | Moeilijk | ✅ Eenvoudig |
| Customization | Beperkt | ✅ Volledig |
| MCP compliance | Volledig | ✅ Core features + extras |
| Spline analysis | ❌ Geen | ✅ **VOLLEDIG WERKEND** |
| Project file parsing | ❌ Geen | ✅ XML parsing |
| Qt 6.4.2 support | ❌ Onbekend | ✅ Ubuntu 24.04 ready |
| Command line options | ❌ Beperkt | ✅ --mcp-mode |
| Live data extraction | ❌ Geen | ✅ **Q_INVOKABLE methods** |

Voor ScanTailor is SimpleMcp duidelijk de betere keuze omdat het:
1. ✅ **Snel geïmplementeerd** was
2. ✅ **Meer doet dan we nodig hebben** - spline analyse bonus!
3. ✅ **Geen build problemen** veroorzaakt
4. ✅ **Perfect aangepast** voor ScanTailor workflows
5. ✅ **Ubuntu 24.04 compatible** met Qt 6.4.2
