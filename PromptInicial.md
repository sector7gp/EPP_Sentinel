# Requerimientos Funcionales – Sistema Autónomo de Detección de Elementos de Protección Personal (EPP)

## 1. Objetivo del Sistema

Desarrollar un sistema autónomo basado en Raspberry Pi Zero con cámara web HD que permita capturar imágenes de operarios en intervalos configurables, analizarlas mediante modelos de Inteligencia Artificial y determinar si el operario utiliza correctamente los Elementos de Protección Personal (EPP) requeridos para su función.

El sistema deberá operar de manera automática, configurable y desacoplada del proveedor de IA utilizado.

---

# 2. Arquitectura General

El sistema estará compuesto por los siguientes módulos:

1. Módulo de captura de imágenes
2. Módulo de configuración
3. Módulo de análisis mediante IA
4. Módulo de almacenamiento y logs
5. Frontend web de administración
6. API Backend
7. Sistema de configuración de perfiles de operarios/EPP

---

# 3. Requerimientos Funcionales

## RF-001 – Captura automática de imágenes

El sistema deberá capturar imágenes automáticamente utilizando una webcam conectada a la Raspberry Pi Zero.

### Características:

* La frecuencia de captura deberá ser configurable.
* El intervalo podrá definirse en segundos o minutos.
* El sistema deberá funcionar de manera autónoma sin intervención humana.
* La captura deberá ejecutarse únicamente dentro de una franja horaria configurable.

### Parámetros configurables:

* Hora de inicio
* Hora de finalización
* Intervalo entre capturas
* Días habilitados

---

## RF-002 – Configuración de calidad de imagen

El sistema deberá permitir configurar la calidad y resolución de las imágenes capturadas.

### Objetivos:

* Optimizar el consumo de ancho de banda
* Reducir costos de procesamiento IA
* Minimizar consumo de tokens en APIs multimodales

### Requisitos:

* Soporte para resolución HD configurable
* Compresión automática de imagen
* Balance entre calidad visual y peso del archivo

---

## RF-003 – Subida automática de imágenes

El sistema deberá subir automáticamente las imágenes capturadas al backend o servicio de análisis.

### Requisitos:

* Subida vía HTTPS
* Reintento automático ante fallos de conectividad
* Cola local temporal en caso de desconexión
* Confirmación de recepción

---

## RF-004 – Análisis de imágenes mediante IA

El sistema deberá analizar las imágenes utilizando servicios de Inteligencia Artificial multimodal.

### Objetivos del análisis:

* Detectar presencia o ausencia de EPP
* Determinar cumplimiento de seguridad
* Generar resultados estructurados

### Elementos de Protección Personal configurables:

* Casco de seguridad
* Gafas de seguridad
* Protección auditiva
* Guantes de seguridad
* Calzado de seguridad
* Ropa de trabajo / industrial
* Protección respiratoria
* Chaleco reflectivo

---

## RF-005 – Configuración dinámica de EPP por perfil de operario

El sistema deberá permitir definir distintos perfiles de operarios.

Cada perfil determinará qué elementos de protección personal son obligatorios.

### Ejemplos:

#### Perfil: Operario de Planta

EPP requeridos:

* Casco
* Chaleco reflectivo
* Calzado de seguridad
* Guantes

#### Perfil: Soldador

EPP requeridos:

* Protección respiratoria
* Gafas
* Guantes
* Ropa industrial

---

## RF-006 – Generación dinámica de prompts para IA

El sistema deberá generar automáticamente el prompt enviado al proveedor de IA según el perfil configurado.

### Requisitos:

* El prompt deberá construirse dinámicamente
* El sistema deberá soportar distintos formatos según proveedor
* El resultado deberá devolverse en formato estructurado JSON

### Ejemplo de salida esperada:

