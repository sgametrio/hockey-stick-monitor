import Vue from "vue"

class api {
   constructor() {
      const BASE_URL = "http://localhost:5000/api"
   }

   sendArduinoData(payload, arduino) {
      return Vue.axios.post(`${BASE_URL}/send_message/${arduino}`, payload)
   }

   startArduinoCommunication(payload, arduino) {
      return Vue.axios.post(`${BASE_URL}/start/${arduino}`, payload)
   }
}