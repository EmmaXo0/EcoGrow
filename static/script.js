// Constantes para los rangos de temperatura y humedad
const TEMP_MIN = 15;
const TEMP_MAX = 35;
const HUM_MIN = 0;
const HUM_MAX = 100;
const WATER_TEMP_MIN = 15;
const WATER_TEMP_MAX = 35;

// Función para calcular la circunferencia del círculo
function calculateCircumference(r) {
    return 2 * Math.PI * r;
}

// Función para actualizar el gráfico de dona basado en el valor
function updateDonutChart(chartId, value, maxValue = 100) {
    const chart = document.getElementById(chartId);
    if (!chart) return;

    const circle = chart.querySelector('.circle');
    const valueDisplay = chart.querySelector('.donut-value');
    const radius = parseInt(circle.getAttribute('r'));
    const circumference = calculateCircumference(radius);

    // Normalizar el valor entre 0-100 para el gráfico
    const normalizedValue = Math.min(100, Math.max(0, (value / maxValue) * 100));

    // Actualizar el valor mostrado (con el valor real, no el normalizado)
    valueDisplay.textContent = value.toFixed(1);

    // Calcular el offset para el dasharray
    const offset = circumference - (normalizedValue / 100 * circumference);

    // Actualizar el círculo
    circle.style.strokeDasharray = `${circumference} ${circumference}`;
    circle.style.strokeDashoffset = offset;

    // Actualizar el atributo data-value
    chart.dataset.value = normalizedValue;
}

// Función para actualizar la barra de temperatura del agua
function updateWaterTempBar(value) {
    const fillElement = document.getElementById('water-temp-fill');
    const valueElement = document.getElementById('water-temp-value');

    if (!fillElement || !valueElement) return;

    // Calcular el porcentaje basado en el rango min-max
    let percentage = ((value - WATER_TEMP_MIN) / (WATER_TEMP_MAX - WATER_TEMP_MIN)) * 100;
    percentage = Math.min(100, Math.max(0, percentage));

    // Actualizar el ancho de la barra
    fillElement.style.width = `${percentage}%`;

    // Actualizar el valor mostrado
    valueElement.textContent = `${value.toFixed(1)}°C`;

    // Cambiar el color basado en la temperatura
    if (value < 20) {
        fillElement.style.background = 'linear-gradient(to right, #3498db, #2980b9)'; // Azul (frío)
    } else if (value > 30) {
        fillElement.style.background = 'linear-gradient(to right, #e74c3c, #c0392b)'; // Rojo (caliente)
    } else {
        fillElement.style.background = 'linear-gradient(to right, #2ecc71, #27ae60)'; // Verde (óptimo)
    }
}

// Función para actualizar los estados de los relés
function updateRelayStatus(rele1, rele2) {
    const rele1Switch = document.getElementById('rele1-switch');
    const rele2Switch = document.getElementById('rele2-switch');

    if (rele1Switch) rele1Switch.checked = rele1;
    if (rele2Switch) rele2Switch.checked = rele2;
}

// Función para actualizar la última hora de actualización
function updateLastUpdate(timestamp) {
    const lastUpdateElement = document.getElementById('last-update');
    if (!lastUpdateElement) return;

    const date = new Date(timestamp * 1000);
    lastUpdateElement.textContent = date.toLocaleTimeString();
}

// Función para controlar un relé
function controlRelay(relayId, state) {
    // Preparar los datos para enviar a la API
    const data = {};
    data[relayId] = state;

    // Llamar a la API
    fetch('/api/relay/control', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(data)
    })
    .then(response => response.json())
    .then(data => {
        console.log('Respuesta del servidor:', data);
        // Aquí podríamos actualizar la UI si es necesario
    })
    .catch(error => {
        console.error('Error al controlar el relé:', error);
        // Revertir el cambio en la UI en caso de error
        document.getElementById(`${relayId}-switch`).checked = !state;
    });
}

// Función para obtener los datos de los sensores
function fetchSensorData() {
    fetch('/api/sensors')
        .then(response => response.json())
        .then(data => {
            // Actualizar los gráficos de dona con los datos recibidos
            updateDonutChart('temp1-chart', data.data.aht10_1.temperatura, TEMP_MAX);
            updateDonutChart('hum1-chart', data.data.aht10_1.humedad, HUM_MAX);
            updateDonutChart('temp2-chart', data.data.aht10_2.temperatura, TEMP_MAX);
            updateDonutChart('hum2-chart', data.data.aht10_2.humedad, HUM_MAX);

            // Actualizar la barra de temperatura del agua
            updateWaterTempBar(data.data.ds18b20.temperatura);

            // Actualizar el estado de los relés
            updateRelayStatus(data.data.reles.rele1, data.data.reles.rele2);

            // Actualizar la hora de la última actualización
            updateLastUpdate(data.timestamp);
        })
        .catch(error => {
            console.error('Error al obtener datos de los sensores:', error);
            document.getElementById('system-status').textContent = 'Desconectado';
            document.getElementById('system-status').style.color = '#e74c3c';
        });
}

// Inicializar la interfaz y configurar eventos
document.addEventListener('DOMContentLoaded', function() {
    // Inicializar los gráficos con valores por defecto
    const charts = document.querySelectorAll('.donut-chart');
    charts.forEach(chart => {
        const value = parseInt(chart.dataset.value);
        const circle = chart.querySelector('.circle');
        const radius = parseInt(circle.getAttribute('r'));
        const circumference = calculateCircumference(radius);
        circle.style.strokeDasharray = `${circumference} ${circumference}`;
        circle.style.strokeDashoffset = circumference - (value / 100 * circumference);
    });

    // Configurar eventos para los switches de los relés
    document.getElementById('rele1-switch').addEventListener('change', function() {
        controlRelay('rele1', this.checked);
    });

    document.getElementById('rele2-switch').addEventListener('change', function() {
        controlRelay('rele2', this.checked);
    });

    // Obtener datos iniciales
    fetchSensorData();

    // Configurar actualización periódica
    setInterval(fetchSensorData, 5000); // Actualizar cada 5 segundos
});
