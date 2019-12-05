import Vue from 'vue'
import App from './App.vue'
import axios from 'axios'
import VueAxios from 'vue-axios'
// import axiosRetry from 'axios-retry'
// import VueApexCharts from 'vue-apexcharts'
 
import './registerServiceWorker'
import router from './router'
import "./components/tailwind.css"

Vue.config.productionTip = false
// axiosRetry(axios, { retries: 3 })
Vue.use(VueAxios, axios)
// Vue.use(VueApexCharts)

new Vue({
  router,
  render: h => h(App)
}).$mount('#app')
