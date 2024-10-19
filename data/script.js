const labels = [];
const thrustData = [];
const voltageData = [];
const currentData = [];
const powerData = [];
const rpmData = [];
const efficiencyData = [];

let throttle = 1000;
let sampling = false;
let calibrationFactor = 1;
let recordedData = [];

const ctx = document.getElementById("dataGraph").getContext("2d");
const chart = new Chart(ctx, {
  type: "line",
  data: {
    labels: labels,
    datasets: [
      {
        label: "Thrust (g)",
        data: thrustData,
        borderColor: "rgb(255, 99, 132)",
        fill: false,
        tension: 0.1,
      },
      {
        label: "Voltage (V)",
        data: voltageData,
        borderColor: "rgb(54, 162, 235)",
        fill: false,
        tension: 0.1,
      },
      {
        label: "Current (A)",
        data: currentData,
        borderColor: "rgb(75, 192, 192)",
        fill: false,
        tension: 0.1,
      },
      {
        label: "Power (W)",
        data: powerData,
        borderColor: "rgb(255, 205, 86)",
        fill: false,
        tension: 0.1,
      },
      {
        label: "RPM",
        data: rpmData,
        borderColor: "rgb(153, 102, 255)",
        fill: false,
        tension: 0.1,
      },
      {
        label: "Efficiency (g/W)",
        data: efficiencyData,
        borderColor: "rgb(255, 159, 64)",
        fill: false,
        tension: 0.1,
      },
    ],
  },
  options: {
    responsive: true,
    plugins: {
      tooltip: {
        callbacks: {
          label: function (context) {
            const label = context.dataset.label || "";
            const value = context.raw;
            return `${label}: ${value}`;
          },
        },
      },
      legend: {
        position: "top",
      },
    },
    interaction: {
      mode: "nearest",
      axis: "x",
      intersect: false,
    },
    scales: {
      x: {
        display: false,
        title: {
          display: true,
          text: "Time (s)",
        },
      },
      y: {
        display: true,
        title: {
          display: true,
          text: "Values",
        },
      },
    },
  },
});

function startCalibration() {
  const knownWeight = parseFloat(document.getElementById("knownWeight").value);

  if (isNaN(knownWeight) || knownWeight <= 0) {
    alert("Please enter a valid known weight.");
    return;
  }

  fetch("/getRawThrust")
    .then((response) => response.json())
    .then((data) => {
      const rawThrust = data.thrust;
      console.log(rawThrust);
      calibrationFactor = knownWeight / rawThrust;
      document.getElementById("calibrationFactor").innerText = calibrationFactor.toFixed(4);

      recordedData = [];
    });
}

function updateThrottle(value) {
  throttle = value;
  document.getElementById("throttleValue").innerText = value;
  fetch(`/setThrottle?value=${value}`);
}

function startSampling() {
  sampling = true;
  fetch("/startSampling");

  document.getElementById("startBtn").disabled = true;
  document.getElementById("stopBtn").disabled = false;
  document.getElementById("printBtn").style.display = "none";
}

function stopSampling() {
  sampling = false;
  fetch("/stopSampling");

  document.getElementById("startBtn").disabled = false;
  document.getElementById("stopBtn").disabled = true;
  document.getElementById("printBtn").style.display = "block";

  populateDataTable();
}

function updateData() {
  if (!sampling) return;
  fetch("/getData")
    .then((response) => response.json())
    .then((data) => {
      document.getElementById("thrust").innerText = data.thrust;
      document.getElementById("voltage").innerText = data.voltage;
      document.getElementById("current").innerText = data.current;

      const powerUsage = (data.voltage * data.current).toFixed(2);
      document.getElementById("power").innerText = powerUsage;

      document.getElementById("rpm").innerText = data.rpm;

      const efficiency =
        data.current !== 0 && data.voltage !== 0
          ? ((data.thrust / (data.voltage * data.current)) * 100).toFixed(2)
          : 0;
      document.getElementById("efficiency").innerText = efficiency;

      const calibratedThrust = data.thrust * calibrationFactor;

      const timestamp = Date.now() / 1000;
      chart.data.labels.push(timestamp);
      thrustData.push(calibratedThrust);
      voltageData.push(data.voltage);
      currentData.push(data.current);
      rpmData.push(data.rpm);
      efficiencyData.push(efficiency);
      powerData.push(powerUsage);

      recordedData.push({
        throttle: throttle,
        thrust: calibratedThrust,
        voltage: data.voltage,
        current: data.current,
        power: powerUsage,
        rpm: data.rpm,
        efficiency: efficiency,
      });

      if (chart.data.labels.length > 20) {
        chart.data.labels.shift();
        thrustData.shift();
        voltageData.shift();
        currentData.shift();
        rpmData.shift();
        efficiencyData.shift();
        powerData.shift();
      }

      chart.update();
    });
}

function populateDataTable() {
  const tbody = document.querySelector("#dataTable tbody");
  tbody.innerHTML = "";

  recordedData.forEach((record) => {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${record.throttle}</td>
      <td>${record.thrust}</td>
      <td>${record.voltage}</td>
      <td>${record.current}</td>
      <td>${record.power}</td>
      <td>${record.rpm}</td>
      <td>${record.efficiency}</td>
    `;
    tbody.appendChild(row);
  });
}

function printData() {
  const printWindow = window.open("", "_blank", "width=800,height=600");
  printWindow.document.write(`
    <html>
      <head>
        <title>Print Data</title>
        <style>
          table {
            width: 100%;
            border-collapse: collapse;
          }
          th, td {
            border: 1px solid black;
            padding: 8px;
            text-align: left;
          }
        </style>
      </head>
      <body>
        <h1>Recorded Data</h1>
        <table>
          <thead>
            <tr>
              <th>Throttle (us)</th>
              <th>Thrust (g)</th>
              <th>Voltage (V)</th>
              <th>Current (A)</th>
              <th>Power (W)</th>
              <th>RPM</th>
              <th>Efficiency (g/W)</th>
            </tr>
          </thead>
          <tbody>
            ${recordedData
              .map(
                (record) => `
              <tr>
                <td>${record.throttle}</td>
                <td>${record.thrust}</td>
                <td>${record.voltage}</td>
                <td>${record.current}</td>
                <td>${record.power}</td>
                <td>${record.rpm}</td>
                <td>${record.efficiency}</td>
              </tr>
            `
              )
              .join("")}
          </tbody>
        </table>
      </body>
    </html>
  `);
  printWindow.document.close();
  printWindow.print();
}

setInterval(updateData, 1000);
