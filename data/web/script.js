let container = document.getElementById("joystick-container");
let stick = document.getElementById("joystick-stick");
let statusText = document.getElementById("status");
let sosBtn = document.getElementById("sos-btn");

let angleXText = document.getElementById("angle-x-display");
let angleYText = document.getElementById("angle-y-display");
let tempText = document.getElementById("temp-display");

let kpInput = document.getElementById("kp-input");
let kpSendBtn = document.getElementById("kp-send-btn");

let kdInput = document.getElementById("kd-input");
let kdSendBtn = document.getElementById("kd-send-btn");

let kiInput = document.getElementById("ki-input");
let kiSendBtn = document.getElementById("ki-send-btn");

let isDragging = false;
let maxR = 110;
let lastSend = 0;

// Значения по умолчанию при загрузке страницы
kpInput.value = 0.21;
kdInput.value = 0.02;
kiInput.value = 0.46;

// ОПРОС ТЕЛЕМЕТРИИ
setInterval(() => {
  fetch("/getAllData")
    .then(r => r.text())
    .then(data => {
      let parts = data.split(",");

      let x = parseInt(parts[0]);
      let y = parseInt(parts[1]);
      let temp = parseInt(parts[2]);

      angleXText.innerHTML = "Угол X: " + x + "&deg;";
      angleXText.style.color =
        Math.abs(x) < 25 ? "#00e676" : "#ff3d00";

      angleYText.innerHTML = "Угол Y: " + y + "&deg;";
      angleYText.style.color =
        Math.abs(y) < 25 ? "#00e676" : "#ff3d00";

      tempText.innerHTML = "Температура: " + temp + "&deg;";
      tempText.style.color =
        temp > 50 ? "#ff3d00" : "#00e676";

      statusText.innerText = "КОМАНДЫ ДОХОДЯТ";
      statusText.style.color = "#555";
    })
    .catch(() => {
      statusText.innerText = "СВЯЗЬ ПОТЕРЯНА";
      statusText.style.color = "#ff3d00";
    });
}, 150);

// KP
kpSendBtn.onclick = () => {
  let val = kpInput.value;

  if (val === "") {
    return alert("Введите значение!");
  }

  fetch("/setKp?val=" + val)
    .then(r => {
      if (r.ok) {
        kpSendBtn.style.background = "#00e676";
      }
    })
    .catch(() => {
      kpSendBtn.style.background = "#ff3d00";
    });
};

// KD
kdSendBtn.onclick = () => {
  let val = kdInput.value;

  if (val === "") {
    return alert("Введите значение!");
  }

  fetch("/setKd?val=" + val)
    .then(r => {
      if (r.ok) {
        kdSendBtn.style.background = "#00e676";
      }
    })
    .catch(() => {
      kdSendBtn.style.background = "#ff3d00";
    });
};

// KI
kiSendBtn.onclick = () => {
  let val = kiInput.value;

  if (val === "") {
    return alert("Введите значение!");
  }

  fetch("/setKi?val=" + val)
    .then(r => {
      if (r.ok) {
        kiSendBtn.style.background = "#00e676";
      }
    })
    .catch(() => {
      kiSendBtn.style.background = "#ff3d00";
    });
};

// SOS
sosBtn.onchange = () => {
  fetch("/action?sos=" + (sosBtn.checked ? 1 : 0));
};

// ОТПРАВКА КОМАНД
function sendAction(s, t, force = false) {
  let now = Date.now();

  if (force || now - lastSend > 60) {
    fetch(`/action?speed=${s}&turn=${t}`);
    lastSend = now;
  }
}

// ДЖОЙСТИК
function move(e) {
  if (!isDragging || sosBtn.checked) {
    return;
  }

  let rect = container.getBoundingClientRect();

  let x =
    (e.touches
      ? e.touches[0].clientX
      : e.clientX) -
    rect.left -
    110;

  let y =
    (e.touches
      ? e.touches[0].clientY
      : e.clientY) -
    rect.top -
    110;

  let dist = Math.sqrt(x * x + y * y);

  if (dist > maxR) {
    x *= maxR / dist;
    y *= maxR / dist;
  }

  stick.style.transform = `translate(${x}px, ${y}px)`;

  sendAction(
    Math.round(y / -maxR * 255),
    Math.round(x / maxR * 45 + 90)
  );
}

container.onmousedown =
container.ontouchstart = (e) => {
  isDragging = true;
  move(e);
};

window.onmousemove =
window.ontouchmove = move;

window.onmouseup =
window.ontouchend = () => {
  if (!isDragging) {
    return;
  }

  isDragging = false;

  stick.style.transform = "translate(0,0)";

  sendAction(0, 90, true);
};