```json
{
  "casco_seguridad": true,
  "gafas_seguridad": false,
  "chaleco_reflectivo": true,
  "cumple_normativa": false,
  "observaciones": "Operario sin gafas de seguridad"
}
```

---

# 4. Compatibilidad con proveedores de IA

## RF-007 – Abstracción de proveedor de IA

La arquitectura deberá desacoplar la lógica de análisis del proveedor utilizado.

### Proveedores soportados inicialmente:

* OpenAI
* Anthropic Claude
* Google Gemini

### Requisitos:

* Configuración mediante API Key
* Selección de proveedor desde frontend
* Cambio dinámico sin reinicio
* Arquitectura extensible para nuevos proveedores

---

# 5. Frontend de Administración

## RF-008 – Panel de configuración

El sistema deberá incluir una interfaz web responsive para administración.

### Funcionalidades:

* Configurar horarios
* Configurar intervalos
* Configurar calidad de imagen
* Configurar perfiles de operarios
* Configurar EPP obligatorios
* Configurar proveedor de IA
* Configurar API Keys

---

## RF-009 – Dashboard de análisis

El sistema deberá mostrar un historial de análisis realizados.

### Información visible:

* Fecha y hora
* Imagen capturada
* Resultado del análisis
* EPP detectados
* Estado de cumplimiento
* Observaciones IA

### Funcionalidades:

* Filtros por fecha
* Filtros por cumplimiento
* Búsqueda
* Exportación de resultados

---

# 6. Logs y Auditoría

## RF-010 – Registro de eventos

El sistema deberá almacenar logs operativos y de análisis.

### Eventos mínimos:

* Captura de imagen
* Fallo de captura
* Subida exitosa
* Error de conexión
* Respuesta IA
* Cambios de configuración

---

# 7. Almacenamiento

## RF-011 – Persistencia de datos

El sistema deberá almacenar:

* Configuraciones
* Logs
* Resultados de análisis
* Imágenes capturadas
* Historial de cumplimiento

---

# 8. Requerimientos No Funcionales

## RNF-001 – Operación autónoma

El sistema deberá reiniciar automáticamente luego de cortes eléctricos o reinicios de la Raspberry Pi.

---

## RNF-002 – Bajo consumo de recursos

La solución deberá estar optimizada para ejecutarse en Raspberry Pi Zero.

### Consideraciones:

* Bajo uso de CPU
* Bajo consumo de memoria
* Procesamiento liviano local
* Delegación del análisis a la nube

---

## RNF-003 – Seguridad

### Requisitos:

* Comunicación HTTPS
* Encriptación de API Keys
* Autenticación de administrador
* Protección de acceso al frontend

---

## RNF-004 – Escalabilidad

La arquitectura deberá permitir agregar múltiples dispositivos Raspberry Pi en el futuro.

---

# 9. Arquitectura Técnica Sugerida

## Hardware

* Raspberry Pi Zero 2 W
* Webcam HD USB
* Fuente estable
* Conectividad WiFi

---

## Backend sugerido

* Node.js / Python
* API REST
* SQLite o PostgreSQL

---

## Frontend sugerido

* React
* Vue.js
* Panel responsive

---

## Comunicación IA

* APIs REST multimodales
* Formato JSON estructurado

---

# 10. Flujo Operativo

1. La Raspberry captura una imagen
2. La imagen se comprime y optimiza
3. La imagen se sube al backend
4. El backend genera el prompt dinámico
5. Se envía la imagen al proveedor IA
6. La IA devuelve el análisis
7. El sistema registra el resultado
8. El frontend actualiza el dashboard
9. Se almacena el historial y logs

---

# 11. Posibles Funcionalidades Futuras

* Alertas automáticas por incumplimiento
* Integración con WhatsApp/Telegram
* Detección de múltiples personas
* Reconocimiento facial
* Métricas por operario
* Dashboard estadístico
* Integración con CCTV/IP Cameras
* Modo offline con IA local
* Entrenamiento de modelos personalizados
