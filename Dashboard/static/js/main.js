const socket = io();

// Variabel Objek Visual
const lvagitator = document.getElementById("lv-agitator");
const lvstorage = document.getElementById("lv-storage");
const sv1 = document.getElementById("sv-1");
const pump2 = document.getElementById("pump-2");
const pump1 = document.getElementById("pump-1");
const pump3 = document.getElementById("pump-3");
const sv2 = document.getElementById("sv-2");
const sv3 = document.getElementById("sv-3");
const sv4 = document.getElementById("sv-4");
const sv5 = document.getElementById("sv-5");
const sv6 = document.getElementById("sv-6");
const agitatorval = document.getElementById("agitator-val");
const storageval = document.getElementById("storage-val");
const filtering = document.getElementById("indikator-filtering");
const standby = document.getElementById("indikator-standby");
const backwash = document.getElementById("indikator-backwash");
const drain = document.getElementById("indikator-drain");
const override = document.getElementById("indikator-override");

// Handle Perubahan Visual
socket.on("data_monitor", (data) => {

// Bar Level Air
lvagitator.style.height = data.level1 + '%';
lvstorage.style.height = data.level2 + '%';
agitatorval.textContent = data.level1 + '%';
storageval.textContent = data.level2 + '%';

// Indikator Mode Filtering
if (data.mode_filtering === 1) {
  filtering.className = "true";
} else {
  filtering.className = "false";
}

// Indikator Mode Standby
if (data.mode_standby === 1) {
  standby.className = "true";
} else {
  standby.className = "false";
}

// Indikator Mode Backwash
if (data.mode_backwash === 1) {
  backwash.className = "true";
} else {
  backwash.className = "false";
}

// Indikator Mode Drain
if (data.mode_drain === 1) {
  drain.className = "true";
} else {
  drain.className = "false";
}

// Indikator Mode Override
if (data.mode_override === 1) {
  override.className = "true";
} else {
  override.className = "false";
}

// Indikator Pump
if (data.pump1 === 1) {
  pump1.className = "true";
} else {
  pump1.className = "false";
}
if (data.pump2 === 1) {
  pump2.className = "true";
} else {
  pump2.className = "false";
}
if (data.pump3 === 1) {
  pump3.className = "true";
} else {
  pump3.className = "false";
}

// Indikator Solenoid Valve
if (data.solenoid1 === 1) {
  sv1.className = "true";
} else {
  sv1.className = "false";
}
if (data.solenoid2 === 1) {
  sv2.className = "true";
} else {
  sv2.className = "false";
}
if (data.solenoid3 === 1) {
  sv3.className = "true";
} else {
  sv3.className = "false";
}
if (data.solenoid4 === 1) {
  sv4.className = "true";
} else {
  sv4.className = "false";
}
if (data.solenoid5 === 1) {
  sv5.className = "true";
} else {
  sv5.className = "false";
}
if (data.solenoid6 === 1) {
  sv6.className = "true";
} else {
  sv6.className = "false";
}
})





// Jika tombol emergency
function emergency() {
  const konfirmasi = confirm("NYALAKAN SOP EMERGENCY?");
  if (konfirmasi) {
    console.log("Emergency Dinyalakan");
    socket.emit('emergency', {status: 'emergency'});
  } else {
    console.log("Emergency dibatalkan");
  }

}
