# QYMERA MAIN - PROFESSIONALIZATION ROADMAP

## Objetivo

Trabajar **exclusivamente sobre la rama `main`** del repositorio `Qymeras-1.1`.

El objetivo de este trabajo es convertir `main` en una **base profesional, estable, mantenible y arquitectónicamente coherente para Qymera**, sin incorporar características pertenecientes a otras líneas de producto.

### Alcance estricto

Este roadmap aplica únicamente a:

* `main`
* firmware Arduino
* ESP8266
* ESP32
* arquitectura actual de runtime
* sensores
* actuadores
* automatizaciones
* persistencia
* networking
* protocolo mesh
* API HTTP
* seguridad
* logging
* Web UI
* tests
* documentación técnica del producto base

### Fuera de alcance

**NO modificar, portar ni fusionar conceptos de:**

* `feature/ai-experiments`
* cualquier branch experimental de AI
* arquitectura ESP-IDF
* Qymera Dashboard
* Qymera Link
* AI sensors / AIDIG / AIANA
* proveedor LLM
* browser agent
* cloud integrations
* Matter
* MQTT
* Zigbee/Z-Wave
* mobile app

Estas líneas pertenecen a otros productos del mismo ecosystem/environment y deben permanecer aisladas.

---

# PRINCIPIO CENTRAL

No agregar features por agregar features.

La prioridad es transformar Qymera desde:

> "firmware que funciona"

hacia:

> "runtime distribuido de automatización local con arquitectura explícita, contratos claros y comportamiento predecible".

La estabilidad y claridad arquitectónica tienen prioridad sobre nuevas capacidades.

---

# ROADMAP

## PHASE 0 - BASELINE Y CONTROL DE ALCANCE

Antes de modificar código:

1. Inspeccionar completamente `main`.
2. Generar un mapa actualizado de:
   * módulos
   * dependencias
   * flujo de ejecución
   * ownership de datos
   * APIs públicas
   * protocolo de red
   * persistencia
   * puntos de entrada
   * tests
3. Confirmar que `main` no contiene código experimental de AI ni dependencias de otras líneas.
4. Ejecutar:
   * build ESP8266
   * build ESP32
   * build ESP32-C3
   * host tests
5. Registrar baseline:
   * RAM
   * flash
   * tiempo aproximado de boot
   * entidades máximas
   * reglas máximas
   * tamaño de packet
   * heap disponible
6. No comenzar refactors importantes hasta documentar el estado inicial.

### Entregables

Crear o actualizar:

```text
docs/professionalization-roadmap.md
docs/runtime-architecture.md
docs/data-model.md
docs/protocol.md
docs/security-model.md
docs/testing-strategy.md
```

No duplicar documentación existente innecesariamente. Consolidar cuando sea posible.

---

## PHASE 1 - ENTITY MODEL

## Problema

Actualmente `Calibration` funciona como una mezcla de:

* sensor
* actuator
* state
* configuration
* remote entity
* network identity
* persistence metadata
* runtime metadata

Esto debe desaparecer progresivamente.

## Objetivo

Introducir un modelo conceptual explícito:

```text
Device
Entity
EntityConfig
EntityState
Capability
Ownership
```

### Reglas

Un `Entity` debe representar una entidad lógica.

La configuración no debe mezclarse conceptualmente con el estado runtime.

Ejemplo:

```text
Entity
 ├── identity
 ├── metadata
 ├── ownership
 ├── capabilities
 ├── configuration
 └── state
```

### Importante

No hacer un rewrite destructivo.

Introducir estructuras nuevas progresivamente y mantener compatibilidad interna durante la migración.

### Resultado esperado

Debe ser posible distinguir claramente:

```text
"qué es"
"cómo está configurado"
"qué estado tiene"
"quién lo posee"
"qué puede hacer"
```

sin inferirlo desde flags dispersos.

---

## PHASE 2 - IDENTITY SYSTEM

## Problema

