# Importar las bibliotecas necesarias
from flask import Flask, render_template, request, jsonify
import json
import threading
import time
from datetime import datetime
import random  # Solo para simulación, remover en producción

# Inicializar la aplicación Flask
app = Flask(__name__, static_url_path='', static_folder='static', template_folder='templates')

# Datos simulados para inicializar los sensores ( estos valores vendrían de los sensores)
sensor_data = {
    'aht10_1': {'temperatura': 25.5, 'humedad': 60.3},
    'aht10_2': {'temperatura': 24.8, 'humedad': 58.7},
    'ds18b20': {'temperatura': 22.3},
    'reles': {'rele1': False, 'rele2': False}
}

# Variables para almacenar el historial de datos (para gráficos futuros)
sensor_history = {
    'aht10_1_temp': [],
    'aht10_1_hum': [],
    'aht10_2_temp': [],
    'aht10_2_hum': [],
    'ds18b20_temp': []
}

# Mutex para proteger el acceso a los datos compartidos
data_lock = threading.Lock()

# Timestamp de la última actualización
last_update = datetime.now().timestamp()

# Variable para controlar la simulación
simulation_active = True

# Función para simular cambios en los datos de los sensores (solo para demostración)
def simulate_sensor_changes():
    global sensor_data, last_update, simulation_active
    while True:
        time.sleep(5)  # Actualizar cada 5 segundos

        # Si recibimos datos reales de ESP32, no simular
        if not simulation_active:
            continue

        with data_lock:
            # Simular pequeños cambios en los valores
            sensor_data['aht10_1']['temperatura'] += random.uniform(-0.5, 0.5)
            sensor_data['aht10_1']['humedad'] += random.uniform(-1, 1)
            sensor_data['aht10_2']['temperatura'] += random.uniform(-0.5, 0.5)
            sensor_data['aht10_2']['humedad'] += random.uniform(-1, 1)
            sensor_data['ds18b20']['temperatura'] += random.uniform(-0.3, 0.3)

            # Mantener los valores dentro de rangos razonables
            sensor_data['aht10_1']['temperatura'] = max(15, min(35, sensor_data['aht10_1']['temperatura']))
            sensor_data['aht10_1']['humedad'] = max(30, min(90, sensor_data['aht10_1']['humedad']))
            sensor_data['aht10_2']['temperatura'] = max(15, min(35, sensor_data['aht10_2']['temperatura']))
            sensor_data['aht10_2']['humedad'] = max(30, min(90, sensor_data['aht10_2']['humedad']))
            sensor_data['ds18b20']['temperatura'] = max(15, min(35, sensor_data['ds18b20']['temperatura']))

            # Actualizar timestamp
            last_update = datetime.now().timestamp()

            # Actualizar historial
            update_sensor_history()

# Función para actualizar el historial de sensores
def update_sensor_history():
    current_time = datetime.now().strftime("%H:%M:%S")
    for key, collection in [
        ('aht10_1_temp', sensor_data['aht10_1']['temperatura']),
        ('aht10_1_hum', sensor_data['aht10_1']['humedad']),
        ('aht10_2_temp', sensor_data['aht10_2']['temperatura']),
        ('aht10_2_hum', sensor_data['aht10_2']['humedad']),
        ('ds18b20_temp', sensor_data['ds18b20']['temperatura'])
    ]:
        sensor_history[key].append({'time': current_time, 'value': round(collection, 1)})
        if len(sensor_history[key]) > 100:
            sensor_history[key].pop(0)

# Ruta principal que renderiza la página web
@app.route('/')
def index():
    return render_template('index.html')

# API para obtener los datos actuales de los sensores
@app.route('/api/sensors', methods=['GET'])
def get_sensor_data():
    with data_lock:
        response = {
            'data': sensor_data,
            'timestamp': last_update
        }
    return jsonify(response)

# API para obtener el historial de datos de los sensores
@app.route('/api/history', methods=['GET'])
def get_sensor_history():
    with data_lock:
        return jsonify(sensor_history)

# API para recibir datos de la ESP32
@app.route('/api/esp32/data', methods=['POST'])
def receive_esp32_data():
    global simulation_active
    try:
        data = request.get_json()
        print("Datos recibidos de ESP32:", data)  # Log para depuración

        # Validar estructura mínima de datos
        required_keys = ['aht10_1', 'aht10_2', 'ds18b20']
        if not all(key in data for key in required_keys):
            return jsonify({'status': 'error', 'message': 'Datos incompletos'}), 400

        # Desactivar la simulación cuando lleguen datos reales
        simulation_active = False

        # Actualizar los datos con los valores recibidos de la ESP32
        with data_lock:
            sensor_data['aht10_1'] = data['aht10_1']
            sensor_data['aht10_2'] = data['aht10_2']
            sensor_data['ds18b20'] = data['ds18b20']
            last_update = datetime.now().timestamp()

            # Actualizar el historial
            update_sensor_history()

        return jsonify({'status': 'success'})
    except Exception as e:
        print(f"Error al procesar datos de ESP32: {str(e)}")  # Log del error
        return jsonify({'status': 'error', 'message': str(e)}), 500

# API para controlar los relés
@app.route('/api/relay/control', methods=['POST'])
def control_relay():
    try:
        data = request.get_json()

        # Validar estructura mínima de datos
        if 'rele1' not in data and 'rele2' not in data:
            return jsonify({'status': 'error', 'message': 'Datos incompletos'}), 400

        # Actualizar el estado de los relés
        with data_lock:
            if 'rele1' in data:
                sensor_data['reles']['rele1'] = bool(data['rele1'])
            if 'rele2' in data:
                sensor_data['reles']['rele2'] = bool(data['rele2'])

        # aquí se enviaría el comando a la ESP32
        # nota: implementar_codigo_para_enviar_comandos_a_esp32()

        return jsonify({'status': 'success', 'reles': sensor_data['reles']})
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

# API para obtener el estado actual de los relés
@app.route('/api/relay/status', methods=['GET'])
def get_relay_status():
    with data_lock:
        return jsonify(sensor_data['reles'])

# Para desarrollo y pruebas, inicia un hilo para simular cambios en los sensores
if __name__ == '__main__':
    # Iniciar el hilo de simulación de sensores
    sim_thread = threading.Thread(target=simulate_sensor_changes, daemon=True)
    sim_thread.start()

    # Iniciar el servidor Flask
    app.run(debug=True, host='0.0.0.0', port=5000)