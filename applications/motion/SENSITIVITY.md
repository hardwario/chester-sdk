# Motion Sensitivity Configuration

Tato dokumentace popisuje konfiguraci citlivosti detekce pohybu pro CHESTER motion aplikaci s CTR-S3 PIR senzory.

## Předdefinované režimy

Aplikace nabízí tři předdefinované režimy citlivosti, které vyvažují rychlost detekce a odolnost proti falešným poplachům:

### Low (Nízká citlivost)
**Parametry:**
- `sensitivity`: 32
- `blind_time`: 3 s
- `pulse_counter`: 3
- `window_time`: 4 s

**Charakteristika:**
- Nejvyšší odolnost proti falešným poplachům
- Nejpomalejší detekce
- Vyžaduje více potvrzení před vyvoláním události
- Vhodné pro prostředí s častým výskytem rušivých vlivů (větve stromů, malá zvířata, atd.)

### Medium (Střední citlivost) - **VÝCHOZÍ**
**Parametry:**
- `sensitivity`: 64
- `blind_time`: 2 s
- `pulse_counter`: 2
- `window_time`: 2 s

**Charakteristika:**
- Vyvážený poměr mezi rychlostí detekce a odolností proti falešným poplachům
- Výchozí nastavení aplikace
- Vhodné pro běžné použití ve vnitřních i venkovních prostorách

### High (Vysoká citlivost)
**Parametry:**
- `sensitivity`: 32
- `blind_time`: 1 s
- `pulse_counter`: 1
- `window_time`: 0 s

**Charakteristika:**
- Nejrychlejší detekce
- Nejvyšší citlivost na pohyb
- Vyšší riziko falešných poplachů
- Vhodné pro aplikace vyžadující okamžitou reakci (bezpečnostní systémy, dveřní čidla)

## Individual režim

Režim `individual` umožňuje ruční nastavení všech parametrů detekce pro pokročilé uživatele.

### Parametry

#### `motion-sens` (Citlivost senzoru)
**Rozsah:** 1-255
**Výchozí hodnota:** 64

**Popis:**
Určuje citlivost PIR senzoru na infračervené záření. Nižší hodnoty znamenají vyšší citlivost.

**Efekt na detekci:**
- **Nízké hodnoty (1-50):** Vyšší citlivost, detekuje i menší objekty nebo pohyb ve větší vzdálenosti
- **Střední hodnoty (51-100):** Standardní citlivost pro běžné použití
- **Vysoké hodnoty (101-255):** Nižší citlivost, detekuje pouze větší objekty nebo pohyb blízko senzoru

**Doporučení:**
- Pro vnitřní prostory: 50-80
- Pro venkovní prostory bez rušení: 40-60
- Pro venkovní prostory s možným rušením: 70-100

---

#### `motion-blind` (Mrtvá doba)
**Rozsah:** 0-10 sekund
**Výchozí hodnota:** 2 s

**Popis:**
Časový interval po detekci pohybu, během kterého jsou další detekce ignorovány. Brání opakovanému vyvolání události stejným pohybem.

**Efekt na detekci:**
- **0 s:** Žádná mrtvá doba, každý pulz může vyvolat událost (riziko duplicitních detekcí)
- **1-2 s:** Minimální mrtvá doba, rychlá reakce na nový pohyb
- **3-5 s:** Střední mrtvá doba, potlačení duplicit
- **6-10 s:** Dlouhá mrtvá doba, výrazné snížení frekvence detekcí

**Doporučení:**
- Pro rychlou detekci: 1-2 s
- Pro běžné použití: 2-3 s
- Pro filtraci častých pohybů: 4-6 s

---

#### `motion-pulse` (Počet pulsů)
**Rozsah:** 1-10
**Výchozí hodnota:** 2

**Popis:**
Minimální počet detekčních pulsů potřebných k vyvolání události detekce pohybu v rámci časového okna `motion-window`.

**Efekt na detekci:**
- **1:** Jeden pulz stačí k detekci (nejrychlejší, nejvíce falešných poplachů)
- **2-3:** Vyžaduje potvrzení (vyvážené nastavení)
- **4-6:** Vyžaduje vícenásobné potvrzení (pomalejší, ale spolehlivější)
- **7-10:** Vyžaduje opakovaný pohyb (minimální falešné poplachy)

**Doporučení:**
- Pro okamžitou detekci: 1
- Pro běžné použití: 2-3
- Pro vysokou spolehlivost: 3-5

---

#### `motion-window` (Časové okno)
**Rozsah:** 0-10 sekund
**Výchozí hodnota:** 2 s

**Popis:**
Časové okno, během kterého musí být zaznamenán požadovaný počet pulsů (`motion-pulse`). Pokud je `motion-window` = 0, časové omezení neplatí.

**Efekt na detekci:**
- **0 s:** Žádné časové omezení, pulsy se počítají bez ohledu na čas
- **1-2 s:** Krátké okno, vyžaduje rychlé opakování pohybu
- **3-5 s:** Střední okno, umožňuje pomalejší pohyb
- **6-10 s:** Dlouhé okno, detekuje i velmi pomalý pohyb

**Doporučení:**
- Pro rychlý pohyb: 1-2 s
- Pro běžné použití: 2-3 s
- Pro pomalý pohyb: 4-6 s
- Bez časového omezení: 0 s

---

## Nastavení parametrů

### Přepnutí režimu
```
config sensitivity <low|medium|high|individual>
```

### Nastavení individuálních parametrů
(Vyžaduje režim `individual`)

```
config motion-sens <1-255>
config motion-blind <0-10>
config motion-pulse <1-10>
config motion-window <0-10>
```

### Příklady použití

#### Rychlá detekce pro dveře
```
config sensitivity individual
config motion-sens 50
config motion-blind 1
config motion-pulse 1
config motion-window 0
```

#### Spolehlivá venkovní detekce
```
config sensitivity individual
config motion-sens 80
config motion-blind 3
config motion-pulse 3
config motion-window 4
```

#### Detekce velmi pomalého pohybu
```
config sensitivity individual
config motion-sens 40
config motion-blind 2
config motion-pulse 2
config motion-window 6
```

---

## Vztah mezi parametry

### Citlivost vs. Spolehlivost
- **Vyšší citlivost** (nižší `motion-sens`, nižší `motion-pulse`, delší `motion-window`) = rychlejší detekce, ale více falešných poplachů
- **Nižší citlivost** (vyšší `motion-sens`, vyšší `motion-pulse`, kratší `motion-window`) = pomalejší detekce, ale méně falešných poplachů

### Filtrační řetězec
1. **PIR senzor** zachytí IR změnu → generuje pulz (citlivost řízena `motion-sens`)
2. **Blind time** filtr ignoruje pulzy během mrtvé doby (`motion-blind`)
3. **Pulse counter** počítá validní pulzy v časovém okně (`motion-pulse` + `motion-window`)
4. **Detekce pohybu** je vyvolána po dosažení požadovaného počtu pulsů

---

## Implementační poznámky

- Parametry jsou uloženy v persistentní konfiguraci (NVS)
- Změny parametrů vyžadují restart aplikace nebo reinicializaci CTR-S3 senzoru
- Oba kanály (L a R) PIR senzoru používají stejnou konfiguraci
- V režimech `low`, `medium`, `high` nejsou individuální parametry viditelné v `config show`
- Parametry `motion-*` jsou zobrazeny pouze v režimu `individual`

---

## Odkazy na kód

- Definice režimů: `src/app_init.c:87-119`
- Konfigurace parametrů: `src/app_config.c:41-50`
- Enum režimů: `src/app_config.h:28-33`