La identidad actual deriva parcialmente de:

```text
chip ID + registration index
```

Esto acopla identidad persistente con orden de registro.

## Objetivo

Separar:

```text
device identity
entity identity
runtime index
```

### Nuevo principio

El índice interno jamás debe ser la identidad lógica.

Implementar una estrategia de IDs estables.

Requisitos:

* device identity estable
* entity identity estable
* índice tratado únicamente como referencia runtime
* persistencia basada en identidad estable
* rules references desacopladas del índice cuando sea viable
* compatibilidad/migración de configuraciones existentes

### Migración

No romper dispositivos previamente configurados.

Diseñar:

```text
legacy UID → new entity ID
```

y documentar la estrategia.

---

## PHASE 3 - PROTOCOL V2

## Problema

El protocolo actual evolucionó incrementalmente alrededor de structs (`Packet`, `PacketV4`, `PacketV5`) y tipos de payload.

Eso debe dejar de crecer de esa manera.

## Objetivo

Definir un protocolo explícito.

Separar conceptualmente:

```text
MESSAGE ENVELOPE
PAYLOAD
```

El envelope debe contemplar conceptualmente:

```text
protocol_version
message_type
source_device
target_device
message_id
sequence
ttl
payload_length
payload
```

### Tipos mínimos

Definir explícitamente:

```text
HELLO
ENTITY_ANNOUNCE
STATE_UPDATE
COMMAND
COMMAND_ACK
COMMAND_ERROR
LOG
```

### Requisitos

El parser debe:

* validar tamaño
* validar versión
* validar tipo
* validar source
* validar target
* validar payload
* rechazar paquetes inválidos
* tolerar paquetes desconocidos cuando sea posible
* evitar interpretar un tipo de mensaje como otro
* detectar duplicados cuando corresponda

### Compatibilidad

Mantener parsing del protocolo actual durante una fase de transición.

No eliminar v1/v2/v3/v4/v5 hasta tener migración definida.

---

## PHASE 4 - COMMAND DELIVERY SEMANTICS

## Problema

UDP/ESP-NOW no garantiza:

```text
sent == received == executed
```

## Objetivo

Introducir semántica explícita para comandos.

Como mínimo:

```text
COMMAND
COMMAND_ACK
COMMAND_ERROR
TIMEOUT
```

Agregar:

* message ID
* sequence
* target
* retry policy
* expiration/TTL
* duplicate detection

Ejemplo:

```text
Node A
  |
  | COMMAND #184
  v
Node B
  |
  | ACK #184
  v
Node A
```

Un comando crítico nunca debe considerarse exitoso simplemente porque `send()` retornó.

Documentar claramente qué mensajes son:

```text
best effort
reliable
idempotent
non-idempotent
```

---

## PHASE 5 - TRANSPORT ABSTRACTION

## Problema

Actualmente UDP y ESP-NOW están demasiado mezclados con lógica de aplicación.

## Objetivo

Separar:

```text
Application Messaging
        ↓
Transport Interface
        ↓
UDP / ESP-NOW
```

Crear una abstracción conceptual:

```text
transport.send(...)
transport.receive(...)
transport.available(...)
transport.peer(...)
```

La lógica de entidades y comandos no debe conocer detalles específicos de UDP/ESP-NOW.

### Resultado

Poder cambiar transporte sin reescribir:

* entity system
* automation system
* command processing
* discovery

---

## PHASE 6 - MESH REDEFINITION

No llamar "mesh" a algo simplemente porque varios nodos se mandan broadcast.

Definir explícitamente qué es Qymera networking.

La arquitectura debe distinguir:

```text
Discovery
Messaging
Transport
Topology
Ownership
Routing
```

Si el sistema continúa siendo broadcast-only, documentarlo honestamente.

No implementar routing complejo salvo que exista un requisito real.

El objetivo de esta fase es eliminar ambigüedad, no inflar el proyecto.

---

## PHASE 7 - AUTOMATION ENGINE 2.0

