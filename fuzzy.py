import numpy as np
import skfuzzy as fuzz
from skfuzzy import control as ctrl
import matplotlib.pyplot as plt
import os
import sys
import json
import time

# console do Windows (cp1252) nao imprime caracteres como -> usados nos logs;
# reconfigura a saida para UTF-8 quando possivel (no-op em terminais que ja suportam)
try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass

try:
    import paho.mqtt.client as mqtt_client
    _PAHO_OK = True
except ImportError:
    _PAHO_OK = False

# salva os graficos na pasta deste script (portavel entre Windows/Linux)
SAIDA = os.path.dirname(os.path.abspath(__file__))

# universos de discurso
e_u  = np.arange(-30, 30.1, 0.5)
de_u = np.arange(-5, 5.05, 0.05)
u_u  = np.arange(-100, 100.1, 1)

erro = ctrl.Antecedent(e_u, 'erro')
dele = ctrl.Antecedent(de_u, 'delta_erro')
acao = ctrl.Consequent(u_u, 'acao')

# pertinencias do erro (NG=muito quente ... PG=muito frio)
erro['NG'] = fuzz.trapmf(e_u, [-30, -30, -20, -10])
erro['NP'] = fuzz.trimf(e_u, [-15, -7, 0])
erro['ZE'] = fuzz.trimf(e_u, [-5, 0, 5])
erro['PP'] = fuzz.trimf(e_u, [0, 7, 15])
erro['PG'] = fuzz.trapmf(e_u, [10, 20, 30, 30])

# pertinencias da variacao do erro
dele['NG'] = fuzz.trapmf(de_u, [-5, -5, -3, -1.5])
dele['NP'] = fuzz.trimf(de_u, [-3, -1.2, 0])
dele['ZE'] = fuzz.trimf(de_u, [-1, 0, 1])
dele['PP'] = fuzz.trimf(de_u, [0, 1.2, 3])
dele['PG'] = fuzz.trapmf(de_u, [1.5, 3, 5, 5])

# pertinencias da acao (RF=resfriar forte ... AF=aquecer forte)
acao['RF'] = fuzz.trapmf(u_u, [-100, -100, -75, -45])
acao['RL'] = fuzz.trimf(u_u, [-60, -30, 0])
acao['MA'] = fuzz.trimf(u_u, [-20, 0, 20])
acao['AL'] = fuzz.trimf(u_u, [0, 30, 60])
acao['AF'] = fuzz.trapmf(u_u, [45, 75, 100, 100])

# base de regras (matriz erro x delta_erro)
matriz = {
    'NG': {'NG':'RF','NP':'RF','ZE':'RF','PP':'RL','PG':'RL'},
    'NP': {'NG':'RF','NP':'RL','ZE':'RL','PP':'RL','PG':'MA'},
    'ZE': {'NG':'RL','NP':'RL','ZE':'MA','PP':'AL','PG':'AL'},
    'PP': {'NG':'MA','NP':'AL','ZE':'AL','PP':'AF','PG':'AF'},
    'PG': {'NG':'AL','NP':'AF','ZE':'AF','PP':'AF','PG':'AF'},
}
regras = [ctrl.Rule(erro[e] & dele[d], acao[s])
          for e, lin in matriz.items() for d, s in lin.items()]

sistema = ctrl.ControlSystem(regras)
sim = ctrl.ControlSystemSimulation(sistema)

# defuzzificacao por centroide (default do skfuzzy = centroid)
acao.defuzzify_method = 'centroid'

# ganho de saida Ku + saturacao: o centroide satura em ~79%, entao Ku o mapeia
# para 100% e saturamos em +/-100% (limite fisico do atuador)
sim.input['erro'] = 30
sim.input['delta_erro'] = 5
sim.compute()
U_RAW_MAX = sim.output['acao']            # ~79.06 %
KU = 100.0 / U_RAW_MAX                     # ~1.265

def comando(u_raw):
    """Aplica o ganho de saida e satura em +/-100% (limite fisico do atuador)."""
    return max(-100.0, min(100.0, KU * u_raw))

print(f"Ku = {KU:.3f}  (centroide maximo = {U_RAW_MAX:.1f}% -> 100%)\n")

# teste rapido: centroide (defuzzificacao) -> comando (apos Ku + saturacao)
for ev, esperado in [(20, "aquecer"), (-20, "resfriar"), (0, "manter")]:
    sim.input['erro'] = ev
    sim.input['delta_erro'] = 0
    sim.compute()
    c = sim.output['acao']
    print(f"erro={ev:+3d}, de=0  ->  centroide={c:+6.1f}%   comando={comando(c):+6.1f}%   (esperado: {esperado})")

# ---- graficos das funcoes de pertinencia ----
fig, ax = plt.subplots(3, 1, figsize=(9, 10))

for t in ['NG','NP','ZE','PP','PG']:
    ax[0].plot(e_u, erro[t].mf, label=t, linewidth=2)
