// Browser-side dashboard logic for the COM3505 IoT assignment.
// This script periodically reads the latest sensor values from Flask and sends
// the selected LED pattern back to the server.

// Cache all frequently used DOM elements in one object. This avoids repeating
// document.getElementById calls throughout the file.
const els = {
  temperature: document.getElementById('temperature'),
  valid: document.getElementById('valid'),
  adc: document.getElementById('adc'),
  voltage: document.getElementById('voltage'),
  resistance: document.getElementById('resistance'),
  macAddress: document.getElementById('macAddress'),
  lastUpdate: document.getElementById('lastUpdate'),
  currentPattern: document.getElementById('currentPattern'),
  patternButtons: document.querySelectorAll('[data-pattern]'),
};

// Convert numeric values into a fixed decimal format for display. If a value is
// missing or invalid, show "--" instead of producing NaN on the page.
function formatNumber(value, decimals = 2) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return '--';
  }
  return Number(value).toFixed(decimals);
}

// Safely update an element's text. The null check keeps the script robust if an
// optional field is removed from the HTML later.
function setText(el, value) {
  if (el) el.textContent = value;
}

// Request the latest state from Flask and update the dashboard values.
async function fetchStatus() {
  try {
    const response = await fetch('/api/status');
    const data = await response.json();

    setText(els.temperature, formatNumber(data.temperature_c, 1));
    setText(els.valid, data.valid ? 'Valid reading' : 'No valid reading');
    setText(els.adc, data.adc ?? '--');
    setText(els.voltage, `${formatNumber(data.voltage, 3)} V`);
    setText(els.resistance, `${formatNumber(data.resistance, 1)} Ω`);
    setText(els.macAddress, data.mac_address || '--');
    setText(els.lastUpdate, data.last_update || 'No data yet');
    setText(els.currentPattern, data.pattern || 'solid');

    // Highlight the currently selected pattern button so the user can see which
    // LED mode is active.
    els.patternButtons.forEach((button) => {
      button.classList.toggle('active', button.dataset.pattern === data.pattern);
    });
  } catch (error) {
    console.error('Could not fetch status:', error);
  }
}

// Send a new LED pattern to Flask. The ESP32 will later poll /api/pattern and
// apply this selected pattern.
async function setPattern(pattern) {
  try {
    await fetch('/api/pattern', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ pattern }),
    });

    await fetchStatus();
  } catch (error) {
    console.error('Could not set pattern:', error);
  }
}

// Attach a click handler to every pattern button defined in index.html.
els.patternButtons.forEach((button) => {
  button.addEventListener('click', () => setPattern(button.dataset.pattern));
});

// Load data immediately, then refresh every two seconds for a live dashboard.
fetchStatus();
setInterval(fetchStatus, 2000);