## Problema

Actualmente `Rule` mezcla arrays de índices, comparadores y acciones.

## Objetivo

Separar conceptualmente:

```text
Trigger
Condition
Condition Tree
Action
Action List
Execution Policy
```

Modelo:

```text
Automation
 ├── trigger
 ├── conditions
 ├── actions
 └── execution_policy
```

### Debe soportar conceptualmente

```text
IF temperature > 30
AND humidity > 80
THEN relay ON
```

y permitir evolucionar hacia:

```text
FOR duration
UNLESS condition
WITH cooldown
WITH retry policy
```

### Implementar primero

* hysteresis
* configurable debounce
* `for N seconds`
* explicit AND/OR tree
* action sequencing
* action failure handling

No agregar features no necesarias todavía.

---

## PHASE 8 - AUTOMATION SAFETY

Toda automatización que controle actuadores debe tener comportamiento definido ante:

* sensor stale
* remote offline
* command failure
* reboot
* clock unavailable
* invalid sensor value
* duplicated event
* delayed packet

Definir políticas explícitas.

Nunca asumir:

```text
remote entity exists
=> remote entity is available
```

Separar claramente:

```text
discovered
online
stale
offline
unknown
```

---

## PHASE 9 - PERSISTENCE ENGINE

## Problema

Actualmente persistence está fuertemente ligada a offsets y estructuras internas.

## Objetivo

Crear una capa formal de persistence:

```text
Storage
 ├── schema version
 ├── records
 ├── validation
 ├── migration
 └── commit strategy
```

### Requisitos

* schema version
* corruption detection
* migration
* default recovery
* atomic-ish writes donde sea posible
* write minimization
* explicit wear strategy

Separar claramente:

```text
configuration
runtime state
identity
rules
```

No depender del índice runtime.

---

## PHASE 10 - MEMORY HARDENING

Target prioritario: ESP8266.

Auditar:

* `String`
* dynamic allocation
* JSON generation
* temporary buffers
* HTTP responses
* logging
* remote entity creation
* mesh packets

Identificar:

```text
heap fragmentation
peak allocations
persistent allocations
stack usage
large temporary buffers
```

### Objetivo

Reducir asignaciones dinámicas donde sea razonable.

No hacer optimización prematura.

Primero medir.

Crear documentación:

```text
docs/memory-budget.md
```

Con:

```text
boot heap
idle heap
UI request heap
mesh peak
automation peak
OTA peak
```

---

## PHASE 11 - RUNTIME SCHEDULER

El `loop()` actual funciona, pero todos los subsistemas dependen del mismo heartbeat.

Formalizarlo como scheduler cooperativo.

Modelo conceptual:

```text
Web task
WiFi task
Mesh RX task
Mesh TX task
Sensor task
Automation task
Persistence task
OTA task
```

Cada tarea debe tener:

* frecuencia
* máximo trabajo por tick
* prioridad
* política de starvation

Ningún módulo debe poder monopolizar `loop()`.

Mantenerlo compatible con Arduino.

No introducir RTOS simplemente para "hacerlo profesional".

---

## PHASE 12 - LOGGING / TELEMETRY SEPARATION

Separar:

```text
local logging
diagnostic logging
network telemetry
```

Nunca asumir que:

```text
log()
=> broadcast
```

Debe ser configurable.

Agregar:

* level
* source
* timestamp
* optional device ID
* optional correlation/message ID

El sistema de logs no debe generar una tormenta de red simplemente porque existe un bug o una regla se ejecuta frecuentemente.

---

## PHASE 13 - HTTP/API REDESIGN

Definir una API coherente.

Separar:

```text
GET state
GET entities
GET config
POST command
POST config
GET diagnostics
```

Dejar de diseñar endpoints individualmente.

Objetivo:

```text
/api/v1/devices
/api/v1/entities
/api/v1/state
/api/v1/commands
/api/v1/rules
/api/v1/diagnostics
```

