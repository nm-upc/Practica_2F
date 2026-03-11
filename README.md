# 📊 ESP32 Frequency Meter (Medidor de Frecuencia con ESP32)

Este proyecto implementa un **medidor de frecuencia utilizando un ESP32**.  
El sistema mide el **tiempo entre interrupciones en un pin digital**, almacena esos tiempos en una **cola circular**, y calcula:

- Frecuencia **máxima**
- Frecuencia **mínima**
- Frecuencia **promedio**

Los resultados se visualizan en **una página web generada por el propio ESP32**.

---

# 🚀 Características

- Medición de frecuencia mediante **interrupciones hardware**
- Uso de **micros()** para medir el período entre pulsos
- **Cola circular** para almacenar muestras de período
- Cálculo de:
  - `Fmax`
  - `Fmin`
  - `Favg`
- Servidor web integrado
- Interfaz web con **actualización automática**
- ESP32 funcionando como **Access Point (AP)**

---

# 🧰 Hardware necesario

- ESP32 (compatible con el código)
- Generador de señal / sensor / circuito que genere pulsos
- Cableado básico

### Pin utilizado

| Pin | Función |
|----|----|
| GPIO 18 | Entrada de señal de frecuencia |

---

# 📡 Configuración WiFi

El ESP32 crea su propia red WiFi:
