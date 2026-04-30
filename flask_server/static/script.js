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

function formatNumber(value, decimals = 2) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return '--';
  }
  return Number(value).toFixed(decimals);
}

async function fetchStatus() {
  try {
    const response = await fetch('/api/status');
    const data = await response.json();

    els.temperature.textContent = formatNumber(data.temperature_c, 1);
    els.valid.textContent = data.valid ? 'Valid reading' : 'No valid reading';
    els.adc.textContent = data.adc ?? '--';
    els.voltage.textContent = `${formatNumber(data.voltage, 3)} V`;
    els.resistance.textContent = `${formatNumber(data.resistance, 1)} Ω`;
    els.macAddress.textContent = data.mac_address || '--';
    els.lastUpdate.textContent = data.last_update || 'No data yet';
    els.currentPattern.textContent = data.pattern || 'solid';

    els.patternButtons.forEach((button) => {
      button.classList.toggle('active', button.dataset.pattern === data.pattern);
    });
  } catch (error) {
    console.error('Could not fetch status:', error);
  }
}

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

els.patternButtons.forEach((button) => {
  button.addEventListener('click', () => setPattern(button.dataset.pattern));
});

fetchStatus();
setInterval(fetchStatus, 2000);
