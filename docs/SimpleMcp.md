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
```bash
export SCANTAILOR_MCP_ENABLE=1
./scantailor-deviant
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

## Implementatie Details

### Lichtgewicht Design
- **~300 lijnen code** vs duizenden in qtmcp
- **Geen externe dependencies** behalve Qt zelf
- **Eenvoudige JSON-RPC over stdio** (MCP standard)
- **Focus op inspection en basis manipulation**

### Qt Integration
Gebruikt standaard Qt introspection:
- `QObject::property()` / `setProperty()`
- `QWidget::findChildren<>()`
- `QMetaObject` reflection
- Bestaande `objectName()` en `metaObject()->className()`

### Performance
- **Lazy evaluation** - alleen scannen wanneer gevraagd
- **Caching mogelijk** voor frequently accessed data
- **Minimal overhead** - alleen actief met environment variable

## Toekomst Uitbreidingen

Eenvoudig uit te breiden met:
- **Event recording** voor user interaction analysis
- **Screenshot capture** voor visual verification
- **Widget state serialization** voor test fixtures
- **Custom ScanTailor specific tools** (project loading, filter settings, etc.)

## Vergelijking

| Feature | qtmcp | SimpleMcp |
|---------|-------|-----------|
| Build complexity | Hoog | Laag |
| Dependencies | Interne Qt tools | Alleen Qt |
| Code size | ~10,000+ lijnen | ~300 lijnen |
| ScanTailor integration | Moeilijk | Eenvoudig |
| Customization | Beperkt | Volledig |
| MCP compliance | Volledig | Core features |

Voor ScanTailor is SimpleMcp de betere keuze omdat het:
1. **Snel te implementeren** was
2. **Precies doet wat we nodig hebben**
3. **Geen build problemen** veroorzaakt
4. **Makkelijk aan te passen** is voor onze specifieke behoeften
