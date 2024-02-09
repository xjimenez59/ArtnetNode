
const char* VARIABLES_DEF_JSON PROGMEM = R"~(
Wifi:
  - st_ssid:
      label: Nom du réseau wifi
      default: '' 
  - st_pass:
      label: mot de passe
      default: ''  
  - st_ip:
      label: ip mask gateway (séparés par un espace)
      default: ''
  - host_name: 
      label: Nom réseau du node<br>({mac} sera remplacé par l adresse mac du node)
      default: ArtnetNode_{mac}

Leds:
  - leds_nb:
      label: Nombre de leds
      default: 50
  - leds_color_order:
      label: Schéma de couleur des leds
      options:
        - GRBW: 0
        - RGBW: 1
        - RGB: 2
        - GRB: 3
        - RBG: 4
        - GBR: 5
        - BGR: 6
        - BRG: 7
      default: GRB
      
Artnet:
  - start_universe:
      label: Numero du premier univers
      default: 0
  - frame_buf_nb:
      label: Nombre de frames dans la file tampon
      range: 5, 30, 1
      default: 10
  - frame_buf_low:
      label: Seuil de niveau bas de la file tampon
      range: 3, 15,1
  - start_delay:
      label: Délai (ms) initial avant de démarrer l'affichage
      range: 100, 1000, 50
      default: 300
)~"; 
