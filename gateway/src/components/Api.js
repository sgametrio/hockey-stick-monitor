import Vue from "vue"

class Api {
   constructor() {
      const BASE_URL = "http://localhost:5000/api"
   }

   async sendData(payload, arduino) {
      return Vue.axios.post(`${BASE_URL}/send_message/${arduino}`, payload)
   }

   async startCommunication(payload, arduino) {
      return Vue.axios.post(`${BASE_URL}/start/${arduino}`, payload)
   }
}

export default new Api()