ax[0].set_title('Erro  (e = SP - T)  [°C]')
ax[0].set_xlabel('Erro [°C]'); ax[0].set_ylabel('Pertinência')
ax[0].legend(loc='upper center', ncol=5); ax[0].grid(alpha=0.3)

for t in ['NG','NP','ZE','PP','PG']:
    ax[1].plot(de_u, dele[t].mf, label=t, linewidth=2)
ax[1].set_title('Variação do Erro  (Δe)  [°C/amostra]')
ax[1].set_xlabel('Δe [°C/amostra]'); ax[1].set_ylabel('Pertinência')
ax[1].legend(loc='upper center', ncol=5); ax[1].grid(alpha=0.3)

for t in ['RF','RL','MA','AL','AF']:
    ax[2].plot(u_u, acao[t].mf, label=t, linewidth=2)
ax[2].set_title('Ação de Controle  (u)  [%]   u>0 aquece, u<0 resfria')
ax[2].set_xlabel('u [%]'); ax[2].set_ylabel('Pertinência')
ax[2].legend(loc='upper center', ncol=5); ax[2].grid(alpha=0.3)

plt.tight_layout()
plt.savefig(os.path.join(SAIDA, 'pertinencias.png'), dpi=130)
print("\ngrafico salvo")

# ---- superficie de controle ----
e_grid  = np.arange(-30, 30.1, 2)
de_grid = np.arange(-5, 5.1, 0.5)
Z = np.zeros((len(de_grid), len(e_grid)))
for i, dv in enumerate(de_grid):
    for j, ev in enumerate(e_grid):
        sim.input['erro'] = ev
        sim.input['delta_erro'] = dv
        sim.compute()
        Z[i, j] = comando(sim.output['acao'])   # superficie do comando (com Ku + saturacao)

fig2 = plt.figure(figsize=(8, 6))
axs = fig2.add_subplot(111, projection='3d')
E, D = np.meshgrid(e_grid, de_grid)
axs.plot_surface(E, D, Z, cmap='coolwarm', edgecolor='none')
axs.set_xlabel('Erro [°C]'); axs.set_ylabel('Δe'); axs.set_zlabel('u [%] (saturado)')
axs.set_title('Superfície de Controle Fuzzy (com Ku + saturação ±100%)')
plt.tight_layout()
plt.savefig(os.path.join(SAIDA, 'superficie.png'), dpi=130)
print("superficie salva")

# ---- simulador MQTT ----
# Publica telemetria no mesmo tópico e esquema JSON do firmware ESP32.
# Use quando não houver hardware disponível para testar o dashboard.
#
#   Como usar:
#     python fuzzy.py --mqtt            (setpoint padrão 45 °C)
#     python fuzzy.py --mqtt --sp 60    (setpoint inicial 60 °C)

MQTT_BROKER      = "test.mosquitto.org"
MQTT_PORT        = 1883
TOPIC_TELEM      = "thermal_chamber/c3_01/telemetry"
TOPIC_SETPOINT   = "thermal_chamber/c3_01/setpoint"
TOPIC_INFERENCE  = "thermal_chamber/c3_01/inference"

UAXIS = list(range(-100, 101, 2))   # 101 pontos, passo 2

_TERMS  = ['NG', 'NP', 'ZE', 'PP', 'PG']
_MATRIX = {
    'NG': {'NG':'RF','NP':'RF','ZE':'RF','PP':'RL','PG':'RL'},
    'NP': {'NG':'RF','NP':'RL','ZE':'RL','PP':'RL','PG':'MA'},
    'ZE': {'NG':'RL','NP':'RL','ZE':'MA','PP':'AL','PG':'AL'},
    'PP': {'NG':'MA','NP':'AL','ZE':'AL','PP':'AF','PG':'AF'},
    'PG': {'NG':'AL','NP':'AF','ZE':'AF','PP':'AF','PG':'AF'},
}


def compute_inference(erro_val: float, de_val: float,
                      temp: float = None, sp_val: float = None, u_val: float = None) -> dict:
    e = float(max(-30.0, min(30.0, erro_val)))
    d = float(max(-5.0,  min(5.0,  de_val)))

    muE = {t: float(fuzz.interp_membership(e_u,  erro[t].mf, e)) for t in _TERMS}
    muD = {t: float(fuzz.interp_membership(de_u, dele[t].mf, d)) for t in _TERMS}

    rules = []
    max_str = 0.0
    for et in _TERMS:
        for dt in _TERMS:
            s   = min(muE[et], muD[dt])
            out = _MATRIX[et][dt]
            rules.append({'e': et, 'd': dt, 'out': out, 's': round(s, 4)})
            if s > max_str:
                max_str = s

    agg = []
    for uv in UAXIS:
        m = 0.0
        for r in rules:
            if r['s'] > 0:
                mu_out  = float(fuzz.interp_membership(u_u, acao[r['out']].mf, uv))
                clipped = min(r['s'], mu_out)
                if clipped > m:
                    m = clipped
        agg.append(round(m, 4))

    sim.input['erro']       = e
    sim.input['delta_erro'] = d
    sim.compute()
    uc   = float(sim.output['acao'])
    ucmd = float(max(-100.0, min(100.0, KU * uc)))

    result = {
        'erro':   round(erro_val, 2),
        'de':     round(de_val, 2),
        'uc':     round(uc, 2),
        'ucmd':   round(ucmd, 1),
        'muE':    {t: round(v, 4) for t, v in muE.items()},
        'muD':    {t: round(v, 4) for t, v in muD.items()},
        'rules':  rules,
        'agg':    agg,
        'maxStr': round(max_str, 4),
    }
    if temp    is not None: result['temp'] = round(float(temp), 2)
    if sp_val  is not None: result['sp']   = round(float(sp_val), 2)
    if u_val   is not None: result['u']    = round(float(u_val), 1)
    return result