No hace falta migrar todo de golpe.

Crear compatibilidad con endpoints actuales durante transición.

---

## PHASE 14 - SECURITY HARDENING

Esta fase es prioritaria.

## El objetivo NO es seguridad bancaria.

Es seguridad razonable para un dispositivo IoT.

### Resolver conceptualmente

* authentication
* authorization
* command authenticity
* replay protection
* CSRF
* secure OTA
* credential handling
* secret storage
* CORS
* remote access model

Eliminar del lenguaje del producto cualquier afirmación equivalente a:

> OTA integrity

si solo se verifica identidad del dispositivo.

Llamarlo por lo que realmente hace.

### Objetivo final

El dispositivo debe diferenciar:

```text
identity
authentication
authorization
integrity
authenticity
```

---

## PHASE 15 - WEB UI

La UI debe consumir una API estable.

No duplicar lógica de negocio dentro del frontend.

Separar:

```text
presentation
API client
state
device model
```

Eliminar conocimiento innecesario de detalles internos del firmware.

La UI debe pensar en:

```text
devices
entities
state
configuration
automations
```

no en:

```text
calibration arrays
indexes
EEPROM slots
```

---

## PHASE 16 - TEST STRATEGY

Los tests actuales deben dejar de representar únicamente helpers aislados.

Crear capas:

```text
Unit Tests
Protocol Tests
State-machine Tests
Persistence Tests
Automation Tests
Integration Tests
Hardware Tests
Stress Tests
```

### Mínimo

Automatizar tests para:

#### Entity

* create
* update
* remove
* identity stability
* remote lifecycle

#### Protocol

* valid packets
* malformed packets
* wrong version
* wrong size
* unknown type
* duplicate packets
* replay
* corrupted payload

#### Commands

* delivery
* ACK
* timeout
* duplicate command
* wrong target

#### Automations

* threshold
* hysteresis
* edge
* time
* interval
* AND
* OR
* actuator
* relay
* dimmer
* cooldown
* delay

#### Persistence

* reboot
* corruption
* missing record
* migration
* factory reset

#### Memory

* repeated HTTP
* repeated discovery
* repeated reconnect
* repeated automation execution

---

## PHASE 17 - STRESS / CHAOS TESTING

Crear escenarios reproducibles:

```text
10 nodes
20 nodes
64 entities
20 rules
rapid packets
packet loss
duplicate packets
reordered packets
WiFi disconnect
WiFi reconnect
ESP reboot
remote disappearance
remote return
HTTP request flood
OTA interruption
storage corruption
```

El sistema debe continuar operando sin:

* watchdog loops
* infinite loops
* heap collapse
* phantom entities
* duplicated entities
* corrupted persistence
* stuck actuators

---

## PHASE 18 - PRODUCT CONTRACT

Actualizar README para que Qymera `main` tenga una definición extremadamente clara.

Debe explicar:

### Qymera ES

```text
local-first
distributed
deterministic
embedded automation runtime
```

### Qymera NO ES

* AI platform
* cloud platform
* home automation ecosystem replacement
* Matter stack
* MQTT broker
* general-purpose IoT cloud

El roadmap futuro puede mencionarse, pero no debe contaminar la definición del producto base.

---

## PHASE 19 - RELEASE MODEL

Establecer:

```text
main = stable
feature/* = development
release/* = stabilization
tag = immutable release
```

Crear:

```text
CHANGELOG.md
VERSION
```

El versionado debe permitir:

```text
firmware version
protocol version
storage schema version
API version
```

independientes.

---

## PHASE 20 - FINAL PRODUCTION GATE

No declarar "production ready" por compilación + smoke test.

La release final debe requerir:

### Build

* ESP8266
* ESP32
* ESP32-C3

### Runtime

* boot
* reconnect
* reboot
* remote discovery
* actuator control
* automation

### Persistence

* repeated reboot
* factory reset
* schema migration

### Network

