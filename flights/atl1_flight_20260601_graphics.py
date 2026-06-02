import re
import io
import pandas as pd
import matplotlib.pyplot as plt

# 1. Leer y limpiar el archivo de telemetría original
lineas_limpias = []
with open('atl1_vuelo.csv', 'r') as f:
    for linea in f:
        # Remover el prefijo del Monitor Serie "HH:MM:SS.xxx -> "
        match = re.match(r'^.*?\s+->\s+(.*)$', linea)
        if match:
            contenido = match.group(1).strip()
            # Ignorar encabezados de archivo internos o líneas vacías
            if contenido.startswith('===') or not contenido:
                continue
            lineas_limpias.append(contenido)

# 2. Convertir a un DataFrame de Pandas
csv_limpio = '\n'.join(lineas_limpias)
df = pd.read_csv(io.StringIO(csv_limpio))

# Convertir el tiempo de milisegundos a segundos para una lectura más fácil
df['t_s'] = df['t_ms'] / 1000.0

# 3. Crear las gráficas de telemetría
fig, axes = plt.subplots(2, 2, figsize=(12, 8), sharex=True)

# Panel 1: Altitud
axes[0, 0].plot(df['t_s'], df['alt_m'], color='blue', linewidth=2)
axes[0, 0].set_title('Altitud vs Tiempo')
axes[0, 0].set_ylabel('Altitud (m)')
axes[0, 0].grid(True)

# Panel 2: Velocidad
axes[0, 1].plot(df['t_s'], df['vel_ms'], color='green', linewidth=2)
axes[0, 1].set_title('Velocidad vs Tiempo')
axes[0, 1].set_ylabel('Velocidad (m/s)')
axes[0, 1].grid(True)

# Panel 3: Aceleración
axes[1, 0].plot(df['t_s'], df['accel_G'], color='red', linewidth=2)
axes[1, 0].set_title('Aceleración vs Tiempo')
axes[1, 0].set_xlabel('Tiempo (s)')
axes[1, 0].set_ylabel('Aceleración (G)')
axes[1, 0].grid(True)

# Panel 4: Presión Barométrica
axes[1, 1].plot(df['t_s'], df['pressure_hPa'], color='purple', linewidth=2)
axes[1, 1].set_title('Presión vs Tiempo')
axes[1, 1].set_xlabel('Tiempo (s)')
axes[1, 1].set_ylabel('Presión (hPa)')
axes[1, 1].grid(True)

plt.tight_layout()
# Guardar la gráfica en un archivo de imagen
plt.savefig('telemetria_vuelo.png', dpi=300)
print("¡Gráfica generada y guardada como 'telemetria_vuelo.png'!")