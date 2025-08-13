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
lvagitator.style.height = data.level1 + '%';
lvstorage.style.height = data.level2 + '%';
agitatorval.textContent = data.level1 + '%';
storageval.textContent = data.level2 + '%';
console.log("mode_filtering =", data.mode_filtering, "tipe =", typeof data.mode_filtering);
console.log("mode_pump1 =", data.pump1, "tipe =", typeof data.pump1);
console.log("mode_pump2 =", data.pump2, "tipe =", typeof data.pump2);
console.log("mode_pump3 =", data.pump3, "tipe =", typeof data.pump3);



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
