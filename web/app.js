const workloads = {
  arithmetic: {
    name: "arithmetic.elf",
    label: "Arithmetic dependency loop",
    command: "./sim --program workloads/arithmetic.elf --config configs/default.json --stats out.json",
    base: {
      cycles: 2112,
      committed: 2980,
      branchMiss: 0.038,
      branchMisses: 42,
      l1i: 0.991,
      l1d: 0.964,
      l2: 0.721,
      robAvg: 37.4,
      rsAvg: 16.2,
      robMax: 61,
      rsMax: 29
    },
    occupancy: [11, 18, 30, 43, 56, 48, 39, 31, 26, 19, 15, 12],
    rs: [4, 7, 12, 20, 27, 24, 17, 13, 10, 7, 5, 4],
    trace: [
      ["0x10000", "addi x5,x0,0", 1, 2, 3, 4, 5, "ok"],
      ["0x10004", "addi x6,x0,100", 1, 2, 3, 4, 5, "ok"],
      ["0x10008", "add x7,x7,x5", 2, 3, 5, 6, 7, "dep"],
      ["0x1000c", "addi x5,x5,1", 2, 3, 4, 5, 7, "ok"],
      ["0x10010", "blt x5,x6,-8", 3, 4, 6, 7, 9, "pred"]
    ]
  },
  branch: {
    name: "branch.elf",
    label: "Alternating branch pattern",
    command: "./sim --program workloads/branch.elf --config configs/default.json --stats out.json",
    base: {
      cycles: 1624,
      committed: 1851,
      branchMiss: 0.188,
      branchMisses: 89,
      l1i: 0.987,
      l1d: 1.0,
      l2: 0.667,
      robAvg: 24.8,
      rsAvg: 10.9,
      robMax: 52,
      rsMax: 23
    },
    occupancy: [8, 16, 33, 21, 9, 28, 41, 18, 10, 32, 25, 11],
    rs: [3, 8, 18, 9, 4, 15, 22, 8, 4, 16, 12, 5],
    trace: [
      ["0x10000", "andi x28,x5,1", 1, 2, 3, 4, 5, "ok"],
      ["0x10004", "beq x28,x0,8", 1, 2, 3, 5, 9, "miss"],
      ["0x10008", "addi x7,x7,3", 2, 3, 4, 0, 0, "flush"],
      ["0x10010", "addi x7,x7,1", 9, 10, 11, 12, 13, "ok"],
      ["0x10014", "blt x5,x6,-20", 10, 11, 12, 13, 15, "pred"]
    ]
  },
  cache: {
    name: "cache.elf",
    label: "Stride cache stress",
    command: "./sim --program workloads/cache.elf --config configs/default.json --stats out.json",
    base: {
      cycles: 6890,
      committed: 2314,
      branchMiss: 0.021,
      branchMisses: 6,
      l1i: 0.993,
      l1d: 0.583,
      l2: 0.804,
      robAvg: 51.2,
      rsAvg: 24.6,
      robMax: 64,
      rsMax: 32
    },
    occupancy: [18, 37, 62, 64, 59, 46, 61, 64, 53, 41, 58, 63],
    rs: [6, 18, 31, 32, 26, 19, 30, 32, 25, 17, 28, 31],
    trace: [
      ["0x10000", "sw x6,0(x5)", 1, 2, 4, 19, 23, "l1d miss"],
      ["0x10004", "lw x28,0(x5)", 1, 2, 24, 39, 42, "wait"],
      ["0x10008", "add x6,x6,x28", 2, 3, 43, 44, 45, "dep"],
      ["0x1000c", "addi x5,x5,64", 2, 3, 4, 5, 45, "ok"],
      ["0x10014", "bne x7,x0,-20", 3, 4, 46, 47, 48, "pred"]
    ]
  }
};

const presets = {
  balanced: { rob: 64, cache: 16, issueBoost: 1, label: "balanced pressure" },
  wide: { rob: 96, cache: 32, issueBoost: 1.18, label: "wide core headroom" },
  tiny: { rob: 32, cache: 8, issueBoost: 0.74, label: "resource constrained" }
};

