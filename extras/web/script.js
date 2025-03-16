const CLEAR_TIMEOUT = 2000;
const HOST_IP = window.location.host
const tabs = document.getElementsByClassName("tab");
// const // status_code = document.getElementById("status");
const message = document.getElementById("message");
const message_con = document.getElementsByClassName("message")[0];

const mqtt_host = document.getElementById("mqtt_host");
const mqtt_port = document.getElementById("mqtt_port");
const mqtt_token = document.getElementById("mqtt_token");
const mqtt_id = document.getElementById("mqtt_id");
const mqtt_pass = document.getElementById("mqtt_pass");
const tele_interval = document.getElementById("tele_interval");
const attr_interval = document.getElementById("attr_interval");
const save_config = document.getElementById("save_config");

const gen_mqtt_id = document.getElementById("gen_mqtt_id");

const wifi_list = document.getElementById("wifi_list");
const esp_wifi = document.getElementById("esp_wifi");
const esp_pass = document.getElementById("esp_pass");
const show_pass = document.getElementById("show_pass");
const submit_wifi = document.getElementById("submit_wifi");

const esp_firmware = document.getElementById("esp_firmware");
const submit_firmware = document.getElementById("submit_firmware");

const exit = document.getElementById("exit");
const restart = document.getElementById("restart");
const erase = document.getElementById("erase");

const generateClientID = () => {
  let result = '';
  const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  const charactersLength = characters.length;
  let counter = 0;
  while (counter < 20) {
    result += characters.charAt(Math.floor(Math.random() * charactersLength));
    counter += 1;
  }
  return result;
}

const getMqttConfig = () => {
  fetch(`http://${HOST_IP}/configinfo`, {
    method: "GET"
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText
    message_con.style.display = "block";
    message.innerText = response.statusText;
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    mqtt_host.value = data.mqtt_host;
    mqtt_port.value = data.mqtt_port;
    mqtt_token.value = data.mqtt_token;
    mqtt_id.value = data.mqtt_id;
    mqtt_pass.value = data.mqtt_pass;
    tele_interval.value = data.tele_interval;
    attr_interval.value = data.attr_interval;
  });
}

const getWiFi = () => {
  fetch(`http://${HOST_IP}/wifi`, {
    method: "GET"
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText
    message_con.style.display = "block";
    message.innerText = response.statusText;
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    let list_of_wifi = "<tr><th>SSID</th><th>SIGNAL</th><th>SECURITY</th></tr>";
    for (const element of data) {
      // console.log(element)
      list_of_wifi += `<tr ssid-data="${element.ssid}" onclick="fillSSID(this)"><td>${element.ssid}</td><td>${element.quality}</td><td>${element.encrytion}</td></tr>`;
    }
    wifi_list.innerHTML = list_of_wifi
  });
  // let wifis = "<tr><th>SSID</th><th>SIGNAL</th><th>SECURITY</th></tr>";
  // for (const element of wiw) {
  //   console.log(element)
  //   wifis += `<tr ssid-data="${element.ssid}" onclick="fillSSID(this)"><td>${element.ssid}</td><td>${element.quality}</td><td>${element.encrytion}</td></tr>`;
  // }
  // wifi_list.innerHTML = wifis
}

document.querySelectorAll("input[name=tab]").forEach(e => {
  e.onclick = (e) => {
    // status_code.innerText = ""
    for (const tab of tabs) tab.style.display = "none";
    if (e.target.checked) {
      document.getElementsByClassName(e.target.value)[0].style.display = "block";
      if (e.target.value === "MQTT") {
        getMqttConfig();
      } else if (e.target.value === "WiFi") {
        getWiFi();
      }
    }
  };
});

save_config.onclick = () => {
  const queryParams = new URLSearchParams();
  queryParams.append("mqtt_host", mqtt_host.value);
  queryParams.append("mqtt_port", mqtt_port.value);
  queryParams.append("mqtt_token", mqtt_token.value);
  queryParams.append("mqtt_id", mqtt_id.value);
  queryParams.append("mqtt_pass", mqtt_pass.value);
  queryParams.append("tele_interval", tele_interval.value);
  queryParams.append("attr_interval", attr_interval.value);

  fetch(`http://${HOST_IP}/configsave?${queryParams.toString()}`, {
    method: "GET",
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText
    message_con.style.display = "block";
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    // console.log(data);
    message.innerText = data.message;
  });
}

submit_wifi.onclick = () => {
  const paramWiFi = new URLSearchParams()
  paramWiFi.append("ssid", esp_wifi.value);
  paramWiFi.append("pass", esp_pass.value);
  fetch(`http://${HOST_IP}/wifisave?${paramWiFi.toString()}`, {
    method: "GET"
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText
    message_con.style.display = "block";
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    // console.log(data);
    message.innerText = data.message;
  });
}

show_pass.onchange = () => {
  show_pass.checked ? esp_pass.type = "text" : esp_pass.type = "password";
}

gen_mqtt_id.onclick = () => {
  mqtt_id.value = generateClientID();
}

esp_firmware.onchange = () => {
  submit_firmware.style.display = "initial";
}

submit_firmware.onclick = async () => {
  const data = new FormData();
  data.append('update', esp_firmware.files[0])
  await fetch(`http://${HOST_IP}/update`, {
    method: "POST",
    body: data
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText
    message_con.style.display = "block";
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    // console.log(data);
    message.innerText = data.message;
  });
}

exit.onclick = () => {
  fetch(`http://${HOST_IP}/exit`, {
    method: "GET"
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText
    message_con.style.display = "block";
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    // console.log(data);
    message.innerText = data.message;
  });
}

restart.onclick = () => {
  fetch(`http://${HOST_IP}/restart`, {
    method: "GET"
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText;
    message_con.style.display = "block";
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    // console.log(data);
    message.innerText = data.message;
  });
}

erase.onclick = () => {
  fetch(`http://${HOST_IP}/erase`, {
    method: "GET"
  }).then((response) => {
    // status_code.innerText = response.status + " " + response.statusText;
    message_con.style.display = "block";
    setTimeout(clear_message, CLEAR_TIMEOUT);
    return response.json();
  }).then((data) => {
    // console.log(data);
    message.innerText = data.message;
  });
}

const tab_handle = () => {
  const radi = document.querySelector("input[name=tab]:checked").value;
  for (const tab of tabs) {
    tab.style.display = "none";
  }
  document.getElementsByClassName(radi)[0].style.display = "block";
  getMqttConfig();
}

const clear_message = () => {
  // status_code.innerText = "";
  message.innerText = "";
  message_con.style.display = "none";
}

const fillSSID = (wifi) => {
  // console.log(wifi.getAttribute("ssid-data"))
  esp_wifi.value = wifi.getAttribute("ssid-data")
}

tab_handle();
