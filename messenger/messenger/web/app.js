// Текущее состояние
var token = localStorage.getItem("token") || "";
var username = localStorage.getItem("username") || "";
var activePeer = "";
var lastMsgId = 0;
var pollTimer = null;
var authMode = "login"; // login или register

// --- Вспомогательные функции ---

function $(id) {
  return document.getElementById(id);
}

function show(el) { el.classList.remove("hidden"); }
function hide(el) { el.classList.add("hidden"); }

// Отправка запроса на сервер
function api(method, path, body, needAuth) {
  var headers = {};
  if (body) {
    headers["Content-Type"] = "application/x-www-form-urlencoded";
  }
  if (needAuth && token) {
    headers["Authorization"] = "TOKEN " + token;
  }

  return fetch(path, {
    method: method,
    headers: headers,
    body: body
  }).then(function(res) {
    return res.json().then(function(data) {
      if (!res.ok) throw new Error(data.error || "Ошибка");
      return data;
    });
  });
}

// --- Экран входа ---

$("btn-tab-login").onclick = function() {
  authMode = "login";
  $("btn-tab-login").classList.add("active");
  $("btn-tab-register").classList.remove("active");
  $("btn-auth").textContent = "Войти";
  $("auth-msg").textContent = "";
};

$("btn-tab-register").onclick = function() {
  authMode = "register";
  $("btn-tab-register").classList.add("active");
  $("btn-tab-login").classList.remove("active");
  $("btn-auth").textContent = "Зарегистрироваться";
  $("auth-msg").textContent = "";
};

$("btn-auth").onclick = function() {
  var user = $("input-user").value.trim();
  var pass = $("input-pass").value;
  var body = "username=" + encodeURIComponent(user) + "&password=" + encodeURIComponent(pass);

  if (authMode === "register") {
    api("POST", "/api/register", body, false)
      .then(function() {
        $("auth-msg").style.color = "green";
        $("auth-msg").textContent = "Готово! Теперь войдите.";
        authMode = "login";
        $("btn-tab-login").click();
      })
      .catch(function(e) { $("auth-msg").textContent = e.message; });
  } else {
    api("POST", "/api/login", body, false)
      .then(function(data) {
        token = data.token;
        username = data.username;
        localStorage.setItem("token", token);
        localStorage.setItem("username", username);
        openChat();
      })
      .catch(function(e) { $("auth-msg").textContent = e.message; });
  }
};

$("btn-logout").onclick = function() {
  api("POST", "/api/logout", "", true).catch(function() {});
  token = "";
  username = "";
  localStorage.removeItem("token");
  localStorage.removeItem("username");
  stopPoll();
  hide($("chat-screen"));
  show($("login-screen"));
};

// --- Экран чата ---

function openChat() {
  hide($("login-screen"));
  show($("chat-screen"));
  $("label-me").textContent = username;
  loadUsers();
  startPoll();
}

function loadUsers() {
  api("GET", "/api/users", null, true)
    .then(function(data) {
      var list = $("user-list");
      list.innerHTML = "";
      if (!data.users || data.users.length === 0) {
        list.innerHTML = "<li class='empty'>Нет других пользователей</li>";
        return;
      }
      data.users.forEach(function(name) {
        var li = document.createElement("li");
        li.textContent = name;
        if (name === activePeer) li.classList.add("active");
        li.onclick = function() { selectPeer(name); };
        list.appendChild(li);
      });
    });
}

function selectPeer(name) {
  activePeer = name;
  lastMsgId = 0;
  $("chat-title").textContent = "Чат с " + name;
  $("input-msg").disabled = false;
  $("btn-send").disabled = false;
  loadHistory();
  loadUsers();
}

function loadHistory() {
  if (!activePeer) return;
  api("GET", "/api/history?peer=" + encodeURIComponent(activePeer) + "&since=0", null, true)
    .then(function(data) {
      renderMessages(data.messages || []);
    });
}

function renderMessages(msgs) {
  var box = $("messages");
  box.innerHTML = "";

  if (msgs.length === 0) {
    box.innerHTML = "<p class='empty'>Нет сообщений</p>";
    return;
  }

  msgs.forEach(function(m) {
    var div = document.createElement("div");
    div.className = "bubble " + (m.from === username ? "me" : "other");
    div.textContent = m.from + ": " + m.text;
    box.appendChild(div);
    if (m.id > lastMsgId) lastMsgId = m.id;
  });

  box.scrollTop = box.scrollHeight;
}

$("btn-send").onclick = function() {
  var text = $("input-msg").value.trim();
  if (!text || !activePeer) return;

  var body = "to=" + encodeURIComponent(activePeer) + "&text=" + encodeURIComponent(text);
  $("input-msg").value = "";

  api("POST", "/api/send", body, true)
    .then(function() { loadHistory(); })
    .catch(function(e) { alert(e.message); });
};

$("input-msg").onkeydown = function(e) {
  if (e.key === "Enter") $("btn-send").click();
};

// Опрос сервера каждые 2 секунды (новые сообщения)
function startPoll() {
  stopPoll();
  pollTimer = setInterval(function() {
    if (!activePeer) {
      loadUsers();
      return;
    }
    api("GET", "/api/history?peer=" + encodeURIComponent(activePeer) + "&since=" + lastMsgId, null, true)
      .then(function(data) {
        if (data.messages && data.messages.length > 0) {
          var box = $("messages");
          if (box.querySelector(".empty")) box.innerHTML = "";
          data.messages.forEach(function(m) {
            var div = document.createElement("div");
            div.className = "bubble " + (m.from === username ? "me" : "other");
            div.textContent = m.from + ": " + m.text;
            box.appendChild(div);
            if (m.id > lastMsgId) lastMsgId = m.id;
          });
          box.scrollTop = box.scrollHeight;
        }
        loadUsers();
      });
  }, 2000);
}

function stopPoll() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = null;
}

// Если уже были сохранены token и username — сразу открываем чат
if (token && username) {
  openChat();
}