const elements = {
  workload: document.querySelector("#workload"),
  predictor: document.querySelector("#predictor"),
  rob: document.querySelector("#rob"),
  cache: document.querySelector("#cache"),
  robValue: document.querySelector("#robValue"),
  cacheValue: document.querySelector("#cacheValue"),
  cliCommand: document.querySelector("#cliCommand"),
  copyCommand: document.querySelector("#copyCommand"),
  downloadStats: document.querySelector("#downloadStats"),
  runName: document.querySelector("#runName"),
  runStatus: document.querySelector("#runStatus"),
  ipcMetric: document.querySelector("#ipcMetric"),
  cycleMetric: document.querySelector("#cycleMetric"),
  branchMetric: document.querySelector("#branchMetric"),
  branchCountMetric: document.querySelector("#branchCountMetric"),
  l1dMetric: document.querySelector("#l1dMetric"),
  l2Metric: document.querySelector("#l2Metric"),
  robMetric: document.querySelector("#robMetric"),
  rsMetric: document.querySelector("#rsMetric"),
  pressureLabel: document.querySelector("#pressureLabel"),
  traceTable: document.querySelector("#traceTable"),
  pipelineCanvas: document.querySelector("#pipelineCanvas"),
  cacheChart: document.querySelector("#cacheChart"),
  occupancyChart: document.querySelector("#occupancyChart")
};

let activePreset = "balanced";
let currentStats = null;

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function percent(value) {
  return `${(value * 100).toFixed(1)}%`;
}

function currentModel() {
  const workload = workloads[elements.workload.value];
  const rob = Number(elements.rob.value);
  const cache = Number(elements.cache.value);
  const preset = presets[activePreset];
  const predictorPenalty = elements.predictor.value === "bimodal" ? 1.28 : 1;
  const robFactor = clamp(rob / 64, 0.5, 1.7);
  const cacheFactor = clamp(cache / 16, 0.4, 2.8);
  const base = workload.base;

  const ipc = clamp(
    (base.committed / base.cycles) * preset.issueBoost * Math.sqrt(robFactor),
    0.18,
    3.8
  );
  const cycles = Math.round(base.committed / ipc);
  const branchMiss = clamp(base.branchMiss * predictorPenalty, 0.004, 0.48);
  const l1d = clamp(base.l1d + Math.log2(cacheFactor) * 0.055, 0.21, 0.995);
  const l1i = clamp(base.l1i + Math.log2(cacheFactor) * 0.012, 0.86, 0.998);
  const l2 = clamp(base.l2 + Math.log2(cacheFactor) * 0.035, 0.34, 0.97);
  const robAvg = clamp(base.robAvg * Math.sqrt(robFactor), 4, rob - 2);
  const rsAvg = clamp(base.rsAvg * preset.issueBoost, 2, 40);

  return {
    workload,
    cycles,
    committed: base.committed,
    ipc,
    branchMiss,
    branchMisses: Math.round(base.branchMisses * predictorPenalty),
    l1i,
    l1d,
    l2,
    robAvg,
    rsAvg,
    robMax: Math.min(rob, Math.round(base.robMax * Math.sqrt(robFactor))),
    rsMax: Math.round(base.rsMax * preset.issueBoost),
    occupancy: workload.occupancy.map((v) => clamp(v * robFactor, 2, rob)),
    rs: workload.rs.map((v) => clamp(v * preset.issueBoost, 1, 42)),
    trace: workload.trace
  };
}

function setPreset(name) {
  activePreset = name;
  document.querySelectorAll(".segment").forEach((button) => {
    button.classList.toggle("is-active", button.dataset.core === name);
  });
  elements.rob.value = presets[name].rob;
  elements.cache.value = presets[name].cache;
  render();
}

function render() {
  currentStats = currentModel();
  elements.robValue.textContent = elements.rob.value;
  elements.cacheValue.textContent = `${elements.cache.value} KB`;
  elements.runName.textContent = currentStats.workload.name;
  elements.runStatus.textContent = "halted with exit code 0";
  elements.ipcMetric.textContent = currentStats.ipc.toFixed(2);
  elements.cycleMetric.textContent = `${currentStats.cycles.toLocaleString()} cycles`;
  elements.branchMetric.textContent = percent(currentStats.branchMiss);
  elements.branchCountMetric.textContent = `${currentStats.branchMisses} misses`;
  elements.l1dMetric.textContent = percent(currentStats.l1d);
  elements.l2Metric.textContent = `L2 hit ${percent(currentStats.l2)}`;
  elements.robMetric.textContent = currentStats.robAvg.toFixed(1);
  elements.rsMetric.textContent = `RS avg ${currentStats.rsAvg.toFixed(1)}`;
  elements.pressureLabel.textContent = presets[activePreset].label;
  elements.cliCommand.textContent = currentStats.workload.command;

  renderPipeline(currentStats);
  renderCacheChart(currentStats);
  renderOccupancyChart(currentStats);
  renderTrace(currentStats);
}

