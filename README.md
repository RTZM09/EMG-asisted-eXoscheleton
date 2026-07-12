# NeuroGrip

NeuroGrip este un sistem robotic bio-ingineresc de tip exoschelet, proiectat pentru asistarea sau reabilitarea mișcărilor de flexie și extensie ale degetelor. Dispozitivul preia potențialele electrice generate de contracțiile musculare prin intermediul a doi senzori EMG (echipați cu punți de achiziție analogică de tip Wheatstone), clasifică intenția utilizatorului în timp real și acționează servomotoarele geometriei degetelor.

Sistemul este dezvoltat pe o arhitectură hardware bazată pe microcontrolerul **ESP32** și structuri portante realizate prin printare 3D.

#### Documentația detaliată poate fi gasită în `NeuroGrip.pdf`.

---

##  Caracteristici Tehnice și Algoritmice

*   **Eșantionare de Înaltă Frecvență:** Achiziție de semnal brut la intervale stricte de $1\text{ ms}$ ($1\text{ kHz}$) prin ferestre glisante (Window Size: 100 eșantioane, Step Size: 50 eșantioane).
*   **Extracție de Caracteristici Biologice:** Calculul în timp real al Valorii Medii Absolute (**MAV**) și al Valorii Medii Pătratice (**RMS**) pentru ambele canale de intrare.
*   **Clasificare prin Distanță Euclidiană (KNN):** Maparea vectorilor de caracteristici în raport cu 5 centroizi precalculați de calibrare (REPAUS, PUMN, ARĂTĂTOR, MIJLOCIU, INELAR).
*   **Filtrare Matematică și Vot Majoritar:** Implementarea unei urne de vot static pentru ultimele 7 decizii consecutive, eliminând fluctuațiile parazite sau zgomotul electromagnetic de înaltă frecvență.
*   **Power Management Algoritmic (Eșalonare):** Acționare secvențială a servomotoarelor la intervale de $50\text{ ms}$ pentru a preveni prăbușirea tensiunii liniei de alimentare (Voltage Sag) cauzată de rezistența internă a bateriilor AA.
*   **Inversare Cinematică Hardware:** Mască software dedicată pentru adaptarea sensului de rotație al servomotoarelor specifice (Inelar și Mic), asigurând coerența mecanică a mișcării.

---

##  Structura Proiectului 
*  `EMG-app_main.cpp` - Singurul cod necesar pentru ESP32.
*  `EMG-app_git.zip` - Arhiva cu codul pentru Android Studio al aplicației de mobil.
---
*   `main.cpp` — Codul principal de producție (clasificare în timp real, state machine și control PWM pe 12 biți via LEDC API).
*   `data_collection.cpp` — Scriptul dedicat achiziției datelor brute de la senzori, utilizat pentru generarea seturilor de date și extragerea coordonatelor pentru `centroizi[]`.
---
*   `electrical.png1` - Schema electrica a proiectului
*   `assembley.step` - Fisier CAD al unui deget mecatronic
---

##  Modelul Matematic Utilizat

Pentru fiecare fereastră analizată, clasificarea se realizează prin minimizarea distanței euclidiene cvadridimensionale față de profilele musculare stocate:

$$d = \sqrt{(MAV_1 - c.mav_1)^2 + (RMS_1 - c.rms_1)^2 + (MAV_2 - c.mav_2)^2 + (RMS_2 - c.rms_2)^2}$$

Unde $c$ reprezintă coordonatele centroidului testat. Decizia finală este validată de filtrul statistic Mode (Vot Majoritar).

---

## 🛠️ Specificații Hardware și Conexiuni

### Alocare Pini ESP32:
*   **Senzor EMG 1 (Canal Principal):** GPIO 34 (ADC1)
*   **Senzor EMG 2 (Canal Secundar):** GPIO 35 (ADC1)
*   **Servo Deget Mare:** GPIO 18 (PWM)
*   **Servo Deget Arătător:** GPIO 23 (PWM)
*   **Servo Deget Mijlociu:** GPIO 19 (PWM)
*   **Servo Deget Inelar:** GPIO 22 (PWM) 
*   **Servo Deget Mic:** GPIO 21 (PWM) 

> !!! **Notă:** Ground-ul (GND) sursei externe de alimentare (pachetul de baterii AA de ~6V) trebuie conectat obligatoriu la pinul GND al ESP32(NU COMUN CU SENZORII) pentru a asigura o masă comună de potențial.

---

##  Flux de Lucru 
*Opțiunea 1 (Calibrare manuală)*
  1.  **Etapa de Calibrare:** Se rulează fișierul de colectare a datelor (`data_collection.cpp`) în timp ce subiectul execută izometric cele 5 gesturi țintă. Datele obținute sunt procesate statistic pentru a extrage noile coordonate ale centroizilor.
  2.  **Actualizare:** Valorile MAV și RMS rezultate se copiază în matricea `centroizi[]` din fișierul principal.
  3.  **Execuție:** Se uploadează `main.cpp` pentru operarea autonomă a exoscheletului AXIS.

*Opțiunea 2 (Calibrare în aplicație)*
  1. Se descarcă `EMG-app_main.cpp` pentru EPS32.
  2. Se descarcă și se deschide `EMG-app_git.zip` folosind Android Studio.
  3. După ce se instalează aplicația pe telefon, se urmează pașii de pe ecran.
