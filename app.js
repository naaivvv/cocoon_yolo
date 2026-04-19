document.addEventListener("DOMContentLoaded", () => {

    // --- 1. CONFIGURATION & STATE ---
    const POLLING_RATE_MS = 500; // Fetch data twice a second
    const API_ENDPOINT = '/api/telemetry'; // Set to your actual Flask/Arduino route

    // Uptime variables
    let systemStartTime = Date.now() - (14 * 86400000 + 22 * 3600000 + 8 * 60000 + 45 * 1000); // Mock starting time
    let systemIsRunning = true;

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
    document.getElementById("btn-start").addEventListener("click", async () => {
        console.log("Sending START command to hardware...");
        systemIsRunning = true;
        try {
            await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: 'start' })
            });
        } catch (error) {
            console.error("Failed to send START:", error);
        }
    });

    document.getElementById("btn-stop").addEventListener("click", async () => {
        console.log("Sending ESTOP command to hardware...");
        systemIsRunning = false;
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

    // --- 5. DOM UPDATERS ---

    // Formatting numbers with commas (e.g. 4208 -> 004,208)
    const formatNumber = (num, padding) => {
        return num.toString().padStart(padding, '0').replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    };

    // Calculate rotation angle for environmental dials (-120deg to +120deg sweep)
    const setDialRotation = (elementId, trackId, value, min, max, accentColor) => {
        const percentage = Math.max(0, Math.min(1, (value - min) / (max - min)));
        // -120 degrees is minimum, +120 degrees is maximum
        const degrees = -120 + (percentage * 240);

        document.getElementById(elementId).style.transform = `translateY(-28px) rotate(${degrees}deg)`;

        // Update the CSS conic-gradient track to fill up to the current value
        const fillDeg = percentage * 240;
        document.getElementById(trackId).style.background =
            `conic-gradient(from -120deg at 50% 50%, #004883 0deg, ${accentColor} ${fillDeg}deg, #161a1e ${fillDeg}deg)`;
    };

    // Helper to set Hardware Status UI specifically matching the Tailwind design
    const setHardwareStatus = (prefix, stateType) => {
        const led = document.getElementById(`led-${prefix}`);
        const status = document.getElementById(`status-${prefix}`);
        const icon = document.getElementById(`icon-${prefix}`);

        // Reset classes
        led.className = "w-3 h-3 rounded-full transition-all duration-300 ";
        status.className = "font-headline text-lg font-bold ";
        icon.style.fontVariationSettings = "'FILL' 0";

        if (stateType === "RUNNING") {
            led.className += "bg-tertiary shadow-[0_0_8px_#bbffb3]";
            status.className += "text-on-surface";
            status.textContent = "RUNNING";
            icon.className = "material-symbols-outlined text-tertiary text-sm";
            icon.style.fontVariationSettings = "'FILL' 1";
        } else if (stateType === "STOPPED") {
            led.className += "bg-surface-container-highest shadow-inner";
            status.className += "text-on-surface-variant";
            status.textContent = "STOPPED";
            icon.className = "material-symbols-outlined text-on-surface-variant text-sm";
            if (prefix.includes('motor')) icon.textContent = "pause_circle";
        } else if (stateType === "CLEAR") {
            led.className += "bg-tertiary/20 border border-tertiary/40";
            status.className += "text-on-surface";
            status.textContent = "CLEAR";
            icon.className = "material-symbols-outlined text-tertiary text-sm";
            icon.textContent = "sensors";
        } else if (stateType === "BLOCKED") {
            led.className += "bg-error shadow-[0_0_8px_#ee7d77]";
            status.className += "text-error";
            status.textContent = "BLOCKED";
            icon.className = "material-symbols-outlined text-error text-sm";
            icon.textContent = "sensors_off";
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
            document.getElementById("val-bad").textContent = formatNumber(data.metrics.bad, 5);
            document.getElementById("camera-fps").textContent = data.metrics.fps;

            // 2. Update Hardware States
            setHardwareStatus("motor-a", data.hardware.motorA);
            setHardwareStatus("hopper", data.hardware.hopper);
            setHardwareStatus("ir-1", data.hardware.ir1);

            // 3. Update Moisture Dial
            const moistureRaw = data.environment.moisture;
            const moisturePercent = Math.round((moistureRaw / 1023) * 100);
            const valMoisture = document.getElementById("val-moisture");
            
            if (moisturePercent > 13) {
                valMoisture.textContent = moisturePercent + "%";
                valMoisture.className = "font-headline text-4xl font-black text-error";
                setDialRotation("dial-moisture", "moisture-track", moisturePercent, 0, 100, "#ee7d77");
            } else {
                valMoisture.textContent = moisturePercent + "%";
                valMoisture.className = "font-headline text-4xl font-black text-tertiary";
                setDialRotation("dial-moisture", "moisture-track", moisturePercent, 0, 100, "#bbffb3");
            }

            // Sync visual feedback
            document.getElementById("sync-indicator").classList.remove("text-error");
            document.getElementById("sync-indicator").classList.add("text-secondary-fixed");
            document.getElementById("sync-indicator").textContent = "LIVE_SYNC";

        } catch (error) {
            console.error("Telemetry fetch failed:", error);
            document.getElementById("sync-indicator").classList.add("text-error");
            document.getElementById("sync-indicator").classList.remove("text-secondary-fixed");
            document.getElementById("sync-indicator").textContent = "CONN_LOST";
        }
    }

    // Start Polling Loop
    setInterval(pollTelemetry, POLLING_RATE_MS);
    pollTelemetry(); // Initial call
});