function setupCanvas(canvas) {
  const rect = canvas.getBoundingClientRect();
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.max(1, Math.round(rect.width * scale));
  canvas.height = Math.max(1, Math.round(rect.height * scale));
  const ctx = canvas.getContext("2d");
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  return { ctx, width: rect.width, height: rect.height };
}

function renderPipeline(stats) {
  const { ctx, width, height } = setupCanvas(elements.pipelineCanvas);
  ctx.clearRect(0, 0, width, height);
  const stages = ["Fetch", "Decode", "Rename", "Issue", "Execute", "Commit"];
  const colors = ["#a8b7c7", "#64d7d2", "#f6b44b", "#b4f06b", "#ef6a55", "#f2f0dd"];
  const pad = width < 720 ? 18 : 34;
  const usable = width - pad * 2;
  const boxW = usable / stages.length - 10;
  const midY = height * 0.48;

  ctx.strokeStyle = "rgba(242,240,221,0.2)";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(pad, midY);
  ctx.lineTo(width - pad, midY);
  ctx.stroke();

  stages.forEach((stage, index) => {
    const x = pad + index * (boxW + 10);
    const pulse = Math.sin(Date.now() / 450 + index) * 5;
    ctx.fillStyle = "rgba(11,12,9,0.76)";
    ctx.strokeStyle = colors[index];
    ctx.lineWidth = 1.5;
    roundRect(ctx, x, midY - 48 + pulse, boxW, 94, 8);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = colors[index];
    ctx.font = "700 12px Avenir Next, Segoe UI, sans-serif";
    ctx.fillText(stage.toUpperCase(), x + 14, midY - 18 + pulse);
    ctx.fillStyle = "#f2f0dd";
    ctx.font = "700 26px Georgia, serif";
    const value = index < 3 ? stats.committed : index === 3 ? stats.rsMax : index === 4 ? stats.robMax : stats.committed;
    ctx.fillText(String(value), x + 14, midY + 20 + pulse);
  });

  const textY = Math.max(34, height - 44);
  ctx.fillStyle = "#9da08d";
  ctx.font = "700 12px Avenir Next, Segoe UI, sans-serif";
  ctx.fillText("Cycle-stepped frontend, rename, issue, execute, and in-order commit", pad, textY);
}

