"""Flask dashboard server for the COM3505 ESP32 IoT assignment.

The ESP32 sends sensor readings to this server using HTTP POST requests.
The browser dashboard reads the latest values from this server and sends the
selected LED pattern back to the ESP32.
"""

from datetime import datetime

from flask import Flask, jsonify, render_template, request
from flask_cors import CORS

# Create the Flask application. Flask automatically looks for HTML files in the
# templates/ folder and static assets in the static/ folder.
app = Flask(__name__)

# CORS is enabled so that requests can be accepted if the dashboard or ESP32
# accesses the server from another local network address during testing.
CORS(app)

# Only these LED pattern names are accepted from the browser. This prevents
# accidental or invalid pattern values being sent to the ESP32.
ALLOWED_PATTERNS = {"solid", "blink", "chase", "rainbow", "fire", "temperature"}

# Shared in-memory state for the dashboard.
# This is reset whenever the Flask server restarts, which is acceptable for this
# coursework prototype because the live ESP32 readings will be sent again.
latest_state = {
    "temperature_c": None,
    "adc": None,
    "voltage": None,
    "resistance": None,
    "valid": False,
    "pattern": "solid",
    "last_update": None,
}


@app.route("/")
def index():
    """Serve the main browser dashboard page."""
    return render_template("index.html")


@app.route("/api/sensor", methods=["POST"])
def receive_sensor_data():
    """Receive NTC thermistor readings posted by the ESP32."""
    data = request.get_json(silent=True)

    # The ESP32 should always send JSON. Return a clear error if the body is
    # missing or malformed so that debugging is easier.
    if not data:
        return jsonify({"error": "No JSON body received"}), 400

    # Store the latest reading. The web page fetches this state periodically via
    # /api/status, so only the most recent value needs to be kept.
    latest_state["temperature_c"] = data.get("temperature_c")
    latest_state["adc"] = data.get("adc")
    latest_state["voltage"] = data.get("voltage")
    latest_state["resistance"] = data.get("resistance")
    latest_state["valid"] = bool(data.get("valid", False))
    latest_state["last_update"] = datetime.now().strftime("%H:%M:%S")

    return jsonify({"status": "ok", "state": latest_state})


@app.route("/api/status", methods=["GET"])
def get_status():
    """Return the latest sensor reading and selected LED pattern to the UI."""
    return jsonify(latest_state)


@app.route("/api/pattern", methods=["GET"])
def get_pattern():
    """Return the current LED pattern as JSON for ESP32 polling."""
    return jsonify({"pattern": latest_state["pattern"]})


@app.route("/api/pattern", methods=["POST"])
def set_pattern():
    """Update the LED pattern selected by the user in the browser dashboard."""
    data = request.get_json(silent=True)

    if not data or "pattern" not in data:
        return jsonify({"error": "Pattern missing"}), 400

    pattern = str(data["pattern"]).strip().lower()

    if pattern not in ALLOWED_PATTERNS:
        return jsonify({"error": f"Unsupported pattern: {pattern}"}), 400

    # The ESP32 reads this value from /api/pattern and applies the corresponding
    # LED animation in its main loop.
    latest_state["pattern"] = pattern
    return jsonify({"status": "ok", "pattern": pattern})


if __name__ == "__main__":
    # host="0.0.0.0" makes the server reachable from other devices on the same
    # WiFi network, including the ESP32. The server runs on port 5001.
    app.run(host="0.0.0.0", port=5001, debug=True)
