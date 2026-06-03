import re
import io
import pandas as pd
import matplotlib.pyplot as plt

# First, clean the raw telemetry data from the serial monitor
clean_lines = []
with open('atl1_vuelo.csv', 'r') as f:
    for line in f:
        # Get rid of the timestamp prefix (HH:MM:SS.xxx ->)
        match = re.match(r'^.*?\s+->\s+(.*)$', line)
        if match:
            content = match.group(1).strip()
            # Skip empty lines or the internal file headers
            if content.startswith('===') or not content:
                continue
            clean_lines.append(content)

# Load everything into a pandas dataframe
csv_data = '\n'.join(clean_lines)
df = pd.read_csv(io.StringIO(csv_data))

# Convert time to seconds to make the plots easier to read
df['t_s'] = df['t_ms'] / 1000.0

# Setup the 2x2 grid for the plots
fig, axes = plt.subplots(2, 2, figsize=(12, 8), sharex=True)

# Altitude plot
axes[0, 0].plot(df['t_s'], df['alt_m'], color='blue', linewidth=2)
axes[0, 0].set_title('Altitude vs Time')
axes[0, 0].set_ylabel('Altitude (m)')
axes[0, 0].grid(True)

# Velocity plot
axes[0, 1].plot(df['t_s'], df['vel_ms'], color='green', linewidth=2)
axes[0, 1].set_title('Velocity vs Time')
axes[0, 1].set_ylabel('Velocity (m/s)')
axes[0, 1].grid(True)

# Acceleration plot
axes[1, 0].plot(df['t_s'], df['accel_G'], color='red', linewidth=2)
axes[1, 0].set_title('Acceleration vs Time')
axes[1, 0].set_xlabel('Time (s)')
axes[1, 0].set_ylabel('Acceleration (G)')
axes[1, 0].grid(True)

# Barometric pressure plot
axes[1, 1].plot(df['t_s'], df['pressure_hPa'], color='purple', linewidth=2)
axes[1, 1].set_title('Pressure vs Time')
axes[1, 1].set_xlabel('Time (s)')
axes[1, 1].set_ylabel('Pressure (hPa)')
axes[1, 1].grid(True)

plt.tight_layout()
# Save the final figure
plt.savefig('telemetria_vuelo.png', dpi=300)
print("Plot generated and saved as 'telemetria_vuelo.png'!")