function roundRect(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

function renderCacheChart(stats) {
  const { ctx, width, height } = setupCanvas(elements.cacheChart);
  const rows = [
    ["L1I", stats.l1i, "#a8b7c7"],
    ["L1D", stats.l1d, "#b4f06b"],
    ["L2", stats.l2, "#f6b44b"]
  ];
  drawBars(ctx, width, height, rows, "Hit rate");
}

function renderOccupancyChart(stats) {
  const { ctx, width, height } = setupCanvas(elements.occupancyChart);
  ctx.clearRect(0, 0, width, height);
  drawLine(ctx, width, height, stats.occupancy, "#b4f06b", Number(elements.rob.value), "ROB");
  drawLine(ctx, width, height, stats.rs, "#64d7d2", 42, "RS");
}

function drawBars(ctx, width, height, rows, label) {
  ctx.clearRect(0, 0, width, height);
  const left = 58;
  const top = 24;
  const barH = 34;
  const gap = 28;
  const maxW = width - left - 34;
  ctx.fillStyle = "#9da08d";
  ctx.font = "700 12px Avenir Next, Segoe UI, sans-serif";
  ctx.fillText(label.toUpperCase(), left, 14);
  rows.forEach(([name, value, color], index) => {
    const y = top + index * (barH + gap);
    ctx.fillStyle = "rgba(242,240,221,0.08)";
    roundRect(ctx, left, y, maxW, barH, 5);
    ctx.fill();
    ctx.fillStyle = color;
    roundRect(ctx, left, y, maxW * value, barH, 5);
    ctx.fill();
    ctx.fillStyle = "#f2f0dd";
    ctx.font = "800 13px Avenir Next, Segoe UI, sans-serif";
    ctx.fillText(name, 8, y + 22);
    ctx.fillText(percent(value), left + maxW - 58, y + 22);
  });
}

function drawLine(ctx, width, height, values, color, maxValue, label) {
  const left = 42;
  const right = width - 18;
  const top = 26;
  const bottom = height - 28;
  ctx.strokeStyle = "rgba(242,240,221,0.12)";
  ctx.lineWidth = 1;
  for (let i = 0; i < 4; i += 1) {
    const y = top + ((bottom - top) * i) / 3;
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(right, y);
    ctx.stroke();
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = 3;
  ctx.beginPath();
  values.forEach((value, index) => {
    const x = left + ((right - left) * index) / (values.length - 1);
    const y = bottom - (clamp(value / maxValue, 0, 1) * (bottom - top));
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
  ctx.fillStyle = color;
  ctx.font = "800 12px Avenir Next, Segoe UI, sans-serif";
  ctx.fillText(label, left, label === "ROB" ? 16 : 34);
}

function renderTrace(stats) {
  const maxCycle = Math.max(...stats.trace.flatMap((row) => row.slice(2, 7)));
  elements.traceTable.innerHTML = "";
  stats.trace.forEach(([pc, mnemonic, fetch, rename, issue, complete, commit, state]) => {
    const row = document.createElement("div");
    row.className = "trace-row";
    const timeline = document.createElement("div");
    timeline.className = "timeline";
    const stages = [
      ["stage-fetch", fetch, rename || fetch + 1],
      ["stage-rename", rename, issue || rename + 1],
      ["stage-issue", issue, complete || issue + 1],
      ["stage-execute", issue, complete || issue + 1],
      ["stage-commit", complete, commit || complete + 1]
    ];
    stages.forEach(([className, start, end]) => {
      if (!start || !end) return;
      const seg = document.createElement("span");
      seg.className = className;
      const left = (start / maxCycle) * 100;
      const width = Math.max(2.5, ((end - start + 1) / maxCycle) * 100);
      seg.style.left = `${left}%`;
      seg.style.width = `${width}%`;
      timeline.appendChild(seg);
    });
    row.innerHTML = `<strong>${pc}</strong><code>${mnemonic}</code>`;
    row.appendChild(timeline);
    const badge = document.createElement("span");
    badge.className = "badge";
    badge.textContent = state;
    row.appendChild(badge);
    elements.traceTable.appendChild(row);
  });
}

function downloadStats() {
  const stats = {
    summary: {
      workload: currentStats.workload.name,
      cycles: currentStats.cycles,
      committedInstructions: currentStats.committed,
      ipc: Number(currentStats.ipc.toFixed(4)),
      halted: true,
      exitCode: 0
    },
    branchPredictor: {
      mode: elements.predictor.value,
      mispredictionRate: Number(currentStats.branchMiss.toFixed(4)),
      mispredictions: currentStats.branchMisses
    },
    caches: {
      l1i: { hitRate: Number(currentStats.l1i.toFixed(4)) },
      l1d: { hitRate: Number(currentStats.l1d.toFixed(4)) },
      l2: { hitRate: Number(currentStats.l2.toFixed(4)) }
    },
    occupancy: {
      rob: { average: Number(currentStats.robAvg.toFixed(2)), max: currentStats.robMax },
      reservationStations: { average: Number(currentStats.rsAvg.toFixed(2)), max: currentStats.rsMax }
    }
  };
  const blob = new Blob([JSON.stringify(stats, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `${currentStats.workload.name.replace(".elf", "")}-stats.json`;
  link.click();
  URL.revokeObjectURL(url);
}

document.querySelectorAll(".segment").forEach((button) => {
  button.addEventListener("click", () => setPreset(button.dataset.core));
});

elements.workload.addEventListener("change", render);
elements.predictor.addEventListener("change", render);
elements.rob.addEventListener("input", render);
elements.cache.addEventListener("input", render);
elements.downloadStats.addEventListener("click", downloadStats);
elements.copyCommand.addEventListener("click", async () => {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(elements.cliCommand.textContent);
  } else {
    const textarea = document.createElement("textarea");
    textarea.value = elements.cliCommand.textContent;
    textarea.style.position = "fixed";
    textarea.style.opacity = "0";
    document.body.appendChild(textarea);
    textarea.select();
    document.execCommand("copy");
    textarea.remove();
  }
  elements.copyCommand.textContent = "Copied";
  setTimeout(() => {
    elements.copyCommand.textContent = "Copy command";
  }, 1200);
});

window.addEventListener("resize", render);
setInterval(() => renderPipeline(currentStats || currentModel()), 800);
render();