def run_listener():
    if not _PAHO_OK:
        print("paho-mqtt não instalado. Execute:  pip install paho-mqtt")
        return

    def on_connect(client, userdata, flags, _rc):
        print(f"[listener] conectado → ouvindo {TOPIC_TELEM}")
        client.subscribe(TOPIC_TELEM)

    def on_message(client, userdata, msg):
        try:
            d             = json.loads(msg.payload.decode())
            error_c       = d.get('error_c')
            delta_error_c = d.get('delta_error_c')
            if error_c is None or delta_error_c is None:
                return
            erro_val = -float(error_c)
            de_val   = -float(delta_error_c)
            temp     = d.get('temp_c')
            sp_val   = d.get('setpoint_c')
            pwm      = d.get('pwm_pct')
            u_val    = -float(pwm) if pwm is not None else None
            result   = compute_inference(erro_val, de_val, temp, sp_val, u_val)
            client.publish(TOPIC_INFERENCE, json.dumps(result))
            print(f"[listener] T={temp}°C  SP={sp_val}°C  erro={erro_val:+.1f}  ucmd={result['ucmd']:+.0f}%")
        except Exception as ex:
            print(f"[listener] erro: {ex}")

    client = mqtt_client.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    print("[listener] iniciado — Ctrl+C para parar")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[listener] encerrado")
        client.disconnect()


def run_simulator(setpoint_inicial: float = 45.0):
    if not _PAHO_OK:
        print("paho-mqtt não instalado. Execute:  pip install paho-mqtt")
        return

    sp      = [float(setpoint_inicial)]
    T       = [24.0]
    e_prev  = [0.0]
    uptime  = [0]

    def on_connect(client, userdata, flags, rc):
        print(f"[mqtt] conectado (rc={rc}) → publicando em {TOPIC_TELEM}")
        client.subscribe(TOPIC_SETPOINT)

    def on_message(client, userdata, msg):
        try:
            sp[0] = float(msg.payload.decode().strip())
            print(f"[mqtt] novo setpoint: {sp[0]} °C")
        except ValueError:
            pass

    client = mqtt_client.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()

    print(f"[simulador] iniciado — Ctrl+C para parar\n")
    try:
        while True:
            e_sp_t = sp[0] - T[0]               # SP-T (convenção fuzzy.py)
            de     = e_sp_t - e_prev[0]
            e_prev[0] = e_sp_t

            sim.input['erro']       = float(max(-30.0, min(30.0, e_sp_t)))
            sim.input['delta_erro'] = float(max(-5.0,  min(5.0,  de)))
            sim.compute()
            u = comando(sim.output['acao'])      # [-100, 100]

            T[0] += 0.012 * u
            T[0]  = max(15.0, min(85.0, T[0]))

            payload = json.dumps({
                "device_id":     "fuzzy-py-sim",
                "temp_c":        round(T[0], 2),
                "setpoint_c":    round(sp[0], 2),
                "error_c":       round(T[0] - sp[0], 2),   # T-SP (convenção firmware)
                "delta_error_c": round(-de, 2),
                "pwm_pct":       round(max(0.0, -u), 1),   # fan speed: apenas resfriamento
                "sensor_ok":     True,
                "safety_mode":   False,
                "uptime_ms":     uptime[0],
            })
            client.publish(TOPIC_TELEM, payload)
            print(f"T={T[0]:.1f}°C  SP={sp[0]:.0f}°C  e={e_sp_t:+.1f}  u={u:+.0f}%")

            uptime[0] += 1000
            time.sleep(1.0)

    except KeyboardInterrupt:
        print("\n[simulador] encerrado")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    import sys
    if "--listen" in sys.argv:
        run_listener()
    elif "--mqtt" in sys.argv:
        sp0 = 45.0
        if "--sp" in sys.argv:
            idx = sys.argv.index("--sp")
            if idx + 1 < len(sys.argv):
                sp0 = float(sys.argv[idx + 1])
        run_simulator(setpoint_inicial=sp0)
