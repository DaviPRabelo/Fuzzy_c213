import numpy as np
import skfuzzy as fuzz
from skfuzzy import control as ctrl
import matplotlib.pyplot as plt
import os

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
