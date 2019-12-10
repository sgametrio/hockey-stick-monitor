import * as ThreeJS from 'three'
import * as OBJLoader from 'three-obj-loader';
OBJLoader(ThreeJS)

class RenderStick {
   constructor({width = 500, height = 500}) {
      this.ThreeJS = ThreeJS
      this.scene = new this.ThreeJS.Scene()
      this.camera = new this.ThreeJS.PerspectiveCamera(75, width / height, 1, 10000)
      this.renderer = new this.ThreeJS.WebGLRenderer({ alpha: true })
      this.ambientLight = new this.ThreeJS.HemisphereLight(0x444444, 0xaaaaaa, 1)
      // this.stickLight = new this.ThreeJS.DirectionalLight(0xffffff, 1)
      this.scene.add(this.ambientLight)
      this.renderer.setSize(width, height)
      this.renderer.setClearColor(0xffffff, 1)
      this.stick = null
      let loader = new this.ThreeJS.OBJLoader()
      loader.load("model/hockey_stick.obj", (stick) => {
         this.stick = stick
         this.scene.add(this.stick)
         this.camera.position.set(1000, 1000, 1000)
         this.camera.lookAt(this.stick.position)
         // this.stickLight.position.set(-20, 100, 0)
         // this.stickLight.target.position.set(this.stick.position)
         // this.scene.add(this.stickLight)
         // this.scene.add(this.stickLight.target)
      })
   }

   rotateAndRender({x, y, z}) {
      // var Ax = BLEsense['accelerometer'].data.Ax.latest() * 0.0174533;
      // var Ay = BLEsense['accelerometer'].data.Ay.latest() * 0.0174533;
      // var Az = BLEsense['accelerometer'].data.Az.latest() * 0.0174533;
      var pitch = Math.atan2((-x), Math.sqrt(y*y + z*z))
      var roll = Math.atan2(y, z)
      this.stick.rotation.x = roll
      this.stick.rotation.y = pitch
      this.stick.rotation.z = 0
      this.renderer.render(this.scene, this.camera)
   }

   // animate() {
   //    requestAnimationFrame(this.animate)
   //    this.rotateAndRender()
   // }

   getRenderer() {
      return this.renderer
   }

   getCamera() {
      return this.camera
   }

   getScene() {
      return this.scene
   }
}

export default new RenderStick({})