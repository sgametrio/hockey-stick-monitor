<template>
   <svg
      width="24"
      height="24"
      stroke="currentColor"
      :fill="fillString"
      :stroke-width="stroke"
      stroke-linecap="round"
      stroke-linejoin="round"
   >
      <use :xlink:href="path"/>
   </svg>
</template>

<script>
export default {
   name: "ColorSvg",
   props: {
      icon: {
         required: true,
         type: String
      },
      stroke: {
         type: String
      },
      fill: {
         type: String
      }
   },
   computed: {
      path () {
         return `./img/icons/feather-sprite.svg#${this.icon}`
      },
      fillString () {
         if (!this.fill) return "none"
         return "currentColor"
      }
   }
}
</script>