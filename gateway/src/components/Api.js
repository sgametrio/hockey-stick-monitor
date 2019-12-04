import Vue from "vue"
import fs from "fs"

class Api {
   constructor() {
      // this.BASE_URL = "http://localhost:5000/api"
      this.BASE_URL = "https://en9px191x4ne9.x.pipedream.net"
   }

   async sendData(payload, arduino) {
      return Vue.axios.post(`${this.BASE_URL}/send_message/${arduino}`, payload)
   }

   async startCommunication(payload, arduino) {
      return Vue.axios.post(`${this.BASE_URL}/start/${arduino}`, payload)
   }

   async stopCommunication(payload, arduino) {
      return Vue.axios.post(`${this.BASE_URL}/stop/${arduino}`, payload)
   }
}

export default new Api()