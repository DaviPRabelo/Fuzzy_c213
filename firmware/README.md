# Thermal Chamber Fuzzy ESP32

Firmware PlatformIO para controle fuzzy de uma câmara térmica usando ESP32, sensor BMP180 e ventoinha/exaustor acionado por PWM.

## Hardware considerado

- ESP32 comum (`board = esp32dev`)
- BMP180 via I2C
- Ventoinha acionada por PWM via transistor/MOSFET externo
- MQTT para setpoint e telemetria

## Pinagem

| Função | GPIO |
|---|---:|
| SDA BMP180 | GPIO5 |
| SCL BMP180 | GPIO4 |
| PWM ventoinha | GPIO2 |

Observação: GPIO2 é pino de strapping no ESP32. Funciona como PWM, mas o circuito externo não deve forçar nível indevido durante o boot.

## Tópicos MQTT

### Setpoint

```text
thermal_chamber/c3_01/setpoint
```

Payload aceito:

```json
45.0
```

ou:

```json
{"setpoint": 45.0}
```

### Telemetria

```text
thermal_chamber/c3_01/telemetry
```

Exemplo:

```json
{
  "device_id": "thermal-chamber-esp32-01",
  "temp_c": 47.35,
  "setpoint_c": 45.00,
  "error_c": 2.35,
  "delta_error_c": 0.18,
  "pwm_pct": 72.0,
  "sensor_ok": true,
  "safety_mode": false,
  "uptime_ms": 123456
}
```

### Status

```text
thermal_chamber/c3_01/status
```

## Configuração de rede

Copie:

```bash
cp include/secrets.example.h include/secrets.h
```

Edite `include/secrets.h` com sua rede Wi-Fi e broker MQTT.

Se `include/secrets.h` não existir, o projeto usa `secrets.example.h` apenas para permitir compilação inicial.

## Comandos

Compilar:

```bash
pio run -e esp32
```

Gravar:

```bash
pio run -e esp32 -t upload
```

Monitor serial:

```bash
pio device monitor
```

Testes do controlador fuzzy no PC:

```bash
pio test -e native
```

## Controle

O erro foi definido como:

```text
erro = temperatura_medida - setpoint
```

Assim, quando a temperatura está acima do setpoint, o erro é positivo e a ventoinha deve aumentar a exaustão.

Como o soprador térmico não é controlado pelo ESP32, a saída fuzzy controla apenas a capacidade de resfriamento/exaustão.
