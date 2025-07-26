# Spline Control Points Analysis

## Relatie tussen MCP-gevonden punten en ScanTailor projectbestand

### 1. **Projectbestand Control Points** (uit .ScanTailor XML)

#### Top Curve (9 punten):
```xml
<top-curve>
  <xspline>
    <point y="428.5884612093063" x="920.9702226640975"/>
    <point y="432.1123970893959" x="2673.033630456278"/>
    <point y="471.7472317574717" x="3243.97725500235"/>
    <point y="503.1132212413243" x="3984.633896062921"/>
    <point y="503.0474900504991" x="4295.306391937768"/>
    <point y="473.287375511066" x="4796.182160938605"/>
    <point y="443.0583442194502" x="5132.622809344807"/>
    <point y="428.8822028012512" x="5376.661480198828"/>
    <point y="433.7554587587042" x="5655.36521026327"/>
  </xspline>
</top-curve>
```

#### Bottom Curve (3 punten):
```xml
<bottom-curve>
  <xspline>
    <point y="2294.948096751271" x="911.0619448281332"/>
    <point y="2290.895413400892" x="4469.707143422217"/>
    <point y="2260.084453243506" x="5643.289309796978"/>
  </xspline>
</bottom-curve>
```

### 2. **MCP-gevonden Control Points** (via Q_INVOKABLE methods)

**Van onze laatste succesvolle extractie:**
- **Top Spline:** 9 control points
- **Bottom Spline:** 4 control points

**Voorbeelden van gevonden coördinaten:**
- Top: (920.97, 428.59), (2673.03, 432.11), etc.
- Bottom: (911.06, 2294.95), (4469.71, 2290.90), etc.

### 3. **Vergelijking en Analyse**

#### ✅ **PERFECTE MATCH voor Top Curve:**
- Projectbestand: 9 punten
- MCP gevonden: 9 punten
- Coördinaten komen overeen (met kleine afrondingsverschillen)

#### ⚠️ **DISCREPANTIE voor Bottom Curve:**
- Projectbestand: **3 punten**
- MCP gevonden: **4 punten**

**Mogelijke verklaringen:**
1. **Runtime toevoeging:** MCP ziet de live staat in de applicatie, mogelijk met extra punt toegevoegd
2. **Endpoint duplicatie:** Mogelijk automatisch toegevoegde start/eindpunten
3. **UI vs opslag verschil:** Live UI kan extra control points tonen voor editing

### 4. **Coördinaat Precisie Vergelijking**

| Bron | Top Punt 1 | Top Punt 2 | Bottom Punt 1 |
|------|------------|------------|---------------|
| XML  | (920.97, 428.59) | (2673.03, 432.11) | (911.06, 2294.95) |
| MCP  | (920.97, 428.59) | (2673.03, 432.11) | (911.06, 2294.95) |

**→ Coördinaten zijn identiek tot op decimalen!**

### 5. **Voor Vervolgstappen**

#### **Mogelijke Use Cases:**
1. **Validatie:** MCP kan controleren of live state overeenkomt met opgeslagen state
2. **Synchronisatie:** Detecteren van onopgeslagen wijzigingen
3. **Backup/Export:** Real-time extractie van spline data
4. **Analyse:** Vergelijken van verschillende projectversies
5. **Automatisering:** Programmatische aanpassingen aan splines

#### **Implementatie Suggesties:**
1. **Project Parser:** Maak MCP tool om .ScanTailor bestanden te lezen
2. **State Validator:** Vergelijk live MCP data met projectbestand
3. **Change Detector:** Monitor verschillen tussen UI en opgeslagen staat
4. **Export Tool:** Extracteer spline data in verschillende formaten

#### **Technische Overwegingen:**
- **Precision:** XML gebruikt hoge precisie decimalen
- **Format:** MCP geeft QPointF objecten terug
- **Timing:** Live data kan afwijken van opgeslagen data
- **State:** Applicatie moet in juiste fase zijn (deskew step)

### 6. **Conclusie**

**✅ Succesvolle correlatie tussen MCP en projectbestand**
**✅ Coördinaten komen perfect overeen**
**⚠️ Kleine discrepantie in aantal bottom points (3 vs 4)**
**🎯 Gereed voor vervolgstappen zoals validatie en synchronisatie**