* packet loss
* duplicates
* malformed traffic
* node disappearance
* node recovery

### Memory

* 24h minimum soak
* repeated UI polling
* mesh traffic

### Security

* unauthorized commands rejected
* OTA authenticity model verified
* credentials handled correctly
* CSRF policy verified
* CORS policy verified

### Documentation

README, architecture, API, protocol, storage and security deben coincidir con el código.

---

## ORDEN DE EJECUCIÓN

No implementar todo en paralelo.

Seguir estrictamente:

```text
0  Baseline
↓
1  Entity Model
↓
2  Identity
↓
3  Protocol
↓
4  Command Delivery
↓
5  Transport Abstraction
↓
6  Mesh Definition
↓
7  Automation Model
↓
8  Automation Safety
↓
9  Persistence
↓
10 Memory
↓
11 Runtime Scheduler
↓
12 Logging
↓
13 HTTP API
↓
14 Security
↓
15 UI
↓
16 Tests
↓
17 Stress
↓
18 Product Contract
↓
19 Release Model
↓
20 Production Gate
```

---

## REGLAS PARA EL AGENTE

1. **Trabajar únicamente en `main`.**
2. No realizar cambios sobre otras branches.
3. No incorporar código desde AI/IDF.
4. No agregar features de AI.
5. No agregar nuevas integraciones externas.
6. No hacer rewrites completos sin necesidad.
7. Cada refactor debe preservar comportamiento existente cuando sea posible.
8. Antes de eliminar una estructura, crear estrategia de migración.
9. Cada cambio arquitectónico debe estar acompañado por tests.
10. Cada cambio de protocolo debe incluir compatibilidad.
11. Cada cambio de persistence debe incluir migration/versioning.
12. No aceptar "build passes" como criterio suficiente.
13. No declarar algo "secure" sin explicar exactamente qué propiedad de seguridad ofrece.
14. No llamar "mesh" a una capacidad que no sea realmente mesh. Documentar las limitaciones.
15. No crear abstracciones únicamente por estética. Toda abstracción debe resolver un problema concreto.
16. Medir memoria antes y después de cambios importantes.
17. No permitir que nuevas funcionalidades aumenten significativamente la complejidad del firmware sin justificación.
18. Evitar dependencias externas innecesarias.

---

## CRITERIO DE ÉXITO

Al finalizar, Qymera `main` debe poder describirse técnicamente así:

```text
Qymera is a deterministic, local-first distributed automation runtime
for ESP-class devices.

It provides:

- stable device/entity identity
- explicit entity model
- deterministic local execution
- versioned persistence
- explicit messaging protocol
- transport abstraction
- reliable actuator command semantics
- safe automation execution
* bounded runtime workloads
* predictable memory usage
* authenticated control boundaries
* versioned HTTP API
* testable architecture
```

Y, fundamentalmente:

```text
AI / Dashboard / Link / ESP-IDF
           |
           | separate products
           |
           X
           |
        Qymera main
           |
           └── stable embedded runtime
```

El resultado buscado no es tener más funcionalidades.

Es tener una base que **pueda soportar más funcionalidades sin convertirse en una colección de parches históricos**.

---

## MÉTODO DE TRABAJO DEL AGENTE

Para cada fase:

1. Inspect.
2. Explain current problem.
3. Define target architecture.
4. Implement smallest safe change.
5. Add tests.
6. Run build matrix.
7. Compare memory footprint.
8. Update documentation.
9. Commit the phase independently.
10. Mark the phase complete only after its acceptance criteria pass.

No saltar directamente a la siguiente fase si la actual rompe invariantes de arquitectura.

Al finalizar cada fase, generar:

```text
docs/audits/phase-XX.md
```

con:

* problema encontrado
* cambios realizados
* compatibilidad
* tests
* riesgos restantes
* métricas
* archivos modificados

La prioridad absoluta es **reducir complejidad accidental y aumentar determinismo**, no aumentar el feature count.