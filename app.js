document.addEventListener("DOMContentLoaded", () => {

    // --- 1. CONFIGURATION & STATE ---
    const POLLING_RATE_MS = 500; // Fetch data twice a second
    const API_ENDPOINT = '/api/telemetry'; // Set to your actual Flask/Arduino route

    // Uptime variables
    let systemStartTime = Date.now() - (14 * 86400000 + 22 * 3600000 + 8 * 60000 + 45 * 1000); // Mock starting time
    let systemIsRunning = false;

    // --- 2. UPTIME CLOCK LOGIC ---
    function updateUptimeClock() {
        const now = Date.now();
        const diffInSeconds = Math.floor((now - systemStartTime) / 1000);

        const days = Math.floor(diffInSeconds / (3600 * 24));
        const hours = Math.floor((diffInSeconds % (3600 * 24)) / 3600);
        const minutes = Math.floor((diffInSeconds % 3600) / 60);
        const seconds = diffInSeconds % 60;

        const formattedTime =
            String(days).padStart(2, '0') + ':' +
            String(hours).padStart(2, '0') + ':' +
            String(minutes).padStart(2, '0') + ':' +
            String(seconds).padStart(2, '0');

        document.getElementById("uptime-clock").textContent = formattedTime;
    }
    // Start clock immediately
    setInterval(updateUptimeClock, 1000);

    // --- 3. THEME TOGGLE LOGIC ---
    const themeBtn = document.getElementById("theme-toggle");
    const themeIcon = document.getElementById("theme-icon");
    const htmlElement = document.documentElement;

    themeBtn.addEventListener("click", () => {
        if (htmlElement.classList.contains("dark")) {
            htmlElement.classList.remove("dark");
            themeIcon.textContent = "dark_mode";
        } else {
            htmlElement.classList.add("dark");
            themeIcon.textContent = "light_mode";
        }
    });

    // --- 4. ACTION BUTTONS ---
    function updateSystemControlsUI() {
        const btnStart = document.getElementById("btn-start");
        const btnStop  = document.getElementById("btn-stop");

        if (systemIsRunning) {
            // START → disabled state showing RUNNING
            btnStart.style.background    = "var(--surface-3)";
            btnStart.style.color         = "var(--tertiary)";
            btnStart.style.cursor        = "not-allowed";
            btnStart.style.boxShadow     = "none";
            btnStart.style.border        = "1px solid var(--border)";
            btnStart.style.pointerEvents = "none";
            btnStart.innerHTML = `<span class="material-symbols-outlined" style="font-size:16px; font-variation-settings:'FILL' 1;">check_circle</span>SYSTEM_RUNNING`;

            // STOP → active red glow
            btnStop.style.background    = "rgba(238,125,119,0.12)";
            btnStop.style.color         = "var(--error)";
            btnStop.style.border        = "1px solid var(--error)";
            btnStop.style.cursor        = "pointer";
            btnStop.style.opacity       = "1";
            btnStop.style.pointerEvents = "auto";
        } else {
            // START → active primary
            btnStart.style.background    = "var(--primary)";
            btnStart.style.color         = "#003a70";
            btnStart.style.cursor        = "pointer";
            btnStart.style.boxShadow     = "0 4px 0 0 rgba(0,60,120,0.3), 0 8px 15px rgba(0,0,0,0.2)";
            btnStart.style.border        = "none";
            btnStart.style.pointerEvents = "auto";
            btnStart.innerHTML = `<span class="material-symbols-outlined" style="font-size:16px; font-variation-settings:'FILL' 1;">play_arrow</span>START_SYSTEM`;

            // STOP → muted/disabled
            btnStop.style.background    = "var(--surface-2)";
            btnStop.style.color         = "var(--text-2)";
            btnStop.style.border        = "1px solid var(--border)";
            btnStop.style.cursor        = "not-allowed";
            btnStop.style.opacity       = "0.4";
            btnStop.style.pointerEvents = "none";
        }
    }

    document.getElementById("btn-start").addEventListener("click", async () => {
        if (systemIsRunning) return;
        console.log("Sending START command to hardware...");
        systemIsRunning = true;
        updateSystemControlsUI();
        try {
            await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: 'start' })
            });
        } catch (error) {
            console.error("Failed to send START:", error);
            systemIsRunning = false;
            updateSystemControlsUI();
        }
    });

    document.getElementById("btn-stop").addEventListener("click", async () => {
        if (!systemIsRunning) return;
        console.log("Sending ESTOP command to hardware...");
        systemIsRunning = false;
        updateSystemControlsUI();
        try {
            await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: 'stop' })
            });
        } catch (error) {
            console.error("Failed to send STOP:", error);
        }
    });

    // Initialize UI
    updateSystemControlsUI();

    // --- 5. DOM UPDATERS ---

    // Formatting numbers with commas (e.g. 4208 -> 004,208)
    const formatNumber = (num, padding) => {
        return num.toString().padStart(padding, '0').replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    };

    // Calculate rotation angle for the dial needle (-120deg to +120deg sweep)
    const setDialRotation = (needleId, trackId, value, min, max, accentColor) => {
        const percentage = Math.max(0, Math.min(1, (value - min) / (max - min)));
        const degrees = -120 + (percentage * 240);
        const el = document.getElementById(needleId);
        if (el) el.style.transform = `translateY(-20px) rotate(${degrees}deg)`;

        // Update conic gradient track opacity/color
        const track = document.getElementById(trackId);
        if (track) {
            const fillDeg = percentage * 240;
            track.style.opacity = "0.35";
            track.style.background =
                `conic-gradient(from -120deg at 50% 50%, #004883 0deg, ${accentColor} ${fillDeg}deg, var(--surface-2) ${fillDeg}deg)`;
        }
    };

    // Helper to set Hardware Status UI using CSS variable tokens
    const setHardwareStatus = (prefix, stateType) => {
        const led    = document.getElementById(`led-${prefix}`);
        const status = document.getElementById(`status-${prefix}`);
        const icon   = document.getElementById(`icon-${prefix}`);

        // Reset
        led.className    = "led";
        status.style.color = "var(--text-2)";
        icon.style.color   = "var(--text-2)";

        if (stateType === "RUNNING") {
            led.classList.add("on-green", "pulse");
            status.style.color = "var(--tertiary)";
            status.textContent  = "RUNNING";
            icon.style.color    = "var(--tertiary)";
            icon.style.fontVariationSettings = "'FILL' 1";
        } else if (stateType === "STOPPED") {
            status.textContent  = "STOPPED";
            icon.style.fontVariationSettings = "'FILL' 0";
            if (prefix.includes('motor')) icon.textContent = "pause_circle";
        } else if (stateType === "CLEAR") {
            led.classList.add("on-green");
            led.style.opacity = "0.4";
            status.textContent  = "CLEAR";
            icon.textContent    = "sensors";
        } else if (stateType === "BLOCKED") {
            led.classList.add("on-red", "pulse");
            status.style.color = "var(--error)";
            status.textContent  = "BLOCKED";
            icon.style.color    = "var(--error)";
            icon.textContent    = "sensors_off";
        }
    };

    // --- 6. HTTP POLLING / DATA FETCH ---
    async function pollTelemetry() {
        try {
            let data;

            const response = await fetch(API_ENDPOINT);
            data = await response.json();

            // 1. Update Telemetry Counters
            document.getElementById("val-total").textContent = formatNumber(data.metrics.total, 6);
            document.getElementById("val-good").textContent = formatNumber(data.metrics.good, 5);
            document.getElementById("val-defect").textContent = formatNumber(data.metrics.defect, 5);
            document.getElementById("val-moisture-count").textContent = formatNumber(data.metrics.moisture_reject, 5);
            document.getElementById("camera-fps").textContent = data.metrics.fps;

            // 2. Update Hardware States
            setHardwareStatus("motor-a", data.hardware.motorA);
            setHardwareStatus("hopper", data.hardware.hopper);
            setHardwareStatus("ir-1", data.hardware.ir1);

            // 3. Update Moisture Dial
            const moisturePercent = Math.round(data.environment.moisture);
            const valMoisture     = document.getElementById("val-moisture");

            if (moisturePercent > 13) {
                valMoisture.textContent    = moisturePercent + "%";
                valMoisture.style.color    = "var(--error)";
                setDialRotation("dial-moisture", "moisture-track", moisturePercent, 0, 100, "var(--error)");
            } else {
                valMoisture.textContent    = moisturePercent + "%";
                valMoisture.style.color    = "var(--tertiary)";
                setDialRotation("dial-moisture", "moisture-track", moisturePercent, 0, 100, "var(--tertiary)");
            }

            // 4. Sync visual feedback
            const syncEl = document.getElementById("sync-indicator");
            syncEl.querySelector("span").style.color = "var(--tertiary)";
            syncEl.querySelector("span").textContent = "LIVE_SYNC";

        } catch (error) {
            console.error("Telemetry fetch failed:", error);
            const syncEl = document.getElementById("sync-indicator");
            syncEl.querySelector("span").style.color = "var(--error)";
            syncEl.querySelector("span").textContent = "CONN_LOST";
        }
    }

    // Start Polling Loop
    setInterval(pollTelemetry, POLLING_RATE_MS);
    pollTelemetry(); // Initial call
});