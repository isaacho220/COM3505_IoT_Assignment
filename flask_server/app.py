from datetime import datetime
from flask import Flask, jsonify, render_template, request
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

ALLOWED_PATTERNS = {"solid", "blink", "chase", "rainbow", "fire", "temperature"}

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
    return render_template("index.html")


@app.route("/api/sensor", methods=["POST"])
def receive_sensor_data():
    """Receive NTC sensor data from ESP32."""
    data = request.get_json(silent=True)

    if not data:
        return jsonify({"error": "No JSON body received"}), 400

    latest_state["temperature_c"] = data.get("temperature_c")
    latest_state["adc"] = data.get("adc")
    latest_state["voltage"] = data.get("voltage")
    latest_state["resistance"] = data.get("resistance")
    latest_state["valid"] = bool(data.get("valid", False))
    latest_state["last_update"] = datetime.now().strftime("%H:%M:%S")

    return jsonify({"status": "ok", "state": latest_state})


@app.route("/api/status", methods=["GET"])
def get_status():
    """Return latest sensor reading and selected LED pattern for the web UI."""
    return jsonify(latest_state)


@app.route("/api/pattern", methods=["GET"])
def get_pattern():
    """Return plain-text pattern for ESP32 polling."""
    return latest_state["pattern"], 200, {"Content-Type": "text/plain; charset=utf-8"}


@app.route("/api/pattern", methods=["POST"])
def set_pattern():
    """Set current LED pattern from the browser UI."""
    data = request.get_json(silent=True)

    if not data or "pattern" not in data:
        return jsonify({"error": "Pattern missing"}), 400

    pattern = str(data["pattern"]).strip().lower()

    if pattern not in ALLOWED_PATTERNS:
        return jsonify({"error": f"Unsupported pattern: {pattern}"}), 400

    latest_state["pattern"] = pattern
    return jsonify({"status": "ok", "pattern": pattern})


if __name__ == "__main__":
    # host="0.0.0.0" lets the ESP32 reach this server over the same WiFi network.
    app.run(host="0.0.0.0", port=5001, debug=True)
